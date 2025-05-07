#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "errcode.h"

//TODO: 프레임 아웃이나 오랫동안 동작했을 때 프로그램을 종료하지 않고 에러 로그는 띄우되 계속 진행하기.
// 지터가 더 이상 커지지 않게 보정은 필요. 단 무시하는 느낌. 기존 프레임을 보정해주지는 않는다.
// 특정 값 이상으로 비정상적으로 큰 경우에만 종료하도록 하기
// 에러 로그의 내용이 사이클 작업 전이냐 후냐에 따라 달라저야 함.
// 또한 POSIX 함수의 거의 발생하지 않는 예외는 고려하지 않는다. 대신 주석으로 작성하기

#define NANOSECONDS_PER_SECOND 1000000000UL
#define TICK_INTERVAL_NS       2000000L
#define BUSY_WAIT_THRESHOLD_NS 2000000L
#define LOG_INTERVAL_CYCLES    600

/* 전역 상태 변수 */
static struct {
    bool quit;                 // 종료 플래그
    errcode_t error_code;      // 종료 시 에러 코드
} g_state = {
    .quit = false,
    .error_code = ERR_NONE
};

/* 함수 선언 */
errcode_t cycle(void);
static errcode_t process_cycle_work(void);
static uint64_t get_current_time_ns(errcode_t* errcode);

// 매크로에서 do-while(0)를 사용하는 주된 이유는:
//  * 문법적 안전성 보장: 매크로가 여러 명령문을 포함할 때 세미콜론 문제나 if-else 구문에서의 문제를 방지합니다.
//  * 컴파일러 최적화: do-while(0)는 컴파일러에 의해 최적화되어 오버헤드가 없습니다.
// 매크로에서 여러 명령문을 안전하게 사용하기 위한 가장 표준적이고 안전한 방법입니다.
// 그래서 대부분의 C 라이브러리(Linux 커널, glibc 등)에서도 이 관행을 따르고 있습니다.


/* 에러 처리 및 종료 매크로 */
#define SET_ERROR_AND_EXIT(err_code) do { \
    g_state.error_code = (err_code); \
    g_state.quit = true; \
    goto exit_cycle; \
} while(0)

int main(void) {
    log_set_level(LOG_INFO);
    log_info("Program started");

    /* 에러 상태 초기화 */
    g_state.quit = false;
    g_state.error_code = ERR_NONE;

    errcode_t err = cycle();

    if (err != ERR_NONE) {
        log_error("Abnormal termination: %d", err);
        return err;
    }

    log_info("Program exited");
    return 0;
}

errcode_t cycle(void) {
    const uint64_t tick_interval = TICK_INTERVAL_NS;
    uint64_t max_cycle_ns = 0;
    uint32_t cycle_count = 0;
    errcode_t err = ERR_NONE;

    /* 첫 tick 시간 설정 */
    uint64_t next_tick = get_current_time_ns(&err);
    if (err != ERR_NONE) {
        SET_ERROR_AND_EXIT(err);
    }

    while (1) {
        /* 각 사이클 시작 시간 측정 */
        const uint64_t cycle_start = get_current_time_ns(&err);
        if (err != ERR_NONE) {
            SET_ERROR_AND_EXIT(err);
        }

        /* 작업 처리 - 내부에서도 에러 처리 가능 */
        err = process_cycle_work();
        if (err != ERR_NONE) {
            SET_ERROR_AND_EXIT(err);
        }

        /* 사이클 종료 시간 측정 */
        const uint64_t cycle_end = get_current_time_ns(&err);
        if (err != ERR_NONE) {
            SET_ERROR_AND_EXIT(err);
        }

        const uint64_t cycle_time_ns = cycle_end - cycle_start;

        /* Update max cycle time */
        if (cycle_time_ns > max_cycle_ns) {
            max_cycle_ns = cycle_time_ns;
            log_info("Max cycle time: %llu ns", max_cycle_ns);
        }

        /* Check timing constraints */
        if (cycle_time_ns > tick_interval) {
            log_error("Frame overrun: %llu ns > %llu ns",
                cycle_time_ns, tick_interval);
            SET_ERROR_AND_EXIT(ERR_TICK_TIMEOUT);
        }

        /* Calculate sleep duration */
        next_tick += tick_interval;

        uint64_t now = get_current_time_ns(&err);
        if (err != ERR_NONE) {
            SET_ERROR_AND_EXIT(err);
        }

        if (now >= next_tick) {
            log_error("Missed deadline: %llu ns", now - next_tick);
            SET_ERROR_AND_EXIT(ERR_TICK_TIMEOUT);
        }

        const uint64_t remaining_ns = next_tick - now;

        /* Sleep strategy */
        if (remaining_ns > BUSY_WAIT_THRESHOLD_NS) {
            struct timespec req = {
                // 여기의 uint64_t -> long 변환은 long이 32비트 이상이면 충분히 안전함.
                // 어차피 사이클 간격을 처리하는데는 틱 간격의 다음 자리수 넘는 값은 필요하지 않음
                // 데이터가 짤려도 괜찮음
                .tv_sec = remaining_ns / NANOSECONDS_PER_SECOND,
                .tv_nsec = remaining_ns % NANOSECONDS_PER_SECOND
            };
            if (nanosleep(&req, NULL) != 0) {
                log_error("nanosleep error: %s", strerror(errno));
                SET_ERROR_AND_EXIT(errno);
            }
        }

        /* Busy wait for precision */
        do {
            now = get_current_time_ns(&err);
            if (err != ERR_NONE) {
                SET_ERROR_AND_EXIT(err);
            }
        } while (now < next_tick);

        /* Periodic logging */
        ++cycle_count;
        if (cycle_count % LOG_INTERVAL_CYCLES == 0) {
            const uint64_t execute_ns = cycle_end - cycle_start;
            //WARN: llu은 플랫폼에 따라 lu로 처리될 수도 있어서
            //      안전한 포맷 매크로 <inttypes.h> 를 사용해야 한다고 함.
            log_info("cycle: %u \t max: %llu \t start: %llu \t end: %lld \t execute: %lld",
                cycle_count,
                max_cycle_ns,
                cycle_start,
                cycle_end,
                execute_ns,
                now);
        }

    }

exit_cycle:
    /* 에러 코드 반환 */
    return g_state.error_code;
}

/* 사이클 내의 실제 작업을 처리하는 함수 */
static errcode_t process_cycle_work(void) {
    // 임시로 더미 작업 제공
    int loop_count = (rand() % (100000 - 1000 + 1)) + 1000;
    int counter = 0;

    for (int i = 0; i < loop_count; i++) {
        counter++;
    }

    return ERR_NONE;
}


// 솔직히 초 단위를 넘어가는 시간을 관리할 필요는 없음. 이건 프로그램 반복 실행 주기를 정하는거니까
// 넘어가는건 그냥 잘라버려도 됨.
static uint64_t get_current_time_ns(errcode_t* errcode) {
    assert(errcode != NULL);

    *errcode = ERR_NONE;

    struct timespec ts;
    const int rs_stat = clock_gettime(CLOCK_MONOTONIC, &ts);
    if (rs_stat != 0) {
        log_error("clock_gettime error: %s", strerror(errno));
        *errcode = ERR_TIME_FUNC;
        return -1;
    }

    return (uint64_t)ts.tv_sec * NANOSECONDS_PER_SECOND + ts.tv_nsec;
}



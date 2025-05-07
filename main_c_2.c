/*
 * 고정된 절대 시간으로 동작하게 만들긴 했는데,
 * sleep 함수 자체가 os에 의존하는거라 스케줄링 등으로 느려질 수 있고, 그때 실패함.
 * 그래서 결국 보정할게 아니라면 몇 ns 남기고 busy wait을 쓰는게 답이긴 할듯.
 * 그럼 실시간 시간으로 sleep하거나 절대 시간으로 sleep하거나 비슷한거 아닌가?
 * 그리고 내 방식은 계속 남은 시간을 구해서 sleep 시키기 때문에 누적이 쌓이지 않음.
 * 그냥 기존 형식을 유지하는게 맞을 듯?
 */

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "errcode.h"
#include "timing_mach.h"   // github.com/Reobos/PosixMachTiming

#define NANOSECONDS_PER_SECOND 1000000000UL
#define TICK_INTERVAL_NS       2000000UL    // 2ms
#define LOG_INTERVAL_CYCLES    6

static struct {
    bool    quit;
    errcode_t error_code;
} g_state = { .quit = false, .error_code = ERR_NONE };

errcode_t cycle(void);
static uint64_t get_current_time_ns(errcode_t* errcode);
static errcode_t process_cycle_work(void);

#define SET_ERROR_AND_EXIT(err_code) do { \
    g_state.error_code = (err_code);     \
    g_state.quit = true;                 \
    goto exit_cycle;                     \
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
    const struct timespec step_ts = {
        .tv_sec  = 0,
        .tv_nsec = TICK_INTERVAL_NS
    };
    struct timespec next_tick_ts;
    uint64_t max_cycle_ns = 0;
    uint32_t cycle_count = 0;
    errcode_t err = ERR_NONE;

    // 1) 타겟 타임과 스텝 초기화
    if (itimer_start(&next_tick_ts, &step_ts) != 0) {
        log_error("itimer_start error: %s", strerror(errno));
        SET_ERROR_AND_EXIT(ERR_TIME_FUNC);
    }

    while (!g_state.quit) {
        // 2) 사이클 시작
        const uint64_t cycle_start = get_current_time_ns(&err);
        if (err != ERR_NONE) SET_ERROR_AND_EXIT(err);

        // 3) 실제 작업
        err = process_cycle_work();
        if (err != ERR_NONE) SET_ERROR_AND_EXIT(err);

        // 4) 사이클 종료 및 시간 계산
        const uint64_t cycle_end = get_current_time_ns(&err);
        if (err != ERR_NONE) SET_ERROR_AND_EXIT(err);

        uint64_t cycle_ns = cycle_end - cycle_start;
        if (cycle_ns > max_cycle_ns) {
            max_cycle_ns = cycle_ns;
            log_info("Max cycle time: %" PRIu64 " ns", max_cycle_ns);
        }
        if (cycle_ns > TICK_INTERVAL_NS) {
            log_error("Frame overrun: %" PRIu64 " > %" PRIu64 " ns",
                      cycle_ns, TICK_INTERVAL_NS);
            SET_ERROR_AND_EXIT(ERR_TICK_TIMEOUT);
        }

        // 5) 다음 절대 시각까지 sleep
        int rs = itimer_step(&next_tick_ts, &step_ts);
        if (rs != 0 && rs != EINTR) {
            log_error("itimer_step error: %s", strerror(errno));
            SET_ERROR_AND_EXIT(ERR_TIME_FUNC);
        }

        // 6) 주기 로깅
        if (++cycle_count % LOG_INTERVAL_CYCLES == 0) {
            log_info("cycle: %" PRIu32
                     "\t max: %" PRIu64
                     "\t exec: %" PRIu64,
                cycle_count,
                max_cycle_ns,
                cycle_ns);
        }
    }

exit_cycle:
    return g_state.error_code;
}

static errcode_t process_cycle_work(void) {
    int loop_count = (rand() % (100000 - 1000 + 1)) + 1000;
    int counter = 0;

    for (int i = 0; i < loop_count; i++) {
        counter++;
    }

    return ERR_NONE;
}

static uint64_t get_current_time_ns(errcode_t* errcode) {
    assert(errcode != NULL);

    *errcode = ERR_NONE;
    struct timespec ts;
    const int rs = clock_gettime(CLOCK_MONOTONIC, &ts);
    if (rs != 0) {
        log_error("clock_gettime error: %s", strerror(errno));
        *errcode = ERR_TIME_FUNC;
        return 0;
    }

    return (uint64_t)ts.tv_sec * NANOSECONDS_PER_SECOND + ts.tv_nsec;
}

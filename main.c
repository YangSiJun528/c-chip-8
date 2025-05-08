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
#include "chip8.h"

#define NANOSECONDS_PER_SECOND 1000000000UL
#define TICK_INTERVAL_NS       2000000UL
#define LOG_INTERVAL_CYCLES    500
#define TIMER_TICK_INTERVAL_NS (16666667L) // 16.666667ms in nanoseconds

/* 전역 상태 변수 */
static struct {
    bool quit;                 // 종료 플래그
    errcode_t error_code;      // 종료 시 에러 코드
} g_state = {
    .quit = false,
    .error_code = ERR_NONE
};

static struct chip8 chip8;

// CHIP-8 폰트 집합 (0–F, 총 16자 × 5바이트 = 80바이트)
static const uint8_t chip8_fontset[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

/* 함수 선언 */
errcode_t cycle(void);
static errcode_t process_cycle_work(void);
static void init_chip8(void);
static uint64_t get_current_time_ns(errcode_t* errcode);
void update_timers(uint64_t tick_interval);

/* 에러 처리 및 종료 매크로 */
#define SET_ERROR_AND_EXIT(err_code) do { \
    g_state.error_code = (err_code); \
    g_state.quit = true; \
    goto exit_cycle; \
} while(0)

int main(void) {
    log_set_level(LOG_INFO);
    log_info("Program started");

    // 랜덤 시드 설정
    srand(time(NULL));

    //chip8 초기화
    init_chip8();

    // 에러 상태 초기화
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
    uint32_t skip_count = 0;
    errcode_t err = ERR_NONE;

    // 첫 tick 시간 설정
    uint64_t next_tick = get_current_time_ns(&err);
    if (err != ERR_NONE) {
        SET_ERROR_AND_EXIT(err);
    }

    while (!g_state.quit) {
        // 각 사이클 시작 시간 측정
        const uint64_t cycle_start = get_current_time_ns(&err);
        if (err != ERR_NONE) {
            SET_ERROR_AND_EXIT(err);
        }

        // 작업 처리
        err = process_cycle_work();
        if (err != ERR_NONE) {
            SET_ERROR_AND_EXIT(err);
        }

        // 사이클 종료 시간 측정
        const uint64_t cycle_end = get_current_time_ns(&err);
        if (err != ERR_NONE) {
            SET_ERROR_AND_EXIT(err);
        }

        const uint64_t cycle_time_ns = cycle_end - cycle_start;

        if (cycle_time_ns > max_cycle_ns) {
            max_cycle_ns = cycle_time_ns;
            log_info("Max cycle time: %llu ns", max_cycle_ns);
        }

        if (cycle_time_ns > tick_interval) {
            // 틱 간격보다 사이클 수행 시간이 더 긴 경우
            // 내부 작업은 시스템 콜을 포함하지 않으므로 이런 딜레이가 생기면 안되므로 바로 실패
            log_error("Frame overrun: %llu ns > %llu ns",
                cycle_time_ns, tick_interval);
            SET_ERROR_AND_EXIT(ERR_TICK_TIMEOUT);
        }

        // 다음 틱 구하기 - 이거 여기 있는게 맞나
        update_timers(tick_interval);

        // 다음 tick 계산
        next_tick += tick_interval;

        // 현재 시간 확인
        uint64_t now = get_current_time_ns(&err);
        if (err != ERR_NONE) {
            SET_ERROR_AND_EXIT(err);
        }

        // 누락된 틱 처리
        if (now >= next_tick) {
            // 누락된 틱 카운트 추가
            uint64_t error_ns = now - next_tick;
            uint32_t missed = (uint32_t)(error_ns / tick_interval) + 1;
            skip_count += missed;
            log_error("Missed %u ticks (error: %llu ns). Total skips: %u",
                missed, error_ns, skip_count);

            // 오차 누적 방지: next_tick 보정
            next_tick += missed * tick_interval;

            // 스킵된 사이클은 처리하지 않고 다음 사이클로
            continue;
        }

        // 다음 틱 시간까지 busy-wait
        do {
            now = get_current_time_ns(&err);
            if (err != ERR_NONE) {
                SET_ERROR_AND_EXIT(err);
            }
        } while (now < next_tick);

        // 정상적으로 실행된 사이클 카운트
        ++cycle_count;
        if (cycle_count % LOG_INTERVAL_CYCLES == 0) {
            const uint64_t exec_ns = cycle_end - cycle_start;
            log_info("cycle: %u \t max: %llu \t exec: %llu \t skips: %u",
                cycle_count, max_cycle_ns, exec_ns, skip_count);
        }
    }

exit_cycle:
    return g_state.error_code;
}

// 실제 작업 처리 함수 (현재는 더미 구현)
static errcode_t process_cycle_work(void) {
    int loop_count = (rand() % (10000 - 1000 + 1)) + 1000;
    int counter = 0;

    for (int i = 0; i < loop_count; i++) {
        counter++;
    }

    return ERR_NONE;
}

static void init_chip8(void) {
    chip8.pc = 0x200;
    chip8.i  = 0;
    chip8.sp = 0;

    memset(chip8.v, 0, sizeof(chip8.v));
    memset(chip8.stack, 0, sizeof(chip8.stack));

    memset(chip8.memory, 0, sizeof(chip8.memory));
    memcpy(chip8.memory, chip8_fontset, sizeof(chip8_fontset));

    chip8.delay_timer = 0;
    chip8.sound_timer = 0;

    memset(chip8.display, 0, sizeof(chip8.display));
}

static uint64_t get_current_time_ns(errcode_t* errcode) {
    assert(errcode != NULL);

    *errcode = ERR_NONE;

    struct timespec ts;
    const int rs_stat = clock_gettime(CLOCK_MONOTONIC, &ts);
    if (rs_stat != 0) {
        log_error("clock_gettime error: %s", strerror(errno));
        *errcode = ERR_TIME_FUNC;
        return (uint64_t) -1; // 최대 값 사용
    }

    return (uint64_t)ts.tv_sec * NANOSECONDS_PER_SECOND + ts.tv_nsec;
}

//TODO: 굳이 함수로 뺄 필요까진 없었나 재사용하지도 않을껀데
void update_timers(const uint64_t tick_interval) {
    static u_int64_t accumulator = 0; // static이라 접근 제한된 전역 변수처럼 동작

    accumulator += tick_interval;

    //TODO: 1번 이상의 처리가 생기는 경우는 경고해야 하나? 느리게 수행되었다는거니까?
    // 지금 호출에서는 고정된 tick_interval 값을 증가하니까 발생하지 않아서 굳이?
    while (accumulator >= TIMER_TICK_INTERVAL_NS) {
        if (chip8.sound_timer > 0) {
            --chip8.sound_timer;
        }
        if (chip8.delay_timer > 0) {
            --chip8.delay_timer;
        }
        accumulator -= TIMER_TICK_INTERVAL_NS;
    }
}

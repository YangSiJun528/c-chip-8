#include <stdlib.h>
#include <unistd.h>
#include "log.h"
#include "errcode.h"

// 1/60초 상수화 및 정수 연산으로 변경
#define TIMESTEP_NS (16666667L) // 16.666667ms in nanoseconds

typedef struct {
    uint8_t delay_timer;
    uint8_t sound_timer;
    uint64_t accumulator; // 나노초 단위 누적기.
} time_register_t;

void update_timers(time_register_t *tr, const uint64_t dt_ns) {
    tr->accumulator += dt_ns;

    while (tr->accumulator >= TIMESTEP_NS) {
        if (tr->delay_timer > 0) {
            --tr->delay_timer;
        }
        if (tr->sound_timer > 0) {
            --tr->sound_timer;
        }
        tr->accumulator -= TIMESTEP_NS;
    }
}

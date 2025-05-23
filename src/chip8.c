#include <assert.h>
#include <stdlib.h>
#include <memory.h>
#include "chip8.h"

#include "output.h"


void initialize_chip8(chip8_t *chip8) {
    assert(chip8 != NULL);

    memset(chip8, 0, sizeof(chip8_t));
    chip8->pc = PROGRAM_START_ADDR;
    memcpy(chip8->memory + FONTSET_ADDR, chip8_fontset, sizeof(chip8_fontset));
}

void update_timers(chip8_t *chip8, uint64_t tick_interval) {
    static u_int64_t accumulator = 0; // static이라 접근 제한된 전역 변수처럼 동작

    accumulator += tick_interval;

    while (accumulator >= TIMER_TICK_INTERVAL_NS) {
        output_clear_display();
        output_print_display(chip8);
        if (chip8->sound_timer > 0) {
            --chip8->sound_timer;
            output_sound_beep();
        }
        if (chip8->delay_timer > 0) {
            --chip8->delay_timer;
        }
        accumulator -= TIMER_TICK_INTERVAL_NS;
    }
}

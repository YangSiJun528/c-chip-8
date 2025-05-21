#ifndef CHIP8_STRUCT_H
#define CHIP8_STRUCT_H

#include <stdint.h>

#include "global_config.h"

struct chip8 {
    uint8_t memory[MEMORY_MAX_SIZE];       // 최대 4kb
    uint16_t stack[STACK_SIZE];         // 2^8 - 서브루틴 중첨 처리
    uint8_t sp;                 // 스택 포인터 (2^8로 충분)
    uint16_t i;                 // 메모리 주소 저장용 레지스터
    uint16_t pc;                // pc 레지스터
    uint8_t v[NUM_REGISTERS];              // 범용 레지스터
    uint8_t delay_timer;        // 딜레이
    uint8_t sound_timer;        // 사운드
    uint8_t display[DISPLAY_WIDTH_BYTES * DISPLAY_HEIGHT]; // 64 * 32 디스플레이
    // 하드웨어 구현이라면 메모리에 포함되지 않고, CPU가 바로 입력 컨트롤러에 접근하는 식으로 될 듯?
    uint8_t keypad[NUM_KEYS]; // 키패드 상태를 저장하는 배열, 각 키의 잔여 틱 수를 저장
};

typedef struct chip8 chip8_t;

void initialize_chip8(chip8_t* chip8);

#endif // CHIP8_STRUCT_H

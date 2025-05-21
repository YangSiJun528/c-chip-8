#ifndef CHIP8_H
#define CHIP8_H

#include <stdint.h>

#define DISPLAY_WIDTH       64
#define DISPLAY_HEIGHT      32
#define DISPLAY_WIDTH_BYTES   (DISPLAY_WIDTH / 8) // 8bit = 1byte라고 가정

#define PIXEL_ON_STR   "██" // 글자는 가로로 기니까 크기를 맞추기 위해서 2글자씩 사용
#define PIXEL_OFF_STR  "  "

struct chip8 {
    uint8_t memory[4096];       // 최대 4kb
    uint16_t stack[16];         // 2^8 - 서브루틴 중첨 처리
    uint8_t sp;                 // 스택 포인터 (2^8로 충분)
    uint16_t i;                 // 메모리 주소 저장용 레지스터
    uint16_t pc;                // pc 레지스터
    uint8_t v[16];              // 범용 레지스터
    uint8_t delay_timer;        // 딜레이
    uint8_t sound_timer;        // 사운드
    uint8_t display[DISPLAY_WIDTH_BYTES * DISPLAY_HEIGHT]; // 64 * 32 디스플레이
    volatile uint8_t keypad[16]; // 키패드 상태를 저장하는 배열, 각 키의 잔여 틱 수를 저장
};

#endif // CHIP8_H

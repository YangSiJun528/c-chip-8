#ifndef CHIP8_CONFIG_H
#define CHIP8_CONFIG_H

//TODO: 이거 연관 있는 것끼리 묶고 이름 통일성 있게 바꾸기

#define FONTSET_ADDR 0x50
#define PROGRAM_START_ADDR 0x200
#define MEMORY_MAX_SIZE 0x4096
#define STACK_SIZE 16
#define NUM_REGISTERS 16
#define NUM_KEYS 16

#define NANOSECONDS_PER_SECOND 1000000000UL
#define TICK_INTERVAL_NS       2000000UL
#define LOG_INTERVAL_CYCLES    500
#define TIMER_TICK_INTERVAL_NS (16666667L) // 16.666667ms in nanoseconds
#define FONT_SIZE 40 // 0x28, 8 byte

// 입력 후 INPUT_TICK 값만큼 값을 유지. //TODO: 이름 바꾸기
#define INPUT_TICK 50 // TICK_INTERVAL_NS(2ms) * 50 = 100ms

#define LOG_LEVEL LOG_DEBUG

#define PROJECT_PATH "/Users/bonditmanager/CLionProjects/c-chip-8/"
#define ROM_PATH PROJECT_PATH "roms/"

#define DISPLAY_WIDTH       64
#define DISPLAY_HEIGHT      32
#define DISPLAY_WIDTH_BYTES   (DISPLAY_WIDTH / 8) // 8bit = 1byte라고 가정

#define PIXEL_ON_STR   "██" // 글자는 가로로 기니까 크기를 맞추기 위해서 2글자씩 사용
#define PIXEL_OFF_STR  "  "
#define PIXEL_STR_LEN  2

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
    0xF0, 0x80, 0xF0, 0x80, 0x80 // F
};

#endif // CHIP8_CONFIG_H

#ifndef CHIP8_H
#define CHIP8_H

#include <stdint.h>

#define DISPLAY_WIDTH       64
#define DISPLAY_HEIGHT      32
#define DISPLAY_WIDTH_BYTES   (DISPLAY_WIDTH / 8) // 8bit = 1byte라고 가정

#define PIXEL_ON_STR   "█" // 이거 멀티바이트 문자라 char 배열처럼 다뤄야 함.
#define PIXEL_OFF_STR  " "

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
};

static void print_border(void) {
    putchar('+');
    for (int i = 0; i < DISPLAY_WIDTH; i++)
        putchar('-');
    puts("+");
}

void print_display(const struct chip8 *chip) {
    if (!chip) {
        fputs("Error: Invalid chip8 pointer\n", stderr);
        return;
    }

    print_border();

    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
        putchar('|');
        int row_offset = y * DISPLAY_WIDTH_BYTES;

        for (int x = 0; x < DISPLAY_WIDTH; x++) {
            int byte_index = row_offset + (x >> 3);
            int bit_index = 7 - (x & 7);
            uint8_t pixel = (chip->display[byte_index] >> bit_index) & 1;
            printf("%s", pixel ? PIXEL_ON_STR : PIXEL_OFF_STR);
        }

        puts("|");
    }

    print_border();
}

#endif // CHIP8_H

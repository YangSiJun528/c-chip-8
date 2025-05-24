#include <stdio.h>
#include <unistd.h>
#include "output.h"
#include "global_config.h"
#include "chip8.h"

static void print_border(void);

void output_clear_display(void) {
    // ANSI 이스케이프 시퀀스를 사용하여 화면 지우기 및 커서 홈으로 이동
    // \x1b[2J : 화면 전체 지우기
    // \x1b[H  : 커서를 홈 위치(1,1)로 이동
    // write 함수는 버퍼링 없이 직접 출력
    write(STDOUT_FILENO, "\x1b[2J\x1b[H", 7);
}

void output_print_display(const chip8_t *chip) {
    // 이거 있어야하나
    if (!chip) {
        fprintf(stderr, "Error: output_print_display - Invalid chip8 pointer\n");
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
        putchar('|');
        putchar('\n');
    }
    print_border();
    fflush(stdout);
}

void output_sound_beep(void) {
    printf("\a"); // 비프 음 (ASCII BEL character)
    fflush(stdout); // 일부 터미널에서는 버퍼링으로 인해 즉시 소리가 나지 않을 수 있으므로 flush
}

static void print_border(void) {
    putchar('+');
    for (int i = 0; i < DISPLAY_WIDTH * PIXEL_STR_LEN; i++) {
        putchar('-');
    }
    putchar('+');
    putchar('\n');
}

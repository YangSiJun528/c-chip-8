#ifndef CHIP8_H
#define CHIP8_H

#include <stdint.h>

#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 32
#define DISPLAY_ROW_BYTES (DISPLAY_WIDTH / 8)

struct chip8 {
  uint8_t memory[4096];       // 최대 4kb
  uint16_t stack[16];         // 2^8 - 서브루틴 중첨 처리
  uint8_t sp;                 // 스택 포인터 (2^8로 충분)
  uint16_t i;                 // 메모리 주소 저장용 레지스터
  uint16_t pc;                // pc 레지스터
  uint8_t v[16];              // 범용 레지스터
  uint8_t delay_timer;        // 딜레이
  uint8_t sound_timer;        // 사운드
  uint8_t display[256];       // 64 * 32 디스플레이, 1비트=1픽셀,
                              // 32개 행에 64(8*8)개의 열
};

#endif // CHIP8_H

extern void print_display(const struct chip8* chip) {
    if (!chip) {
        printf("Error: Invalid chip8 pointer\n");
        return;
    }

    // 디스플레이 테두리 상단 출력
    printf("+");
    for (int i = 0; i < DISPLAY_WIDTH; i++) {
        printf("-");
    }
    printf("+\n");

    // 디스플레이 내용 출력
    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
        printf("|"); // 왼쪽 테두리

        // 각 행의 픽셀 출력
        for (int x = 0; x < DISPLAY_WIDTH; x++) {
            // 픽셀의 바이트 위치와 비트 위치 계산
            int byte_pos = y * DISPLAY_ROW_BYTES + (x / 8);
            int bit_pos = 7 - (x % 8); // 각 바이트에서 MSB가 왼쪽에 있으므로 7-bit_pos

            // 해당 비트가 설정되어 있는지 확인
            uint8_t pixel = (chip->display[byte_pos] >> bit_pos) & 0x01;

            // 픽셀 상태에 따라 문자 출력
            printf("%c", pixel ? 'X' : ' ');
        }

        printf("|\n"); // 오른쪽 테두리
    }

    // 디스플레이 테두리 하단 출력
    printf("+");
    for (int i = 0; i < DISPLAY_WIDTH; i++) {
        printf("-");
    }
    printf("+\n");
}

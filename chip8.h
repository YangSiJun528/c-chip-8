#ifndef CHIP8_H
#define CHIP8_H

#include <stdint.h>

struct chip8 {
  uint8_t memory[4096 / 8];   // 2^12 - 즉 주소의 크기는 12bit
  uint16_t stack[16];         // 2^8 - 서브루틴 중첨 처리
  uint8_t sp;                 // 스택 포인터 (2^8로 충분)
  uint16_t i;                 // 메모리 주소 저장용 레지스터
  uint16_t pc;                // pc 레지스터
  uint8_t v[16];              // 범용 레지스터
  uint8_t delay_timer;        // 딜레이
  uint8_t sound_timer;        // 사운드
  uint64_t display[32];       // 64 * 32 디스플레이
};

#endif // CHIP8_H

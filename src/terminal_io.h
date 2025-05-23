#ifndef TERMINAL_IO_H
#define TERMINAL_IO_H
#include "errcode.h"
#include "chip8.h"
#include <stdbool.h>

 // 초기화: raw 모드 설정, 키보드 스레드 시작
 // quit_flag는 main의 g_state.quit의 주소를 받아 스레드 종료에 사용
 errcode_t terminal_io_init(chip8_t *chip8_instance, volatile bool *quit_flag_ptr);

// 종료: 키보드 스레드 종료, 터미널 모드 복원
void terminal_io_shutdown(void);

#endif //TERMINAL_IO_H

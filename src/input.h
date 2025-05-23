#ifndef INPUT_H
#define INPUT_H

#include "errcode.h"
#include "chip8.h"
#include "global_config.h" // INPUT_TICK 사용을 위해 추가
#include <pthread.h>
#include <stdbool.h> // bool 사용을 위해 추가
#include <stdint.h>  // uint8_t, int8_t 사용을 위해 추가

errcode_t input_initialize(void);
void input_shutdown(void);

// 메인 루프에서 주기적으로 호출하여 keypad 값 감소 처리
void input_process_keys(chip8_t *chip8);

// 키 상태 확인 함수 (Opcode Ex9E, ExA1 용)
bool input_is_key_pressed(chip8_t *chip8, uint8_t key_index);
bool input_is_key_not_pressed(chip8_t *chip8, uint8_t key_index);

// 새로 눌린 키 확인 함수 (Opcode Fx0A 용)
int8_t input_get_newly_pressed_key(chip8_t *chip8);

// 키보드 스레드에서 키 상태 설정 함수
void input_set_key_down(chip8_t *chip8, int key_index);

#endif //INPUT_H

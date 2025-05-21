#include "input.h"
#include "log.h"
#include <stdio.h>   // snprintf
#include <string.h>  // strlen

pthread_mutex_t input_mutex;

errcode_t input_initialize(void) {
    pthread_mutex_init(&input_mutex, NULL);
    return ERR_NONE;
}

void input_shutdown(void) {
    pthread_mutex_destroy(&input_mutex);
    log_info("Input module shut down.");
}

// 키패드 타이머 감소
void input_process_keys(chip8_t *chip8) {
    pthread_mutex_lock(&input_mutex);
    {
        for (int i = 0; i < 16; i++) {
            if (chip8->keypad[i] > 0) {
                --chip8->keypad[i];
            }
        }
//        // 키패드 값 로깅 (특정 주기로)
//        if (cycle_count % 100 == 0) { // LOG_KEYPAD_INTERVAL_CYCLES와 같은 상수로 대체 가능
//            char keypad_log[128] = {0};
//            int offset = 0;
//
//            offset += snprintf(keypad_log + offset,
//                               sizeof(keypad_log) - offset, "Keypad: [");
//            for (int i = 0; i < 16; i++) {
//                offset += snprintf(keypad_log + offset,
//                                   sizeof(keypad_log) - offset,
//                                   "%d%s", chip8->keypad[i],
//                                   (i < 15) ? ", " : "");
//            }
//            offset += snprintf(keypad_log + offset,
//                               sizeof(keypad_log) - offset, "]");
//
//            log_debug("%s", keypad_log);
//        }
    }
    pthread_mutex_unlock(&input_mutex);
}

// 키가 눌렸는지 확인 (Opcode Ex9E 용)
bool input_is_key_pressed(chip8_t *chip8, uint8_t key_index) {
    bool pressed = false;
    pthread_mutex_lock(&input_mutex);
    pressed = (chip8->keypad[key_index] > 0);
    pthread_mutex_unlock(&input_mutex);
    return pressed;
}

// 키가 안 눌렸는지 확인 (Opcode ExA1 용)
bool input_is_key_not_pressed(chip8_t *chip8, uint8_t key_index) {
    bool not_pressed = true;
    if (key_index >= 16) { // 유효하지 않은 키 인덱스
        log_warn("Invalid key_index %u in input_is_key_not_pressed", key_index);
        return true; // 안전하게 안눌린 것으로 처리
    }
    pthread_mutex_lock(&input_mutex);
    not_pressed = (chip8->keypad[key_index] == 0);
    pthread_mutex_unlock(&input_mutex);
    return not_pressed;
}

// 새로 눌린 키의 인덱스를 반환, 없으면 -1 (Opcode Fx0A 용)
int8_t input_get_newly_pressed_key(chip8_t *chip8) {
    int8_t pressed_key_idx = -1;
    pthread_mutex_lock(&input_mutex);
    for (int i = 0; i < 16; ++i) {
        if (chip8->keypad[i] == INPUT_TICK) {
            pressed_key_idx = (int8_t)i;
            break;
        }
    }
    pthread_mutex_unlock(&input_mutex);
    return pressed_key_idx;
}

// 키보드 스레드에서 키가 눌렸음을 설정
void input_set_key_down(chip8_t *chip8, int key_index) {
    pthread_mutex_lock(&input_mutex);
    chip8->keypad[key_index] = INPUT_TICK;
    pthread_mutex_unlock(&input_mutex);
}

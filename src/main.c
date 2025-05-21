#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <termios.h>
#include <signal.h>
#include <pthread.h>


#include "log.h"
#include "errcode.h"
#include "chip8_struct.h"
#include "chip8_config.h"
#include "input.h"

/* 전역 상태 변수 */
static struct {
    bool quit; // 종료 플래그
    errcode_t error_code; // 종료 시 에러 코드
} g_state = {
    .quit = false,
    .error_code = ERR_NONE
};

// 필요에 따라 변경 가능
static const char KEY_MAPPING[16] = {
    '1', '2', '3', '4', // 0, 1, 2, 3
    'q', 'w', 'e', 'r', // 4, 5, 6, 7
    'a', 's', 'd', 'f', // 8, 9, A, B
    'z', 'x', 'c', 'v' // C, D, E, F
};

// 뮤텍스 선언 - last_key 접근 시 사용
static pthread_mutex_t input_mutex = PTHREAD_MUTEX_INITIALIZER;

static struct chip8 chip8;

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

static struct termios orig_term;

/* 함수 선언 */
errcode_t cycle(void);
static errcode_t process_cycle_work(void);
static errcode_t init_chip8(void);
static uint64_t get_current_time_ns(errcode_t *errcode);
void update_timers(uint64_t tick_interval);
void enable_raw_mode();
void disable_raw_mode();
void *keyboard_thread(void *arg);
void handle_sigint(int sig);
int get_key_index(char key);
void print_border(void);
void print_display(const struct chip8 *chip);
void clear_display(void);
void sound_beep(void);

/* 에러 처리 및 종료 매크로 */
#define SET_ERROR_AND_EXIT(err_code) do { \
    g_state.error_code = (err_code); \
    g_state.quit = true; \
    goto exit_cycle; \
} while(0)

int main(void) {
    // 로깅 전용 파일 생성 - 디스플레이 출력을 위해서 분리
    const char *log_filename = "mylog.txt";

    char log_path[512];

    const size_t total_len = strlen(PROJECT_PATH) + strlen(log_filename);
    assert(total_len < sizeof(log_path)); // 오버플로우 에러
    snprintf(log_path, sizeof(log_path), "%s%s", PROJECT_PATH, log_filename);

    FILE *logfile = fopen(log_path, "a");
    if (!logfile) {
        perror("log file open error");
        return 1;
    }
    log_add_fp(logfile, LOG_LEVEL);
    //log_set_level(LOG_LEVEL);
    log_set_level(LOG_INFO);
    log_info("Program started");

    // 랜덤 시드 설정
    srand(time(NULL));

    // 터미널 설정
    enable_raw_mode();
    // 프로그램 종료 시 터미널 설정 복원 콜백함수 등록
    atexit(disable_raw_mode);
    // SIGINT 시그널 발생 (주로 ctrl+c) 시 사용자 정의 처리
    signal(SIGINT, handle_sigint);

    // 키보드 입력 스레드 생성
    pthread_t kb_thread;
    if (pthread_create(&kb_thread, NULL, keyboard_thread, NULL) != 0) {
        log_error("Thread creation failed: %s", strerror(errno));
        return ERR_THREAD_CREATION_FAILED;
    }

    //chip8 초기화
    errcode_t init_err = init_chip8();
    if (init_err != ERR_NONE) {
        log_error("Abnormal termination: %d", init_err);
        return init_err;
    }

    input_initialize(); //TODO: 에러코드 반환함.

    // 에러 상태 초기화
    g_state.quit = false;
    g_state.error_code = ERR_NONE;

    errcode_t err = cycle();

    if (err != ERR_NONE) {
        log_error("Abnormal termination: %d", err);
        return err;
    }

    log_info("Program exited");
    return 0;
}

errcode_t cycle(void) {
    const uint64_t tick_interval = TICK_INTERVAL_NS;
    uint64_t max_cycle_ns = 0;
    uint32_t cycle_count = 0;
    uint32_t skip_count = 0;
    errcode_t err = ERR_NONE;

    // 첫 tick 시간 설정
    uint64_t next_tick = get_current_time_ns(&err);
    if (err != ERR_NONE) {
        SET_ERROR_AND_EXIT(err);
    }

    while (!g_state.quit) {
        // 각 사이클 시작 시간 측정
        const uint64_t cycle_start = get_current_time_ns(&err);
        if (err != ERR_NONE) {
            SET_ERROR_AND_EXIT(err);
        }

        // 작업 처리
        err = process_cycle_work();
        if (err != ERR_NONE) {
            SET_ERROR_AND_EXIT(err);
        }

        // 사이클 종료 시간 측정
        const uint64_t cycle_end = get_current_time_ns(&err);
        if (err != ERR_NONE) {
            SET_ERROR_AND_EXIT(err);
        }

        const uint64_t cycle_time_ns = cycle_end - cycle_start;

        if (cycle_time_ns > max_cycle_ns) {
            max_cycle_ns = cycle_time_ns;
            log_info("Max cycle time: %llu ns", max_cycle_ns);
        }

        if (cycle_time_ns > tick_interval) {
            // 틱 간격보다 사이클 수행 시간이 더 긴 경우
            // 내부 작업은 시스템 콜을 포함하지 않으므로 이런 딜레이가 생기면 안되므로 바로 실패
            log_error("Frame overrun: %llu ns > %llu ns",
                      cycle_time_ns, tick_interval);
            SET_ERROR_AND_EXIT(ERR_TICK_TIMEOUT);
        }

        // 다음 틱 구하기 - 이거 여기 있는게 맞나
        update_timers(tick_interval);

        // 다음 tick 계산
        next_tick += tick_interval;

        // 현재 시간 확인
        uint64_t now = get_current_time_ns(&err);
        if (err != ERR_NONE) {
            SET_ERROR_AND_EXIT(err);
        }

        // 누락된 틱 처리
        if (now >= next_tick) {
            // 누락된 틱 카운트 추가
            uint64_t error_ns = now - next_tick;
            uint32_t missed = (uint32_t) (error_ns / tick_interval) + 1;
            skip_count += missed;
            log_error("Missed %u ticks (error: %llu ns). Total skips: %u",
                      missed, error_ns, skip_count);

            // 오차 누적 방지: next_tick 보정
            next_tick += missed * tick_interval;

            // 스킵된 사이클은 처리하지 않고 다음 사이클로
            // TODO: 이거 뺴는게 맞을듯? 보정만 하고 성공 처리는 하긴 해야지
            continue;
        }

        // 다음 틱 시간까지 busy-wait
        do {
            now = get_current_time_ns(&err);
            if (err != ERR_NONE) {
                SET_ERROR_AND_EXIT(err);
            }
        } while (now < next_tick);

        // 정상적으로 실행된 사이클 카운트
        ++cycle_count;
        if (cycle_count % LOG_INTERVAL_CYCLES == 0) {
            const uint64_t exec_ns = cycle_end - cycle_start;
            log_debug("cycle: %u \t max: %llu \t exec: %llu \t skips: %u",
                      cycle_count, max_cycle_ns, exec_ns, skip_count);
        }

        // 키패드 상태 업데이트: 눌린 키의 타이머 감소
        input_process_keys(&chip8);
    }

exit_cycle:
    input_shutdown();
    return g_state.error_code;
}

// 실제 작업 처리 함수 (현재는 더미 구현)
static errcode_t process_cycle_work(void) {
    const uint16_t opcode = (chip8.memory[chip8.pc] << 8)
                            | chip8.memory[chip8.pc + 1];
    log_trace("opcode 0x%04x", opcode);

    chip8.pc += 2;

    //return ERR_NONE;

    switch (opcode & 0xF000) {
        case 0x0000: {
            switch (opcode) {
                case 0x00E0: {
                    memset(chip8.display, 0, sizeof(chip8.display));
                    break;
                }
                case 0x00EE: {
                    chip8.pc = chip8.stack[chip8.sp];
                    --chip8.sp;
                    break;
                }
                default: {
                    // 0NNN & default
                    // 기계어 루틴 실행 - 구현 X
                    /* This instruction is only used on the old computers
                     * on which Chip-8 was originally implemented.
                     * It is ignored by modern interpreters. */
                    assert(false);
                }
            }
            break;
        }
        case 0x1000: {
            // 1nnn - JP addr
            const u_int16_t nnn = opcode & 0x0FFF;
            chip8.pc = nnn;
            break;
        }
        case 0x2000: {
            // 2nnn - CALL addr
            ++chip8.sp;
            chip8.stack[chip8.sp] = chip8.pc;

            const u_int16_t nnn = opcode & 0x0FFF;
            chip8.pc = nnn;
            break;
        }
        case 0x3000: {
            // 3xkk - SE Vx, byte
            const uint8_t vx = (opcode & 0x0F00) >> 8;
            const uint8_t nn = (opcode & 0x00FF); // 사실 & 없어도 될거같긴 함.
            if (chip8.v[vx] == nn) {
                chip8.pc += 2;
            }
            break;
        }
        case 0x4000: {
            // 4xkk - SNE Vx, byte
            const uint8_t vx = (opcode & 0x0F00) >> 8;
            const uint8_t nn = (opcode & 0x00FF);
            if (chip8.v[vx] != nn) {
                chip8.pc += 2;
            }
            break;
        }
        case 0x5000: {
            // 5xy0 - SE Vx, Vy
            assert((opcode & 0x000F) == 0);

            const uint8_t vx = (opcode & 0x0F00) >> 8;
            const uint8_t vy = (opcode & 0x00F0) >> 4;
            if (chip8.v[vx] == chip8.v[vy]) {
                chip8.pc += 2;
            }
            break;
        }
        case 0x6000: {
            // 6xkk - LD Vx, byte
            const uint8_t vx = (opcode & 0x0F00) >> 8;
            const uint8_t kk = (opcode & 0x00FF);
            chip8.v[vx] = kk;
            break;
        }
        case 0x7000: {
            // 7xkk - ADD Vx, byte
            const uint8_t vx = (opcode & 0x0F00) >> 8;
            const uint8_t kk = (opcode & 0x00FF);
            chip8.v[vx] = chip8.v[vx] + kk;
            break;
        }
        case 0x8000: {
            // 8xyn(N = 0-6, E)
            const uint8_t vx = (opcode & 0x0F00) >> 8;
            const uint8_t vy = (opcode & 0x00F0) >> 4;
            const uint8_t n = (opcode & 0x000F);

            assert(n == 0 || n == 1 || n == 2 || n == 3 ||
                n == 4 || n == 5 || n == 6 || n == 7 || n == 0xE);

            switch (n) {
                case 0x00: {
                    // 8xy0 - LD Vx, Vy
                    chip8.v[vx] = chip8.v[vy];
                    break;
                }
                case 0x01: {
                    // 8xy1 - OR Vx, Vy
                    chip8.v[vx] = chip8.v[vx] | chip8.v[vy];
                    break;
                }
                case 0x02: {
                    // 8xy2 - AND Vx, Vy
                    chip8.v[vx] = chip8.v[vx] & chip8.v[vy];
                    break;
                }
                case 0x03: {
                    // 8xy3 - XOR Vx, Vy
                    chip8.v[vx] = chip8.v[vx] ^ chip8.v[vy];
                    break;
                }
                case 0x04: {
                    // 8xy4 - ADD Vx, Vy
                    uint16_t sum = chip8.v[vx] + chip8.v[vy];

                    // set VF = carry
                    chip8.v[0xF] = (sum > 0xFF) ? 1 : 0;

                    chip8.v[vx] = sum & 0xFF;
                    break;
                }
                case 0x05: {
                    // 8xy5 - SUB Vx, Vy

                    // set VF = NOT borrow
                    chip8.v[0xF] = (chip8.v[vx] > chip8.v[vy]);

                    chip8.v[vx] = chip8.v[vx] - chip8.v[vy];
                    break;
                }
                case 0x06: {
                    // 8xy6 - SHR Vx {, Vy}
                    // Shift Right, {, Vy}는 옵션. 일부 구현해서 사용함.

                    // set VF = least-significant bit
                    chip8.v[0xF] = chip8.v[vx] & 0x1;

                    // Vx를 2로 나눔
                    chip8.v[vx] = chip8.v[vx] >> 1;
                    break;
                }
                case 0x07: {
                    // 8xy7 - SUBN Vx, Vy
                    // Subtract with Borrow

                    // set VF = NOT borrow
                    chip8.v[0xF] = (chip8.v[vy] > chip8.v[vx]);

                    chip8.v[vx] = chip8.v[vy] - chip8.v[vx];
                    break;
                }
                case 0x0E: {
                    // 8xyE - SHL Vx {, Vy}
                    // Shift Left

                    // set VF = most significant bit
                    chip8.v[0xF] = (chip8.v[vx] & 0x80) >> 7;

                    // Vx를 2로 곱함
                    chip8.v[vx] = chip8.v[vx] << 1;
                    break;
                }
                default:
                    assert(false);
            }
            break;
        }
        case 0x9000: {
            // 9xy0 - SNE Vx, Vy
            const uint8_t vx = (opcode & 0x0F00) >> 8;
            const uint8_t vy = (opcode & 0x00F0) >> 4;
            if (chip8.v[vx] != chip8.v[vy]) {
                chip8.pc += 2;
            }
            break;
        }
        case 0xA000: {
            // Annn - LD I, addr
            const u_int16_t nnn = opcode & 0x0FFF;
            chip8.i = nnn;
            break;
        }
        case 0xB000: {
            // Bnnn - JP V0, addr
            const u_int16_t nnn = opcode & 0x0FFF;
            chip8.pc = nnn + chip8.v[0];
            break;
        }
        case 0xC000: {
            // Cxkk - RND Vx, byte
            const uint8_t vx = (opcode & 0x0F00) >> 8;
            const uint8_t kk = (opcode & 0x00FF);
            chip8.v[vx] = (u_int8_t) (rand() % 256) & kk;
            break;
        }
        case 0xD000: {
            // Dxyn - DRW Vx, Vy, nibble: draw n-byte sprite at (Vx, Vy)
            const uint8_t vx = (opcode & 0x0F00) >> 8;
            const uint8_t vy = (opcode & 0x00F0) >> 4;
            const uint8_t n = (opcode & 0x000F);

            const uint8_t x = chip8.v[vx];
            const uint8_t y = chip8.v[vy];
            assert(x < 64 && y < 32);

            bool is_collision = false;
            for (uint8_t byte = 0; byte < n; ++byte) {
                const uint8_t sprite_byte = chip8.memory[chip8.i + byte];

                // Y축 wrapping: 화면 아래를 넘어가면 위로
                const uint8_t py = (y + byte) % 32;

                for (uint8_t bit = 0; bit < 8; ++bit) {
                    const uint8_t sprite_pixel =
                            (sprite_byte >> (7 - bit)) & 0x1;

                    // 충돌 감지에서 이 값이 0인 경우를 고려하지 않아도 되고 연산이 줄어 효율적
                    // XOR 연산은 특성 상 값이 0이라면 조기종료 가능
                    if (!sprite_pixel) continue;

                    // X축 wrapping: 화면 우측을 넘어가면 좌측으로
                    const uint8_t px = (x + bit) % 64;

                    // 버퍼 인덱스 계산: 몇 행 몇 바이트
                    const uint16_t byte_index = (py * 64 + px) / 8;
                    const uint8_t bit_mask = 1 << (7 - (px % 8));

                    // 충돌 감지: 기존 픽셀이 켜져 있는지
                    if (chip8.display[byte_index] & bit_mask) {
                        is_collision = true;
                    }
                    // XOR 그리는 이유는 그냥 요구사항임
                    chip8.display[byte_index] ^= bit_mask;
                }
            }
            // VF에 충돌 플래그 기록
            chip8.v[0xF] = is_collision ? 1 : 0;
            break;
        }
        case 0xE000: {
            const uint8_t vx = (opcode & 0x0F00) >> 8;
            const uint8_t keypad_idx = chip8.v[vx];

            if ((opcode & 0x00FF) == 0x009E) {
                if (input_is_key_pressed(&chip8, keypad_idx)) {
                    chip8.pc += 2;
                }
            }
            if ((opcode & 0x00FF) == 0x00A1) {
                if (input_is_key_not_pressed(&chip8, keypad_idx)) {
                    chip8.pc += 2;
                }
            }
            break;
        }
        case 0xF000: {
            const uint8_t vx = (opcode & 0x0F00) >> 8;
            switch (opcode & 0x00FF) {
                case 0x0007: {
                    // Fx07 - LD Vx, DT
                    chip8.v[vx] = chip8.delay_timer;
                    break;
                }
                case 0x000A: {
                    // Fx0A - LD Vx, K
                    const uint8_t vx = (opcode & 0x0F00) >> 8;
                    u_int8_t pressed_key_idx =
                        input_get_newly_pressed_key(&chip8);

                    if (pressed_key_idx != -1) {
                        chip8.v[vx] = pressed_key_idx;
                    } else {
                        // 신규 입력이 없으면 이 명령어를 다시 수행하도록 pc값 수정
                        chip8.pc -= 2;
                    }
                    break;
                }
                case 0x0015: {
                    // Fx15 - LD DT, Vx
                    chip8.delay_timer = chip8.v[vx];
                    break;
                }
                case 0x0018: {
                    // Fx18 - LD ST, Vx
                    chip8.sound_timer = chip8.v[vx];
                    break;
                }
                case 0x001E: {
                    // Fx1E - ADD I, Vx
                    chip8.i += chip8.v[vx];
                    break;
                }
                case 0x0029: {
                    // Fx29 - LD F, Vx

                    // 각 문자는 5바이트
                    chip8.i = FONTSET_ADDR + (chip8.v[vx] * FONT_SIZE / 8);
                    break;
                }
                case 0x0033: {
                    // Fx33 - LD B, Vx
                    chip8.memory[chip8.i] = chip8.v[vx] / 100;
                    chip8.memory[chip8.i + 1] = (chip8.v[vx] % 100) / 10;
                    chip8.memory[chip8.i + 2] = chip8.v[vx] % 10;
                    break;
                }
                case 0x0055: {
                    // Fx55 - LD [I], Vx
                    for (uint8_t r = 0; r <= vx; r++) {
                        chip8.memory[chip8.i + r] = chip8.v[r];
                    }
                    break;
                }
                case 0x0065: {
                    // Fx65 - LD Vx, [I]
                    for (uint8_t r = 0; r <= vx; r++) {
                        chip8.v[r] = chip8.memory[chip8.i + r];
                    }
                    break;
                }
                default:
                    return ERR_NO_SUPPORTED_OPCODE;
            }
            break;
        }
    }
    return ERR_NONE;
}

static errcode_t init_chip8(void) {
    memset(&chip8, 0, sizeof(chip8));

    chip8.pc = PROGRAM_START_ADDR;

    memcpy(chip8.memory + FONTSET_ADDR, chip8_fontset, sizeof(chip8_fontset));

    const char *rom_filename = "Pong (1 player).ch8";

    const int DEST_SIZE = 512;
    char rom_path[DEST_SIZE];

    const size_t len_path = strlen(ROM_PATH);
    const size_t len_file = strlen(rom_filename);
    const size_t total_len = len_path + len_file;  // 널 문자는 아래에서 직접 추가

    // 버퍼 오버플로우 방지: 널 문자 포함해서도 DEST_SIZE 이하인지 확인
    assert(total_len + 1 <= DEST_SIZE);

    // 1) ROM_PATH 복사 (널 문자는 아직 붙이지 않음)
    strncpy(rom_path, ROM_PATH, DEST_SIZE - 1);
    // 2) rom_filename 복사 (남은 공간에)
    strncpy(rom_path + len_path, rom_filename, DEST_SIZE - len_path - 1);
    // 3) 마지막 바이트에 널 명시
    rom_path[DEST_SIZE - 1] = '\0';

    FILE *rom = fopen(rom_path, "rb");
    if (!rom) {
        log_error("Failed to open ROM: %s", strerror(errno));
        return ERR_FILE_NOT_FOUND;
    }

    fseek(rom, 0, SEEK_END);
    long rom_size = ftell(rom);
    fseek(rom, 0, SEEK_SET);

    size_t n = fread(chip8.memory + PROGRAM_START_ADDR, 1, rom_size, rom);
    fclose(rom);
    if (n != (size_t) rom_size || n > MEMORY_MAX_SIZE) {
        log_error("Abnormal ROM size: %ld", rom_size);
        fclose(rom);
        return ERR_ROM_TOO_LARGE;
    }

    return ERR_NONE;
}


static uint64_t get_current_time_ns(errcode_t *errcode) {
    assert(errcode != NULL);

    *errcode = ERR_NONE;

    struct timespec ts;
    const int rs_stat = clock_gettime(CLOCK_MONOTONIC, &ts);
    if (rs_stat != 0) {
        log_error("clock_gettime error: %s", strerror(errno));
        *errcode = ERR_TIME_FUNC;
        return (uint64_t) -1; // 최대 값 사용
    }

    return (uint64_t) ts.tv_sec * NANOSECONDS_PER_SECOND + ts.tv_nsec;
}

//TODO: 굳이 함수로 뺄 필요까진 없었나 재사용하지도 않을껀데
void update_timers(const uint64_t tick_interval) {
    static u_int64_t accumulator = 0; // static이라 접근 제한된 전역 변수처럼 동작

    accumulator += tick_interval;

    //TODO: 1번 이상의 처리가 생기는 경우는 경고해야 하나? 느리게 수행되었다는거니까?
    // 지금 호출에서는 고정된 tick_interval 값을 증가하니까 발생하지 않아서 굳이?
    while (accumulator >= TIMER_TICK_INTERVAL_NS) {
        if (chip8.sound_timer > 0) {
            --chip8.sound_timer;
            sound_beep();
        }
        if (chip8.delay_timer > 0) {
            --chip8.delay_timer;
        }
        //TODO: 여기 리팩토링좀 하기
        clear_display();
        print_display(&chip8);
        accumulator -= TIMER_TICK_INTERVAL_NS;
    }
}

// 키보드 입력 처리 스레드 함수
void *keyboard_thread(void *arg) {
    (void) arg;
    while (!g_state.quit) {
        char c;
        //TODO: ssize_t기 뭐지
        const ssize_t bytes_read = read(STDIN_FILENO, &c, 1);
        if (bytes_read > 0) {
            // C가 keypad 값 안에 속하는지 체크, 아니면 스킵
            const int key_idx = get_key_index(c);
            if (key_idx >= 0) {
                input_set_key_down(&chip8, key_idx);
                log_trace("key pressed: %c (ASCII: %d), keypad[%d] = %d",
                          c, (int)c, key_idx, chip8.keypad[key_idx]);
            }
        }
    }
    return NULL;
}

// 입력된 키에 해당하는 CHIP-8 키패드 인덱스를 반환
int get_key_index(char key) {
    // 대문자인 경우에만 소문자로 변환 (비트 OR 연산 사용)
    if (key >= 'A' && key <= 'Z') {
        key |= 0x20;
    }

    for (int i = 0; i < 16; i++) {
        char mapped = KEY_MAPPING[i];
        if (mapped >= 'A' && mapped <= 'Z') {
            mapped |= 0x20;
        }

        if (mapped == key) {
            return i;
        }
    }
    return -1;
}

void handle_sigint(int sig) {
    log_debug("here is handle_sigint()");
    (void) sig;
    g_state.quit = true;
}

// 터미널 raw 모드 진입
void enable_raw_mode() {
    // 파일 디스크립터 fildes에 연결된 터미널의 현재 속성을 읽어서 orig_term*에 저장
    tcgetattr(STDIN_FILENO, &orig_term);
    struct termios raw = orig_term;
    // ECHO: 입력한 문자를 화면에 에코(반복) 출력
    // ICANON: 캐논컬(라인 단위) 모드를 사용해 줄바꿈 전까지 입력을 버퍼에 저장
    // ISIG: Ctrl-C, Ctrl-Z 등 시그널 생성 키를 처리
    // 위 3가지 flag 비활성화
    // c_lflag: local flags
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    // c_cc: control chars
    raw.c_cc[VMIN] = 0; //VMIN: 비캐논컬 모드에서 read()가 반환하기 위한 최소 바이트 수
    raw.c_cc[VTIME] = 0; //VTIME: 비캐논컬 모드에서 read()가 타임아웃하기 전 대기 시간
    // raw 설정을 STDIN_FILENO에 적용 (TCSANOW: 즉시 적용 flag)
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

// 터미널 원복
void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_term);
}

void print_border(void) {
    putchar('+');
    for (int i = 0; i < DISPLAY_WIDTH * 2; i++)
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

void clear_display(void) {
    write(STDOUT_FILENO, "\x1b[2J", 4); // 전체 지우기
    write(STDOUT_FILENO, "\x1b[H", 3); // 커서 홈
}

void sound_beep(void) {
    printf("\a"); // 비프 음 내기
    fflush(stdout); // 버퍼 비우기 - 바로 출력
}

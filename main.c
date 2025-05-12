#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "errcode.h"
#include "chip8.h"

#define NANOSECONDS_PER_SECOND 1000000000UL
#define TICK_INTERVAL_NS       2000000UL
#define LOG_INTERVAL_CYCLES    500
#define TIMER_TICK_INTERVAL_NS (16666667L) // 16.666667ms in nanoseconds
#define FONTSET_ADDR 0x50 // TODO: 이름 Base addr이 더 나은듯?
#define FONT_SIZE 40 // 0x28, 8 byte
#define PROGRAM_START_ADDR 0x200
#define MEMORY_MAX_SIZE 0x4096
#define LOG_LEVEL LOG_TRACE

/* 전역 상태 변수 */
static struct {
    bool quit; // 종료 플래그
    errcode_t error_code; // 종료 시 에러 코드
} g_state = {
    .quit = false,
    .error_code = ERR_NONE
};

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

/* 함수 선언 */
errcode_t cycle(void);
static errcode_t process_cycle_work(void);
static errcode_t init_chip8(void);
static uint64_t get_current_time_ns(errcode_t *errcode);
void update_timers(uint64_t tick_interval);

/* 에러 처리 및 종료 매크로 */
#define SET_ERROR_AND_EXIT(err_code) do { \
    g_state.error_code = (err_code); \
    g_state.quit = true; \
    goto exit_cycle; \
} while(0)

int main(void) {
    log_set_level(LOG_LEVEL);
    log_info("Program started");

    // 랜덤 시드 설정
    srand(time(NULL));

    //chip8 초기화
    init_chip8();

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
        if (cycle_count % LOG_INTERVAL_CYCLES == 0 || LOG_LEVEL == LOG_TRACE) {
            const uint64_t exec_ns = cycle_end - cycle_start;
            log_info("cycle: %u \t max: %llu \t exec: %llu \t skips: %u",
                     cycle_count, max_cycle_ns, exec_ns, skip_count);
        }

        if (LOG_LEVEL == LOG_TRACE && cycle_count % 600 == 0) {
            print_display(&chip8);
        }
    }

exit_cycle:
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
                n == 4 || n == 5 || n == 6 || n == 0xE);

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
                    chip8.v[vx] = chip8.v[vx] + chip8.v[vy];

                    // set VF = carry
                    if ((chip8.v[vx] & 0xFF00) > 0) {
                        chip8.v[0xF] = 1;
                        chip8.v[vx] = chip8.v[vx] & 0x00FF;
                    } else {
                        chip8.v[0xF] = 0;
                    }
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

                    chip8.v[vx] = chip8.v[vx] >> 1;
                    break;
                }
                case 0x07: {
                    // 8xy7 - SUBN Vx, Vy
                    // Subtract with Borrow

                    // set VF = NOT borrow
                    chip8.v[0xF] = (chip8.v[vy] > chip8.v[vx]);

                    chip8.v[vy] = chip8.v[vy] - chip8.v[vx];
                    break;
                }
                case 0x0E: {
                    // 8xyE - SHL Vx {, Vy}
                    // Shift Left

                    // set VF = most significant bit
                    chip8.v[0xF] = chip8.v[vx] & 0x8000;

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
            chip8.v[0xF] = is_collision? 1 : 0;
            break;
        }
        case 0xE000: {
            if ((opcode & 0x00FF) == 0x009E) {
                // Ex9E - SKP Vx
                // const uint8_t vx = (opcode & 0x0F00) >> 8;
                //TODO: vx 키보드 눌림 체크 & 눌렸으면 PC 증가
                assert(false);
            }
            if ((opcode & 0x00FF) == 0x00A1) {
                // ExA1 - SKNP Vx
                // const uint8_t vx = (opcode & 0x0F00) >> 8;
                //TODO: vx 키보드 눌림 체크 & 안눌렸으면 PC 증가
                assert(false);
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
                    // TODO: 키 입력을 기다렸다가, 눌린 키 값을 Vx에 저장
                    //  Blocking 키 대기 로직 필요
                    assert(false);
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
                    chip8.i = FONTSET_ADDR + (chip8.v[vx] * FONT_SIZE);
                    break;
                }
                case 0x0033: {
                    // Fx33 - LD B, Vx
                    chip8.memory[chip8.i] = vx / 100;
                    chip8.memory[chip8.i + 1] = (vx % 100) / 10;
                    chip8.memory[chip8.i + 2] = vx % 10;
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
        default:
            return ERR_NO_SUPPORTED_OPCODE;
        }
    }
    return ERR_NONE;
}

static errcode_t init_chip8(void) {
    memset(&chip8, 0, sizeof(chip8));

    chip8.pc = PROGRAM_START_ADDR;

    memcpy(chip8.memory + FONTSET_ADDR, chip8_fontset, sizeof(chip8_fontset));

    const char *rom_path =
            "/Users/bonditmanager/CLionProjects/c-chip-8/IBM_Logo.ch8";
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
        }
        if (chip8.delay_timer > 0) {
            --chip8.delay_timer;
        }
        accumulator -= TIMER_TICK_INTERVAL_NS;
    }
}

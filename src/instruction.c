#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include "instruction.h"
#include "input.h"
#include "log.h"

//TODO: 이거 좀 더 함수 추상화 단계별로 나눠서 처리하면 좋을 듯


errcode_t execute_instruction(chip8_t *chip8) {
    const uint16_t opcode = (chip8->memory[chip8->pc] << 8)
                            | chip8->memory[chip8->pc + 1];
    log_trace("opcode 0x%04x", opcode);

    chip8->pc += 2;

    //return ERR_NONE;

    switch (opcode & 0xF000) {
        case 0x0000: {
            switch (opcode) {
                case 0x00E0: {
                    memset(chip8->display, 0, sizeof(chip8->display));
                    break;
                }
                case 0x00EE: {
                    chip8->pc = chip8->stack[chip8->sp];
                    --chip8->sp;
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
            chip8->pc = nnn;
            break;
        }
        case 0x2000: {
            // 2nnn - CALL addr
            ++chip8->sp;
            chip8->stack[chip8->sp] = chip8->pc;

            const u_int16_t nnn = opcode & 0x0FFF;
            chip8->pc = nnn;
            break;
        }
        case 0x3000: {
            // 3xkk - SE Vx, byte
            const uint8_t vx = (opcode & 0x0F00) >> 8;
            const uint8_t nn = (opcode & 0x00FF); // 사실 & 없어도 될거같긴 함.
            if (chip8->v[vx] == nn) {
                chip8->pc += 2;
            }
            break;
        }
        case 0x4000: {
            // 4xkk - SNE Vx, byte
            const uint8_t vx = (opcode & 0x0F00) >> 8;
            const uint8_t nn = (opcode & 0x00FF);
            if (chip8->v[vx] != nn) {
                chip8->pc += 2;
            }
            break;
        }
        case 0x5000: {
            // 5xy0 - SE Vx, Vy
            assert((opcode & 0x000F) == 0);

            const uint8_t vx = (opcode & 0x0F00) >> 8;
            const uint8_t vy = (opcode & 0x00F0) >> 4;
            if (chip8->v[vx] == chip8->v[vy]) {
                chip8->pc += 2;
            }
            break;
        }
        case 0x6000: {
            // 6xkk - LD Vx, byte
            const uint8_t vx = (opcode & 0x0F00) >> 8;
            const uint8_t kk = (opcode & 0x00FF);
            chip8->v[vx] = kk;
            break;
        }
        case 0x7000: {
            // 7xkk - ADD Vx, byte
            const uint8_t vx = (opcode & 0x0F00) >> 8;
            const uint8_t kk = (opcode & 0x00FF);
            chip8->v[vx] = chip8->v[vx] + kk;
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
                    chip8->v[vx] = chip8->v[vy];
                    break;
                }
                case 0x01: {
                    // 8xy1 - OR Vx, Vy
                    chip8->v[vx] = chip8->v[vx] | chip8->v[vy];
                    break;
                }
                case 0x02: {
                    // 8xy2 - AND Vx, Vy
                    chip8->v[vx] = chip8->v[vx] & chip8->v[vy];
                    break;
                }
                case 0x03: {
                    // 8xy3 - XOR Vx, Vy
                    chip8->v[vx] = chip8->v[vx] ^ chip8->v[vy];
                    break;
                }
                case 0x04: {
                    // 8xy4 - ADD Vx, Vy
                    uint16_t sum = chip8->v[vx] + chip8->v[vy];

                    // set VF = carry
                    chip8->v[0xF] = (sum > 0xFF) ? 1 : 0;

                    chip8->v[vx] = sum & 0xFF;
                    break;
                }
                case 0x05: {
                    // 8xy5 - SUB Vx, Vy

                    // set VF = NOT borrow
                    chip8->v[0xF] = (chip8->v[vx] > chip8->v[vy]);

                    chip8->v[vx] = chip8->v[vx] - chip8->v[vy];
                    break;
                }
                case 0x06: {
                    // 8xy6 - SHR Vx {, Vy}
                    // Shift Right, {, Vy}는 옵션. 일부 구현해서 사용함.

                    // set VF = least-significant bit
                    chip8->v[0xF] = chip8->v[vx] & 0x1;

                    // Vx를 2로 나눔
                    chip8->v[vx] = chip8->v[vx] >> 1;
                    break;
                }
                case 0x07: {
                    // 8xy7 - SUBN Vx, Vy
                    // Subtract with Borrow

                    // set VF = NOT borrow
                    chip8->v[0xF] = (chip8->v[vy] > chip8->v[vx]);

                    chip8->v[vx] = chip8->v[vy] - chip8->v[vx];
                    break;
                }
                case 0x0E: {
                    // 8xyE - SHL Vx {, Vy}
                    // Shift Left

                    // set VF = most significant bit
                    chip8->v[0xF] = (chip8->v[vx] & 0x80) >> 7;

                    // Vx를 2로 곱함
                    chip8->v[vx] = chip8->v[vx] << 1;
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
            if (chip8->v[vx] != chip8->v[vy]) {
                chip8->pc += 2;
            }
            break;
        }
        case 0xA000: {
            // Annn - LD I, addr
            const u_int16_t nnn = opcode & 0x0FFF;
            chip8->i = nnn;
            break;
        }
        case 0xB000: {
            // Bnnn - JP V0, addr
            const u_int16_t nnn = opcode & 0x0FFF;
            chip8->pc = nnn + chip8->v[0];
            break;
        }
        case 0xC000: {
            // Cxkk - RND Vx, byte
            const uint8_t vx = (opcode & 0x0F00) >> 8;
            const uint8_t kk = (opcode & 0x00FF);
            chip8->v[vx] = (u_int8_t) (rand() % 256) & kk;
            break;
        }
        case 0xD000: {
            // Dxyn - DRW Vx, Vy, nibble: draw n-byte sprite at (Vx, Vy)
            const uint8_t vx = (opcode & 0x0F00) >> 8;
            const uint8_t vy = (opcode & 0x00F0) >> 4;
            const uint8_t n = (opcode & 0x000F);

            const uint8_t x = chip8->v[vx];
            const uint8_t y = chip8->v[vy];
            assert(x < 64 && y < 32);

            bool is_collision = false;
            for (uint8_t byte = 0; byte < n; ++byte) {
                const uint8_t sprite_byte = chip8->memory[chip8->i + byte];

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
                    if (chip8->display[byte_index] & bit_mask) {
                        is_collision = true;
                    }
                    // XOR 그리는 이유는 그냥 요구사항임
                    chip8->display[byte_index] ^= bit_mask;
                }
            }
            // VF에 충돌 플래그 기록
            chip8->v[0xF] = is_collision ? 1 : 0;
            break;
        }
        case 0xE000: {
            const uint8_t vx = (opcode & 0x0F00) >> 8;
            const uint8_t keypad_idx = chip8->v[vx];

            if ((opcode & 0x00FF) == 0x009E) {
                if (input_is_key_pressed(chip8, keypad_idx)) {
                    chip8->pc += 2;
                }
            }
            if ((opcode & 0x00FF) == 0x00A1) {
                if (input_is_key_not_pressed(chip8, keypad_idx)) {
                    chip8->pc += 2;
                }
            }
            break;
        }
        case 0xF000: {
            const uint8_t vx = (opcode & 0x0F00) >> 8;
            switch (opcode & 0x00FF) {
                case 0x0007: {
                    // Fx07 - LD Vx, DT
                    chip8->v[vx] = chip8->delay_timer;
                    break;
                }
                case 0x000A: {
                    // Fx0A - LD Vx, K
                    u_int8_t pressed_key_idx =
                        input_get_newly_pressed_key(chip8);

                    if (pressed_key_idx != -1) {
                        chip8->v[vx] = pressed_key_idx;
                    } else {
                        // 신규 입력이 없으면 이 명령어를 다시 수행하도록 pc값 수정
                        chip8->pc -= 2;
                    }
                    break;
                }
                case 0x0015: {
                    // Fx15 - LD DT, Vx
                    chip8->delay_timer = chip8->v[vx];
                    break;
                }
                case 0x0018: {
                    // Fx18 - LD ST, Vx
                    chip8->sound_timer = chip8->v[vx];
                    break;
                }
                case 0x001E: {
                    // Fx1E - ADD I, Vx
                    chip8->i += chip8->v[vx];
                    break;
                }
                case 0x0029: {
                    // Fx29 - LD F, Vx

                    // 각 문자는 5바이트
                    chip8->i = FONTSET_ADDR + (chip8->v[vx] * FONT_SIZE / 8);
                    break;
                }
                case 0x0033: {
                    // Fx33 - LD B, Vx
                    chip8->memory[chip8->i] = chip8->v[vx] / 100;
                    chip8->memory[chip8->i + 1] = (chip8->v[vx] % 100) / 10;
                    chip8->memory[chip8->i + 2] = chip8->v[vx] % 10;
                    break;
                }
                case 0x0055: {
                    // Fx55 - LD [I], Vx
                    for (uint8_t r = 0; r <= vx; r++) {
                        chip8->memory[chip8->i + r] = chip8->v[r];
                    }
                    break;
                }
                case 0x0065: {
                    // Fx65 - LD Vx, [I]
                    for (uint8_t r = 0; r <= vx; r++) {
                        chip8->v[r] = chip8->memory[chip8->i + r];
                    }
                    break;
                }
                default: {
                    return ERR_NO_SUPPORTED_OPCODE;
                }
            }
            break;
        }
        default: {
            return ERR_NO_SUPPORTED_OPCODE;
        }
    }
    return ERR_NONE;
}

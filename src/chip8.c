#include <assert.h>
#include <stdlib.h>
#include <memory.h>
#include "chip8.h"

#include "output.h"


void initialize_chip8(chip8_t *chip8) {
    assert(chip8 != NULL);

    memset(chip8, 0, sizeof(chip8_t));
    chip8->pc = PROGRAM_START_ADDR;
    memcpy(chip8->memory + FONTSET_ADDR, chip8_fontset, sizeof(chip8_fontset));
}

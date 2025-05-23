#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include "chip8_struct.h"
#include "errcode.h"

errcode_t execute_instruction(chip8_t *chip8);

#endif // INSTRUCTION_H

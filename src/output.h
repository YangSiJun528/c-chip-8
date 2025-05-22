#ifndef OUTPUT_H
#define OUTPUT_H

#include <stdbool.h>
#include "chip8_struct.h"

void output_clear_display(void);
void output_print_display(const chip8_t *chip);
void output_sound_beep(void);

#endif // OUTPUT_H

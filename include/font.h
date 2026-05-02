#pragma once
#include <stdint.h>

void font_draw_char(uint32_t *fb, uint32_t stride, uint32_t x, uint32_t y,
                    char symbol, uint32_t fg, uint32_t bg);
void font_draw_str(uint32_t *fb, uint32_t stride, uint32_t x, uint32_t y,
                   const char *string, uint32_t fg, uint32_t bg);

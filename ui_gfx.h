#ifndef UI_GFX_H
#define UI_GFX_H

#include <stdint.h>
#include "hal.h"

uint32_t ui_blend(uint32_t a, uint32_t b, int pct_b);
void ui_fill_rrect(int x, int y, int w, int h, int r, uint32_t rgb);
void ui_draw_rrect(int x, int y, int w, int h, int r, uint32_t rgb);
void ui_card(int x, int y, int w, int h, int r, uint32_t bg, uint32_t border, uint32_t shadow, int selected, uint32_t accent);
void ui_progress_bar(int x, int y, int w, int h, int pct, uint32_t track, uint32_t fill);
void ui_draw_ascii(int x, int y, const char *str, int scale, uint32_t color);
int  ui_ascii_width(const char *str, int scale);
void ui_draw_badge(int x, int y, const char *tag, uint32_t bg, uint32_t fg);
void ui_draw_str(int x, int y, const char *utf8, int max_w, uint32_t rgb);

#endif

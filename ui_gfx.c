#include "ui_gfx.h"
#include <stdlib.h>
#include <string.h>

static const uint8_t FONT5X7[][5] = {
    {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E},
    {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36},
    {0x3E,0x41,0x41,0x41,0x22}, {0x7F,0x41,0x41,0x22,0x1C},
    {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A}, {0x7F,0x08,0x08,0x08,0x7F},
    {0x00,0x41,0x7F,0x41,0x00}, {0x20,0x40,0x41,0x3F,0x01},
    {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F},
    {0x3E,0x41,0x41,0x41,0x3E}, {0x7F,0x09,0x09,0x09,0x06},
    {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7F,0x01,0x01},
    {0x3F,0x40,0x40,0x40,0x3F}, {0x1F,0x20,0x40,0x20,0x1F},
    {0x3F,0x40,0x38,0x40,0x3F}, {0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43},
    {0x23,0x13,0x08,0x64,0x62}, {0x20,0x10,0x08,0x04,0x02},
    {0x08,0x08,0x3E,0x08,0x08}, {0x08,0x08,0x08,0x08,0x08},
    {0x00,0x36,0x36,0x00,0x00}, {0x00,0x60,0x60,0x00,0x00},
    {0x00,0x00,0x5F,0x00,0x00}
};

static int aidx(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'Z') return 10 + (ch - 'A');
    if (ch >= 'a' && ch <= 'z') return 10 + (ch - 'a');
    switch (ch) {
        case '%': return 36; case '/': return 37; case '+': return 38;
        case '-': return 39; case ':': return 40; case '.': return 41;
        case '!': return 42; default: return -1;
    }
}

uint32_t ui_blend(uint32_t a, uint32_t b, int pct_b) {
    if (pct_b <= 0) return a;
    if (pct_b >= 100) return b;
    int pa = 100 - pct_b;
    int r  = ((int)((a >> 16) & 0xFF) * pa + (int)((b >> 16) & 0xFF) * pct_b) / 100;
    int g  = ((int)((a >> 8) & 0xFF)  * pa + (int)((b >> 8) & 0xFF)  * pct_b) / 100;
    int bl = ((int)(a & 0xFF)         * pa + (int)(b & 0xFF)         * pct_b) / 100;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)bl;
}

void ui_fill_rrect(int x, int y, int w, int h, int r, uint32_t rgb) {
    if (w <= 0 || h <= 0) return;
    if (r <= 1) { hal_fill_rect(x, y, w, h, rgb); return; }
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    hal_fill_rect(x, y + r, w, h - r * 2, rgb);
    int r2 = r * r;
    for (int dy = 0; dy < r; dy++) {
        int cy = r - 1 - dy, dx = 0;
        while ((dx + 1) * (dx + 1) + cy * cy <= r2) dx++;
        int inset = r - dx; if (inset < 0) inset = 0;
        hal_fill_rect(x + inset, y + dy, w - inset * 2, 1, rgb);
        hal_fill_rect(x + inset, y + h - 1 - dy, w - inset * 2, 1, rgb);
    }
}

void ui_draw_rrect(int x, int y, int w, int h, int r, uint32_t rgb) {
    if (w <= 2 || h <= 2) return;
    if (r <= 1) { hal_draw_rect(x, y, w, h, rgb); return; }
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    hal_fill_rect(x + r, y, w - r * 2, 1, rgb);
    hal_fill_rect(x + r, y + h - 1, w - r * 2, 1, rgb);
    hal_fill_rect(x, y + r, 1, h - r * 2, rgb);
    hal_fill_rect(x + w - 1, y + r, 1, h - r * 2, rgb);
    int r2 = r * r;
    for (int dy = 0; dy < r; dy++) {
        int cy = r - 1 - dy, dx = 0;
        while ((dx + 1) * (dx + 1) + cy * cy <= r2) dx++;
        int inset = r - dx;
        hal_put_pixel(x + inset, y + dy, rgb);
        hal_put_pixel(x + w - 1 - inset, y + dy, rgb);
        hal_put_pixel(x + inset, y + h - 1 - dy, rgb);
        hal_put_pixel(x + w - 1 - inset, y + h - 1 - dy, rgb);
    }
}

void ui_card(int x, int y, int w, int h, int r, uint32_t bg, uint32_t border, uint32_t shadow, int selected, uint32_t accent) {
    ui_fill_rrect(x + 2, y + 3, w, h, r, shadow);
    ui_fill_rrect(x, y, w, h, r, bg);
    if (selected) {
        ui_draw_rrect(x, y, w, h, r, accent);
        ui_draw_rrect(x + 1, y + 1, w - 2, h - 2, r > 1 ? r - 1 : 0, accent);
    } else {
        ui_draw_rrect(x, y, w, h, r, border);
    }
}

void ui_progress_bar(int x, int y, int w, int h, int pct, uint32_t track, uint32_t fill) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    int r = h / 2;
    ui_fill_rrect(x, y, w, h, r, track);
    int fw = (w * pct) / 100;
    if (fw > 0) {
        if (fw < h) fw = h;
        if (fw > w) fw = w;
        ui_fill_rrect(x, y, fw, h, r, fill);
    }
}

void ui_draw_ascii(int x, int y, const char *str, int scale, uint32_t color) {
    if (!str || scale < 1) return;
    int cx = x;
    for (int i = 0; str[i]; i++) {
        int id = aidx(str[i]);
        if (id >= 0) {
            for (int c = 0; c < 5; c++) {
                uint8_t bits = FONT5X7[id][c];
                for (int r = 0; r < 7; r++)
                    if (bits & (1u << r)) hal_fill_rect(cx + c * scale, y + r * scale, scale, scale, color);
            }
        }
        cx += 6 * scale;
    }
}

int ui_ascii_width(const char *str, int scale) {
    if (!str) return 0;
    int n = (int)strlen(str);
    return n > 0 ? (n * 6 * scale - scale) : 0;
}

void ui_draw_badge(int x, int y, const char *tag, uint32_t bg, uint32_t fg) {
    int tw = ui_ascii_width(tag, 1);
    ui_fill_rrect(x, y, tw + 10, 16, 5, bg);
    ui_draw_ascii(x + 5, y + 5, tag, 1, fg);
}

void ui_draw_str(int x, int y, const char *utf8, int max_w, uint32_t rgb) {
    if (utf8 && utf8[0]) hal_draw_line_utf8(x, y, utf8, (int)strlen(utf8), max_w, rgb);
}


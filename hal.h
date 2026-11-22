#ifndef HAL_H
#define HAL_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    EKEY_NONE = 0,
    EKEY_UP,
    EKEY_DOWN,
    EKEY_LEFT,
    EKEY_RIGHT,
    EKEY_OK,
    EKEY_SPACE,     /* 暫停 / 繼續 */
    EKEY_BACK,      /* ESC */
    EKEY_BACKSPACE,
    EKEY_TAB1,
    EKEY_TAB2,
    EKEY_TAB3,
    EKEY_TAB4,
    EKEY_SPEED,     /* S */
    EKEY_AUTO,      /* A: 派遣最近單位 */
    EKEY_CALL,      /* C: 接聽 911 來電 */
    EKEY_CHAR
} E911Key;

int  hal_get_width(void);
int  hal_get_height(void);

void hal_clear(uint32_t rgb);
void hal_put_pixel(int x, int y, uint32_t rgb);
void hal_fill_rect(int x, int y, int w, int h, uint32_t rgb);
void hal_draw_rect(int x, int y, int w, int h, uint32_t rgb);
void hal_fill_circle(int cx, int cy, int r, uint32_t rgb);
void hal_draw_circle(int cx, int cy, int r, uint32_t rgb);
void hal_draw_line(int x0, int y0, int x1, int y1, uint32_t rgb);

void hal_set_text_bg(uint32_t rgb);
void hal_draw_line_utf8(int x, int y, const char *utf8_text, int byte_len, int max_px_w, uint32_t rgb);
void hal_clear_text_in_rect(int rx, int ry, int rw, int rh);
void hal_dim_rect(int rx, int ry, int rw, int rh);
void hal_present(void);

uint32_t    hal_get_seed(void);
const char *hal_get_save_path(void);
void        hal_request_quit(void);

#endif

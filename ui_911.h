#ifndef UI_911_H
#define UI_911_H

#include "hal.h"

void ui911_init(void);
void ui911_shutdown(void);
void ui911_on_key(E911Key key, char raw_char);
void ui911_on_touch(int tx, int ty);
void ui911_tick(int dt_ms);
int  ui911_needs_redraw(void);
void ui911_render(void);

#endif

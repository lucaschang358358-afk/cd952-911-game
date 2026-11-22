#include "ui_911.h"
#include "sim_911.h"
#include "city_map.h"
#include "calls_911.h"
#include "ui_gfx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    SCR_TITLE = 0,
    SCR_PRESHIFT,
    SCR_SHIFT,
    SCR_CALL_MODAL,
    SCR_DEBRIEF,
    SCR_CHEAT,
    SCR_PAUSE
} E911Screen;

/* 明亮現代 CAD 調度配色 */
#define COL_BG      0xEEF3F8
#define COL_WHITE   0xFFFFFF
#define COL_SUB     0xF8FAFC
#define COL_BORDER  0xCBD5E1
#define COL_SHADOW  0xC5D1DE
#define COL_NAV     0x0F2540
#define COL_TEXT    0x0F172A
#define COL_MUTED   0x64748B
#define COL_BLUE    0x2563EB
#define COL_GREEN   0x16A34A
#define COL_AMBER   0xD97706
#define COL_RED     0xDC2626
#define COL_PURPLE  0x7C3AED

static E911Screen g_scr = SCR_TITLE;
static int g_redraw = 1;
static int g_city_sel = 0;
static int g_pre_tab = 0;   /* 0=買載具, 1=升級裝備, 2=地圖部署 */
static int g_shop_scroll = 0;
static int g_unit_sel = 0, g_unit_scroll = 0;
static int g_inc_sel = -1, g_inc_scroll = 0;
static int g_pause_sel = 0;
static int g_render_acc = 0;

static char g_toast[128] = {0};
static int  g_toast_ms  = 0;

static void set_toast(const char *s) { snprintf(g_toast, sizeof(g_toast), "%.127s", s ? s : ""); g_toast_ms = 4500; }

static int has_save_file(void) {
    FILE *f = fopen(hal_get_save_path(), "rb"); if (!f) return 0;
    uint32_t m = 0; int ok = (fread(&m, sizeof(m), 1, f) == 1 && m == SAVE_911_MAGIC);
    fclose(f); return ok;
}

static uint32_t dept_color(int d) {
    if (d == DEPT_POLICE) return COL_BLUE;
    if (d == DEPT_FIRE)   return COL_AMBER;
    return COL_GREEN;
}

static int list_active_incidents(int *out_idxs, int max_n) {
    int cnt = 0;
    for (int prio = 1; prio <= 3; prio++) {
        for (int i = 0; i < MAX_INCIDENTS && cnt < max_n; i++) {
            if (g_911.incidents[i].active && g_911.incidents[i].priority == prio) out_idxs[cnt++] = i;
        }
    }
    return cnt;
}

void ui911_init(void) {
    sim911_init();
    g_scr = SCR_TITLE;
    g_city_sel = 0;
    g_redraw = 1;
}

void ui911_shutdown(void) { sim911_save(); }
int  ui911_needs_redraw(void) { return g_redraw; }

void ui911_tick(int dt_ms) {
    if (g_toast_ms > 0) {
        g_toast_ms -= dt_ms;
        if (g_toast_ms <= 0) { g_toast[0] = '\0'; g_redraw = 1; }
    }
    if (g_scr == SCR_SHIFT && g_911.in_shift && !g_911.paused) {
        int major = sim911_tick(dt_ms);
        if (!g_911.in_shift) { g_scr = SCR_DEBRIEF; g_redraw = 1; return; }
        g_render_acc += dt_ms;
        if (major || g_render_acc >= 350) { g_render_acc = 0; g_redraw = 1; }
    }
}

/* ============================================================
 * 按鍵事件
 * ============================================================ */
void ui911_on_key(E911Key key, char raw_char) {
    (void)raw_char;
    g_redraw = 1;
    int is_ok = (key == EKEY_OK);

    if (g_scr == SCR_TITLE) {
        if (key == EKEY_LEFT) g_city_sel = (g_city_sel + NUM_CITIES - 1) % NUM_CITIES;
        else if (key == EKEY_RIGHT) g_city_sel = (g_city_sel + 1) % NUM_CITIES;
        else if (key == EKEY_BACK) hal_request_quit();
        else if (is_ok || key == EKEY_SPACE) {
            sim911_new_career(g_city_sel);
            g_pre_tab = 0; g_scr = SCR_PRESHIFT;
        }
        return;
    }

    if (g_scr == SCR_CHEAT) { if (key == EKEY_BACK) g_scr = g_911.in_shift ? SCR_SHIFT : SCR_PRESHIFT; return; }
    if (g_scr == SCR_CALL_MODAL) {
        if (key == EKEY_BACK) { g_scr = SCR_SHIFT; return; }
        if (g_911.call.step < 2) {
            if (key == EKEY_TAB1) sim911_call_choose(0);
            else if (key == EKEY_TAB2) sim911_call_choose(1);
            else if (key == EKEY_TAB3) sim911_call_choose(2);
        } else {
            char m[96];
            if (key == EKEY_OK || key == EKEY_TAB1) { sim911_call_resolve(1, m, sizeof(m)); set_toast(m); g_scr = SCR_SHIFT; }
            else if (key == EKEY_TAB2) { sim911_call_resolve(0, m, sizeof(m)); set_toast(m); g_scr = SCR_SHIFT; }
        }
        return;
    }

    if (g_scr == SCR_DEBRIEF) {
        if (is_ok || key == EKEY_SPACE || key == EKEY_BACK) { g_pre_tab = 0; g_scr = SCR_PRESHIFT; }
        return;
    }

    if (g_scr == SCR_PAUSE) {
        if (key == EKEY_BACK) { g_scr = g_911.in_shift ? SCR_SHIFT : SCR_PRESHIFT; return; }
        if (key == EKEY_UP) g_pause_sel = (g_pause_sel + 2) % 3;
        else if (key == EKEY_DOWN) g_pause_sel = (g_pause_sel + 1) % 3;
        else if (is_ok) {
            if (g_pause_sel == 0) g_scr = g_911.in_shift ? SCR_SHIFT : SCR_PRESHIFT;
            else if (g_pause_sel == 1) { sim911_save(); g_scr = SCR_TITLE; }
            else { sim911_save(); hal_request_quit(); }
        }
        return;
    }

    if (g_scr == SCR_PRESHIFT) {
        if (key == EKEY_BACK) { g_pause_sel = 0; g_scr = SCR_PAUSE; return; }
        if (key == EKEY_TAB1) g_pre_tab = 0;
        else if (key == EKEY_TAB2) g_pre_tab = 1;
        else if (key == EKEY_TAB3) g_pre_tab = 2;
        else if (key == EKEY_SPACE) { sim911_start_shift(); g_inc_sel = -1; g_scr = SCR_SHIFT; }
        return;
    }

    /* SCR_SHIFT 快捷鍵 */
    if (key == EKEY_SPACE) { g_911.paused = !g_911.paused; return; }
    if (key == EKEY_SPEED) { g_911.speed_idx = (g_911.speed_idx + 1) % 3; return; }
    if (key == EKEY_CALL && g_911.call.ringing) { g_911.paused = 1; g_scr = SCR_CALL_MODAL; return; }
    if (key == EKEY_AUTO && g_inc_sel >= 0 && g_911.incidents[g_inc_sel].active) {
        char m[96]; sim911_dispatch_nearest_for_inc(g_inc_sel, m, sizeof(m)); set_toast(m); return;
    }
    if (key == EKEY_BACK) { g_pause_sel = 0; g_scr = SCR_PAUSE; }
}

/* ============================================================
 * 全畫面觸控互動
 * ============================================================ */
void ui911_on_touch(int tx, int ty) {
    int sw = hal_get_width(), sh = hal_get_height();
    g_redraw = 1;

    if (g_scr == SCR_TITLE) {
        /* 4 座城市卡片 (y = 134..274) */
        if (ty >= 130 && ty <= 278) {
            int c = (tx - 24) / 190;
            if (c >= 0 && c < NUM_CITIES) g_city_sel = c;
            return;
        }
        int hs = has_save_file();
        if (ty >= 312 && ty <= 364) {
            if (hs && tx < sw / 2) { if (sim911_load()) { g_scr = g_911.in_shift ? SCR_SHIFT : SCR_PRESHIFT; } }
            else { sim911_new_career(g_city_sel); g_pre_tab = 0; g_scr = SCR_PRESHIFT; }
        } else if (ty >= 382 && ty <= 426) hal_request_quit();
        return;
    }

    if (g_scr == SCR_CHEAT) {
        int mw = 580, mh = 360, mx = (sw - mw) / 2, my = (sh - mh) / 2;
        if (tx < mx || tx > mx + mw || ty < my || ty > my + mh) { g_scr = g_911.in_shift ? SCR_SHIFT : SCR_PRESHIFT; return; }
        int cw = (mw - 48) / 2;
        char m[96];
        if (ty >= my + 56 && ty <= my + 104) {
            if (tx < mx + mw / 2) { g_911.budget += 50000; set_toast("[外掛] 預算注入 +$50,000！"); }
            else { g_911.budget += 500000; set_toast("[外掛] 預算注入 +$500,000！"); }
            sim911_save(); return;
        }
        if (ty >= my + 114 && ty <= my + 162) {
            if (tx < mx + mw / 2) { g_911.cheat_inf_budget = !g_911.cheat_inf_budget; set_toast(g_911.cheat_inf_budget ? "[外掛] 無限市府預算已開啟！" : "[外掛] 已關閉無限預算"); }
            else { g_911.cheat_god_units = !g_911.cheat_god_units; set_toast(g_911.cheat_god_units ? "[外掛] 載具無敵無限油已開啟！" : "[外掛] 已關閉載具無敵"); }
            sim911_save(); return;
        }
        if (ty >= my + 172 && ty <= my + 220) {
            if (tx < mx + mw / 2) { g_911.weather = WTH_CLEAR; set_toast("[外掛] 已強制切換為晴朗天氣 (直升機解禁)"); }
            else { g_911.reputation = 100; sim911_refuel_rest_all(); set_toast("[外掛] 聲譽 100% 且全單位瞬間回滿補給！"); }
            return;
        }
        if (ty >= my + 230 && ty <= my + 278) {
            if (tx < mx + mw / 2) {
                if (g_911.in_shift) { sim911_trigger_major_disaster(m, sizeof(m)); set_toast(m); }
                else set_toast("請先開始值班再觸發重大複合災難！");
            } else { g_scr = g_911.in_shift ? SCR_SHIFT : SCR_PRESHIFT; }
            return;
        }
        (void)cw;
        if (ty >= my + mh - 50) g_scr = g_911.in_shift ? SCR_SHIFT : SCR_PRESHIFT;
        return;
    }

    if (g_scr == SCR_CALL_MODAL) {
        int mw = 640, mh = 410, mx = (sw - mw) / 2, my = (sh - mh) / 2;
        if (g_911.call.step < 2) {
            for (int i = 0; i < 3; i++) {
                int ry = my + 216 + i * 54;
                if (tx >= mx + 20 && tx <= mx + mw - 20 && ty >= ry && ty <= ry + 44) { sim911_call_choose(i); return; }
            }
        } else {
            char m[96];
            if (ty >= my + 320 && ty <= my + 378) {
                if (tx < mx + mw / 2) { sim911_call_resolve(1, m, sizeof(m)); set_toast(m); }
                else { sim911_call_resolve(0, m, sizeof(m)); set_toast(m); }
                g_911.paused = 0; g_scr = SCR_SHIFT;
            }
        }
        return;
    }

    if (g_scr == SCR_DEBRIEF) {
        if (ty >= 360) { g_pre_tab = 0; g_scr = SCR_PRESHIFT; }
        return;
    }

    if (g_scr == SCR_PAUSE) {
        int pw = 340, ph = 220, px = (sw - pw) / 2, py = (sh - ph) / 2;
        if (tx < px || tx > px + pw || ty < py || ty > py + ph) { g_scr = g_911.in_shift ? SCR_SHIFT : SCR_PRESHIFT; return; }
        int idx = (ty - (py + 56)) / 48;
        if (idx >= 0 && idx < 3) { g_pause_sel = idx; ui911_on_key(EKEY_OK, 0); }
        return;
    }

    /* 班前整備畫面觸控 */
    if (g_scr == SCR_PRESHIFT) {
        if (ty <= 42) {
            if (tx >= sw - 260 && tx < sw - 176) { g_scr = SCR_CHEAT; return; }
            if (tx >= sw - 170) { sim911_start_shift(); g_inc_sel = -1; g_scr = SCR_SHIFT; return; }
            return;
        }
        if (ty >= 48 && ty <= 82) {
            int t = (tx - 16) / 180;
            if (t >= 0 && t < 3) g_pre_tab = t;
            return;
        }
        char m[96];
        if (g_pre_tab == 0) {
            if (ty >= 92 && ty < sh - 46) {
                int r = (ty - 92) / 68, idx = g_shop_scroll + r;
                if (r >= 0 && r < 5 && idx < NUM_VEH_TYPES && tx >= sw - 130) {
                    sim911_buy_vehicle(idx, m, sizeof(m)); set_toast(m);
                }
            } else if (ty >= sh - 46) {
                if (tx < 160 && g_shop_scroll > 0) g_shop_scroll -= 5;
                else if (tx > sw - 160 && g_shop_scroll + 5 < NUM_VEH_TYPES) g_shop_scroll += 5;
                if (g_shop_scroll < 0) g_shop_scroll = 0;
            }
        } else if (g_pre_tab == 1) {
            if (ty >= 92 && ty < sh - 46) {
                int r = (ty - 92) / 68, idx = g_unit_scroll + r;
                if (r >= 0 && r < 5 && idx < g_911.unit_count) {
                    g_unit_sel = idx;
                    if (tx >= sw - 336 && tx < sw - 230) { sim911_upgrade_unit(idx, 0, m, sizeof(m)); set_toast(m); }
                    else if (tx >= sw - 224 && tx < sw - 118) { sim911_upgrade_unit(idx, 1, m, sizeof(m)); set_toast(m); }
                    else if (tx >= sw - 112) { sim911_upgrade_unit(idx, 2, m, sizeof(m)); set_toast(m); }
                }
            } else if (ty >= sh - 46) {
                if (tx < 160 && g_unit_scroll > 0) g_unit_scroll -= 5;
                else if (tx > sw - 160 && g_unit_scroll + 5 < g_911.unit_count) g_unit_scroll += 5;
                if (g_unit_scroll < 0) g_unit_scroll = 0;
            }
        } else {
            /* 地圖預先部署：左選單位，右點地圖設待命點! */
            if (tx < 236 && ty >= 92 && ty < sh - 40) {
                int r = (ty - 92) / 48, idx = g_unit_scroll + r;
                if (r >= 0 && r < 7 && idx < g_911.unit_count) g_unit_sel = idx;
            } else if (tx >= 244 && ty >= 92 && ty < sh - 40 && g_unit_sel < g_911.unit_count) {
                int gx, gy;
                city_px_to_grid(tx, ty, 244, 92, sw - 260, sh - 134, &gx, &gy);
                sim911_set_unit_home(g_unit_sel, gx, gy, m, sizeof(m));
                set_toast(m);
            }
        }
        return;
    }

    /* SCR_SHIFT 值班主畫面觸控 */
    if (ty <= 40) {
        if (g_911.call.ringing && tx >= sw - 406 && tx < sw - 294) { g_911.paused = 1; g_scr = SCR_CALL_MODAL; return; }
        if (tx >= sw - 288 && tx < sw - 216) { g_scr = SCR_CHEAT; return; }
        if (tx >= sw - 210 && tx < sw - 136) { g_911.speed_idx = (g_911.speed_idx + 1) % 3; return; }
        if (tx >= sw - 130 && tx < sw - 66)  { g_911.paused = !g_911.paused; return; }
        if (tx >= sw - 62) { g_pause_sel = 0; g_scr = SCR_PAUSE; return; }
        return;
    }

    char m[96];
    /* 左側 CAD 面板 (x < 252) */
    if (tx < 252) {
        /* 上半：案件列表 (y = 46..234) */
        if (ty >= 46 && ty <= 202) {
            int list[MAX_INCIDENTS], cnt = list_active_incidents(list, MAX_INCIDENTS);
            int r = (ty - 68) / 44, pos = g_inc_scroll + r;
            if (r >= 0 && r < 3 && pos < cnt) g_inc_sel = list[pos];
            return;
        }
        if (ty > 202 && ty <= 236) {
            if (g_inc_sel >= 0 && g_911.incidents[g_inc_sel].active) {
                sim911_dispatch_nearest_for_inc(g_inc_sel, m, sizeof(m)); set_toast(m);
            } else set_toast("請先點選一個案件再自動派遣");
            return;
        }
        /* 下半：單位列表 (y = 244..444) -> 點待命單位立刻派往選中案件! */
        if (ty >= 264 && ty <= 446) {
            int r = (ty - 264) / 36, idx = g_unit_scroll + r;
            if (r >= 0 && r < 5 && idx < g_911.unit_count) {
                g_unit_sel = idx;
                if (tx >= 190) { sim911_recall_unit(idx, m, sizeof(m)); set_toast(m); }
                else if (g_inc_sel >= 0 && g_911.incidents[g_inc_sel].active) {
                    sim911_dispatch_unit(idx, g_inc_sel, m, sizeof(m)); set_toast(m);
                } else set_toast("請先點地圖或左側選一個案件！");
            }
            return;
        }
        if (ty > 446) {
            if (tx < 126 && g_unit_scroll > 0) g_unit_scroll -= 5;
            else if (tx >= 126 && g_unit_scroll + 5 < g_911.unit_count) g_unit_scroll += 5;
            if (g_unit_scroll < 0) g_unit_scroll = 0;
        }
        return;
    }

    /* 右側城市地圖點擊：直接點選最近的案件! */
    int mx = 258, my = 46, mw = sw - 266, mh = sh - 108;
    if (tx >= mx && tx <= mx + mw && ty >= my && ty <= my + mh) {
        int best = -1, best_d = 28 * 28;
        for (int i = 0; i < MAX_INCIDENTS; i++) {
            if (!g_911.incidents[i].active) continue;
            int ix, iy; city_grid_to_px(g_911.incidents[i].gx, g_911.incidents[i].gy, mx, my, mw, mh, &ix, &iy);
            int d2 = (tx - ix) * (tx - ix) + (ty - iy) * (ty - iy);
            if (d2 < best_d) { best_d = d2; best = i; }
        }
        if (best >= 0) {
            g_inc_sel = best;
            char selmsg[128]; snprintf(selmsg, sizeof(selmsg), "已選取 #%d %.45s，點左側單位或 [自動派車 A] 支援", g_911.incidents[best].id_num, g_911.incidents[best].title);
            set_toast(selmsg);
        }
    }
}

/* ============================================================
 * 介面渲染
 * ============================================================ */
static void render_units_and_incidents_on_map(int mx, int my, int mw, int mh, int show_home_only) {
    /* 1. 畫所有單位的 A* 行駛路線 */
    if (!show_home_only) {
        for (int i = 0; i < g_911.unit_count; i++) {
            const DispatchUnit *u = &g_911.units[i];
            if (!u->active || u->status == USTAT_IDLE || u->status == USTAT_ONSCENE || u->status == USTAT_DISABLED) continue;
            uint32_t lc = dept_color(u->dept);
            int px0, py0; city_grid_to_px(u->gx, u->gy, mx, my, mw, mh, &px0, &py0);
            if (u->is_heli) {
                int tx, ty; city_grid_to_px(u->dest_x, u->dest_y, mx, my, mw, mh, &tx, &ty);
                hal_draw_line(px0, py0, tx, ty, lc);
            } else {
                for (int k = u->path_idx; k < u->path_len; k++) {
                    int nx, ny; city_grid_to_px(u->path_x[k], u->path_y[k], mx, my, mw, mh, &nx, &ny);
                    hal_draw_line(px0, py0, nx, ny, lc);
                    px0 = nx; py0 = ny;
                }
            }
        }
    }

    /* 2. 畫事件標記 (P1紅 / P2橘 / P3藍 + 生命條) */
    if (!show_home_only) {
        for (int i = 0; i < MAX_INCIDENTS; i++) {
            const Incident *inc = &g_911.incidents[i];
            if (!inc->active) continue;
            int ix, iy; city_grid_to_px(inc->gx, inc->gy, mx, my, mw, mh, &ix, &iy);
            uint32_t col = (inc->priority == 1) ? COL_RED : ((inc->priority == 2) ? COL_AMBER : COL_BLUE);
            if (i == g_inc_sel) hal_draw_circle(ix, iy, 12, 0x0F172A);
            hal_fill_circle(ix, iy, 9, col);
            hal_draw_circle(ix, iy, 9, COL_WHITE);
            char pt[4]; snprintf(pt, sizeof(pt), "P%d", inc->priority);
            ui_draw_ascii(ix - 5, iy - 3, pt, 1, COL_WHITE);
            ui_progress_bar(ix - 12, iy + 11, 24, 4, inc->vital_hp, 0x1E293B, inc->vital_hp > 40 ? COL_GREEN : COL_RED);
        }
    }

    /* 3. 畫所有警消救單位光點 */
    for (int i = 0; i < g_911.unit_count; i++) {
        const DispatchUnit *u = &g_911.units[i];
        if (!u->active) continue;
        int ux, uy;
        city_grid_to_px(show_home_only ? u->home_x : u->gx, show_home_only ? u->home_y : u->gy, mx, my, mw, mh, &ux, &uy);
        uint32_t col = (u->status == USTAT_DISABLED) ? 0x64748B : dept_color(u->dept);
        if (show_home_only && i == g_unit_sel) hal_draw_circle(ux, uy, 10, COL_RED);
        hal_fill_circle(ux, uy, 6, col);
        hal_draw_circle(ux, uy, 6, COL_WHITE);
        const char *tag = (u->dept == DEPT_POLICE) ? "P" : ((u->dept == DEPT_FIRE) ? "F" : "M");
        ui_draw_ascii(ux - 2, uy - 3, tag, 1, COL_WHITE);
    }
}

static void render_title(int sw, int sh) {
    hal_clear(COL_NAV);
    hal_set_text_bg(COL_NAV);
    ui_draw_str(sw / 2 - 190, 26, "911 緊急調度中心  CAD 真實模擬器", 400, 0x7DD3FC);
    ui_draw_str(sw / 2 - 230, 62, "真實 A* 路網塞車 · 40+ 動態危機 · 18 組 911 線上急救通話 · 10 種載具", 480, 0xCBD5E1);
    ui_draw_str(24, 102, "選擇你的執勤城市：", 200, 0xFACC15);

    for (int i = 0; i < NUM_CITIES; i++) {
        const CityDef *c = city_get(i);
        int cx = 24 + i * 190, sel = (g_city_sel == i);
        ui_card(cx, 132, 180, 144, 12, sel ? COL_WHITE : 0x1B365D, sel ? 0x38BDF8 : 0x334155, 0x091322, sel, 0x38BDF8);
        hal_set_text_bg(sel ? COL_WHITE : 0x1B365D);
        ui_draw_str(cx + 12, 146, c->name, 156, sel ? COL_NAV : COL_WHITE);
        ui_draw_badge(cx + 12, 174, c->eng, sel ? COL_BLUE : 0x334155, COL_WHITE);
        /* 分兩行印出城市特色 */
        ui_draw_str(cx + 12, 202, c->desc, 156, sel ? COL_MUTED : 0x94A3B8);
        ui_draw_str(cx + 12, 244, sel ? "已選擇此城市" : "點擊選擇", 156, sel ? COL_GREEN : 0x64748B);
    }

    int hs = has_save_file();
    if (hs) {
        ui_fill_rrect(24, 312, sw / 2 - 34, 52, 12, COL_GREEN); hal_set_text_bg(COL_GREEN);
        ui_draw_str(116, 328, "繼續上次執勤存檔", 180, COL_WHITE);
        ui_fill_rrect(sw / 2 + 10, 312, sw / 2 - 34, 52, 12, COL_BLUE); hal_set_text_bg(COL_BLUE);
        ui_draw_str(sw / 2 + 96, 328, "開啟新城市執勤生涯", 200, COL_WHITE);
    } else {
        ui_fill_rrect(160, 312, sw - 320, 52, 12, COL_GREEN); hal_set_text_bg(COL_GREEN);
        ui_draw_str(sw / 2 - 96, 328, "進入勤務中心開始執勤！", 220, COL_WHITE);
    }
    ui_fill_rrect(sw / 2 - 90, 384, 180, 40, 10, 0x334155); hal_set_text_bg(0x334155);
    ui_draw_str(sw / 2 - 36, 395, "離開程式", 80, COL_WHITE);
    (void)sh;
}

static void render_preshift(int sw, int sh) {
    hal_clear(COL_BG);
    hal_fill_rect(0, 0, sw, 42, COL_NAV);
    hal_set_text_bg(COL_NAV);
    char hdr[128];
    snprintf(hdr, sizeof(hdr), "第 %d 天整備 · %s · 預算:$%d %s · 聲譽:%d%% · 單位:%d/%d",
             g_911.day, city_get(g_911.city_id)->name, g_911.budget, g_911.cheat_inf_budget ? "[無限]" : "",
             g_911.reputation, g_911.unit_count, MAX_UNITS);
    ui_draw_str(12, 12, hdr, sw - 280, COL_WHITE);

    ui_fill_rrect(sw - 258, 7, 80, 28, 7, COL_PURPLE); hal_set_text_bg(COL_PURPLE); ui_draw_str(sw - 246, 12, "外掛MOD", 60, COL_WHITE);
    ui_fill_rrect(sw - 170, 7, 158, 28, 7, COL_GREEN); hal_set_text_bg(COL_GREEN); ui_draw_str(sw - 156, 12, "開始今日值班 >", 136, COL_WHITE);

    const char *pt[3] = { "1. 採購新載具 (10種)", "2. 裝備與專長升級", "3. 地圖預先部署" };
    for (int i = 0; i < 3; i++) {
        int px = 16 + i * 180; uint32_t bg = (g_pre_tab == i) ? COL_BLUE : COL_WHITE;
        ui_fill_rrect(px, 48, 172, 32, 8, bg); if (g_pre_tab != i) ui_draw_rrect(px, 48, 172, 32, 8, COL_BORDER);
        hal_set_text_bg(bg); ui_draw_str(px + 14, 55, pt[i], 148, (g_pre_tab == i) ? COL_WHITE : COL_TEXT);
    }

    if (g_pre_tab == 0) {
        for (int r = 0; r < 5; r++) {
            int idx = g_shop_scroll + r; if (idx >= NUM_VEH_TYPES) break;
            const VehicleSpec *sp = sim911_veh_spec(idx);
            int ry = 92 + r * 68;
            ui_card(16, ry, sw - 32, 60, 10, COL_WHITE, COL_BORDER, COL_SHADOW, 0, 0);
            ui_draw_badge(28, ry + 12, sp->short_code, dept_color(sp->dept), COL_WHITE);
            hal_set_text_bg(COL_WHITE);
            char l1[96], l2[112];
            snprintf(l1, sizeof(l1), "%s · 售價 $%d · 速度:%d · 效率:%d %s", sp->name, sp->price, sp->speed, sp->power, sp->is_heli ? "[空中]" : "[地面]");
            snprintf(l2, sizeof(l2), "%s", sp->desc);
            ui_draw_str(86, ry + 8, l1, sw - 224, COL_TEXT);
            ui_draw_str(86, ry + 34, l2, sw - 224, COL_MUTED);
            uint32_t bb = (g_911.cheat_inf_budget || g_911.budget >= sp->price) ? COL_GREEN : 0x94A3B8;
            ui_fill_rrect(sw - 126, ry + 12, 98, 36, 8, bb); hal_set_text_bg(bb); ui_draw_str(sw - 104, ry + 21, "購買入列", 68, COL_WHITE);
        }
        ui_fill_rrect(16, sh - 40, 120, 30, 8, COL_WHITE); ui_draw_rrect(16, sh - 40, 120, 30, 8, COL_BORDER); hal_set_text_bg(COL_WHITE); ui_draw_str(44, sh - 34, "< 上一頁", 80, COL_TEXT);
        ui_fill_rrect(sw - 136, sh - 40, 120, 30, 8, COL_WHITE); ui_draw_rrect(sw - 136, sh - 40, 120, 30, 8, COL_BORDER); ui_draw_str(sw - 108, sh - 34, "下一頁 >", 80, COL_TEXT);
    } else if (g_pre_tab == 1) {
        for (int r = 0; r < 5; r++) {
            int idx = g_unit_scroll + r; if (idx >= g_911.unit_count) break;
            const DispatchUnit *u = &g_911.units[idx];
            const VehicleSpec *sp = sim911_veh_spec(u->veh_type);
            int ry = 92 + r * 68;
            ui_card(16, ry, sw - 32, 60, 10, COL_WHITE, COL_BORDER, COL_SHADOW, idx == g_unit_sel, COL_BLUE);
            ui_draw_badge(28, ry + 12, u->callsign, dept_color(u->dept), COL_WHITE);
            hal_set_text_bg(COL_WHITE);
            char l1[80]; snprintf(l1, sizeof(l1), "%s (防彈:%s 裝備:%s 專訓:%s)", sp->name, u->upg_armor ? "有" : "無", u->upg_gear ? "有" : "無", u->upg_crew ? "有" : "無");
            ui_draw_str(96, ry + 20, l1, sw - 440, COL_TEXT);

            uint32_t c0 = u->upg_armor ? 0x94A3B8 : COL_BLUE;
            uint32_t c1 = u->upg_gear  ? 0x94A3B8 : COL_AMBER;
            uint32_t c2 = u->upg_crew  ? 0x94A3B8 : COL_GREEN;
            ui_fill_rrect(sw - 336, ry + 12, 102, 36, 7, c0); hal_set_text_bg(c0); ui_draw_str(sw - 328, ry + 21, u->upg_armor ? "防彈已裝" : "防彈 $8K", 88, COL_WHITE);
            ui_fill_rrect(sw - 224, ry + 12, 102, 36, 7, c1); hal_set_text_bg(c1); ui_draw_str(sw - 216, ry + 21, u->upg_gear  ? "專裝已配" : "專裝$12K", 88, COL_WHITE);
            ui_fill_rrect(sw - 112, ry + 12, 92,  36, 7, c2); hal_set_text_bg(c2); ui_draw_str(sw - 104, ry + 21, u->upg_crew  ? "已受訓"   : "專訓$10K", 78, COL_WHITE);
        }
        ui_fill_rrect(16, sh - 40, 120, 30, 8, COL_WHITE); ui_draw_rrect(16, sh - 40, 120, 30, 8, COL_BORDER); hal_set_text_bg(COL_WHITE); ui_draw_str(44, sh - 34, "< 上頁", 80, COL_TEXT);
        ui_fill_rrect(sw - 136, sh - 40, 120, 30, 8, COL_WHITE); ui_draw_rrect(sw - 136, sh - 40, 120, 30, 8, COL_BORDER); ui_draw_str(sw - 108, sh - 34, "下頁 >", 80, COL_TEXT);
    } else {
        /* 地圖預部署 */
        for (int r = 0; r < 7; r++) {
            int idx = g_unit_scroll + r; if (idx >= g_911.unit_count) break;
            const DispatchUnit *u = &g_911.units[idx];
            int ry = 92 + r * 48, sel = (idx == g_unit_sel);
            ui_card(16, ry, 218, 42, 8, sel ? 0xEFF6FF : COL_WHITE, sel ? COL_BLUE : COL_BORDER, COL_SHADOW, sel, COL_BLUE);
            ui_draw_badge(24, ry + 12, u->callsign, dept_color(u->dept), COL_WHITE);
            hal_set_text_bg(sel ? 0xEFF6FF : COL_WHITE);
            ui_draw_str(86, ry + 12, sim911_veh_spec(u->veh_type)->name, 140, COL_TEXT);
        }
        city_render_map(g_911.city_id, 244, 92, sw - 260, sh - 134, 0, WTH_CLEAR, g_911.hosp_used, g_911.jail_used);
        render_units_and_incidents_on_map(244, 92, sw - 260, sh - 134, 1);
    }

    hal_set_text_bg(COL_BG);
    ui_draw_str(152, sh - 32, g_toast[0] ? g_toast : "整備完成後點右上 [開始今日值班 >] 進入 08:00-20:00 勤務！", sw - 304, COL_BLUE);
}

static void render_shift(int sw, int sh) {
    hal_clear(COL_BG);
    hal_fill_rect(0, 0, sw, 40, COL_NAV);
    hal_set_text_bg(COL_NAV);
    int rush = sim911_is_rush_hour();
    char hdr[112];
    snprintf(hdr, sizeof(hdr), "D%d %02d:%02d %s · %s · $%d · %d%%",
             g_911.day, (g_911.shift_min / 60) % 24, g_911.shift_min % 60,
             rush ? "[尖峰塞車!]" : "", sim911_weather_name(g_911.weather),
             g_911.budget, g_911.reputation);
    ui_draw_str(8, 11, hdr, sw - 416, rush ? 0xFCA5A5 : COL_WHITE);

    if (g_911.call.ringing) {
        ui_fill_rrect(sw - 406, 6, 110, 28, 7, COL_RED); hal_set_text_bg(COL_RED);
        ui_draw_str(sw - 396, 11, "911來電! [C]", 94, COL_WHITE);
    }
    ui_fill_rrect(sw - 288, 6, 70, 28, 7, COL_PURPLE); hal_set_text_bg(COL_PURPLE); ui_draw_str(sw - 278, 11, "外掛MOD", 56, COL_WHITE);
    ui_fill_rrect(sw - 210, 6, 72, 28, 7, 0x2C4263); hal_set_text_bg(0x2C4263);
    char sp[16]; snprintf(sp, sizeof(sp), "速度%dx", (g_911.speed_idx == 2 ? 4 : (g_911.speed_idx == 1 ? 2 : 1)));
    ui_draw_str(sw - 200, 11, sp, 56, 0x7DD3FC);
    ui_fill_rrect(sw - 130, 6, 62, 28, 7, g_911.paused ? COL_GREEN : COL_AMBER); hal_set_text_bg(g_911.paused ? COL_GREEN : COL_AMBER);
    ui_draw_str(sw - 118, 11, g_911.paused ? "繼續" : "暫停", 44, COL_WHITE);
    ui_fill_rrect(sw - 62, 6, 54, 28, 7, 0x475569); hal_set_text_bg(0x475569); ui_draw_str(sw - 52, 11, "選單", 38, COL_WHITE);

    /* 左側 CAD 控制欄 (x = 6..250) */
    ui_card(6, 46, 244, 192, 10, COL_WHITE, COL_BORDER, COL_SHADOW, 0, 0);
    hal_set_text_bg(COL_WHITE);
    int inc_list[MAX_INCIDENTS], icnt = list_active_incidents(inc_list, MAX_INCIDENTS);
    char ihdr[48]; snprintf(ihdr, sizeof(ihdr), "進行中案件 (%d) [點選後派車]", icnt);
    ui_draw_str(14, 52, ihdr, 228, COL_NAV);

    for (int r = 0; r < 3; r++) {
        int pos = g_inc_scroll + r; if (pos >= icnt) break;
        int idx = inc_list[pos]; const Incident *inc = &g_911.incidents[idx];
        int ry = 72 + r * 44, sel = (idx == g_inc_sel);
        uint32_t bg = sel ? 0xEFF6FF : COL_SUB;
        ui_fill_rrect(12, ry, 232, 40, 7, bg); ui_draw_rrect(12, ry, 232, 40, 7, sel ? COL_BLUE : COL_BORDER);
        hal_set_text_bg(bg);
        char l1[64], l2[64];
        snprintf(l1, sizeof(l1), "P%d %.55s", inc->priority, inc->title);
        snprintf(l2, sizeof(l2), "需 警%d 消%d 救%d · 命%d%%", inc->work_pol > 0 ? inc->req_pol : 0, inc->work_fire > 0 ? inc->req_fire : 0, inc->work_ems > 0 ? inc->req_ems : 0, inc->vital_hp);
        ui_draw_str(18, ry + 3,  l1, 220, inc->priority == 1 ? COL_RED : COL_TEXT);
        ui_draw_str(18, ry + 21, l2, 220, COL_MUTED);
    }
    ui_fill_rrect(12, 206, 232, 26, 7, COL_GREEN); hal_set_text_bg(COL_GREEN);
    ui_draw_str(44, 211, "一鍵派遣最近單位 [A]", 180, COL_WHITE);

    /* 左下：單位狀態列表 (點單位立刻派往選中案件!) */
    ui_card(6, 244, 244, sh - 250, 10, COL_WHITE, COL_BORDER, COL_SHADOW, 0, 0);
    hal_set_text_bg(COL_WHITE); ui_draw_str(14, 250, "執勤單位 (點派車 / 右點召回)", 228, COL_NAV);
    for (int r = 0; r < 5; r++) {
        int idx = g_unit_scroll + r; if (idx >= g_911.unit_count) break;
        const DispatchUnit *u = &g_911.units[idx];
        int ry = 268 + r * 36;
        ui_fill_rrect(12, ry, 174, 32, 6, COL_SUB); ui_draw_rrect(12, ry, 174, 32, 6, COL_BORDER);
        ui_draw_badge(16, ry + 8, u->callsign, dept_color(u->dept), COL_WHITE);
        hal_set_text_bg(COL_SUB);
        char us[32]; snprintf(us, sizeof(us), "%s 油%d%%", sim911_unit_status_code(u->status), u->fuel);
        ui_draw_str(74, ry + 7, us, 110, (u->status == USTAT_IDLE) ? COL_GREEN : COL_TEXT);

        ui_fill_rrect(190, ry, 54, 32, 6, 0x475569); hal_set_text_bg(0x475569); ui_draw_str(200, ry + 7, "召回", 36, COL_WHITE);
    }
    ui_fill_rrect(12, sh - 32, 112, 22, 5, COL_SUB); ui_draw_rrect(12, sh - 32, 112, 22, 5, COL_BORDER); hal_set_text_bg(COL_SUB); ui_draw_str(42, sh - 29, "< 上頁", 60, COL_TEXT);
    ui_fill_rrect(130, sh - 32, 114, 22, 5, COL_SUB); ui_draw_rrect(130, sh - 32, 114, 22, 5, COL_BORDER); ui_draw_str(160, sh - 29, "下頁 >", 60, COL_TEXT);

    /* 右側高精細戰術城市地圖 */
    int mx = 258, my = 46, mw = sw - 266, mh = sh - 108;
    city_render_map(g_911.city_id, mx, my, mw, mh, rush, g_911.weather, g_911.hosp_used, g_911.jail_used);
    render_units_and_incidents_on_map(mx, my, mw, mh, 0);

    /* 底部即時無線電 CAD 通聯日誌 */
    int ry = my + mh + 6;
    ui_card(mx, ry, mw, 52, 8, COL_NAV, 0x334155, COL_SHADOW, 0, 0); hal_set_text_bg(COL_NAV);
    ui_draw_str(mx + 10, ry + 6,  g_toast[0] ? g_toast : (g_911.radio_count > 0 ? g_911.radio_log[0] : "CAD 無線電頻道監聽中..."), mw - 20, g_toast[0] ? 0xFACC15 : 0x4ADE80);
    if (g_911.radio_count > 1) ui_draw_str(mx + 10, ry + 28, g_911.radio_log[1], mw - 20, 0x94A3B8);
}

static void render_call_modal(int sw, int sh) {
    int mw = 640, mh = 410, mx = (sw - mw) / 2, my = (sh - mh) / 2;
    hal_clear_text_in_rect(mx - 8, my - 8, mw + 16, mh + 16); hal_dim_rect(0, 0, sw, sh);
    ui_card(mx, my, mw, mh, 14, COL_WHITE, COL_RED, 0x1E293B, 1, COL_RED);
    ui_fill_rrect(mx + 2, my + 2, mw - 4, 42, 12, COL_RED); hal_set_text_bg(COL_RED);

    const CallScript *sc = calls_get_script(g_911.call.script_id);
    char hdr[96]; snprintf(hdr, sizeof(hdr), "911 緊急通話中：%s  ·  恐慌度 %d%%", sc->title, g_911.call.panic);
    ui_draw_str(mx + 18, my + 12, hdr, mw - 36, COL_WHITE);

    /* 報案人陳述與回應框 */
    ui_card(mx + 20, my + 56, mw - 40, 144, 10, COL_SUB, COL_BORDER, 0xE2E8F0, 0, 0);
    hal_set_text_bg(COL_SUB);
    ui_draw_str(mx + 32, my + 66, "【報案人通話內容】", 200, COL_RED);
    const char *line_txt = (g_911.call.step == 0) ? sc->caller_open : g_911.call.last_reply;
    size_t len = strlen(line_txt), sp = 0, u = 0;
    while (sp < len && u < 58) { unsigned char c = (unsigned char)line_txt[sp]; if ((c & 0x80) == 0) { sp++; u++; } else if ((c & 0xF0) == 0xE0) { sp += 3; u += 2; } else { sp++; u++; } }
    hal_draw_line_utf8(mx + 32, my + 96, line_txt, (int)sp, mw - 64, COL_TEXT);
    if (sp < len) hal_draw_line_utf8(mx + 32, my + 124, line_txt + sp, (int)(len - sp), mw - 64, COL_TEXT);

    char emd_s[128];
    snprintf(emd_s, sizeof(emd_s), "EMD 狀態：%s  ·  威脅評估：%s",
             g_911.call.emd_bonus ? "已成功實施線上急救/自保指導!" : "尚未給予有效指導",
             g_911.call.info_revealed ? (sc->is_prank ? "疑似惡作劇報案" : (sc->armed_threat ? "高危武裝威脅!" : "確認真實危難")) : "待詢問確認");
    ui_draw_str(mx + 32, my + 166, emd_s, mw - 64, g_911.call.emd_bonus ? COL_GREEN : COL_BLUE);

    if (g_911.call.step < 2) {
        const CallChoice *chs = (g_911.call.step == 0) ? sc->step0 : sc->step1;
        for (int i = 0; i < 3; i++) {
            int ry = my + 216 + i * 54;
            ui_fill_rrect(mx + 20, ry, mw - 40, 44, 9, COL_BLUE); hal_set_text_bg(COL_BLUE);
            char opt[112]; snprintf(opt, sizeof(opt), "[%d] %s", i + 1, chs[i].label);
            ui_draw_str(mx + 34, ry + 13, opt, mw - 68, COL_WHITE);
        }
    } else {
        hal_set_text_bg(COL_WHITE);
        ui_draw_str(mx + 24, my + 240, "通話詢問結束！請依據判斷決定是否立案派車：", mw - 48, COL_TEXT);
        ui_draw_str(mx + 24, my + 272, "(若判斷為惡作劇電話，請點右側「掛斷不派車」可省下警力並獲得聲譽獎勵！)", mw - 48, COL_MUTED);
        ui_fill_rrect(mx + 20, my + 324, 290, 52, 10, COL_GREEN); hal_set_text_bg(COL_GREEN); ui_draw_str(mx + 86, my + 340, "立即立案派車支援!", 180, COL_WHITE);
        ui_fill_rrect(mx + 330, my + 324, 290, 52, 10, COL_RED); hal_set_text_bg(COL_RED); ui_draw_str(mx + 402, my + 340, "掛斷 (惡作劇/無效)", 180, COL_WHITE);
    }
}

static void render_debrief(int sw, int sh) {
    hal_clear(COL_NAV);
    int pw = 540, ph = 380, px = (sw - pw) / 2, py = (sh - ph) / 2;
    ui_card(px, py, pw, ph, 16, COL_WHITE, COL_BLUE, 0x091322, 1, COL_BLUE);
    ui_fill_rrect(px + 2, py + 2, pw - 4, 44, 14, COL_BLUE); hal_set_text_bg(COL_BLUE);
    char hdr[64]; snprintf(hdr, sizeof(hdr), "第 %d 天值班結算報告 (08:00 - 20:00)", g_911.day - 1);
    ui_draw_str(px + 116, py + 14, hdr, 340, COL_WHITE);

    hal_set_text_bg(COL_WHITE);
    char l1[96], l2[96], l3[96], l4[96], l5[96];
    snprintf(l1, sizeof(l1), "成功處置案件：%d 件   ·   失職/漏接：%d 件", g_911.shift_handled, g_911.shift_failed);
    snprintf(l2, sizeof(l2), "線上急救指導成功：%d 次   ·   識破惡作劇：%d 通", g_911.shift_emd_saves, g_911.shift_pranks_blocked);
    snprintf(l3, sizeof(l3), "今日獎金與市府撥款：+$%d   ·   失職罰款：-$%d", g_911.shift_earned, g_911.shift_fines);
    snprintf(l4, sizeof(l4), "目前市民滿意度 (聲譽)：%d%%   ·   總預算：$%d", g_911.reputation, g_911.budget);
    snprintf(l5, sizeof(l5), "全體載具已自動返隊加油、維修與人員輪休完畢！");
    ui_draw_str(px + 32, py + 72,  l1, pw - 64, COL_TEXT);
    ui_draw_str(px + 32, py + 114, l2, pw - 64, COL_GREEN);
    ui_draw_str(px + 32, py + 156, l3, pw - 64, COL_BLUE);
    ui_draw_str(px + 32, py + 198, l4, pw - 64, COL_AMBER);
    ui_draw_str(px + 32, py + 244, l5, pw - 64, COL_MUTED);

    ui_fill_rrect(px + 60, py + ph - 68, pw - 120, 48, 12, COL_GREEN); hal_set_text_bg(COL_GREEN);
    ui_draw_str(px + 168, py + ph - 53, "進入隔日班前整備 >", 220, COL_WHITE);
}

static void render_cheat_modal(int sw, int sh) {
    int mw = 580, mh = 360, mx = (sw - mw) / 2, my = (sh - mh) / 2;
    hal_clear_text_in_rect(mx - 8, my - 8, mw + 16, mh + 16); hal_dim_rect(0, 0, sw, sh);
    ui_card(mx, my, mw, mh, 14, COL_WHITE, COL_PURPLE, 0x1E293B, 1, COL_PURPLE);
    ui_fill_rrect(mx + 2, my + 2, mw - 4, 42, 12, COL_PURPLE); hal_set_text_bg(COL_PURPLE);
    ui_draw_str(mx + 148, my + 12, "911 沙盒與外掛控制台 (MOD MENU)", 320, COL_WHITE);

    int cw = (mw - 48) / 2;
    const char *btns[4][2] = {
        { "預算注入 +$50,000", "預算注入 +$500,000" },
        { g_911.cheat_inf_budget ? "無限市府預算 [開]" : "無限市府預算 [關]",
          g_911.cheat_god_units  ? "載具無敵無限油 [開]" : "載具無敵無限油 [關]" },
        { "強制晴朗天氣 (解禁直升機)", "聲譽 100% & 全員瞬間滿血滿油" },
        { "觸發重大複合災難 (測試)", "關閉控制台 [ESC]" }
    };
    uint32_t cols[4][2] = {
        { COL_GREEN, COL_GREEN },
        { g_911.cheat_inf_budget ? COL_GREEN : 0x475569, g_911.cheat_god_units ? COL_GREEN : 0x475569 },
        { COL_BLUE, COL_AMBER },
        { COL_RED, 0x334155 }
    };
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 2; c++) {
            int bx = mx + 18 + c * (cw + 12), by = my + 58 + r * 58;
            ui_fill_rrect(bx, by, cw, 46, 9, cols[r][c]); hal_set_text_bg(cols[r][c]);
            ui_draw_str(bx + 18, by + 14, btns[r][c], cw - 30, COL_WHITE);
        }
    }
    hal_set_text_bg(COL_WHITE); ui_draw_str(mx + 20, my + mh - 36, g_toast[0] ? g_toast : "點任一按鈕立即生效並自動存檔！", mw - 40, COL_PURPLE);
}

void ui911_render(void) {
    int sw = hal_get_width(), sh = hal_get_height();
    g_redraw = 0;

    if (g_scr == SCR_TITLE) { render_title(sw, sh); hal_present(); return; }
    if (g_scr == SCR_DEBRIEF) { render_debrief(sw, sh); hal_present(); return; }

    if (g_scr == SCR_PRESHIFT || (g_scr == SCR_CHEAT && !g_911.in_shift) || (g_scr == SCR_PAUSE && !g_911.in_shift)) {
        render_preshift(sw, sh);
    } else {
        render_shift(sw, sh);
    }

    if (g_scr == SCR_CALL_MODAL) render_call_modal(sw, sh);
    else if (g_scr == SCR_CHEAT) render_cheat_modal(sw, sh);
    else if (g_scr == SCR_PAUSE) {
        int pw = 340, ph = 220, px = (sw - pw) / 2, py = (sh - ph) / 2;
        hal_clear_text_in_rect(px - 8, py - 8, pw + 16, ph + 16); hal_dim_rect(0, 0, sw, sh);
        ui_card(px, py, pw, ph, 14, COL_WHITE, COL_BLUE, 0x1E293B, 1, COL_BLUE); hal_set_text_bg(COL_WHITE);
        ui_draw_str(px + 116, py + 16, "系統暫停選單", 140, COL_TEXT);
        const char *po[3] = { "繼續執勤 (ESC)", "存檔並回主畫面", "存檔並離開" };
        for (int i = 0; i < 3; i++) {
            uint32_t bg = (i == g_pause_sel) ? COL_BLUE : COL_SUB;
            ui_fill_rrect(px + 28, py + 56 + i * 48, pw - 56, 38, 8, bg);
            if (i != g_pause_sel) ui_draw_rrect(px + 28, py + 56 + i * 48, pw - 56, 38, 8, COL_BORDER);
            hal_set_text_bg(bg); ui_draw_str(px + 84, py + 66 + i * 48, po[i], 180, (i == g_pause_sel) ? COL_WHITE : COL_TEXT);
        }
    }

    hal_present();
}

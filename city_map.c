#include "city_map.h"
#include "ui_gfx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static CityDef g_cities[NUM_CITIES];

/* 工具：建立標準棋盤幹道基底 */
static void make_base_grid(CityDef *c) {
    memset(c->tiles, TILE_BLOCK, sizeof(c->tiles));
    for (int y = 0; y < MAP_GH; y++) {
        for (int x = 0; x < MAP_GW; x++) {
            if (x == 1 || x == 7 || x == 12 || x == 17 || x == 22 ||
                y == 1 || y == 5 || y == 10 || y == 14) {
                c->tiles[y][x] = TILE_ARTERIAL;
            } else if ((x % 3 == 1) || (y % 3 == 1)) {
                c->tiles[y][x] = TILE_STREET;
            }
        }
    }
}

static void add_river_v(CityDef *c, int col, int width, int b1, int b2, int b3) {
    for (int y = 0; y < MAP_GH; y++) {
        for (int w = 0; w < width; w++) {
            int x = col + w;
            if (x >= 0 && x < MAP_GW) {
                if (y == b1 || y == b2 || y == b3) c->tiles[y][x] = TILE_BRIDGE;
                else c->tiles[y][x] = TILE_WATER;
            }
        }
    }
}

static void add_river_h(CityDef *c, int row, int height, int b1, int b2, int b3) {
    for (int x = 0; x < MAP_GW; x++) {
        for (int h = 0; h < height; h++) {
            int y = row + h;
            if (y >= 0 && y < MAP_GH) {
                if (x == b1 || x == b2 || x == b3) c->tiles[y][x] = TILE_BRIDGE;
                else c->tiles[y][x] = TILE_WATER;
            }
        }
    }
}

static void add_park(CityDef *c, int x0, int y0, int w, int h) {
    for (int y = y0; y < y0 + h && y < MAP_GH; y++)
        for (int x = x0; x < x0 + w && x < MAP_GW; x++)
            if (c->tiles[y][x] == TILE_BLOCK) c->tiles[y][x] = TILE_PARK;
}

void city_init_all(void) {
    /* 0: 台北都會區 (淡水河西側 + 基隆河東北 + 大安森林公園) */
    CityDef *c0 = &g_cities[0];
    c0->name = "台北都會區"; c0->eng = "TAIPEI";
    c0->desc = "密集商業街區與跨河橋樑瓶頸，尖峰車流高";
    make_base_grid(c0);
    add_river_v(c0, 5, 2, 1, 5, 10);
    add_park(c0, 13, 6, 4, 4);
    c0->hospitals[0] = (FacilityNode){ 12, 5,  "台大醫院", 6 };
    c0->hospitals[1] = (FacilityNode){ 22, 10, "三總內湖", 5 };
    c0->police_hq[0] = (FacilityNode){ 7,  10, "中正警總", 6 };
    c0->police_hq[1] = (FacilityNode){ 17, 1,  "信義分局", 5 };
    c0->fire_hq[0]   = (FacilityNode){ 7,  5,  "城中消防", 4 };
    c0->fire_hq[1]   = (FacilityNode){ 17, 14, "文山消防", 4 };
    c0->districts[0] = "信義商圈"; c0->districts[1] = "西門町"; c0->districts[2] = "大安區";
    c0->districts[3] = "內湖園區"; c0->districts[4] = "板橋橋頭"; c0->districts[5] = "士林夜市";

    /* 1: 雪梨港灣區 (橫向雪梨港灣 + 港灣大橋 + 皇家植物園) */
    CityDef *c1 = &g_cities[1];
    c1->name = "雪梨港灣區"; c1->eng = "SYDNEY";
    c1->desc = "雪梨港灣分隔南北岸，港灣大橋為關鍵樞紐";
    make_base_grid(c1);
    add_river_h(c1, 6, 2, 7, 12, 17);
    add_park(c1, 18, 9, 4, 4);
    c1->hospitals[0] = (FacilityNode){ 12, 10, "RPA 皇家醫院", 6 };
    c1->hospitals[1] = (FacilityNode){ 12, 1,  "北岸醫院",     5 };
    c1->police_hq[0] = (FacilityNode){ 17, 10, "雪梨CBD警總",  6 };
    c1->police_hq[1] = (FacilityNode){ 7,  1,  "北雪梨分局",   5 };
    c1->fire_hq[0]   = (FacilityNode){ 7,  14, "港灣消防局",   4 };
    c1->fire_hq[1]   = (FacilityNode){ 22, 5,  "莫斯曼分隊",   4 };
    c1->districts[0] = "環形碼頭"; c1->districts[1] = "達令港"; c1->districts[2] = "北岸大道";
    c1->districts[3] = "邦代海灘"; c1->districts[4] = "查茨伍德"; c1->districts[5] = "雪梨歌劇院";

    /* 2: 紐約曼哈頓 (哈德遜河 + 東河 + 中央公園) */
    CityDef *c2 = &g_cities[2];
    c2->name = "紐約曼哈頓"; c2->eng = "NEW YORK";
    c2->desc = "高密度摩天大廈與武裝案件高發，考驗 SWAT 調度";
    make_base_grid(c2);
    add_river_v(c2, 3, 1, 1, 5, 14);
    add_river_v(c2, 19, 1, 1, 10, 14);
    add_park(c2, 9, 2, 5, 6);
    c2->hospitals[0] = (FacilityNode){ 17, 10, "貝爾維尤醫院", 6 };
    c2->hospitals[1] = (FacilityNode){ 7,  1,  "西奈山醫院",   5 };
    c2->police_hq[0] = (FacilityNode){ 12, 14, "NYPD 總局",   7 };
    c2->police_hq[1] = (FacilityNode){ 17, 5,  "中城分局",     5 };
    c2->fire_hq[0]   = (FacilityNode){ 7,  10, "FDNY 第8分隊", 4 };
    c2->fire_hq[1]   = (FacilityNode){ 12, 1,  "哈林消防隊",   4 };
    c2->districts[0] = "時代廣場"; c2->districts[1] = "華爾街"; c2->districts[2] = "中央公園";
    c2->districts[3] = "布魯克林橋"; c2->districts[4] = "蘇活區"; c2->districts[5] = "第五大道";

    /* 3: 東京新宿區 (高架幹線 + 皇居護城河 + 新宿御苑) */
    CityDef *c3 = &g_cities[3];
    c3->name = "東京新宿區"; c3->eng = "TOKYO";
    c3->desc = "極度繁忙的交通樞紐與高樓火警挑戰";
    make_base_grid(c3);
    add_river_v(c3, 15, 1, 1, 5, 10);
    add_park(c3, 8, 11, 5, 3);
    c3->hospitals[0] = (FacilityNode){ 7,  5,  "東京醫大醫院", 6 };
    c3->hospitals[1] = (FacilityNode){ 22, 10, "慶應醫院",     5 };
    c3->police_hq[0] = (FacilityNode){ 12, 5,  "警視廳新宿",   6 };
    c3->police_hq[1] = (FacilityNode){ 17, 14, "澀谷警察署",   5 };
    c3->fire_hq[0]   = (FacilityNode){ 1,  10, "西新宿消防",   4 };
    c3->fire_hq[1]   = (FacilityNode){ 22, 1,  "四谷消防署",   4 };
    c3->districts[0] = "歌舞伎町"; c3->districts[1] = "新宿西口"; c3->districts[2] = "澀谷十字路";
    c3->districts[3] = "新宿御苑"; c3->districts[4] = "六本木"; c3->districts[5] = "首都高交流道";
}

const CityDef *city_get(int city_id) {
    return &g_cities[(city_id >= 0 && city_id < NUM_CITIES) ? city_id : 0];
}

int city_is_driveable(int city_id, int gx, int gy) {
    if (gx < 0 || gx >= MAP_GW || gy < 0 || gy >= MAP_GH) return 0;
    uint8_t t = city_get(city_id)->tiles[gy][gx];
    return (t == TILE_STREET || t == TILE_ARTERIAL || t == TILE_BRIDGE);
}

void city_nearest_road(int city_id, int gx, int gy, int *ox, int *oy) {
    if (gx < 0) gx = 0;
    if (gx >= MAP_GW) gx = MAP_GW - 1;
    if (gy < 0) gy = 0;
    if (gy >= MAP_GH) gy = MAP_GH - 1;
    if (city_is_driveable(city_id, gx, gy)) { *ox = gx; *oy = gy; return; }
    int best_d = 9999, bx = 7, by = 5;
    for (int y = 0; y < MAP_GH; y++) {
        for (int x = 0; x < MAP_GW; x++) {
            if (city_is_driveable(city_id, x, y)) {
                int d = abs(x - gx) + abs(y - gy);
                if (d < best_d) { best_d = d; bx = x; by = y; }
            }
        }
    }
    *ox = bx; *oy = by;
}

/* 真實 A* 道路尋路：優先走暢通幹道，尖峰會自動評估改走替代街區 */
int city_find_path(int city_id, int sx, int sy, int tx, int ty, int rush_hour,
                   uint8_t out_x[MAX_PATH_LEN], uint8_t out_y[MAX_PATH_LEN]) {
    city_nearest_road(city_id, sx, sy, &sx, &sy);
    city_nearest_road(city_id, tx, ty, &tx, &ty);
    if (sx == tx && sy == ty) { out_x[0] = (uint8_t)tx; out_y[0] = (uint8_t)ty; return 1; }

    const CityDef *c = city_get(city_id);
    const int N = MAP_GW * MAP_GH;
    int g_score[MAP_GW * MAP_GH];
    int f_score[MAP_GW * MAP_GH];
    int parent[MAP_GW * MAP_GH];
    uint8_t closed[MAP_GW * MAP_GH];
    uint8_t open[MAP_GW * MAP_GH];

    for (int i = 0; i < N; i++) { g_score[i] = 999999; f_score[i] = 999999; parent[i] = -1; closed[i] = 0; open[i] = 0; }
    int start = sy * MAP_GW + sx, goal = ty * MAP_GW + tx;
    g_score[start] = 0;
    f_score[start] = (abs(tx - sx) + abs(ty - sy)) * 10;
    open[start] = 1;

    static const int DX[4] = { 1, -1, 0, 0 };
    static const int DY[4] = { 0, 0, 1, -1 };

    int found = 0;
    for (int iter = 0; iter < N; iter++) {
        int cur = -1, best_f = 999999;
        for (int i = 0; i < N; i++) {
            if (open[i] && !closed[i] && f_score[i] < best_f) { best_f = f_score[i]; cur = i; }
        }
        if (cur < 0) break;
        if (cur == goal) { found = 1; break; }
        open[cur] = 0; closed[cur] = 1;

        int cx = cur % MAP_GW, cy = cur / MAP_GW;
        for (int d = 0; d < 4; d++) {
            int nx = cx + DX[d], ny = cy + DY[d];
            if (!city_is_driveable(city_id, nx, ny)) continue;
            int ni = ny * MAP_GW + nx;
            if (closed[ni]) continue;

            uint8_t tt = c->tiles[ny][nx];
            int step_cost = 14;
            if (tt == TILE_ARTERIAL) step_cost = rush_hour ? 20 : 9;
            else if (tt == TILE_BRIDGE) step_cost = rush_hour ? 24 : 11;

            int ng = g_score[cur] + step_cost;
            if (ng < g_score[ni]) {
                parent[ni] = cur;
                g_score[ni] = ng;
                f_score[ni] = ng + (abs(tx - nx) + abs(ty - ny)) * 10;
                open[ni] = 1;
            }
        }
    }

    if (!found) { out_x[0] = (uint8_t)tx; out_y[0] = (uint8_t)ty; return 1; }

    int tmp_x[MAP_GW * MAP_GH], tmp_y[MAP_GW * MAP_GH], len = 0;
    int cur = goal;
    while (cur >= 0 && cur != start && len < MAP_GW * MAP_GH) {
        tmp_x[len] = cur % MAP_GW;
        tmp_y[len] = cur / MAP_GW;
        len++;
        cur = parent[cur];
    }
    int out_len = 0;
    for (int i = len - 1; i >= 0 && out_len < MAX_PATH_LEN; i--) {
        out_x[out_len] = (uint8_t)tmp_x[i];
        out_y[out_len] = (uint8_t)tmp_y[i];
        out_len++;
    }
    return out_len > 0 ? out_len : 1;
}

void city_grid_to_px(int gx, int gy, int mx, int my, int mw, int mh, int *px, int *py) {
    *px = mx + (gx * mw) / MAP_GW + mw / (MAP_GW * 2);
    *py = my + (gy * mh) / MAP_GH + mh / (MAP_GH * 2);
}

void city_px_to_grid(int px, int py, int mx, int my, int mw, int mh, int *gx, int *gy) {
    int x = ((px - mx) * MAP_GW) / mw;
    int y = ((py - my) * MAP_GH) / mh;
    if (x < 0) x = 0;
    if (x >= MAP_GW) x = MAP_GW - 1;
    if (y < 0) y = 0;
    if (y >= MAP_GH) y = MAP_GH - 1;
    *gx = x; *gy = y;
}

void city_render_map(int city_id, int mx, int my, int mw, int mh,
                     int rush_hour, int weather,
                     const int hosp_used[2], const int jail_used[2]) {
    const CityDef *c = city_get(city_id);
    /* 天氣微微影響地圖明暗氛圍 (0=晴, 1=豪雨, 2=濃霧, 3=颱風) */
    uint32_t col_block  = (weather == 0) ? 0xDCE3EC : ((weather == 2) ? 0xCFD8E3 : 0xC1CCD9);
    uint32_t col_water  = (weather == 3) ? 0x5B8FB9 : 0x7EC8E3;
    uint32_t col_park   = (weather == 0) ? 0xA3D9A5 : 0x8EC494;
    uint32_t col_street = 0xF8FAFC;
    uint32_t col_art    = rush_hour ? 0xFCA5A5 : 0xFEF08A; /* 尖峰主幹道變紅警示塞車! */
    uint32_t col_bridge = rush_hour ? 0xEF4444 : 0xF59E0B;

    hal_fill_rect(mx, my, mw, mh, col_block);

    for (int gy = 0; gy < MAP_GH; gy++) {
        int y0 = my + (gy * mh) / MAP_GH;
        int y1 = my + ((gy + 1) * mh) / MAP_GH;
        for (int gx = 0; gx < MAP_GW; gx++) {
            int x0 = mx + (gx * mw) / MAP_GW;
            int x1 = mx + ((gx + 1) * mw) / MAP_GW;
            uint8_t t = c->tiles[gy][gx];
            uint32_t col = col_block;
            if (t == TILE_WATER) col = col_water;
            else if (t == TILE_PARK) col = col_park;
            else if (t == TILE_STREET) col = col_street;
            else if (t == TILE_ARTERIAL) col = col_art;
            else if (t == TILE_BRIDGE) col = col_bridge;

            hal_fill_rect(x0, y0, x1 - x0, y1 - y0, col);
            if (t == TILE_BLOCK) {
                /* 精緻街區建築陰影格 */
                hal_fill_rect(x0 + 2, y0 + 2, (x1 - x0) - 4, (y1 - y0) - 4, ui_blend(col_block, 0x94A3B8, 22));
            } else if (t == TILE_ARTERIAL || t == TILE_BRIDGE) {
                /* 雙向分隔虛線 */
                int cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
                hal_fill_rect(cx - 1, cy - 1, 2, 2, rush_hour ? 0x991B1B : 0xCA8A04);
            }
        }
    }

    /* 繪製 6 座關鍵基地圖標：醫院 H (含床位數)、警局 P (含拘留數)、消防局 F */
    for (int i = 0; i < 2; i++) {
        int px, py;
        char b[12];
        /* 醫院 */
        city_grid_to_px(c->hospitals[i].gx, c->hospitals[i].gy, mx, my, mw, mh, &px, &py);
        int hfull = (hosp_used && hosp_used[i] >= c->hospitals[i].cap);
        snprintf(b, sizeof(b), "H%d/%d", hosp_used ? hosp_used[i] : 0, c->hospitals[i].cap);
        ui_draw_badge(px - 16, py - 8, b, hfull ? 0xDC2626 : 0x16A34A, 0xFFFFFF);

        /* 警局 */
        city_grid_to_px(c->police_hq[i].gx, c->police_hq[i].gy, mx, my, mw, mh, &px, &py);
        int jfull = (jail_used && jail_used[i] >= c->police_hq[i].cap);
        snprintf(b, sizeof(b), "P%d/%d", jail_used ? jail_used[i] : 0, c->police_hq[i].cap);
        ui_draw_badge(px - 16, py - 8, b, jfull ? 0xB91C1C : 0x1D4ED8, 0xFFFFFF);

        /* 消防局 */
        city_grid_to_px(c->fire_hq[i].gx, c->fire_hq[i].gy, mx, my, mw, mh, &px, &py);
        ui_draw_badge(px - 10, py - 8, "FD", 0xD97706, 0xFFFFFF);
    }

    hal_draw_rect(mx, my, mw, mh, 0x64748B);
}

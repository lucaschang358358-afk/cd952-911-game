#ifndef CITY_MAP_H
#define CITY_MAP_H

#include <stdint.h>

#define MAP_GW 24
#define MAP_GH 16
#define MAX_PATH_LEN 80
#define NUM_CITIES 4

typedef enum {
    TILE_BLOCK = 0,     /* 一般市區建築街區 (不可通行) */
    TILE_STREET,        /* 一般市區道路 */
    TILE_ARTERIAL,      /* 主幹道/快速道路 (平時快，尖峰易塞車!) */
    TILE_WATER,         /* 河川/港灣 (地面不可通行) */
    TILE_BRIDGE,        /* 跨河大橋 (必經瓶頸，尖峰塞車!) */
    TILE_PARK           /* 公園綠地 */
} MapTile;

typedef struct {
    int gx, gy;
    const char *name;
    int cap;
} FacilityNode;

typedef struct {
    const char *name;
    const char *eng;
    const char *desc;
    FacilityNode hospitals[2];
    FacilityNode police_hq[2];
    FacilityNode fire_hq[2];
    const char *districts[6];
    uint8_t tiles[MAP_GH][MAP_GW];
} CityDef;

void           city_init_all(void);
const CityDef *city_get(int city_id);
int            city_is_driveable(int city_id, int gx, int gy);
void           city_nearest_road(int city_id, int gx, int gy, int *ox, int *oy);

/* 真實 A* 道路尋路 (考量主幹道速率與尖峰塞車權重!) */
int city_find_path(int city_id, int sx, int sy, int tx, int ty, int rush_hour,
                   uint8_t out_x[MAX_PATH_LEN], uint8_t out_y[MAX_PATH_LEN]);

/* 繪製明亮清晰的 CAD 戰術城市地圖 */
void city_render_map(int city_id, int mx, int my, int mw, int mh,
                     int rush_hour, int weather,
                     const int hosp_used[2], const int jail_used[2]);

void city_grid_to_px(int gx, int gy, int mx, int my, int mw, int mh, int *px, int *py);
void city_px_to_grid(int px, int py, int mx, int my, int mw, int mh, int *gx, int *gy);

#endif

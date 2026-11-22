#include "sim_911.h"
#include "hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Sim911World g_911;

static uint32_t g_rng = 0x91191122u;
static uint32_t rng_u32(void) {
    uint32_t x = g_rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    g_rng = x;
    return x;
}
static int rng_range(int lo, int hi) { return (hi <= lo) ? lo : (lo + (int)(rng_u32() % (uint32_t)(hi - lo + 1))); }

static const VehicleSpec VEH_SPECS[NUM_VEH_TYPES] = {
    { "警用巡邏車",     "P",    DEPT_POLICE, 0,  18000, 14, 14, 0, 0, "標準雙人巡邏車，適合一般治安與交通管制" },
    { "警用高速重機",   "PM",   DEPT_POLICE, 0,  14000, 21, 10, 0, 0, "極速穿梭車陣，尖峰塞車影響減半，擅長追捕" },
    { "SWAT 特勤裝甲車","SWAT", DEPT_POLICE, 0,  48000, 12, 26, 1, 1, "重裝防彈＋突擊步槍，專治持槍搶案與人質挾持" },
    { "警用空中直升機", "PH",   DEPT_POLICE, 1,  85000, 28, 18, 1, 0, "無視塞車與河川直線抵達！(颱風天停飛)" },
    { "水箱消防車",     "F",    DEPT_FIRE,   0,  26000, 12, 18, 0, 0, "高壓水砲主力滅火車，快速壓制建築與車輛火警" },
    { "雲梯救助化災車", "FL",   DEPT_FIRE,   0,  52000, 11, 25, 1, 1, "配備油壓剪破拆與防毒面具，專攻受困與毒氣外洩" },
    { "消防滅火直升機", "FH",   DEPT_FIRE,   1,  92000, 26, 22, 0, 1, "高空水袋精準灌救，無視交通瓶頸！(颱風停飛)" },
    { "標準加護救護車", "M",    DEPT_EMS,    0,  22000, 15, 18, 0, 0, "配備擔架與氧氣，可快速搶救並將傷患送醫" },
    { "快反救護機車",   "MM",   DEPT_EMS,    0,  15000, 22, 13, 0, 1, "內建 AED 電擊器，極速搶救心臟驟停穩住生命！" },
    { "空中醫療直升機", "MH",   DEPT_EMS,    1,  96000, 30, 24, 0, 1, "頂級空中加護病房，跨海跨河秒送醫院！(颱風停飛)" }
};

const VehicleSpec *sim911_veh_spec(int t) { return &VEH_SPECS[(t >= 0 && t < NUM_VEH_TYPES) ? t : 0]; }

const char *sim911_weather_name(int w) {
    switch (w) {
        case WTH_RAIN:    return "豪雨 (地面減速)";
        case WTH_FOG:     return "濃霧 (視線不佳)";
        case WTH_TYPHOON: return "颱風 (直升機停飛!)";
        default:          return "晴朗";
    }
}

const char *sim911_unit_status_code(int st) {
    switch (st) {
        case USTAT_IDLE:      return "10-8 待命";
        case USTAT_ENROUTE:   return "Code3 趕赴";
        case USTAT_ONSCENE:   return "10-97 處置";
        case USTAT_TRANSPORT: return "10-15 送醫押解";
        case USTAT_RETURNING: return "10-23 返隊";
        default:              return "損毀/負傷";
    }
}

int sim911_is_rush_hour(void) {
    return (g_911.shift_min >= 480 && g_911.shift_min <= 570) ||
           (g_911.shift_min >= 1020 && g_911.shift_min <= 1140);
}

void sim911_radio_push(const char *msg) {
    if (!msg || !msg[0]) return;
    for (int i = MAX_RADIO_LOG - 1; i > 0; i--) memcpy(g_911.radio_log[i], g_911.radio_log[i - 1], sizeof(g_911.radio_log[0]));
    snprintf(g_911.radio_log[0], sizeof(g_911.radio_log[0]), "[%02d:%02d] %.110s",
             (g_911.shift_min / 60) % 24, g_911.shift_min % 60, msg);
    if (g_911.radio_count < MAX_RADIO_LOG) g_911.radio_count++;
}

void sim911_save(void) {
    if (!g_911.initialized) return;
    g_911.magic = SAVE_911_MAGIC;
    FILE *f = fopen(hal_get_save_path(), "wb");
    if (!f) return;
    fwrite(&g_911, sizeof(g_911), 1, f);
    fclose(f);
}

int sim911_load(void) {
    FILE *f = fopen(hal_get_save_path(), "rb");
    if (!f) return 0;
    Sim911World tmp;
    int ok = (fread(&tmp, sizeof(tmp), 1, f) == 1 && tmp.magic == SAVE_911_MAGIC && tmp.initialized);
    fclose(f);
    if (!ok) return 0;
    g_911 = tmp;
    return 1;
}

void sim911_init(void) {
    g_rng = hal_get_seed() ^ 0x9110A5A5u;
    if (g_rng == 0) g_rng = 0x13579BDFu;
    city_init_all();
    memset(&g_911, 0, sizeof(g_911));
}

static void add_starter_unit(int veh_type, int gx, int gy) {
    if (g_911.unit_count >= MAX_UNITS) return;
    const VehicleSpec *sp = sim911_veh_spec(veh_type);
    DispatchUnit *u = &g_911.units[g_911.unit_count++];
    memset(u, 0, sizeof(*u));
    u->active = 1;
    u->veh_type = (uint8_t)veh_type;
    u->dept = sp->dept;
    u->is_heli = sp->is_heli;
    u->upg_armor = (uint8_t)sp->has_armor;
    u->upg_gear  = (uint8_t)sp->has_special;
    city_nearest_road(g_911.city_id, gx, gy, &u->gx, &u->gy);
    u->home_x = u->gx; u->home_y = u->gy;
    u->status = USTAT_IDLE; u->target_inc = -1;
    u->fuel = 100; u->fatigue = 0; u->hp = 100;
    snprintf(u->callsign, sizeof(u->callsign), "%s-%02d", sp->short_code, g_911.unit_count);
}

void sim911_new_career(int city_id) {
    memset(&g_911, 0, sizeof(g_911));
    g_911.magic = SAVE_911_MAGIC;
    g_911.initialized = 1;
    g_911.city_id = (city_id >= 0 && city_id < NUM_CITIES) ? city_id : 0;
    g_911.day = 1;
    g_911.budget = 65000;
    g_911.reputation = 78;
    g_911.shift_min = 480;
    g_911.next_inc_id = 101;

    const CityDef *c = city_get(g_911.city_id);
    /* 起始專業基礎編制：2 警車、1 消防車、2 救護車 (分散部署於基地) */
    add_starter_unit(VEH_PATROL,    c->police_hq[0].gx, c->police_hq[0].gy);
    add_starter_unit(VEH_PATROL,    c->police_hq[1].gx, c->police_hq[1].gy);
    add_starter_unit(VEH_FIRE_ENG,  c->fire_hq[0].gx,   c->fire_hq[0].gy);
    add_starter_unit(VEH_AMBULANCE, c->hospitals[0].gx, c->hospitals[0].gy);
    add_starter_unit(VEH_AMBULANCE, c->hospitals[1].gx, c->hospitals[1].gy);

    sim911_radio_push("CAD 勤務中心上線！請整備載具與裝備後開始值班");
    sim911_save();
}

void sim911_refuel_rest_all(void) {
    for (int i = 0; i < g_911.unit_count; i++) {
        DispatchUnit *u = &g_911.units[i];
        u->gx = u->home_x; u->gy = u->home_y;
        u->status = USTAT_IDLE; u->target_inc = -1;
        u->fuel = 100; u->fatigue = 0; u->hp = 100;
        u->path_len = 0; u->path_idx = 0;
    }
}

void sim911_start_shift(void) {
    g_911.in_shift = 1;
    g_911.shift_min = 480; /* 08:00 */
    g_911.shift_sec_acc = 0;
    g_911.paused = 0;
    g_911.hosp_used[0] = 0; g_911.hosp_used[1] = 0;
    g_911.jail_used[0] = 0; g_911.jail_used[1] = 0;
    memset(g_911.incidents, 0, sizeof(g_911.incidents));
    memset(&g_911.call, 0, sizeof(g_911.call));
    g_911.shift_handled = 0; g_911.shift_failed = 0;
    g_911.shift_pranks_blocked = 0; g_911.shift_emd_saves = 0;
    g_911.shift_earned = 0; g_911.shift_fines = 0;
    g_911.spawn_cd = 2000;
    g_911.call_cd  = 7000;

    /* 依天數隨機決定今日天氣 */
    int wr = rng_range(0, 99);
    if (g_911.day == 1) g_911.weather = WTH_CLEAR;
    else if (wr < 52) g_911.weather = WTH_CLEAR;
    else if (wr < 74) g_911.weather = WTH_RAIN;
    else if (wr < 88) g_911.weather = WTH_FOG;
    else g_911.weather = WTH_TYPHOON;

    sim911_refuel_rest_all();
    char buf[96];
    snprintf(buf, sizeof(buf), "第 %d 天值班開始！天氣：%s", g_911.day, sim911_weather_name(g_911.weather));
    sim911_radio_push(buf);
}

static void pick_random_road(int *ox, int *oy) {
    for (int tries = 0; tries < 60; tries++) {
        int x = rng_range(1, MAP_GW - 2), y = rng_range(1, MAP_GH - 2);
        if (city_is_driveable(g_911.city_id, x, y)) { *ox = x; *oy = y; return; }
    }
    *ox = 12; *oy = 5;
}

static int find_free_inc_slot(void) {
    for (int i = 0; i < MAX_INCIDENTS; i++) if (!g_911.incidents[i].active) return i;
    return -1;
}

static void spawn_incident_from_params(const char *title, int gx, int gy, int prio,
                                       int rp, int rf, int re, int armed, int fire, int crit,
                                       int hazmat, int jaws, int emd_stab) {
    int slot = find_free_inc_slot();
    if (slot < 0) return;
    Incident *inc = &g_911.incidents[slot];
    memset(inc, 0, sizeof(*inc));
    inc->active = 1;
    inc->id_num = g_911.next_inc_id++;
    snprintf(inc->title, sizeof(inc->title), "%.55s", title);
    const CityDef *c = city_get(g_911.city_id);
    snprintf(inc->district, sizeof(inc->district), "%s", c->districts[rng_range(0, 5)]);
    city_nearest_road(g_911.city_id, gx, gy, &inc->gx, &inc->gy);
    inc->priority = prio;
    inc->req_pol = rp; inc->req_fire = rf; inc->req_ems = re;
    inc->max_pol = rp * 100; inc->work_pol = inc->max_pol;
    inc->max_fire = rf * 100; inc->work_fire = inc->max_fire;
    inc->max_ems = re * 100; inc->work_ems = inc->max_ems;
    inc->armed_threat = armed; inc->fire_hazard = fire; inc->critical_hp = crit;
    inc->req_hazmat = hazmat; inc->req_jaws = jaws; inc->emd_stabilized = emd_stab;
    inc->vital_hp = emd_stab ? 100 : (crit ? 82 : 100);
    inc->fire_lvl = fire ? 25 : 0;
    inc->reward_cash = 3200 + (rp + rf + re) * 1800 + (prio == 1 ? 2500 : 0);

    char msg[96];
    snprintf(msg, sizeof(msg), "[P%d] #%d %s (%s)", prio, inc->id_num, inc->title, inc->district);
    sim911_radio_push(msg);
}

static void spawn_ambient_incident(void) {
    static const struct {
        const char *title; int prio, rp, rf, re, armed, fire, crit, haz, jaws;
    } POOL[12] = {
        { "市區酒駕追撞事故",     2, 1, 0, 1, 0, 0, 0, 0, 0 },
        { "商店竊盜現行犯",       3, 1, 0, 0, 0, 0, 0, 0, 0 },
        { "餐廳廚房抽油煙機起火", 2, 0, 1, 0, 0, 1, 0, 0, 0 },
        { "路人熱衰竭昏倒",       2, 0, 0, 1, 0, 0, 1, 0, 0 },
        { "銀行提款機持槍搶劫",   1, 2, 0, 1, 1, 0, 1, 0, 0 },
        { "十字路口連環車禍受困", 1, 1, 1, 2, 0, 1, 1, 0, 1 },
        { "住宅區電線走火冒濃煙", 1, 0, 2, 1, 0, 1, 0, 0, 0 },
        { "幫派街頭持械鬥毆",     1, 2, 0, 1, 1, 0, 0, 0, 0 },
        { "老人家中跌倒骨折",     3, 0, 0, 1, 0, 0, 0, 0, 0 },
        { "工廠化學桶洩漏刺鼻煙", 1, 1, 2, 1, 0, 1, 0, 1, 0 },
        { "捷運站突發心肌梗塞",   1, 0, 0, 1, 0, 0, 1, 0, 0 },
        { "住宅火警伴隨瓦斯瓶",   1, 1, 2, 1, 0, 1, 1, 1, 0 }
    };
    int max_idx = (g_911.day == 1) ? 8 : 12;
    int k = rng_range(0, max_idx - 1);
    int gx, gy; pick_random_road(&gx, &gy);
    spawn_incident_from_params(POOL[k].title, gx, gy, POOL[k].prio, POOL[k].rp, POOL[k].rf, POOL[k].re,
                               POOL[k].armed, POOL[k].fire, POOL[k].crit, POOL[k].haz, POOL[k].jaws, 0);
}

void sim911_trigger_major_disaster(char *out_msg, int msg_sz) {
    int gx, gy; pick_random_road(&gx, &gy);
    spawn_incident_from_params("大型複合災難：高架油罐車連環爆炸", gx, gy, 1, 2, 3, 3, 0, 1, 1, 1, 1, 0);
    snprintf(out_msg, (size_t)msg_sz, "重大複合災難爆發！急需大量警消救增援！");
}

static void route_unit_to(DispatchUnit *u, int tx, int ty) {
    u->dest_x = tx; u->dest_y = ty;
    if (u->is_heli) {
        u->path_x[0] = (uint8_t)tx; u->path_y[0] = (uint8_t)ty;
        u->path_len = 1; u->path_idx = 0;
    } else {
        u->path_len = city_find_path(g_911.city_id, u->gx, u->gy, tx, ty, sim911_is_rush_hour(), u->path_x, u->path_y);
        u->path_idx = 0;
    }
}

int sim911_dispatch_unit(int uid, int inc_idx, char *out_msg, int msg_sz) {
    if (uid < 0 || uid >= g_911.unit_count || inc_idx < 0 || inc_idx >= MAX_INCIDENTS) return 0;
    DispatchUnit *u = &g_911.units[uid];
    Incident *inc = &g_911.incidents[inc_idx];
    if (!u->active || !inc->active) return 0;
    if (u->status == USTAT_DISABLED) { snprintf(out_msg, (size_t)msg_sz, "%s 負傷損毀中，無法出勤！", u->callsign); return 0; }
    if (u->status == USTAT_TRANSPORT) { snprintf(out_msg, (size_t)msg_sz, "%s 正在押解/送醫途中，無法改派！", u->callsign); return 0; }
    if (u->is_heli && g_911.weather == WTH_TYPHOON) {
        snprintf(out_msg, (size_t)msg_sz, "強風颱風警報！%s 空中停飛，請改派地面載具", u->callsign); return 0;
    }
    if (!g_911.cheat_god_units && u->fuel < 10) {
        snprintf(out_msg, (size_t)msg_sz, "%s 油料耗盡！請先召回基地補給", u->callsign); return 0;
    }

    u->target_inc = inc_idx;
    u->status = USTAT_ENROUTE;
    route_unit_to(u, inc->gx, inc->gy);
    char rmsg[96];
    snprintf(rmsg, sizeof(rmsg), "%s: Code 3 鳴笛趕赴 #%d %s", u->callsign, inc->id_num, inc->title);
    sim911_radio_push(rmsg);
    snprintf(out_msg, (size_t)msg_sz, "已派遣 %s 趕赴 #%d！", u->callsign, inc->id_num);
    return 1;
}

int sim911_dispatch_nearest_for_inc(int inc_idx, char *out_msg, int msg_sz) {
    if (inc_idx < 0 || inc_idx >= MAX_INCIDENTS || !g_911.incidents[inc_idx].active) return 0;
    Incident *inc = &g_911.incidents[inc_idx];
    int dispatched = 0;
    int need[3] = { inc->work_pol > 0 ? inc->req_pol : 0, inc->work_fire > 0 ? inc->req_fire : 0, inc->work_ems > 0 ? inc->req_ems : 0 };

    /* 扣除已在路上或現場的同部門單位 */
    for (int i = 0; i < g_911.unit_count; i++) {
        DispatchUnit *u = &g_911.units[i];
        if (u->target_inc == inc_idx && (u->status == USTAT_ENROUTE || u->status == USTAT_ONSCENE)) {
            if (need[u->dept] > 0) need[u->dept]--;
        }
    }

    for (int d = 0; d < 3; d++) {
        while (need[d] > 0) {
            int best_u = -1, best_dist = 99999;
            for (int i = 0; i < g_911.unit_count; i++) {
                DispatchUnit *u = &g_911.units[i];
                if (!u->active || u->dept != d || u->status != USTAT_IDLE) continue;
                if (u->is_heli && g_911.weather == WTH_TYPHOON) continue;
                if (!g_911.cheat_god_units && u->fuel < 12) continue;
                int dist = abs(u->gx - inc->gx) + abs(u->gy - inc->gy);
                /* 若為武裝威脅，強烈優先派有防彈的 SWAT! */
                if (inc->armed_threat && u->upg_armor) dist -= 12;
                if (dist < best_dist) { best_dist = dist; best_u = i; }
            }
            if (best_u < 0) break;
            char tmp[96];
            if (sim911_dispatch_unit(best_u, inc_idx, tmp, sizeof(tmp))) { dispatched++; need[d]--; }
            else break;
        }
    }
    if (dispatched > 0) snprintf(out_msg, (size_t)msg_sz, "CAD 自動派遣：已派出 %d 組最近單位支援 #%d！", dispatched, inc->id_num);
    else snprintf(out_msg, (size_t)msg_sz, "目前無多餘待命對應單位可派 (或該案件已派足)");
    return dispatched;
}

int sim911_recall_unit(int uid, char *out_msg, int msg_sz) {
    if (uid < 0 || uid >= g_911.unit_count) return 0;
    DispatchUnit *u = &g_911.units[uid];
    if (u->status == USTAT_DISABLED) { snprintf(out_msg, (size_t)msg_sz, "%s 損毀中無法移動", u->callsign); return 0; }
    u->target_inc = -1;
    u->status = USTAT_RETURNING;
    route_unit_to(u, u->home_x, u->home_y);
    snprintf(out_msg, (size_t)msg_sz, "%s: 10-23 返隊加油與休整", u->callsign);
    sim911_radio_push(out_msg);
    return 1;
}

static void send_unit_to_facility(DispatchUnit *u) {
    const CityDef *c = city_get(g_911.city_id);
    if (u->dept == DEPT_EMS) {
        /* 找最近且急診床位未滿的醫院！若爆滿自動轉送另一間 */
        int d0 = abs(u->gx - c->hospitals[0].gx) + abs(u->gy - c->hospitals[0].gy);
        int d1 = abs(u->gx - c->hospitals[1].gx) + abs(u->gy - c->hospitals[1].gy);
        int pref = (d0 <= d1) ? 0 : 1, alt = 1 - pref;
        int pick = (g_911.hosp_used[pref] < c->hospitals[pref].cap) ? pref : alt;
        if (pick != pref) sim911_radio_push("醫院急診爆滿！救護車自動轉送次要醫院");
        u->dest_fac_idx = pick;
        u->status = USTAT_TRANSPORT;
        route_unit_to(u, c->hospitals[pick].gx, c->hospitals[pick].gy);
    } else if (u->dept == DEPT_POLICE) {
        int d0 = abs(u->gx - c->police_hq[0].gx) + abs(u->gy - c->police_hq[0].gy);
        int d1 = abs(u->gx - c->police_hq[1].gx) + abs(u->gy - c->police_hq[1].gy);
        int pref = (d0 <= d1) ? 0 : 1, alt = 1 - pref;
        int pick = (g_911.jail_used[pref] < c->police_hq[pref].cap) ? pref : alt;
        u->dest_fac_idx = pick;
        u->status = USTAT_TRANSPORT;
        route_unit_to(u, c->police_hq[pick].gx, c->police_hq[pick].gy);
    } else {
        u->status = USTAT_RETURNING;
        route_unit_to(u, u->home_x, u->home_y);
    }
}

int sim911_tick(int dt_ms) {
    if (!g_911.in_shift || g_911.paused || dt_ms <= 0) return 0;
    static const int SPDS[3] = { 1, 2, 4 };
    int mult = SPDS[(g_911.speed_idx >= 0 && g_911.speed_idx < 3) ? g_911.speed_idx : 0];
    int sim_ms = dt_ms * mult;
    int major = 0;

    /* 1. 推進班次時鐘 (每 1000 模擬毫秒 = 遊戲內 2 分鐘 -> 12 小時班次約 6 分鐘爽快節奏) */
    g_911.shift_sec_acc += sim_ms;
    while (g_911.shift_sec_acc >= 500) {
        g_911.shift_sec_acc -= 500;
        g_911.shift_min++;
        if (g_911.shift_min >= 1200) { /* 20:00 下班結算! */
            sim911_end_shift_settle();
            return 1;
        }
    }

    /* 2. 醫院床位與警局拘留室隨時間逐步釋放 */
    g_911.fac_clear_acc += sim_ms;
    if (g_911.fac_clear_acc >= 8000) {
        g_911.fac_clear_acc = 0;
        for (int i = 0; i < 2; i++) {
            if (g_911.hosp_used[i] > 0) g_911.hosp_used[i]--;
            if (g_911.jail_used[i] > 0) g_911.jail_used[i]--;
        }
    }

    /* 3. 隨機生成街頭事件與 911 來電 */
    g_911.spawn_cd -= sim_ms;
    if (g_911.spawn_cd <= 0) {
        spawn_ambient_incident();
        int base_cd = 12000 - g_911.day * 900; if (base_cd < 5200) base_cd = 5200;
        g_911.spawn_cd = rng_range(base_cd, base_cd + 5000);
        major = 1;
    }

    if (!g_911.call.ringing) {
        g_911.call_cd -= sim_ms;
        if (g_911.call_cd <= 0) {
            g_911.call.ringing = 1;
            g_911.call.script_id = rng_range(0, calls_count() - 1);
            g_911.call.step = 0;
            g_911.call.panic = calls_get_script(g_911.call.script_id)->init_panic;
            g_911.call.emd_bonus = 0; g_911.call.info_revealed = 0;
            g_911.call.ring_timer = 18000;
            g_911.call.last_reply[0] = '\0';
            pick_random_road(&g_911.call.gx, &g_911.call.gy);
            sim911_radio_push(">>> 911 緊急專線來電！請按 [接聽來電 C] <<<");
            g_911.call_cd = rng_range(22000, 34000);
            major = 1;
        }
    } else if (g_911.call.step == 0) {
        g_911.call.ring_timer -= sim_ms;
        if (g_911.call.ring_timer <= 0) {
            g_911.call.ringing = 0;
            if (!calls_get_script(g_911.call.script_id)->is_prank) {
                g_911.reputation -= 6; if (g_911.reputation < 0) g_911.reputation = 0;
                g_911.shift_failed++;
                sim911_radio_push("漏接 911 緊急來電！市民聲譽 -6%");
            }
            major = 1;
        }
    }

    /* 4. 更新所有單位移動 (真實 A* 路網 + 塞車 + 天氣) 與現場處置 */
    int rush = sim911_is_rush_hour();
    const CityDef *city = city_get(g_911.city_id);
    for (int i = 0; i < g_911.unit_count; i++) {
        DispatchUnit *u = &g_911.units[i];
        if (!u->active || u->status == USTAT_IDLE || u->status == USTAT_DISABLED) continue;

        const VehicleSpec *sp = sim911_veh_spec(u->veh_type);
        if (u->status == USTAT_ENROUTE || u->status == USTAT_TRANSPORT || u->status == USTAT_RETURNING) {
            int spd = sp->speed;
            if (!u->is_heli) {
                uint8_t tile = city->tiles[u->gy][u->gx];
                if (rush && (tile == TILE_ARTERIAL || tile == TILE_BRIDGE)) {
                    spd = (u->veh_type == VEH_POL_MOTO || u->veh_type == VEH_EMS_MOTO) ? (spd * 80) / 100 : (spd * 55) / 100;
                }
                if (g_911.weather == WTH_RAIN) spd = (spd * 80) / 100;
                else if (g_911.weather == WTH_FOG) spd = (spd * 85) / 100;
                else if (g_911.weather == WTH_TYPHOON) spd = (spd * 70) / 100;
            }
            if (!g_911.cheat_god_units && u->fatigue > 75) spd = (spd * 80) / 100;
            if (spd < 4) spd = 4;

            u->move_acc += (sim_ms * spd) / 100;
            while (u->move_acc >= 60) {
                u->move_acc -= 60;
                if (u->is_heli) {
                    if (u->gx < u->dest_x) u->gx++; else if (u->gx > u->dest_x) u->gx--;
                    if (u->gy < u->dest_y) u->gy++; else if (u->gy > u->dest_y) u->gy--;
                } else if (u->path_idx < u->path_len) {
                    u->gx = u->path_x[u->path_idx];
                    u->gy = u->path_y[u->path_idx];
                    u->path_idx++;
                }
                if (!g_911.cheat_god_units && u->fuel > 0 && rng_range(0, 3) == 0) u->fuel--;

                /* 抵達檢查 */
                if (u->gx == u->dest_x && u->gy == u->dest_y) {
                    if (u->status == USTAT_ENROUTE) {
                        if (u->target_inc >= 0 && g_911.incidents[u->target_inc].active) {
                            u->status = USTAT_ONSCENE;
                            char rb[96]; snprintf(rb, sizeof(rb), "%s: 10-97 抵達 #%d 現場開始處置", u->callsign, g_911.incidents[u->target_inc].id_num);
                            sim911_radio_push(rb);
                        } else {
                            u->status = USTAT_IDLE; u->target_inc = -1;
                        }
                    } else if (u->status == USTAT_TRANSPORT) {
                        if (u->dept == DEPT_EMS && g_911.hosp_used[u->dest_fac_idx] < city->hospitals[u->dest_fac_idx].cap)
                            g_911.hosp_used[u->dest_fac_idx]++;
                        else if (u->dept == DEPT_POLICE && g_911.jail_used[u->dest_fac_idx] < city->police_hq[u->dest_fac_idx].cap)
                            g_911.jail_used[u->dest_fac_idx]++;
                        u->status = USTAT_RETURNING;
                        route_unit_to(u, u->home_x, u->home_y);
                    } else if (u->status == USTAT_RETURNING) {
                        u->status = USTAT_IDLE; u->target_inc = -1;
                        u->fuel = 100; if (u->fatigue > 25) u->fatigue -= 25; else u->fatigue = 0;
                    }
                    break;
                }
            }
        } else if (u->status == USTAT_ONSCENE) {
            if (u->target_inc < 0 || !g_911.incidents[u->target_inc].active) {
                u->status = USTAT_RETURNING; route_unit_to(u, u->home_x, u->home_y); continue;
            }
            Incident *inc = &g_911.incidents[u->target_inc];
            int eff = sp->power + (u->upg_gear ? 8 : 0) + (u->upg_crew ? 6 : 0);
            if (!g_911.cheat_god_units && u->fatigue > 80) eff = (eff * 75) / 100;

            /* 真實現場風險：沒防彈的巡邏車硬衝武裝搶匪 -> 員警中彈倒地 (Officer Down)! */
            if (u->dept == DEPT_POLICE && inc->armed_threat && !u->upg_armor && !g_911.cheat_god_units && !inc->officer_down_done) {
                if (rng_range(0, 100) < 4) {
                    inc->officer_down_done = 1;
                    u->status = USTAT_DISABLED; u->hp = 20; u->target_inc = -1;
                    inc->req_ems++; inc->work_ems += 100; inc->max_ems += 100; inc->critical_hp = 1;
                    sim911_radio_push("MAYDAY! 警員中彈倒地！急需 SWAT 與救護車增援！");
                    major = 1;
                    continue;
                }
            }

            /* 若案件需要化災/破拆裝備但消防車未裝備，效率減半 */
            if (u->dept == DEPT_FIRE && (inc->req_hazmat || inc->req_jaws) && !u->upg_gear) eff /= 2;

            int delta = (eff * sim_ms) / 550; if (delta < 1) delta = 1;
            if (u->dept == DEPT_POLICE && inc->work_pol > 0) { inc->work_pol -= delta; if (inc->work_pol < 0) inc->work_pol = 0; }
            else if (u->dept == DEPT_FIRE && inc->work_fire > 0) {
                inc->work_fire -= delta; if (inc->work_fire < 0) inc->work_fire = 0;
                if (inc->fire_lvl > 0) { inc->fire_lvl -= 2; if (inc->fire_lvl < 0) inc->fire_lvl = 0; }
            } else if (u->dept == DEPT_EMS && inc->work_ems > 0) {
                inc->work_ems -= delta; if (inc->work_ems < 0) inc->work_ems = 0;
                if (inc->vital_hp < 95) inc->vital_hp += 1;
            }
            if (!g_911.cheat_god_units && u->fatigue < 100 && rng_range(0, 9) == 0) u->fatigue++;
        }
    }

    /* 5. 更新所有進行中案件：完成結算 / 火勢蔓延 / 傷患生命倒數 */
    for (int i = 0; i < MAX_INCIDENTS; i++) {
        Incident *inc = &g_911.incidents[i];
        if (!inc->active) continue;

        /* 檢查是否已全部部門處置完畢 (Code 4!) */
        if (inc->work_pol <= 0 && inc->work_fire <= 0 && inc->work_ems <= 0) {
            inc->active = 0;
            g_911.shift_handled++; g_911.total_handled_all++;
            g_911.budget += inc->reward_cash; g_911.shift_earned += inc->reward_cash;
            if (g_911.reputation < 100) g_911.reputation += (inc->priority == 1 ? 3 : 2);
            if (g_911.reputation > 100) g_911.reputation = 100;

            char rb[96]; snprintf(rb, sizeof(rb), "Code 4! #%d %s 處理完畢 (+$%d)", inc->id_num, inc->title, inc->reward_cash);
            sim911_radio_push(rb);

            /* 現場單位轉入送醫/押解或返隊 */
            for (int u = 0; u < g_911.unit_count; u++) {
                if (g_911.units[u].target_inc == i) {
                    g_911.units[u].target_inc = -1;
                    if (g_911.units[u].status == USTAT_ONSCENE) send_unit_to_facility(&g_911.units[u]);
                    else if (g_911.units[u].status == USTAT_ENROUTE) {
                        g_911.units[u].status = USTAT_RETURNING;
                        route_unit_to(&g_911.units[u], g_911.units[u].home_x, g_911.units[u].home_y);
                    }
                }
            }
            major = 1;
            continue;
        }

        /* 檢查現場有沒有對應單位壓制 */
        int has_ems = 0, has_fire = 0;
        for (int u = 0; u < g_911.unit_count; u++) {
            if (g_911.units[u].target_inc == i && g_911.units[u].status == USTAT_ONSCENE) {
                if (g_911.units[u].dept == DEPT_EMS) has_ems = 1;
                if (g_911.units[u].dept == DEPT_FIRE) has_fire = 1;
            }
        }

        /* 動態惡化：沒人救的火會蔓延、危急傷患生命值會掉！ */
        if (rng_range(0, 100) < (sim_ms / 22 + 1)) {
            if (inc->fire_hazard && !has_fire && inc->fire_lvl < 100) {
                inc->fire_lvl += 2;
                if (inc->fire_lvl >= 85 && inc->work_fire < 260) {
                    inc->work_fire += 40; inc->req_fire = 2;
                    sim911_radio_push("警告！火勢失控蔓延升級為大型火警！");
                }
            }
            int drain = inc->critical_hp ? (inc->emd_stabilized ? 1 : 2) : 1;
            if (has_ems) drain = 0;
            inc->vital_hp -= drain;
            if (inc->vital_hp <= 0) {
                inc->active = 0;
                g_911.shift_failed++;
                int fine = (inc->priority == 1) ? 4500 : 2000;
                if (!g_911.cheat_inf_budget) { g_911.budget -= fine; if (g_911.budget < 0) g_911.budget = 0; }
                g_911.shift_fines += fine;
                g_911.reputation -= (inc->priority == 1 ? 8 : 4);
                if (g_911.reputation < 0) g_911.reputation = 0;
                char rb[96]; snprintf(rb, sizeof(rb), "失職！#%d %s 搶救不及 (-$%d)", inc->id_num, inc->title, fine);
                sim911_radio_push(rb);
                for (int u = 0; u < g_911.unit_count; u++) {
                    if (g_911.units[u].target_inc == i) {
                        g_911.units[u].target_inc = -1; g_911.units[u].status = USTAT_RETURNING;
                        route_unit_to(&g_911.units[u], g_911.units[u].home_x, g_911.units[u].home_y);
                    }
                }
                major = 1;
            }
        }
    }

    return major;
}

void sim911_end_shift_settle(void) {
    g_911.in_shift = 0;
    int grant = 12000 + g_911.reputation * 180 + g_911.day * 2500;
    g_911.budget += grant;
    g_911.shift_earned += grant;
    g_911.day++;
    sim911_refuel_rest_all();
    sim911_save();
}

int sim911_buy_vehicle(int veh_type, char *out_msg, int msg_sz) {
    if (g_911.unit_count >= MAX_UNITS) { snprintf(out_msg, (size_t)msg_sz, "編制已達上限 (%d 組)！", MAX_UNITS); return 0; }
    const VehicleSpec *sp = sim911_veh_spec(veh_type);
    if (!g_911.cheat_inf_budget && g_911.budget < sp->price) {
        snprintf(out_msg, (size_t)msg_sz, "預算不足！需要 $%d", sp->price); return 0;
    }
    if (!g_911.cheat_inf_budget) g_911.budget -= sp->price;
    const CityDef *c = city_get(g_911.city_id);
    int gx = (sp->dept == DEPT_POLICE) ? c->police_hq[0].gx : ((sp->dept == DEPT_FIRE) ? c->fire_hq[0].gx : c->hospitals[0].gx);
    int gy = (sp->dept == DEPT_POLICE) ? c->police_hq[0].gy : ((sp->dept == DEPT_FIRE) ? c->fire_hq[0].gy : c->hospitals[0].gy);
    add_starter_unit(veh_type, gx, gy);
    snprintf(out_msg, (size_t)msg_sz, "新載具入列：%s (%s)！", sp->name, g_911.units[g_911.unit_count - 1].callsign);
    sim911_save(); return 1;
}

int sim911_upgrade_unit(int uid, int upg_type, char *out_msg, int msg_sz) {
    if (uid < 0 || uid >= g_911.unit_count) return 0;
    DispatchUnit *u = &g_911.units[uid];
    int cost = (upg_type == 0) ? 8000 : ((upg_type == 1) ? 12000 : 10000);
    if ((upg_type == 0 && u->upg_armor) || (upg_type == 1 && u->upg_gear) || (upg_type == 2 && u->upg_crew)) {
        snprintf(out_msg, (size_t)msg_sz, "%s 該項目已升級！", u->callsign); return 0;
    }
    if (!g_911.cheat_inf_budget && g_911.budget < cost) {
        snprintf(out_msg, (size_t)msg_sz, "預算不足！升級需 $%d", cost); return 0;
    }
    if (!g_911.cheat_inf_budget) g_911.budget -= cost;
    if (upg_type == 0) { u->upg_armor = 1; snprintf(out_msg, (size_t)msg_sz, "%s 已配發防彈/防護重裝！", u->callsign); }
    else if (upg_type == 1) { u->upg_gear = 1; snprintf(out_msg, (size_t)msg_sz, "%s 已配發專業特勤/破拆/AED裝備！", u->callsign); }
    else { u->upg_crew = 1; snprintf(out_msg, (size_t)msg_sz, "%s 隊員已完成高階專長訓練！", u->callsign); }
    sim911_save(); return 1;
}

int sim911_set_unit_home(int uid, int gx, int gy, char *out_msg, int msg_sz) {
    if (uid < 0 || uid >= g_911.unit_count) return 0;
    DispatchUnit *u = &g_911.units[uid];
    city_nearest_road(g_911.city_id, gx, gy, &u->home_x, &u->home_y);
    u->gx = u->home_x; u->gy = u->home_y;
    snprintf(out_msg, (size_t)msg_sz, "%s 待命點已部署至 (%d, %d)", u->callsign, u->home_x, u->home_y);
    sim911_save(); return 1;
}

void sim911_call_choose(int choice_idx) {
    if (!g_911.call.ringing || choice_idx < 0 || choice_idx >= 3) return;
    const CallScript *sc = calls_get_script(g_911.call.script_id);
    const CallChoice *ch = (g_911.call.step == 0) ? &sc->step0[choice_idx] : &sc->step1[choice_idx];
    g_911.call.panic += ch->panic_delta;
    if (g_911.call.panic < 0) g_911.call.panic = 0;
    if (g_911.call.panic > 100) g_911.call.panic = 100;
    if (ch->emd_bonus) g_911.call.emd_bonus = 1;
    if (ch->reveal_info) g_911.call.info_revealed = 1;
    snprintf(g_911.call.last_reply, sizeof(g_911.call.last_reply), "%s", ch->reply);
    g_911.call.step = ch->next_step;
}

void sim911_call_resolve(int do_dispatch, char *out_msg, int msg_sz) {
    if (!g_911.call.ringing) return;
    const CallScript *sc = calls_get_script(g_911.call.script_id);
    int gx = g_911.call.gx, gy = g_911.call.gy, emd = g_911.call.emd_bonus;
    g_911.call.ringing = 0;

    if (sc->is_prank) {
        if (!do_dispatch) {
            g_911.shift_pranks_blocked++;
            if (g_911.reputation < 100) g_911.reputation += 2;
            snprintf(out_msg, (size_t)msg_sz, "精準識破惡作劇報案！省下寶貴警力，聲譽 +2%%");
            sim911_radio_push("已攔截無效/惡作劇 911 來電");
        } else {
            g_911.reputation -= 3; if (g_911.reputation < 0) g_911.reputation = 0;
            snprintf(out_msg, (size_t)msg_sz, "誤派單位處理惡作劇電話！聲譽 -3%%");
        }
        return;
    }

    if (!do_dispatch) {
        g_911.shift_failed++;
        g_911.reputation -= 12; if (g_911.reputation < 0) g_911.reputation = 0;
        snprintf(out_msg, (size_t)msg_sz, "嚴重失職！掛斷真實緊急求救電話，聲譽 -12%%");
        sim911_radio_push("嚴重警告：真實求救遭掛斷！");
        return;
    }

    if (emd) {
        g_911.shift_emd_saves++;
        if (g_911.reputation < 100) g_911.reputation += 2;
    }
    spawn_incident_from_params(sc->title, gx, gy, sc->priority, sc->req_pol, sc->req_fire, sc->req_ems,
                               sc->armed_threat, sc->fire_hazard, sc->critical_hp, sc->req_hazmat, sc->req_jaws, emd);
    snprintf(out_msg, (size_t)msg_sz, emd ? "已立案並成功給予線上急救指導 (傷患穩定)！請快派車" : "911 案件已立案標記在地圖上！請立即派車");
}

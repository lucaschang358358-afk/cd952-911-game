#ifndef SIM_911_H
#define SIM_911_H

#include <stdint.h>
#include "city_map.h"
#include "calls_911.h"

#define MAX_UNITS      18
#define MAX_INCIDENTS  14
#define MAX_RADIO_LOG  16
#define SAVE_911_MAGIC 0x39313131u /* "9111" */

typedef enum {
    DEPT_POLICE = 0,
    DEPT_FIRE,
    DEPT_EMS
} DeptType;

typedef enum {
    VEH_PATROL = 0,     /* 警用巡邏車 */
    VEH_POL_MOTO,       /* 警用重機 (極速) */
    VEH_SWAT,           /* SWAT 特勤裝甲車 (防彈+重火) */
    VEH_POL_HELI,       /* 警用直升機 (無視塞車，颱風停飛) */
    VEH_FIRE_ENG,       /* 水箱消防車 */
    VEH_FIRE_LADDER,    /* 雲梯救助車 (含油壓剪+化災) */
    VEH_FIRE_HELI,      /* 消防直升機 */
    VEH_AMBULANCE,      /* 標準救護車 */
    VEH_EMS_MOTO,       /* 快反救護機車 */
    VEH_EMS_HELI,       /* 空中醫療直升機 */
    NUM_VEH_TYPES
} VehicleType;

typedef enum {
    USTAT_IDLE = 0,     /* 10-8 待命 */
    USTAT_ENROUTE,      /* Code 3 趕赴現場 */
    USTAT_ONSCENE,      /* 10-97 現場處置 */
    USTAT_TRANSPORT,    /* 10-15 送醫/押解 */
    USTAT_RETURNING,    /* 10-23 返隊補給 */
    USTAT_DISABLED      /* 負傷/損毀 (需班後修復) */
} UnitStatus;

typedef enum {
    WTH_CLEAR = 0,      /* 晴朗 */
    WTH_RAIN,           /* 豪雨 (-20%車速) */
    WTH_FOG,            /* 濃霧 (-15%車速) */
    WTH_TYPHOON         /* 強風颱風 (-30%車速 + 直升機停飛!) */
} WeatherType;

typedef struct {
    const char *name;
    const char *short_code;
    uint8_t dept;
    uint8_t is_heli;
    int price;
    int speed;          /* 移動速度基準 (越高越快) */
    int power;          /* 處置效率 */
    int has_armor;      /* 內建防彈/重裝 */
    int has_special;    /* 內建特勤/破拆化災/重症AED */
    const char *desc;
} VehicleSpec;

typedef struct {
    int active;
    char callsign[10];
    uint8_t veh_type;
    uint8_t dept;
    uint8_t is_heli;
    uint8_t upg_armor;      /* 已升級防彈/防護裝備 */
    uint8_t upg_gear;       /* 已升級專業裝備 (步槍/油壓剪防毒/AED重症) */
    uint8_t upg_crew;       /* 已受進階專長訓練 (談判/高級救護/化災) */

    int gx, gy;             /* 當前座標 */
    int home_x, home_y;     /* 預部署待命點 */
    int status;             /* UnitStatus */
    int target_inc;         /* 目標案件索引 (-1=無) */
    int dest_x, dest_y;     /* 押解/送醫目標點 */
    int dest_fac_idx;       /* 0 或 1 醫院/警局 */

    int fuel;               /* 0..100 */
    int fatigue;            /* 0..100 */
    int hp;                 /* 0..100 */

    uint8_t path_x[MAX_PATH_LEN];
    uint8_t path_y[MAX_PATH_LEN];
    int path_len;
    int path_idx;
    int move_acc;
} DispatchUnit;

typedef struct {
    int active;
    int id_num;
    char title[56];
    char district[24];
    int gx, gy;
    int priority;           /* 1=P1, 2=P2, 3=P3 */

    int req_pol, req_fire, req_ems;
    int work_pol, work_fire, work_ems; /* 剩餘處置進度 (0=完成) */
    int max_pol, max_fire, max_ems;

    int armed_threat;
    int fire_hazard;
    int critical_hp;
    int req_hazmat;
    int req_jaws;
    int emd_stabilized;
    int officer_down_done;

    int vital_hp;           /* 0..100 (傷患/人質生命或時限倒數) */
    int fire_lvl;           /* 0..100 (火勢蔓延度) */
    int reward_cash;
} Incident;

typedef struct {
    int ringing;
    int script_id;
    int step;               /* 0, 1, 2(決定派車/掛斷) */
    int panic;
    int emd_bonus;
    int info_revealed;
    int ring_timer;         /* 未接聽倒數 */
    int gx, gy;
    char last_reply[112];
} ActiveCall;

typedef struct {
    uint32_t magic;
    int initialized;

    int city_id;
    int day;
    int budget;
    int reputation;         /* 0..100 */

    /* 班次狀態 */
    int in_shift;
    int shift_min;          /* 480 (08:00) .. 1200 (20:00) */
    int shift_sec_acc;
    int weather;            /* WeatherType */
    int paused;
    int speed_idx;          /* 0=1x, 1=2x, 2=4x */

    /* 醫院急診室與警局拘留室佔用數 */
    int hosp_used[2];
    int jail_used[2];
    int fac_clear_acc;

    DispatchUnit units[MAX_UNITS];
    int unit_count;

    Incident incidents[MAX_INCIDENTS];
    int next_inc_id;
    int spawn_cd;
    int call_cd;

    ActiveCall call;

    /* 本班次統計 (Debrief 結算) */
    int shift_handled;
    int shift_failed;
    int shift_pranks_blocked;
    int shift_emd_saves;
    int shift_earned;
    int shift_fines;
    int total_handled_all;

    /* 沙盒 / 外掛模式 */
    int cheat_inf_budget;
    int cheat_god_units;

    /* 即時無線電通聯紀錄 */
    char radio_log[MAX_RADIO_LOG][128];
    int  radio_count;
} Sim911World;

extern Sim911World g_911;

const VehicleSpec *sim911_veh_spec(int veh_type);
const char        *sim911_weather_name(int w);
const char        *sim911_unit_status_code(int st);
int                sim911_is_rush_hour(void);

void sim911_init(void);
void sim911_save(void);
int  sim911_load(void);
void sim911_new_career(int city_id);

void sim911_start_shift(void);
int  sim911_tick(int dt_ms); /* 回傳 1=有重大事件或班次結束 */
void sim911_end_shift_settle(void);

/* 整備與裝備購買 */
int  sim911_buy_vehicle(int veh_type, char *out_msg, int msg_sz);
int  sim911_upgrade_unit(int uid, int upg_type, char *out_msg, int msg_sz); /* 0=防彈防護, 1=專業裝備, 2=人員培訓 */
int  sim911_set_unit_home(int uid, int gx, int gy, char *out_msg, int msg_sz);
void sim911_refuel_rest_all(void);

/* 戰術派遣 */
int  sim911_dispatch_unit(int uid, int inc_idx, char *out_msg, int msg_sz);
int  sim911_dispatch_nearest_for_inc(int inc_idx, char *out_msg, int msg_sz);
int  sim911_recall_unit(int uid, char *out_msg, int msg_sz);

/* 911 來電互動 */
void sim911_call_choose(int choice_idx);
void sim911_call_resolve(int do_dispatch, char *out_msg, int msg_sz);

/* 沙盒與外掛 */
void sim911_trigger_major_disaster(char *out_msg, int msg_sz);
void sim911_radio_push(const char *msg);

#endif

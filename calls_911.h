#ifndef CALLS_911_H
#define CALLS_911_H

#include <stdint.h>

#define NUM_CALL_SCRIPTS 18

typedef struct {
    const char *label;      /* 調度員選項文字 */
    const char *reply;      /* 報案人回應 */
    int panic_delta;        /* 恐慌值變化 (負值=安撫成功) */
    int emd_bonus;          /* 1=成功給出正確線上急救/自保指導 (穩住生命條!) */
    int reveal_info;        /* 1=問出確切地址與威脅等級 */
    int next_step;          /* 1=進入第二步, 2=結束對話進入派車判斷 */
} CallChoice;

typedef struct {
    const char *title;
    const char *caller_open;
    int init_panic;         /* 0..100 */
    int is_prank;           /* 1=惡作劇/無效報案 (應該直接掛斷!) */
    int priority;           /* 1=P1危及生命, 2=P2緊急, 3=P3一般 */
    int req_pol;            /* 需要警力數 (0..3) */
    int req_fire;           /* 需要消防數 (0..3) */
    int req_ems;            /* 需要救護數 (0..3) */
    int armed_threat;       /* 1=歹徒持槍/重武裝 (一般巡邏車硬上會中彈!) */
    int fire_hazard;        /* 1=會蔓延的火勢 */
    int critical_hp;        /* 1=傷患生命倒數 */
    int req_hazmat;         /* 1=需要防毒/化災裝備 */
    int req_jaws;           /* 1=需要油壓剪破拆 */
    CallChoice step0[3];
    CallChoice step1[3];
} CallScript;

const CallScript *calls_get_script(int id);
int               calls_count(void);

#endif

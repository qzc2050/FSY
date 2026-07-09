#include "hist_5min_query.h"
#include "hist_5min.h"
#include "fsy_upload.h"
#include "fsy_link.h"
#include "fsy_regmap.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

#ifndef HIST_5MIN_QUERY_INTERVAL_MS
#define HIST_5MIN_QUERY_INTERVAL_MS  80U
#endif

/* 查询启动后先等 ACK/总线空闲，再发首条历史帧（与 printf 同口时尤其必要） */
#ifndef HIST_5MIN_QUERY_FIRST_DELAY_MS
#define HIST_5MIN_QUERY_FIRST_DELAY_MS  120U
#endif

/*
 * 默认关闭：HISTQ 的 printf 与 0x23 协议帧共用 USART1，
 * 会把历史帧冲成乱码（上位机常见 LOG 609xxx count=1）。
 * 需要串口调试时再定义 HIST_5MIN_QUERY_DEBUG。
 */
#ifdef HIST_5MIN_QUERY_DEBUG
#define HISTQ_LOG(...)  printf(__VA_ARGS__)
#else
#define HISTQ_LOG(...)  ((void)0)
#endif

typedef enum {
    HIST_Q_IDLE = 0,
    HIST_Q_UPLOADING,
} HistQState_t;

static HistQState_t s_state;
static uint32_t s_ts_start;
static uint32_t s_ts_end;
static int16_t s_scan_idx;
static uint16_t s_queued;
static uint32_t s_last_send_tick;
static uint32_t s_query_start_tick;

static void hist_q_finish(void)
{
    HISTQ_LOG("[HISTQ] done queued=%u\r\n", (unsigned)s_queued);
    Fsy_Regmap_ClearHistQueryRegs();
    s_state = HIST_Q_IDLE;
    s_ts_start = 0U;
    s_ts_end = 0U;
    s_scan_idx = -1;
    s_queued = 0U;
    s_last_send_tick = 0U;
    s_query_start_tick = 0U;
}

void Hist5Min_Query_Start(const uint8_t start_dt8[8], const uint8_t end_dt8[8])
{
    uint16_t count;

    if ((start_dt8 == NULL) || (end_dt8 == NULL)) {
        return;
    }

    /* 新查询打断旧查询 */
    s_state = HIST_Q_IDLE;

    s_ts_start = Hist5Min_Dt8ToUnix(start_dt8);
    s_ts_end = Hist5Min_Dt8ToUnix(end_dt8);
    if ((s_ts_start == 0U) || (s_ts_end == 0U) || (s_ts_end < s_ts_start)) {
        HISTQ_LOG("[HISTQ] invalid range, ignore\r\n");
        Fsy_Regmap_ClearHistQueryRegs();
        return;
    }

    count = Hist5Min_GetCount();
    s_queued = 0U;
    s_scan_idx = (count > 0U) ? (int16_t)(count - 1U) : (int16_t)-1;
    s_last_send_tick = 0U;
    s_query_start_tick = HAL_GetTick();

    if (s_scan_idx < 0) {
        HISTQ_LOG("[HISTQ] empty flash, done\r\n");
        Fsy_Regmap_ClearHistQueryRegs();
        return;
    }

    s_state = HIST_Q_UPLOADING;
    HISTQ_LOG("[HISTQ] start %lu~%lu count=%u\r\n",
              (unsigned long)s_ts_start, (unsigned long)s_ts_end, (unsigned)count);
}

void Hist5Min_Query_Pump(void)
{
    uint32_t now;
    Hist5MinRecord_t rec;
    uint8_t dt8[8];

    if (s_state != HIST_Q_UPLOADING) {
        return;
    }

    now = HAL_GetTick();
    if ((now - s_query_start_tick) < HIST_5MIN_QUERY_FIRST_DELAY_MS) {
        return;
    }
    if ((s_last_send_tick != 0U) &&
        ((now - s_last_send_tick) < HIST_5MIN_QUERY_INTERVAL_MS)) {
        return;
    }

    while (s_scan_idx >= 0) {
        if (Hist5Min_Read((uint16_t)s_scan_idx, &rec) != 0) {
            s_scan_idx--;
            continue;
        }
        if ((rec.unix_ts < s_ts_start) || (rec.unix_ts > s_ts_end)) {
            s_scan_idx--;
            continue;
        }
        if (Hist5Min_UnixToDt8(rec.unix_ts, dt8) != 0) {
            s_scan_idx--;
            continue;
        }

        if (Fsy_Upload_Send5MinHist(dt8, rec.dose_x100, Fsy_Link_WriteUpload) < 0) {
            return;
        }

        s_queued++;
        s_last_send_tick = now;
        s_scan_idx--;
        return;
    }

    hist_q_finish();
}

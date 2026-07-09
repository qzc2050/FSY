#ifndef HIST_5MIN_H
#define HIST_5MIN_H

#include <stdint.h>
#include "pcf85063.h"
#include "ext_flash_layout.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 单条记录：与 ext_flash_layout 一致（8B） */
typedef struct {
    uint32_t unix_ts;    /* 窗结束时刻（本地墙钟伪 Unix 秒） */
    uint32_t dose_x100;  /* D5 μSv×100（与协议一致，保留 0.01μSv） */
} Hist5MinRecord_t;

#define HIST_5MIN_META_MAGIC  0x444D354Du  /* "DM5M" */
#define HIST_5MIN_MAX_RECORDS DATA_5_MIN_MAX_RECORDS

/**
 * 初始化 5min Flash 环（读 meta；损坏则重建空环）
 * @return 0 成功，其它失败
 */
int Hist5Min_Init(void);

/**
 * 追加一条记录（环满覆盖最旧）；不对时改写旧 unix_ts
 * @param unix_ts 窗结束时刻
 * @param dose_x100 D5 μSv×100
 * @return 0 成功，其它失败
 */
int Hist5Min_Write(uint32_t unix_ts, uint32_t dose_x100);

/**
 * 有效记录数（0..288）
 */
uint16_t Hist5Min_GetCount(void);

/**
 * 按逻辑索引读（0=最旧 … count-1=最新）
 * @return 0 成功，其它失败
 */
int Hist5Min_Read(uint16_t logical_index, Hist5MinRecord_t *out);

/**
 * RTC 日历 → 伪 Unix 秒（本地字段按无时区公式；year<100 视为 2000+yy）
 * @return 0 表示失败
 */
uint32_t Hist5Min_DateTimeToUnix(const Pcf85063_DateTime_t *dt);

/** 协议 8B data_time → 伪 Unix；非法返回 0 */
uint32_t Hist5Min_Dt8ToUnix(const uint8_t dt8[8]);

/** 伪 Unix → 协议 8B data_time；失败返回 -1 */
int Hist5Min_UnixToDt8(uint32_t unix_ts, uint8_t out_dt8[8]);

#ifdef __cplusplus
}
#endif

#endif /* HIST_5MIN_H */

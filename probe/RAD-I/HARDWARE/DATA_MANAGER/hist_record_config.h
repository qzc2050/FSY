#ifndef __HIST_RECORD_CONFIG_H
#define __HIST_RECORD_CONFIG_H

#include "dm_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/*==========================================================================
 * 历史记录应用层配置
 * 独立于底层 dm_config.h
 *==========================================================================*/

/** 最大记录数（影响扇区数量） */
#ifndef HIST_RECORD_MAX_RECORDS
#define HIST_RECORD_MAX_RECORDS  300U
#endif

/** 每条记录大小（字节） */
#define HIST_RECORD_SIZE_BYTES   8U  /* uint32 unix_ts + uint32 dose_uSv */

/** 日期时间字符串长度 */
#ifndef HIST_DATE_TIME_LEN
#define HIST_DATE_TIME_LEN       15U
#endif

/** 剂量值字符串长度 */
#ifndef HIST_DOSE_VALUE_LEN
#define HIST_DOSE_VALUE_LEN      10U
#endif

/** 扇区大小（与底层一致） */
#define HIST_SECTOR_SIZE         DM_SECTOR_SIZE  /* 4096 字节 */

/** 存储空间基址 */
#define HIST_STORAGE_BASE        0x00000000U

/** 存储空间大小（自动计算） */
#define HIST_FORMAL_SECTORS      ((HIST_RECORD_MAX_RECORDS + (HIST_SECTOR_SIZE / HIST_RECORD_SIZE_BYTES) - 1U) / \
                                  (HIST_SECTOR_SIZE / HIST_RECORD_SIZE_BYTES))
#define HIST_CACHE_SECTORS       HIST_FORMAL_SECTORS
#define HIST_TEMP_SECTORS        1U
#define HIST_META_SECTORS        1U

#define HIST_STORAGE_SIZE        ((HIST_FORMAL_SECTORS + HIST_CACHE_SECTORS + \
                                  HIST_TEMP_SECTORS + HIST_META_SECTORS) * HIST_SECTOR_SIZE)

/** 元数据魔数（5 分钟历史记录专用） */
#define HIST_META_MAGIC          0x444D354Du  /* "DM5M" */

/** 串口/日志打印标签（兼容旧 DATA_5_MIN 命名） */
#ifndef DATA_5_MIN_LABEL
#define DATA_5_MIN_LABEL         "[5min]"
#endif

#ifndef DATA_5_MIN_MAX_RECORDS
#define DATA_5_MIN_MAX_RECORDS     HIST_RECORD_MAX_RECORDS
#endif

/*==========================================================================
 * 调试配置
 *==========================================================================*/

/** 是否启用时间调整详细调试 */
#ifndef HIST_DEBUG_TIME_ADJUST
#define HIST_DEBUG_TIME_ADJUST   1U
#endif

/** 调试打印宏 */
#if HIST_DEBUG_TIME_ADJUST
#define HIST_DEBUG(fmt, ...)  printf("[HIST] " fmt, ##__VA_ARGS__)
#else
#define HIST_DEBUG(fmt, ...)  ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* __HIST_RECORD_CONFIG_H */

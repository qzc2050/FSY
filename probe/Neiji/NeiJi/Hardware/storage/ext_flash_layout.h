#ifndef __EXT_FLASH_LAYOUT_H
#define __EXT_FLASH_LAYOUT_H

#include "w25qxx.h"
#include <stdint.h>

/**
 * 外部 QSPI Flash（W25Q64JV：8MB）布局：
 * - 5min 元数据扇区（1×4KB）
 * - 5min 数据扇区（按 max_records 自动计算，环形主存储）
 * - 5min 缓存扇区（与数据扇区同数量，第二轮追加写，满则迁移到最旧数据扇区）
 * - 尾部：配置区（DEVICE_CFG_*）
 *
 * 每条记录 EXT_FLASH_DATA_RECORD_BYTES 字节：uint32 unix_ts(LE) + uint32 dose_x100(LE)
 *
 * 扩展记录数：修改 DATA_5_MIN_MAX_RECORDS，数据/缓存扇区数会自动重算。
 */

#define EXT_FLASH_META_SECTOR_SIZE 4096U

#ifndef EXT_FLASH_DATA_RECORD_BYTES
#define EXT_FLASH_DATA_RECORD_BYTES 8U
#endif

#ifndef DATA_5_MIN_MAX_RECORDS
#define DATA_5_MIN_MAX_RECORDS 288U  /* 12 条/h × 24h */
#endif

/** 向上对齐（#if 用，勿加强转） */
#define EXT_FLASH_ALIGN_UP(x, a) (((x) + (a)-1U) & ~((a)-1U))

/** 编译期除法上取整 */
#define EXT_FLASH_DM_CEIL_DIV(a, b) (((a) + (b)-1U) / (b))

/** 每扇区可存放的记录条数（4096 / 8 = 512） */
#define EXT_FLASH_DM_SLOTS_PER_SECTOR \
    (EXT_FLASH_META_SECTOR_SIZE / EXT_FLASH_DATA_RECORD_BYTES)

/** 主数据区扇区数：覆盖 max_records 条记录 */
#define EXT_FLASH_DM_DATA_SECTOR_COUNT \
    EXT_FLASH_DM_CEIL_DIV(DATA_5_MIN_MAX_RECORDS, EXT_FLASH_DM_SLOTS_PER_SECTOR)

/**
 * 缓存区扇区数：与主数据区相同，可容纳一整轮 max_records 追加写后再做扇区迁移。
 */
#define EXT_FLASH_DM_CACHE_SECTOR_COUNT EXT_FLASH_DM_DATA_SECTOR_COUNT

#define EXT_FLASH_DM_DATA_REGION_SIZE \
    (EXT_FLASH_DM_DATA_SECTOR_COUNT * EXT_FLASH_META_SECTOR_SIZE)

#define EXT_FLASH_DM_CACHE_REGION_SIZE \
    (EXT_FLASH_DM_CACHE_SECTOR_COUNT * EXT_FLASH_META_SECTOR_SIZE)

/** 5min 元数据扇区 */
#define EXT_FLASH_DATA_5MIN_META_BASE 0x00000000U

/** 5min 主数据扇区起始 */
#define EXT_FLASH_DATA_5MIN_RECORDS_BASE \
    (EXT_FLASH_DATA_5MIN_META_BASE + EXT_FLASH_META_SECTOR_SIZE)

/** 5min 缓存扇区起始（紧跟主数据区，4KB 对齐） */
#define EXT_FLASH_DATA_5MIN_CACHE_BASE \
    (EXT_FLASH_DATA_5MIN_RECORDS_BASE + EXT_FLASH_DM_DATA_REGION_SIZE)

/** 5min 整区结束（含缓存） */
#define EXT_FLASH_DATA_5MIN_REGION_END \
    (EXT_FLASH_DATA_5MIN_CACHE_BASE + EXT_FLASH_DM_CACHE_REGION_SIZE)

/** 配置区：Flash 末尾 64KB */
#ifndef EXT_FLASH_CONFIG_REGION_SIZE
#define EXT_FLASH_CONFIG_REGION_SIZE 0x10000U
#endif
#define EXT_FLASH_CONFIG_REGION_BASE (W25QxJV_FLASH_SIZE - EXT_FLASH_CONFIG_REGION_SIZE)

#if (EXT_FLASH_DATA_5MIN_REGION_END > EXT_FLASH_CONFIG_REGION_BASE)
#error "5min 数据/缓存区与末尾配置区重叠，请减小 DATA_5_MIN_MAX_RECORDS"
#endif

/** 主数据区内第 i 个扇区基址（i 从 0 起） */
#define EXT_FLASH_DM_DATA_SECTOR_BASE(i) \
    (EXT_FLASH_DATA_5MIN_RECORDS_BASE + (uint32_t)(i) * EXT_FLASH_META_SECTOR_SIZE)

/** 缓存区内第 i 个扇区基址 */
#define EXT_FLASH_DM_CACHE_SECTOR_BASE(i) \
    (EXT_FLASH_DATA_5MIN_CACHE_BASE + (uint32_t)(i) * EXT_FLASH_META_SECTOR_SIZE)

/** W25Q 擦除块大小（4KB，与 META 扇区一致） */
#define EXT_FLASH_SECTOR_SIZE           EXT_FLASH_META_SECTOR_SIZE

/**
 * 配置区子布局（64KB 末尾区，device_config 主备 + 自检）：
 *   +0x0000  主配置（DeviceCfgBlob）
 *   +0x1000  备配置
 *   +0x2000  上电自检 scratch（W25Q_Port_SelfTest）
 */
#define EXT_FLASH_CFG_PRIMARY_ADDR      EXT_FLASH_CONFIG_REGION_BASE
#define EXT_FLASH_CFG_BACKUP_ADDR \
    (EXT_FLASH_CONFIG_REGION_BASE + EXT_FLASH_SECTOR_SIZE)
#define EXT_FLASH_SELFTEST_ADDR \
    (EXT_FLASH_CONFIG_REGION_BASE + 2U * EXT_FLASH_SECTOR_SIZE)

#endif

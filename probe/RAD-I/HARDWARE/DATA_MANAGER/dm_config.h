#ifndef __DM_CONFIG_H
#define __DM_CONFIG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*==========================================================================
 * 数据管理配置头文件
 * 所有可配置参数都在这里定义
 *==========================================================================*/

/*==========================================================================
 * 通用配置
 *==========================================================================*/

/** 元数据魔数（通用） */
#define DM_META_MAGIC_GENERIC  0x444D4745u  /* "DMGE" */

/** 元数据版本号 */
#define DM_META_VERSION         2U

/** 扇区大小（字节） */
#define DM_SECTOR_SIZE          4096U

/*==========================================================================
 * 写入策略配置
 *==========================================================================*/

/** 每写入 N 条记录后保存一次元数据（降低 Flash 擦除次数） */
#ifndef DM_META_SAVE_INTERVAL
#define DM_META_SAVE_INTERVAL   2U
#endif

/** 写入失败最大重试次数 */
#ifndef DM_WRITE_MAX_ATTEMPTS
#define DM_WRITE_MAX_ATTEMPTS   4U
#endif

/** 是否跳过写后验证（仅调试用，正常应为 0） */
#ifndef DM_SKIP_WRITE_VERIFY
#define DM_SKIP_WRITE_VERIFY    0U
#endif

/*==========================================================================
 * 读取配置
 *==========================================================================*/

/** 批量读取最大行数 */
#ifndef DM_READALL_MAX_LINES
#define DM_READALL_MAX_LINES    256U
#endif

/** 批量读取块大小 */
#ifndef DM_BULK_BLOCK_MAX
#define DM_BULK_BLOCK_MAX       2048U
#endif

/*==========================================================================
 * 调试配置
 *==========================================================================*/

/** 是否启用调试打印 */
#ifndef DM_DEBUG_ENABLE
#define DM_DEBUG_ENABLE         1U
#endif

/** 调试打印宏 */
#if DM_DEBUG_ENABLE
#define DM_DEBUG(fmt, ...)  printf("[DM] " fmt, ##__VA_ARGS__)
#else
#define DM_DEBUG(fmt, ...)  ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* __DM_CONFIG_H */

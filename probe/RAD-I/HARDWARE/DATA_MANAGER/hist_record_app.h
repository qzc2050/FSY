#ifndef __HIST_RECORD_APP_H
#define __HIST_RECORD_APP_H

#include "dm_core.h"
#include "hist_record_format.h"
#include "hist_record_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/*==========================================================================
 * 历史记录应用层 API
 * 基于核心层实现的历史记录管理功能
 *==========================================================================*/

/*==========================================================================
 * 初始化接口
 *==========================================================================*/

/**
 * 初始化历史记录管理
 * @return 0 成功，其他失败
 */
int HistRecord_Init(void);

/*==========================================================================
 * 基础读写接口
 *==========================================================================*/

/**
 * 写入一条历史记录
 * @param datetime 日期时间字符串（格式：YYYYMMDD,HHMMSS）
 * @param dose_uSv 剂量值（微希沃特 uSv）
 * @return 0 成功，其他失败
 */
int HistRecord_Write(const char *datetime, uint32_t dose_uSv);

/**
 * 读取一条历史记录
 * @param index 逻辑索引（从 0 开始）
 * @param out_datetime 输出日期时间缓冲区（至少 HIST_DATE_TIME_LEN 字节）
 * @param out_dose_value 输出剂量值缓冲区（至少 HIST_DOSE_VALUE_LEN 字节）
 * @return 0 成功，其他失败
 */
int HistRecord_Read(
    uint16_t index,
    char *out_datetime,
    char *out_dose_value
);

/**
 * 获取有效记录数
 * @return 有效记录数
 */
uint16_t HistRecord_GetValidCount(void);

/**
 * 读取所有记录并打印
 * @return 读取的记录数
 */
uint16_t HistRecord_ReadAll(void);

/**
 * 打印单条记录
 * @param index 逻辑索引
 * @return 0 成功，其他失败
 */
int HistRecord_Print(uint16_t index);

/**
 * 清空所有历史记录
 * @return 0 成功，其他失败
 */
int HistRecord_Clear(void);

/**
 * 按逻辑索引读取原始记录（Unix 时间戳 + 剂量 uSv）
 * @param index 逻辑索引（从 0 开始）
 * @param out_unix_ts 输出 Unix 时间戳
 * @param out_dose_uSv 输出剂量值（微希沃特）
 * @return 0 成功，其他失败
 */
int HistRecord_ReadRecordRaw(uint16_t index, uint32_t *out_unix_ts, float *out_dose_uSv);

/*==========================================================================
 * 时间调整功能（核心业务逻辑 - 严格按照 md 文档）
 *==========================================================================*/

/**
 * 时间回调处理（时间往回调整）
 * @param new_datetime 新的日期时间字符串
 * @return 0 成功，其他失败
 */
int HistRecord_AdjustTimeBackward(const char *new_datetime);

/**
 * 时间调快处理（时间往前调整）
 * @param new_datetime 新的日期时间字符串
 * @return 0 成功，其他失败
 */
int HistRecord_AdjustTimeForward(const char *new_datetime);

/*==========================================================================
 * 内部接口（供应用层使用，不对外暴露）
 *==========================================================================*/

/**
 * 扫描所有扇区，计算有效记录
 * @param reference_ts 参考时间戳
 * @return 有效记录数
 */
uint16_t HistRecord_ScanValidRecords(uint32_t reference_ts);

/**
 * 处理临时扇区（时间回调时使用）
 * @param source_sector_type 源扇区类型
 * @param source_sector_idx 源扇区索引
 * @return 0 成功，其他失败
 */
int HistRecord_ProcessTempSector(
    DM_SectorType_t source_sector_type,
    uint16_t source_sector_idx
);

/**
 * 扇区迁移（缓存扇区满后迁移到正式扇区）
 * @param cache_sector_idx 缓存扇区索引
 * @return 0 成功，其他失败
 */
int HistRecord_MigrateCacheSector(uint16_t cache_sector_idx);

/**
 * 获取全局管理器实例
 */
DM_Manager_t* HistRecord_GetManager(void);

#ifdef __cplusplus
}
#endif

#endif /* __HIST_RECORD_APP_H */

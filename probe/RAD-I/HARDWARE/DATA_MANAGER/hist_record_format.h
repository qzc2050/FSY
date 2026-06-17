#ifndef __HIST_RECORD_FORMAT_H
#define __HIST_RECORD_FORMAT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "hist_record_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/*==========================================================================
 * 历史记录数据格式定义
 * 定义存储到 Flash 中的数据结构
 *==========================================================================*/

/*==========================================================================
 * 数据结构定义
 *==========================================================================*/

/**
 * 历史记录数据结构（8 字节）
 * 存储格式：高 4 字节时间戳 + 低 4 字节剂量值（微希沃特）
 */
typedef struct {
    uint32_t unix_ts;      /* Unix 时间戳（秒） */
    uint32_t dose_uSv;     /* 剂量值（微希沃特 uSv） */
} __attribute__((packed)) HistRecord_t;

/* 确保结构体大小为 8 字节 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    _Static_assert(sizeof(HistRecord_t) == 8, "HistRecord_t size must be 8 bytes");
#else
    /* C89/C99 使用编译时检查技巧 */
    typedef char hist_record_size_check[(sizeof(HistRecord_t) == 8) ? 1 : -1];
#endif

/*==========================================================================
 * 时间戳转换函数
 *==========================================================================*/

/**
 * 日期时间字符串转换为 Unix 时间戳
 * @param datetime 日期时间字符串（格式：YYYYMMDD,HHMMSS）
 * @return Unix 时间戳，0 表示失败
 */
uint32_t HistFormat_DatetimeToTimestamp(const char *datetime);

/**
 * Unix 时间戳转换为日期时间字符串
 * @param timestamp Unix 时间戳
 * @param out_datetime 输出缓冲区（至少 HIST_DATE_TIME_LEN 字节）
 * @return 0 成功，其他失败
 */
int HistFormat_TimestampToDatetime(uint32_t timestamp, char *out_datetime);

/**
 * 剂量值（微希沃特）转换为字符串
 * @param dose_uSv 剂量值（微希沃特）
 * @param out_value 输出缓冲区（至少 HIST_DOSE_VALUE_LEN 字节）
 * @return 0 成功，其他失败
 */
int HistFormat_DoseToString(uint32_t dose_uSv, char *out_value);

/**
 * 剂量字符串转换为微希沃特
 * @param value 剂量字符串（格式：XX.XXuSv 或 XX.XXmSv）
 * @return 剂量值（微希沃特），0 表示失败
 */
uint32_t HistFormat_StringToDose(const char *value);

/*==========================================================================
 * 记录验证和初始化
 *==========================================================================*/

/**
 * 验证记录是否有效
 * @param data 记录数据指针
 * @return true 有效，false 无效
 */
bool HistFormat_ValidateRecord(const uint8_t *data);

/**
 * 初始化记录（设置为空白）
 * @param data 记录数据指针
 */
void HistFormat_InitRecord(uint8_t *data);

/*==========================================================================
 * 工具函数
 *==========================================================================*/

/**
 * 检查记录是否为空白（0xFF）
 * @param record 记录指针
 * @return true 空白，false 非空白
 */
bool HistFormat_IsBlank(const HistRecord_t *record);

/**
 * 检查记录是否有效（非空白且时间戳>0）
 * @param record 记录指针
 * @return true 有效，false 无效
 */
bool HistFormat_IsValid(const HistRecord_t *record);

/**
 * 复制记录
 * @param dest 目标记录
 * @param src 源记录
 */
void HistFormat_CopyRecord(HistRecord_t *dest, const HistRecord_t *src);

/*==========================================================================
 * 内联函数实现
 *==========================================================================*/

static inline bool HistFormat_IsBlank(const HistRecord_t *record)
{
    if (record == NULL) {
        return false;
    }
    return (record->unix_ts == 0xFFFFFFFF && record->dose_uSv == 0xFFFFFFFF);
}

static inline bool HistFormat_IsValid(const HistRecord_t *record)
{
    if (record == NULL) {
        return false;
    }
    return (record->unix_ts > 0 && record->unix_ts != 0xFFFFFFFF);
}

static inline void HistFormat_CopyRecord(HistRecord_t *dest, const HistRecord_t *src)
{
    if (dest != NULL && src != NULL) {
        *dest = *src;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* __HIST_RECORD_FORMAT_H */

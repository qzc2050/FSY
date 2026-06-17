#include "hist_record_format.h"
#include <string.h>
#include <stdio.h>

/*==========================================================================
 * 内部辅助函数声明
 *==========================================================================*/

static int is_leap_year(int year);
static uint32_t mktime_manual(int year, int month, int day, int hour, int min, int sec);

/*==========================================================================
 * 时间戳转换函数实现
 *==========================================================================*/

/**
 * 日期时间字符串转换为 Unix 时间戳
 * 格式：YYYYMMDD,HHMMSS
 */
uint32_t HistFormat_DatetimeToTimestamp(const char *datetime)
{
    int year, month, day, hour, min, sec;
    
    if (datetime == NULL || strlen(datetime) < 15) {
        return 0;
    }
    
    /* 解析字符串：YYYYMMDD,HHMMSS */
    year = (datetime[0] - '0') * 1000 + (datetime[1] - '0') * 100 + 
           (datetime[2] - '0') * 10 + (datetime[3] - '0');
    month = (datetime[4] - '0') * 10 + (datetime[5] - '0');
    day = (datetime[6] - '0') * 10 + (datetime[7] - '0');
    hour = (datetime[9] - '0') * 10 + (datetime[10] - '0');
    min = (datetime[11] - '0') * 10 + (datetime[12] - '0');
    sec = (datetime[13] - '0') * 10 + (datetime[14] - '0');
    
    /* 验证范围 */
    if (month < 1 || month > 12 || day < 1 || day > 31 ||
        hour > 23 || min > 59 || sec > 59) {
        return 0;
    }
    
    return mktime_manual(year, month, day, hour, min, sec);
}

/**
 * Unix 时间戳转换为日期时间字符串
 */
int HistFormat_TimestampToDatetime(uint32_t timestamp, char *out_datetime)
{
    uint32_t days, seconds;
    uint32_t year, month, day, hour, min, sec;
    int i;
    const uint8_t days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    if (out_datetime == NULL || timestamp == 0) {
        return -1;
    }
    
    /* 计算天数和秒数（从 1970-01-01 00:00:00 开始） */
    days = timestamp / 86400;
    seconds = timestamp % 86400;
    
    /* 计算时分秒 */
    hour = seconds / 3600;
    min = (seconds % 3600) / 60;
    sec = seconds % 60;
    
    /* 计算年份 */
    year = 1970;
    while (1) {
        uint32_t days_in_year = is_leap_year(year) ? 366 : 365;
        if (days < days_in_year) {
            break;
        }
        days -= days_in_year;
        year++;
    }
    
    /* 计算月份 */
    month = 1;
    while (month <= 12) {
        uint8_t dim = days_in_month[month];
        if (month == 2 && is_leap_year(year)) {
            dim = 29;
        }
        if (days < dim) {
            break;
        }
        days -= dim;
        month++;
    }
    
    /* 计算日期 */
    day = days + 1;
    
    /* 格式化输出：YYYYMMDD,HHMMSS */
    sprintf(out_datetime, "%04u%02u%02u,%02u%02u%02u",
            (unsigned)year, (unsigned)month, (unsigned)day,
            (unsigned)hour, (unsigned)min, (unsigned)sec);
    
    return 0;
}

/**
 * 剂量值（微希沃特）转换为字符串
 */
int HistFormat_DoseToString(uint32_t dose_uSv, char *out_value)
{
    if (out_value == NULL) {
        return -1;
    }
    
    /* 如果剂量值 >= 1000 uSv，显示为 mSv */
    if (dose_uSv >= 1000) {
        uint32_t mSv = dose_uSv / 1000;
        uint32_t frac = (dose_uSv % 1000) / 100;  /* 保留 1 位小数 */
        sprintf(out_value, "%u.%umSv", (unsigned)mSv, (unsigned)frac);
    } else {
        /* 显示为 uSv，保留 1 位小数 */
        uint32_t whole = dose_uSv / 10;
        uint32_t frac = dose_uSv % 10;
        sprintf(out_value, "%u.%uuSv", (unsigned)whole, (unsigned)frac);
    }
    
    return 0;
}

/**
 * 剂量字符串转换为微希沃特
 */
uint32_t HistFormat_StringToDose(const char *value)
{
    float dose_float;
    
    if (value == NULL) {
        return 0;
    }
    
    /* 解析剂量值 */
    if (strstr(value, "mSv") != NULL) {
        /* mSv 单位 */
        sscanf(value, "%f", &dose_float);
        return (uint32_t)(dose_float * 1000.0f);
    } else if (strstr(value, "uSv") != NULL) {
        /* uSv 单位 */
        sscanf(value, "%f", &dose_float);
        return (uint32_t)dose_float;
    } else {
        /* 无单位，假设为 uSv */
        sscanf(value, "%f", &dose_float);
        return (uint32_t)dose_float;
    }
}

/*==========================================================================
 * 记录验证和初始化
 *==========================================================================*/

/**
 * 验证记录是否有效
 */
bool HistFormat_ValidateRecord(const uint8_t *data)
{
    const HistRecord_t *record;
    
    if (data == NULL) {
        return false;
    }
    
    record = (const HistRecord_t *)data;
    
    /* 检查时间戳是否有效 */
    if (record->unix_ts == 0 || record->unix_ts == 0xFFFFFFFF) {
        return false;
    }
    
    /* 检查时间戳是否合理（2000-2100 年） */
    if (record->unix_ts < 946684800U || record->unix_ts > 4102444800U) {
        return false;
    }
    
    return true;
}

/**
 * 初始化记录（设置为空白）
 */
void HistFormat_InitRecord(uint8_t *data)
{
    if (data != NULL) {
        memset(data, 0xFF, sizeof(HistRecord_t));
    }
}

/*==========================================================================
 * 内部辅助函数实现
 *==========================================================================*/

/**
 * 判断是否为闰年
 */
static int is_leap_year(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

/**
 * 手动实现 mktime 函数（将日期时间转换为 Unix 时间戳）
 */
static uint32_t mktime_manual(int year, int month, int day, int hour, int min, int sec)
{
    static const uint16_t month_days[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    uint32_t timestamp;
    int y, leaps, days;
    
    /* 计算从 1970 年到指定年份的总天数 */
    y = year - 1970;
    leaps = (y > 0) ? ((y + 1) / 4) - ((y + 1) / 100) + ((y + 1) / 400) : 0;
    days = y * 365 + leaps;
    
    /* 加上月份天数 */
    days += month_days[month - 1];
    
    /* 如果是闰年且月份>2，加 1 天 */
    if (month > 2 && is_leap_year(year)) {
        days++;
    }
    
    /* 加上日期 */
    days += day - 1;
    
    /* 计算时间戳 */
    timestamp = ((uint32_t)days * 86400U) + ((uint32_t)hour * 3600U) + 
                ((uint32_t)min * 60U) + (uint32_t)sec;
    
    return timestamp;
}

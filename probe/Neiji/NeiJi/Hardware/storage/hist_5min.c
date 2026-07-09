#include "hist_5min.h"
#include "flash_fs_mutex.h"
#include "w25qxx.h"
#include <string.h>
#include <stdio.h>

typedef struct {
    uint32_t magic;
    uint32_t write_idx; /* 下一条写入槽位 0..MAX-1 */
    uint32_t count;     /* 有效条数 0..MAX */
    uint32_t reserved;
} Hist5MinMeta_t;

static Hist5MinMeta_t s_meta;
static uint8_t s_ready;
/* 满环改写时用；勿放任务栈（defaultTask 仅 4KB） */
static uint8_t s_sector_buf[EXT_FLASH_META_SECTOR_SIZE] __attribute__((aligned(4)));

static int is_leap_year(int year)
{
    return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

static uint32_t mktime_manual(int year, int month, int day, int hour, int min, int sec)
{
    static const uint16_t month_days[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int y;
    int leaps;
    int days;

    if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31 ||
        hour > 23 || min > 59 || sec > 59) {
        return 0U;
    }

    y = year - 1970;
    leaps = (y > 0) ? ((y + 1) / 4) - ((y + 1) / 100) + ((y + 1) / 400) : 0;
    days = y * 365 + leaps;
    days += month_days[month - 1];
    if (month > 2 && is_leap_year(year)) {
        days++;
    }
    days += day - 1;

    return ((uint32_t)days * 86400U) + ((uint32_t)hour * 3600U) +
           ((uint32_t)min * 60U) + (uint32_t)sec;
}

static uint32_t record_addr(uint32_t slot)
{
    return EXT_FLASH_DATA_5MIN_RECORDS_BASE + slot * EXT_FLASH_DATA_RECORD_BYTES;
}

static int meta_load(Hist5MinMeta_t *out)
{
    if (W25Qx_QSPI_FastRead((uint8_t *)out, EXT_FLASH_DATA_5MIN_META_BASE,
                            (uint32_t)sizeof(*out)) != QSPI_OK) {
        return -1;
    }
    return 0;
}

static int meta_save(const Hist5MinMeta_t *meta)
{
    /* 扇区擦除后只 program meta；勿在栈上开 4KB */
    if (W25Qx_QSPI_Erase_Block(EXT_FLASH_DATA_5MIN_META_BASE) != QSPI_OK) {
        return -1;
    }
    if (W25Qx_QSPI_Write((uint8_t *)meta, EXT_FLASH_DATA_5MIN_META_BASE,
                         (uint32_t)sizeof(*meta)) != QSPI_OK) {
        return -1;
    }
    return 0;
}

static int meta_reset_empty(void)
{
    memset(&s_meta, 0, sizeof(s_meta));
    s_meta.magic = HIST_5MIN_META_MAGIC;
    s_meta.write_idx = 0U;
    s_meta.count = 0U;

    if (W25Qx_QSPI_Erase_Block(EXT_FLASH_DATA_5MIN_RECORDS_BASE) != QSPI_OK) {
        return -1;
    }
    return meta_save(&s_meta);
}

static int meta_valid(const Hist5MinMeta_t *m)
{
    if (m->magic != HIST_5MIN_META_MAGIC) {
        return 0;
    }
    if (m->write_idx >= HIST_5MIN_MAX_RECORDS) {
        return 0;
    }
    if (m->count > HIST_5MIN_MAX_RECORDS) {
        return 0;
    }
    return 1;
}

uint32_t Hist5Min_DateTimeToUnix(const Pcf85063_DateTime_t *dt)
{
    int year;

    if (dt == NULL || dt->online == 0U) {
        return 0U;
    }
    year = (int)dt->year;
    if (year < 100) {
        year += 2000;
    }
    return mktime_manual(year, (int)dt->month, (int)dt->day,
                         (int)dt->hour, (int)dt->minute, (int)dt->second);
}

uint32_t Hist5Min_Dt8ToUnix(const uint8_t dt8[8])
{
    Pcf85063_DateTime_t dt;

    if (dt8 == NULL) {
        return 0U;
    }
    if (dt8[1] < 1U || dt8[1] > 12U || dt8[2] < 1U || dt8[2] > 31U ||
        dt8[3] > 23U || dt8[4] > 59U || dt8[5] > 59U) {
        return 0U;
    }
    memset(&dt, 0, sizeof(dt));
    dt.year = (uint16_t)(2000U + (uint16_t)(dt8[0] % 100U));
    dt.month = dt8[1];
    dt.day = dt8[2];
    dt.hour = dt8[3];
    dt.minute = dt8[4];
    dt.second = dt8[5];
    dt.online = 1U;
    return Hist5Min_DateTimeToUnix(&dt);
}

int Hist5Min_UnixToDt8(uint32_t unix_ts, uint8_t out_dt8[8])
{
    uint32_t days;
    uint32_t seconds;
    uint32_t year;
    uint32_t month;
    uint32_t day;
    uint32_t hour;
    uint32_t min;
    uint32_t sec;
    const uint8_t days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (out_dt8 == NULL || unix_ts == 0U) {
        return -1;
    }

    days = unix_ts / 86400U;
    seconds = unix_ts % 86400U;
    hour = seconds / 3600U;
    min = (seconds % 3600U) / 60U;
    sec = seconds % 60U;

    year = 1970U;
    while (1) {
        uint32_t days_in_year = is_leap_year((int)year) ? 366U : 365U;
        if (days < days_in_year) {
            break;
        }
        days -= days_in_year;
        year++;
    }

    month = 1U;
    while (month <= 12U) {
        uint8_t dim = days_in_month[month];
        if (month == 2U && is_leap_year((int)year)) {
            dim = 29U;
        }
        if (days < dim) {
            break;
        }
        days -= dim;
        month++;
    }
    day = days + 1U;

    memset(out_dt8, 0, 8U);
    out_dt8[0] = (uint8_t)(year % 100U);
    out_dt8[1] = (uint8_t)month;
    out_dt8[2] = (uint8_t)day;
    out_dt8[3] = (uint8_t)hour;
    out_dt8[4] = (uint8_t)min;
    out_dt8[5] = (uint8_t)sec;
    return 0;
}

int Hist5Min_Init(void)
{
    Hist5MinMeta_t meta;

    flash_fs_lock();
    s_ready = 0U;

    if (meta_load(&meta) != 0 || !meta_valid(&meta)) {
        printf("[HIST5] meta invalid, reset empty ring\r\n");
        if (meta_reset_empty() != 0) {
            flash_fs_unlock();
            return -1;
        }
    } else {
        s_meta = meta;
    }

    s_ready = 1U;
    flash_fs_unlock();
    printf("[HIST5] init ok count=%lu write_idx=%lu max=%u\r\n",
           (unsigned long)s_meta.count,
           (unsigned long)s_meta.write_idx,
           (unsigned)HIST_5MIN_MAX_RECORDS);
    return 0;
}

uint16_t Hist5Min_GetCount(void)
{
    return s_ready ? (uint16_t)s_meta.count : 0U;
}

int Hist5Min_Read(uint16_t logical_index, Hist5MinRecord_t *out)
{
    uint32_t slot;
    uint32_t oldest;

    if (!s_ready || out == NULL || logical_index >= s_meta.count) {
        return -1;
    }

    /* 环：最旧 = write_idx（满环时）或 0（未满） */
    if (s_meta.count < HIST_5MIN_MAX_RECORDS) {
        oldest = 0U;
    } else {
        oldest = s_meta.write_idx;
    }
    slot = (oldest + (uint32_t)logical_index) % HIST_5MIN_MAX_RECORDS;

    flash_fs_lock();
    if (W25Qx_QSPI_FastRead((uint8_t *)out, record_addr(slot),
                            (uint32_t)sizeof(*out)) != QSPI_OK) {
        flash_fs_unlock();
        return -1;
    }
    flash_fs_unlock();
    return 0;
}

/**
 * 环已满时：读出整扇区记录区、替换 write_idx 槽、擦除后写回。
 * 未满时：直接 program 空白槽（NOR 1→0）。
 */
int Hist5Min_Write(uint32_t unix_ts, uint32_t dose_x100)
{
    Hist5MinRecord_t rec;
    uint32_t slot;

    if (!s_ready || unix_ts == 0U) {
        return -1;
    }

    rec.unix_ts = unix_ts;
    rec.dose_x100 = dose_x100;
    slot = s_meta.write_idx;

    flash_fs_lock();

    if (s_meta.count < HIST_5MIN_MAX_RECORDS) {
        if (W25Qx_QSPI_Write((uint8_t *)&rec, record_addr(slot),
                             (uint32_t)sizeof(rec)) != QSPI_OK) {
            flash_fs_unlock();
            return -1;
        }
        s_meta.write_idx = (slot + 1U) % HIST_5MIN_MAX_RECORDS;
        s_meta.count++;
    } else {
        uint32_t data_bytes = HIST_5MIN_MAX_RECORDS * EXT_FLASH_DATA_RECORD_BYTES;

        if (W25Qx_QSPI_FastRead(s_sector_buf, EXT_FLASH_DATA_5MIN_RECORDS_BASE,
                                data_bytes) != QSPI_OK) {
            flash_fs_unlock();
            return -1;
        }
        memcpy(s_sector_buf + slot * EXT_FLASH_DATA_RECORD_BYTES, &rec, sizeof(rec));

        if (W25Qx_QSPI_Erase_Block(EXT_FLASH_DATA_5MIN_RECORDS_BASE) != QSPI_OK) {
            flash_fs_unlock();
            return -1;
        }
        if (W25Qx_QSPI_Write(s_sector_buf, EXT_FLASH_DATA_5MIN_RECORDS_BASE,
                             data_bytes) != QSPI_OK) {
            flash_fs_unlock();
            return -1;
        }
        s_meta.write_idx = (slot + 1U) % HIST_5MIN_MAX_RECORDS;
        /* count 保持 MAX */
    }

    if (meta_save(&s_meta) != 0) {
        flash_fs_unlock();
        return -1;
    }

    flash_fs_unlock();
    return 0;
}

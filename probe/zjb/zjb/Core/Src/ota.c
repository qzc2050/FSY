#include "ota.h"
#include "stm32f1xx_hal.h"
#include <string.h>

/* ---------------------------------------------------------------------------
 * 模块内部状态
 * ---------------------------------------------------------------------------
 */
static OtaState_e  s_state        = OTA_STATE_IDLE;
static uint32_t    s_total_size   = 0U;   /* 期望接收的固件总字节数 */
static uint32_t    s_written      = 0U;   /* 已写入 Download 区的字节数 */
static uint8_t     s_realtime_muted = 0U; /* OTA 期间静默主动上报，避免与应答抢 USART1 */
static uint32_t    s_last_activity_tick = 0U;

#define OTA_REALTIME_MUTE_TIMEOUT_MS   5000U

static void OTA_TouchActivity(void)
{
    s_realtime_muted = 1U;
    s_last_activity_tick = HAL_GetTick();
}

/* ---------------------------------------------------------------------------
 * 软件 CRC32（标准多项式 0x04C11DB7，与 config_flash.c 同款）
 * ---------------------------------------------------------------------------
 */
static uint32_t OTA_Crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFU;
    uint32_t i;
    uint8_t  j;

    for (i = 0U; i < len; i++)
    {
        crc ^= ((uint32_t)data[i] << 24);
        for (j = 0U; j < 8U; j++)
        {
            if ((crc & 0x80000000U) != 0U)
            {
                crc = (crc << 1) ^ 0x04C11DB7U;
            }
            else
            {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/* ---------------------------------------------------------------------------
 * 内部 Flash 操作
 * ---------------------------------------------------------------------------
 */

/* 按页擦除一段连续 Flash */
static int OTA_FlashErase(uint32_t addr, uint32_t pages)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t page_error = 0U;

    erase.TypeErase   = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = addr;
    erase.NbPages     = pages;

    if (HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK)
    {
        return -1;
    }
    return 0;
}

/* 按 32-bit 字写入 Flash（addr 和 len 必须 4 字节对齐） */
static int OTA_FlashWrite(uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint32_t i;
    uint32_t words = (len + 3U) / 4U;
    uint8_t  tmp[4];
    uint32_t word_val;

    for (i = 0U; i < words; i++)
    {
        /* 处理末尾不足 4 字节的情况 */
        uint32_t remain = len - i * 4U;
        if (remain >= 4U)
        {
            memcpy(&word_val, &data[i * 4U], 4U);
        }
        else
        {
            memset(tmp, 0xFFU, 4U);
            memcpy(tmp, &data[i * 4U], remain);
            memcpy(&word_val, tmp, 4U);
        }

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                              addr + i * 4U,
                              word_val) != HAL_OK)
        {
            return -1;
        }
    }
    return 0;
}

/* 写 OTA Flag 页 */
static int OTA_WriteFlag(const OtaFlag_t *flag)
{
    int ret;

    HAL_FLASH_Unlock();

    /* 擦除 OTA Flag 页 */
    FLASH_EraseInitTypeDef erase;
    uint32_t page_error = 0U;
    erase.TypeErase   = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = OTA_FLAG_FLASH_ADDR;
    erase.NbPages     = 1U;
    if (HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return -1;
    }

    ret = OTA_FlashWrite(OTA_FLAG_FLASH_ADDR, (const uint8_t *)flag, sizeof(OtaFlag_t));

    HAL_FLASH_Lock();
    return ret;
}

/* ---------------------------------------------------------------------------
 * 公共 API
 * ---------------------------------------------------------------------------
 */

void OTA_Init(void)
{
    s_state      = OTA_STATE_IDLE;
    s_total_size = 0U;
    s_written    = 0U;
    s_realtime_muted = 0U;
    s_last_activity_tick = 0U;
}

OtaState_e OTA_GetState(void)
{
    return s_state;
}

uint32_t OTA_GetWrittenBytes(void)
{
    return s_written;
}

uint32_t OTA_GetTotalSize(void)
{
    return s_total_size;
}

uint8_t OTA_IsRealtimeMuted(void)
{
    return s_realtime_muted;
}

void OTA_Service(void)
{
    if (s_realtime_muted == 0U)
    {
        return;
    }

    /* OTA 正在校验/等待重启期间保持静默，避免在最后阶段打断流程 */
    if ((s_state == OTA_STATE_VERIFY) || (s_state == OTA_STATE_DONE))
    {
        return;
    }

    if ((HAL_GetTick() - s_last_activity_tick) >= OTA_REALTIME_MUTE_TIMEOUT_MS)
    {
        OTA_Abort();
    }
}

int OTA_StartSession(uint32_t total_size)
{
    /* 参数检查：固件不能超过 Download 区也不能超过 App 区 */
    if ((total_size == 0U) ||
        (total_size > DOWNLOAD_FLASH_SIZE) ||
        (total_size > APP_FLASH_SIZE))
    {
        s_state = OTA_STATE_ERROR;
        return -1;
    }

    s_total_size = total_size;
    s_written    = 0U;

    /* 计算需要擦除的页数（向上取整） */
    uint32_t pages_needed = (total_size + FLASH_PAGE_SIZE - 1U) / FLASH_PAGE_SIZE;
    if (pages_needed > DOWNLOAD_FLASH_PAGES)
    {
        pages_needed = DOWNLOAD_FLASH_PAGES;
    }

    HAL_FLASH_Unlock();
    int ret = OTA_FlashErase(DOWNLOAD_FLASH_ADDR, pages_needed);
    HAL_FLASH_Lock();

    if (ret != 0)
    {
        s_state = OTA_STATE_ERROR;
        return -1;
    }

    OTA_TouchActivity();
    s_state = OTA_STATE_STARTED;
    return 0;
}

int OTA_WriteChunk(const uint8_t *data, uint16_t len)
{
    if (s_state != OTA_STATE_STARTED)
    {
        return -1;
    }
    if (data == NULL || len == 0U)
    {
        return -1;
    }
    /* 防越界 */
    if ((s_written + (uint32_t)len) > s_total_size)
    {
        s_state = OTA_STATE_ERROR;
        return -1;
    }

    uint32_t dest = DOWNLOAD_FLASH_ADDR + s_written;

    HAL_FLASH_Unlock();
    int ret = OTA_FlashWrite(dest, data, (uint32_t)len);
    HAL_FLASH_Lock();

    if (ret != 0)
    {
        s_state = OTA_STATE_ERROR;
        return -1;
    }

    s_written += (uint32_t)len;
    OTA_TouchActivity();
    return 0;
}

int OTA_Finish(uint32_t expected_crc32)
{
    if (s_state != OTA_STATE_STARTED)
    {
        return -1;
    }
    if (s_written != s_total_size)
    {
        /* 未接收完整 */
        s_state = OTA_STATE_ERROR;
        return -1;
    }

    s_state = OTA_STATE_VERIFY;
    OTA_TouchActivity();

    /* 校验 Download 区 CRC32 */
    uint32_t actual_crc = OTA_Crc32((const uint8_t *)DOWNLOAD_FLASH_ADDR, s_total_size);
    if (actual_crc != expected_crc32)
    {
        s_state = OTA_STATE_ERROR;
        return -1;
    }

    /* 构造 OTA Flag 并写入 Flash */
    OtaFlag_t flag;
    memset(&flag, 0, sizeof(flag));
    flag.magic      = OTA_FLAG_MAGIC;
    flag.app_size   = s_total_size;
    flag.app_crc32  = expected_crc32;
    flag.status     = OTA_STATUS_PENDING;
    /* 计算 flag 本身的校验（不含最后 flag_crc 字段） */
    flag.flag_crc   = OTA_Crc32((const uint8_t *)&flag,
                                 sizeof(OtaFlag_t) - sizeof(uint32_t));

    if (OTA_WriteFlag(&flag) != 0)
    {
        s_state = OTA_STATE_ERROR;
        return -1;
    }

    s_state = OTA_STATE_DONE;
    return 0;
}

void OTA_Abort(void)
{
    s_state      = OTA_STATE_IDLE;
    s_total_size = 0U;
    s_written    = 0U;
    s_realtime_muted = 0U;
    s_last_activity_tick = 0U;
}

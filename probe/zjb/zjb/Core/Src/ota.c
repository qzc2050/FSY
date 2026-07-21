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
static uint8_t     s_reset_pending = 0U;
static uint32_t    s_reset_at_tick = 0U;

#define OTA_REALTIME_MUTE_TIMEOUT_MS   120000U
/* 留足 App 在 DONE ACK 丢失后读取 204 状态或重发 DONE 的时间 */
#define OTA_RESET_DELAY_MS             15000U
#ifndef FLASH_PAGE_SIZE
#define FLASH_PAGE_SIZE                1024U
#endif

static void OTA_TouchActivity(void)
{
    s_realtime_muted = 1U;
    s_last_activity_tick = HAL_GetTick();
}

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

static int OTA_FlashErasePages(uint32_t addr, uint32_t pages)
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

static int OTA_FlashWrite(uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint32_t i;
    uint32_t words = (len + 3U) / 4U;
    uint8_t  tmp[4];
    uint32_t word_val;

    for (i = 0U; i < words; i++)
    {
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

static int OTA_WriteFlag(const OtaFlag_t *flag)
{
    int ret;

    HAL_FLASH_Unlock();

    {
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
    }

    ret = OTA_FlashWrite(OTA_FLAG_FLASH_ADDR, (const uint8_t *)flag, sizeof(OtaFlag_t));

    HAL_FLASH_Lock();
    return ret;
}

void OTA_Init(void)
{
    s_state      = OTA_STATE_IDLE;
    s_total_size = 0U;
    s_written    = 0U;
    s_realtime_muted = 0U;
    s_last_activity_tick = 0U;
    s_reset_pending = 0U;
    s_reset_at_tick = 0U;
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
    if (s_reset_pending != 0U)
    {
        if ((int32_t)(HAL_GetTick() - s_reset_at_tick) >= 0)
        {
            NVIC_SystemReset();
        }
        return;
    }

    if (s_realtime_muted == 0U)
    {
        return;
    }

    if ((s_state == OTA_STATE_VERIFY) || (s_state == OTA_STATE_DONE))
    {
        return;
    }

    if ((HAL_GetTick() - s_last_activity_tick) >= OTA_REALTIME_MUTE_TIMEOUT_MS)
    {
        OTA_Abort();
    }
}

void OTA_RequestReset(void)
{
    if (s_state == OTA_STATE_DONE)
    {
        s_reset_pending = 1U;
        s_reset_at_tick = HAL_GetTick() + OTA_RESET_DELAY_MS;
    }
}

int OTA_StartSession(uint32_t total_size)
{
    uint32_t pages_needed;
    int ret;

    if ((total_size == 0U) ||
        (total_size > DOWNLOAD_FLASH_SIZE) ||
        (total_size > APP_FLASH_SIZE))
    {
        s_state = OTA_STATE_ERROR;
        return -1;
    }

    /*
     * START 一次性预擦除，完成后协议层才回 ACK。
     * 避免 DATA ACK 后擦页阻塞 Flash 取指/USART 中断，导致下一包丢字节。
     */
    s_total_size = total_size;
    s_written    = 0U;
    s_reset_pending = 0U;

    OTA_TouchActivity();
    pages_needed = (total_size + FLASH_PAGE_SIZE - 1U) / FLASH_PAGE_SIZE;
    if (pages_needed > DOWNLOAD_FLASH_PAGES)
    {
        pages_needed = DOWNLOAD_FLASH_PAGES;
    }

    HAL_FLASH_Unlock();
    ret = OTA_FlashErasePages(DOWNLOAD_FLASH_ADDR, pages_needed);
    HAL_FLASH_Lock();
    if (ret != 0)
    {
        s_state = OTA_STATE_ERROR;
        return -1;
    }

    s_state = OTA_STATE_STARTED;
    OTA_TouchActivity();
    return 0;
}

int OTA_WriteChunk(const uint8_t *data, uint16_t len)
{
    uint32_t dest;
    int ret;

    if (s_state != OTA_STATE_STARTED)
    {
        return -1;
    }
    if ((data == NULL) || (len == 0U) || (len > 128U))
    {
        return -1;
    }
    if ((s_written + (uint32_t)len) > s_total_size)
    {
        s_state = OTA_STATE_ERROR;
        return -1;
    }

    dest = DOWNLOAD_FLASH_ADDR + s_written;
    HAL_FLASH_Unlock();
    ret = OTA_FlashWrite(dest, data, (uint32_t)len);
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

int OTA_CommitPending(void)
{
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
        s_state = OTA_STATE_ERROR;
        return -1;
    }

    s_state = OTA_STATE_VERIFY;
    OTA_TouchActivity();

    {
        uint32_t actual_crc = OTA_Crc32((const uint8_t *)DOWNLOAD_FLASH_ADDR, s_total_size);
        if (actual_crc != expected_crc32)
        {
            s_state = OTA_STATE_ERROR;
            return -1;
        }
    }

    {
        OtaFlag_t flag;
        memset(&flag, 0, sizeof(flag));
        flag.magic      = OTA_FLAG_MAGIC;
        flag.app_size   = s_total_size;
        flag.app_crc32  = expected_crc32;
        flag.status     = OTA_STATUS_PENDING;
        flag.flag_crc   = OTA_Crc32((const uint8_t *)&flag,
                                     sizeof(OtaFlag_t) - sizeof(uint32_t));

        if (OTA_WriteFlag(&flag) != 0)
        {
            s_state = OTA_STATE_ERROR;
            return -1;
        }
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
    s_reset_pending = 0U;
    s_reset_at_tick = 0U;
}

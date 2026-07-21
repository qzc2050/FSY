#include "ota.h"
#include "flash_layout.h"
#include "set_flash.h"

#include "main.h"
#include "stm32h7xx_hal_flash.h"
#include "stm32h7xx_hal_flash_ex.h"

#include <string.h>

#define OTA_FLASH_WORD_SIZE           32U
#define OTA_FLASH_SECTOR_SIZE         0x00020000U
#define OTA_FLASH_SECTOR_COUNT        7U
/* LoRa OTA 超时恢复会查询状态并重试，给链路抖动保留 5 分钟恢复窗口 */
#define OTA_REALTIME_MUTE_TIMEOUT_MS  300000U

static OtaState_e s_state = OTA_STATE_IDLE;
static uint32_t   s_total_size;
static uint32_t   s_written;
static uint8_t    s_realtime_muted;
static uint32_t   s_last_activity_tick;
static uint8_t    s_reset_pending;
static uint32_t   s_done_tick;

static uint8_t  s_prog_buf[OTA_FLASH_WORD_SIZE] __attribute__((aligned(32)));
static uint32_t s_prog_fill;
static uint32_t s_flash_cursor;
static uint8_t  s_erased_mask;
static uint8_t  s_flash_open;

static uint8_t s_word_tmp[OTA_FLASH_WORD_SIZE] __attribute__((aligned(32)));

static uint8_t  s_pending_chunk[OTA_CHUNK_MAX_BYTES];
static uint16_t s_pending_len;
static uint8_t  s_has_pending;

/* DONE：校验通过后先回 0x20，再在 CommitPending 写 Flag（擦 Set 扇区较慢） */
static OtaFlag_t s_pending_flag;
static uint8_t   s_flag_pending;

static uint32_t OTA_Crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFU;
    uint32_t i;
    uint8_t j;

    for (i = 0U; i < len; i++) {
        crc ^= ((uint32_t)data[i] << 24);
        for (j = 0U; j < 8U; j++) {
            if ((crc & 0x80000000U) != 0U) {
                crc = (crc << 1) ^ 0x04C11DB7U;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

static void OTA_TouchActivity(void)
{
    s_realtime_muted = 1U;
    s_last_activity_tick = HAL_GetTick();
}

static void OTA_InvalidateRange(uint32_t addr, uint32_t len)
{
    uint32_t start = addr & ~31U;
    int32_t size = (int32_t)((len + (addr - start) + 31U) & ~31U);

    SCB_InvalidateDCache_by_Addr((uint32_t *)start, size);
}

static void OTA_CleanRange(void *addr, uint32_t len)
{
    uint32_t start = ((uint32_t)(uintptr_t)addr) & ~31U;
    int32_t size = (int32_t)((len + (((uint32_t)(uintptr_t)addr) - start) + 31U) & ~31U);

    SCB_CleanDCache_by_Addr((uint32_t *)start, size);
}

static int OTA_FlashOpen(void)
{
    if (s_flash_open != 0U) {
        return 0;
    }
    if (HAL_FLASH_Unlock() != HAL_OK) {
        return -1;
    }
    s_flash_open = 1U;
    return 0;
}

static void OTA_FlashClose(void)
{
    if (s_flash_open == 0U) {
        return;
    }
    (void)HAL_FLASH_Lock();
    s_flash_open = 0U;
}

static int OTA_ProgramWord(uint32_t abs_addr, const uint8_t *word32)
{
    memcpy(s_word_tmp, word32, OTA_FLASH_WORD_SIZE);
    OTA_CleanRange(s_word_tmp, OTA_FLASH_WORD_SIZE);

    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS_BANK2);

    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, abs_addr,
                          (uint32_t)(uintptr_t)s_word_tmp) != HAL_OK) {
        return -1;
    }

    OTA_InvalidateRange(abs_addr, OTA_FLASH_WORD_SIZE);
    return 0;
}

static int OTA_EnsureSectorErased(uint32_t rel_offset)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t sector_error = 0U;
    uint32_t sector;
    uint32_t abs_addr;

    if (rel_offset >= APP_DOWNLOAD_FLASH_SIZE) {
        return -1;
    }

    sector = rel_offset / OTA_FLASH_SECTOR_SIZE;
    if (sector >= OTA_FLASH_SECTOR_COUNT) {
        return -1;
    }
    if ((s_erased_mask & (uint8_t)(1U << sector)) != 0U) {
        return 0;
    }

    abs_addr = APP_DOWNLOAD_FLASH_ADDR + (sector * OTA_FLASH_SECTOR_SIZE);

    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Banks = FLASH_BANK_2;
    erase.Sector = sector;
    erase.NbSectors = 1U;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS_BANK2);

    if (HAL_FLASHEx_Erase(&erase, &sector_error) != HAL_OK) {
        return -1;
    }

    OTA_InvalidateRange(abs_addr, OTA_FLASH_SECTOR_SIZE);
    s_erased_mask |= (uint8_t)(1U << sector);
    return 0;
}

/* START 时按固件长度预擦全部扇区，DATA 阶段不再擦除 */
static int OTA_PreEraseForSize(uint32_t total_size)
{
    uint32_t sectors;
    uint32_t s;

    sectors = (total_size + OTA_FLASH_SECTOR_SIZE - 1U) / OTA_FLASH_SECTOR_SIZE;
    if (sectors == 0U) {
        return -1;
    }
    if (sectors > OTA_FLASH_SECTOR_COUNT) {
        sectors = OTA_FLASH_SECTOR_COUNT;
    }

    for (s = 0U; s < sectors; s++) {
        if (OTA_EnsureSectorErased(s * OTA_FLASH_SECTOR_SIZE) != 0) {
            return -1;
        }
    }
    return 0;
}

static int OTA_FlushProgBuf(void)
{
    uint32_t abs_addr;

    if (s_prog_fill == 0U) {
        return 0;
    }

    if (s_prog_fill < OTA_FLASH_WORD_SIZE) {
        memset(&s_prog_buf[s_prog_fill], 0xFF, OTA_FLASH_WORD_SIZE - s_prog_fill);
    }

    if (OTA_EnsureSectorErased(s_flash_cursor) != 0) {
        return -1;
    }

    abs_addr = APP_DOWNLOAD_FLASH_ADDR + s_flash_cursor;
    if (OTA_ProgramWord(abs_addr, s_prog_buf) != 0) {
        return -1;
    }

    s_flash_cursor += OTA_FLASH_WORD_SIZE;
    s_prog_fill = 0U;
    return 0;
}

static int OTA_WriteBytes(const uint8_t *data, uint16_t len)
{
    uint16_t i;

    if (OTA_FlashOpen() != 0) {
        return -1;
    }

    for (i = 0U; i < len; i++) {
        s_prog_buf[s_prog_fill++] = data[i];
        s_written++;
        if (s_prog_fill >= OTA_FLASH_WORD_SIZE) {
            if (OTA_FlushProgBuf() != 0) {
                return -1;
            }
        }
    }

    return 0;
}

void OTA_Init(void)
{
    s_state = OTA_STATE_IDLE;
    s_total_size = 0U;
    s_written = 0U;
    s_realtime_muted = 0U;
    s_last_activity_tick = 0U;
    s_reset_pending = 0U;
    s_done_tick = 0U;
    s_prog_fill = 0U;
    s_flash_cursor = 0U;
    s_erased_mask = 0U;
    s_flash_open = 0U;
    s_pending_len = 0U;
    s_has_pending = 0U;
    s_flag_pending = 0U;
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
    if ((s_reset_pending != 0U) && (s_state == OTA_STATE_DONE)) {
        if ((HAL_GetTick() - s_done_tick) >= 50U) {
            s_reset_pending = 0U;
            NVIC_SystemReset();
        }
        return;
    }

    if (s_realtime_muted == 0U) {
        return;
    }
    if ((s_state == OTA_STATE_VERIFY) || (s_state == OTA_STATE_DONE)) {
        return;
    }
    if ((HAL_GetTick() - s_last_activity_tick) >= OTA_REALTIME_MUTE_TIMEOUT_MS) {
        OTA_Abort();
    }
}

void OTA_Abort(void)
{
    OTA_FlashClose();
    s_state = OTA_STATE_IDLE;
    s_total_size = 0U;
    s_written = 0U;
    s_realtime_muted = 0U;
    s_last_activity_tick = 0U;
    s_prog_fill = 0U;
    s_flash_cursor = 0U;
    s_erased_mask = 0U;
    s_pending_len = 0U;
    s_has_pending = 0U;
    s_flag_pending = 0U;
}

int OTA_StartSession(uint32_t total_size)
{
    OTA_TouchActivity();

    if ((total_size == 0U) ||
        (total_size > APP_DOWNLOAD_FLASH_SIZE) ||
        (total_size > APP_FLASH_SIZE)) {
        s_state = OTA_STATE_ERROR;
        return -1;
    }

    OTA_FlashClose();

    s_total_size = total_size;
    s_written = 0U;
    s_prog_fill = 0U;
    s_flash_cursor = 0U;
    s_erased_mask = 0U;
    s_reset_pending = 0U;
    s_pending_len = 0U;
    s_has_pending = 0U;
    s_flag_pending = 0U;

    if (OTA_FlashOpen() != 0) {
        s_state = OTA_STATE_ERROR;
        return -1;
    }
    if (OTA_PreEraseForSize(total_size) != 0) {
        OTA_FlashClose();
        s_state = OTA_STATE_ERROR;
        return -1;
    }
    /* 会话期间保持 Unlock，避免每包 Lock/Unlock */

    OTA_TouchActivity();
    s_state = OTA_STATE_STARTED;
    return 0;
}

int OTA_WriteChunk(const uint8_t *data, uint16_t len)
{
    if (s_state != OTA_STATE_STARTED) {
        return -1;
    }
    if ((data == NULL) || (len == 0U) || (len > OTA_CHUNK_MAX_BYTES)) {
        return -1;
    }

    /* 上一包须已由链路层在发 0x20 后 CommitPending；此处只入队，保证先 ACK 再擦写 */
    if (s_has_pending != 0U) {
        s_state = OTA_STATE_ERROR;
        return -1;
    }

    if ((s_written + (uint32_t)len) > s_total_size) {
        s_state = OTA_STATE_ERROR;
        return -1;
    }

    memcpy(s_pending_chunk, data, len);
    s_pending_len = len;
    s_has_pending = 1U;
    OTA_TouchActivity();
    return 0;
}

int OTA_CommitPending(void)
{
    if (s_has_pending != 0U) {
        if (s_state != OTA_STATE_STARTED) {
            s_has_pending = 0U;
            s_pending_len = 0U;
            return -1;
        }

        s_has_pending = 0U;

        if (OTA_WriteBytes(s_pending_chunk, s_pending_len) != 0) {
            s_pending_len = 0U;
            OTA_FlashClose();
            s_state = OTA_STATE_ERROR;
            return -1;
        }

        s_pending_len = 0U;
        OTA_TouchActivity();
    }

    /* 0x20 已发出后再写 Set Flag（扇区擦除可能数秒） */
    if (s_flag_pending != 0U) {
        s_flag_pending = 0U;
        if (SetFlash_WriteRegion(SET_OTA_FLAG_OFFSET, &s_pending_flag,
                                 sizeof(s_pending_flag)) != 0) {
            s_state = OTA_STATE_ERROR;
            return -1;
        }
        s_state = OTA_STATE_DONE;
        /* ACK 早已发出；写完 Flag 后立即复位进 Boot，勿再等 OTA_Service */
        HAL_Delay(20);
        NVIC_SystemReset();
    }

    return 0;
}

int OTA_Finish(uint32_t expected_crc32)
{
    uint32_t actual_crc;

    if (s_state != OTA_STATE_STARTED) {
        return -1;
    }

    /* 只提交 DATA，不要在这里写 Flag */
    if (s_has_pending != 0U) {
        s_has_pending = 0U;
        if (OTA_WriteBytes(s_pending_chunk, s_pending_len) != 0) {
            s_pending_len = 0U;
            OTA_FlashClose();
            s_state = OTA_STATE_ERROR;
            return -1;
        }
        s_pending_len = 0U;
    }

    if (s_written != s_total_size) {
        OTA_FlashClose();
        s_state = OTA_STATE_ERROR;
        return -1;
    }

    s_state = OTA_STATE_VERIFY;
    OTA_TouchActivity();

    if (OTA_FlashOpen() != 0) {
        s_state = OTA_STATE_ERROR;
        return -1;
    }
    if (OTA_FlushProgBuf() != 0) {
        OTA_FlashClose();
        s_state = OTA_STATE_ERROR;
        return -1;
    }
    OTA_FlashClose();

    OTA_InvalidateRange(APP_DOWNLOAD_FLASH_ADDR, s_total_size);
    actual_crc = OTA_Crc32((const uint8_t *)APP_DOWNLOAD_FLASH_ADDR, s_total_size);
    if (actual_crc != expected_crc32) {
        s_state = OTA_STATE_ERROR;
        return -1;
    }

    memset(&s_pending_flag, 0, sizeof(s_pending_flag));
    s_pending_flag.magic = OTA_FLAG_MAGIC;
    s_pending_flag.app_size = s_total_size;
    s_pending_flag.app_crc32 = expected_crc32;
    s_pending_flag.status = OTA_STATUS_PENDING;
    s_pending_flag.flag_crc = OTA_Crc32((const uint8_t *)&s_pending_flag, 32U);
    s_flag_pending = 1U;

    OTA_TouchActivity();
    return 0;
}

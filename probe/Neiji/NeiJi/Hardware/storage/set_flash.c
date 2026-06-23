#include "set_flash.h"
#include "flash_layout.h"

#include "cmsis_os.h"
#include "main.h"
#include "stm32h7xx_hal_flash.h"
#include "stm32h7xx_hal_flash_ex.h"

#include <stdint.h>
#include <string.h>

#define SET_PRESERVE_BYTES   0x1000U
#define SET_FLASH_WORD_SIZE  32U
#define SET_FLASH_TIMEOUT    60000U

static uint8_t s_set_preserve[SET_PRESERVE_BYTES] __attribute__((aligned(32)));
static uint8_t s_word_buf[SET_FLASH_WORD_SIZE] __attribute__((aligned(32)));
static uint32_t s_last_hal_error;

uint32_t SetFlash_GetLastError(void)
{
    return s_last_hal_error;
}

static void flash_enter_critical(void)
{
    if (osKernelGetState() == osKernelRunning) {
        vTaskSuspendAll();
    }
}

static void flash_exit_critical(void)
{
    if (osKernelGetState() == osKernelRunning) {
        (void)xTaskResumeAll();
    }
}

static void flash_invalidate_range(uint32_t addr, uint32_t len)
{
    uint32_t start = addr & ~31U;
    int32_t size = (int32_t)((len + (addr - start) + 31U) & ~31U);

    SCB_InvalidateDCache_by_Addr((uint32_t *)start, size);
}

static int flash_erase_set_sector(void)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t sector_error = 0U;

    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Banks = FLASH_BANK_2;
    erase.Sector = FLASH_SECTOR_7;
    erase.NbSectors = 1U;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS_BANK2);

    if (HAL_FLASHEx_Erase(&erase, &sector_error) != HAL_OK) {
        s_last_hal_error = HAL_FLASH_GetError();
        return -1;
    }
    if (FLASH_WaitForLastOperation(SET_FLASH_TIMEOUT, FLASH_BANK_2) != HAL_OK) {
        s_last_hal_error = HAL_FLASH_GetError();
        return -1;
    }

    flash_invalidate_range(SET_FLASH_ADDR, SET_PRESERVE_BYTES);
    return 0;
}

static int flash_program_words(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint32_t offset;

    for (offset = 0U; offset < len; offset += SET_FLASH_WORD_SIZE) {
        uint32_t chunk = len - offset;

        memset(s_word_buf, 0xFF, sizeof(s_word_buf));
        if (chunk > SET_FLASH_WORD_SIZE) {
            chunk = SET_FLASH_WORD_SIZE;
        }
        memcpy(s_word_buf, buf + offset, chunk);

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, addr + offset,
                              (uint32_t)(uintptr_t)s_word_buf) != HAL_OK) {
            s_last_hal_error = HAL_FLASH_GetError();
            return -1;
        }
        if (FLASH_WaitForLastOperation(SET_FLASH_TIMEOUT, FLASH_BANK_2) != HAL_OK) {
            s_last_hal_error = HAL_FLASH_GetError();
            return -1;
        }
    }

    flash_invalidate_range(addr, len);
    return 0;
}

static int flash_program_preserve(void)
{
    uint32_t program_len = SET_PRESERVE_BYTES;

    return flash_program_words(SET_FLASH_ADDR, s_set_preserve, program_len);
}

int SetFlash_Read(uint32_t offset, void *buf, uint32_t len)
{
    uint32_t addr;

    if ((buf == NULL) || (len == 0U)) {
        return -1;
    }
    if ((offset + len) > SET_FLASH_SIZE) {
        return -1;
    }

    addr = SET_FLASH_ADDR + offset;
    flash_invalidate_range(addr, len);
    memcpy(buf, (const void *)addr, len);
    return 0;
}

static int flash_write_preserve_locked(void)
{
    int ret = -1;

    if (HAL_FLASH_Unlock() != HAL_OK) {
        s_last_hal_error = HAL_FLASH_GetError();
        return -1;
    }
    if (HAL_FLASHEx_Unlock_Bank2() != HAL_OK) {
        s_last_hal_error = HAL_FLASH_GetError();
        goto lock_out;
    }
    if (flash_erase_set_sector() != 0) {
        goto lock_out;
    }
    if (flash_program_preserve() != 0) {
        goto lock_out;
    }

    ret = 0;

lock_out:
    (void)HAL_FLASHEx_Lock_Bank2();
    (void)HAL_FLASH_Lock();
    return ret;
}

int SetFlash_WriteRegions(const uint32_t *offsets,
                          const void *const *datas,
                          const uint32_t *lens,
                          uint32_t count)
{
    uint32_t i;
    int ret = -1;

    if ((offsets == NULL) || (datas == NULL) || (lens == NULL) || (count == 0U)) {
        return -1;
    }

    s_last_hal_error = 0U;

    if (SetFlash_Read(0U, s_set_preserve, SET_PRESERVE_BYTES) != 0) {
        return -1;
    }

    for (i = 0U; i < count; i++) {
        uint32_t off = offsets[i];
        uint32_t len = lens[i];

        if ((datas[i] == NULL) || (len == 0U)) {
            return -1;
        }
        if ((off + len) > SET_PRESERVE_BYTES) {
            return -1;
        }
        memcpy(&s_set_preserve[off], datas[i], len);
    }

    flash_enter_critical();
    ret = flash_write_preserve_locked();
    flash_exit_critical();
    return ret;
}

int SetFlash_WriteRegion(uint32_t offset, const void *data, uint32_t len)
{
    const void *datas[1];
    uint32_t offsets[1];
    uint32_t lens[1];

    offsets[0] = offset;
    datas[0] = data;
    lens[0] = len;
    return SetFlash_WriteRegions(offsets, datas, lens, 1U);
}

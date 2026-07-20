#include "bl_ota.h"
#include "flash_layout.h"
#include "stm32h7xx_hal.h"

#include <stdio.h>
#include <string.h>

#define BL_FLASH_SECTOR_SIZE   0x00020000U  /* 128KB */
#define BL_FLASHWORD_SIZE      32U          /* H7 一次编程 256bit */

static uint32_t BL_Crc32(const uint8_t *data, uint32_t len)
{
  uint32_t crc = 0xFFFFFFFFU;
  uint32_t i;
  uint8_t j;

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

static uint32_t BL_AlignUp(uint32_t value, uint32_t align)
{
  return (value + (align - 1U)) & ~(align - 1U);
}

static void BL_AddrToBankSector(uint32_t addr, uint32_t *bank, uint32_t *sector)
{
  if (addr >= 0x08100000U)
  {
    *bank = FLASH_BANK_2;
    *sector = (addr - 0x08100000U) / BL_FLASH_SECTOR_SIZE;
  }
  else
  {
    *bank = FLASH_BANK_1;
    *sector = (addr - 0x08000000U) / BL_FLASH_SECTOR_SIZE;
  }
}

static int BL_EraseRange(uint32_t start_addr, uint32_t byte_len)
{
  FLASH_EraseInitTypeDef erase;
  uint32_t sector_error = 0U;
  uint32_t end_addr;
  uint32_t addr;
  uint32_t bank;
  uint32_t sector;

  if (byte_len == 0U)
  {
    return 0;
  }

  end_addr = start_addr + BL_AlignUp(byte_len, BL_FLASH_SECTOR_SIZE);

  if (HAL_FLASH_Unlock() != HAL_OK)
  {
    return -1;
  }

  for (addr = start_addr; addr < end_addr; addr += BL_FLASH_SECTOR_SIZE)
  {
    BL_AddrToBankSector(addr, &bank, &sector);

    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Banks = bank;
    erase.Sector = sector;
    erase.NbSectors = 1U;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    if (HAL_FLASHEx_Erase(&erase, &sector_error) != HAL_OK)
    {
      HAL_FLASH_Lock();
      return -1;
    }
  }

  HAL_FLASH_Lock();
  return 0;
}

static int BL_ProgramFlashWords(uint32_t dest, const uint8_t *src, uint32_t len)
{
  /* 32 字节对齐缓冲，满足 FLASHWORD 要求 */
  static uint8_t word_buf[BL_FLASHWORD_SIZE] __attribute__((aligned(32)));
  uint32_t offset = 0U;

  if (((dest % BL_FLASHWORD_SIZE) != 0U) || (len == 0U))
  {
    return -1;
  }

  if (HAL_FLASH_Unlock() != HAL_OK)
  {
    return -1;
  }

  while (offset < len)
  {
    uint32_t chunk = len - offset;

    if (chunk >= BL_FLASHWORD_SIZE)
    {
      memcpy(word_buf, src + offset, BL_FLASHWORD_SIZE);
    }
    else
    {
      memset(word_buf, 0xFF, BL_FLASHWORD_SIZE);
      memcpy(word_buf, src + offset, chunk);
    }

    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD,
                          dest + offset,
                          (uint32_t)word_buf) != HAL_OK)
    {
      HAL_FLASH_Lock();
      return -1;
    }

    offset += BL_FLASHWORD_SIZE;
  }

  HAL_FLASH_Lock();
  return 0;
}

static int BL_ReadOtaFlag(OtaFlag_t *out)
{
  uint32_t calc;

  if (out == NULL)
  {
    return -1;
  }

  memcpy(out, (const void *)SET_OTA_FLAG_ADDR, sizeof(OtaFlag_t));

  if (out->magic != OTA_FLAG_MAGIC)
  {
    return -1;
  }

  /* flag_crc：对结构体前 32 字节（不含 flag_crc 自身） */
  calc = BL_Crc32((const uint8_t *)out, 32U);
  if (calc != out->flag_crc)
  {
    return -1;
  }

  return 0;
}

static int BL_ClearOtaFlag(void)
{
  /* Set 扇区现仅作 OTA 元数据；设备配置在 W25Q，整扇区擦除即可清 Flag */
  return BL_EraseRange(SET_FLASH_ADDR, SET_FLASH_SIZE);
}

static int BL_CopyDownloadToApp(uint32_t app_size)
{
  uint32_t offset = 0U;
  static uint8_t chunk_buf[256] __attribute__((aligned(32)));

  if ((app_size == 0U) ||
      (app_size > APP_DOWNLOAD_FLASH_SIZE) ||
      (app_size > APP_FLASH_SIZE))
  {
    return -1;
  }

  if (BL_EraseRange(APP_FLASH_ADDR, app_size) != 0)
  {
    return -1;
  }

  while (offset < app_size)
  {
    uint32_t chunk = app_size - offset;
    uint32_t write_len;

    if (chunk > sizeof(chunk_buf))
    {
      chunk = (uint32_t)sizeof(chunk_buf);
    }

    memcpy(chunk_buf, (const void *)(APP_DOWNLOAD_FLASH_ADDR + offset), chunk);
    write_len = BL_AlignUp(chunk, BL_FLASHWORD_SIZE);
    if (write_len > chunk)
    {
      memset(chunk_buf + chunk, 0xFF, write_len - chunk);
    }

    if (BL_ProgramFlashWords(APP_FLASH_ADDR + offset, chunk_buf, write_len) != 0)
    {
      return -1;
    }

    offset += chunk;
  }

  return 0;
}

int BL_OtaTryUpdate(void)
{
  OtaFlag_t flag;
  uint32_t calc;

  if (BL_ReadOtaFlag(&flag) != 0)
  {
    printf("[NeijiBoot] ota: no valid flag, skip\r\n");
    return 0;
  }

  if (flag.status != OTA_STATUS_PENDING)
  {
    printf("[NeijiBoot] ota: status=0x%08lX, skip\r\n",
           (unsigned long)flag.status);
    return 0;
  }

  printf("[NeijiBoot] ota: PENDING size=%lu crc=0x%08lX\r\n",
         (unsigned long)flag.app_size,
         (unsigned long)flag.app_crc32);

  if ((flag.app_size == 0U) ||
      (flag.app_size > APP_DOWNLOAD_FLASH_SIZE) ||
      (flag.app_size > APP_FLASH_SIZE))
  {
    printf("[NeijiBoot] ota: bad size\r\n");
    return -1;
  }

  calc = BL_Crc32((const uint8_t *)APP_DOWNLOAD_FLASH_ADDR, flag.app_size);
  if (calc != flag.app_crc32)
  {
    printf("[NeijiBoot] ota: download CRC fail calc=0x%08lX\r\n",
           (unsigned long)calc);
    return -1;
  }

  printf("[NeijiBoot] ota: CRC ok, copy Download->App...\r\n");
  if (BL_CopyDownloadToApp(flag.app_size) != 0)
  {
    printf("[NeijiBoot] ota: copy FAIL\r\n");
    return -1;
  }

  calc = BL_Crc32((const uint8_t *)APP_FLASH_ADDR, flag.app_size);
  if (calc != flag.app_crc32)
  {
    printf("[NeijiBoot] ota: app CRC fail after copy\r\n");
    return -1;
  }

  if (BL_ClearOtaFlag() != 0)
  {
    printf("[NeijiBoot] ota: clear flag FAIL\r\n");
    return -1;
  }

  printf("[NeijiBoot] ota: update DONE\r\n");
  return 0;
}

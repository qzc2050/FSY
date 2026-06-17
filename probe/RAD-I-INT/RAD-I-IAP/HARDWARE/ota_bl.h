#ifndef __OTA_BL_H
#define __OTA_BL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ===========================================================================
 * Flash 分区地址表 (STM32H743 - 双 Bank, 每 Bank 8 个扇区，每扇区 128KB)
 * ===========================================================================
 * 总 Flash: 2MB (Bank1: 1MB + Bank2: 1MB)
 * 扇区布局:
 *   Bank1 (0x08000000-0x080FFFFF):
 *     - 扇区 0 (0x08000000-0x0801FFFF): Bootloader (128KB)
 *     - 扇区 1-7 (0x08020000-0x080FFFFF): App 区 (896KB)
 *   Bank2 (0x08100000-0x081FFFFF):
 *     - 扇区 8-14 (0x08100000-0x081EFFFF): Download 区 (896KB)
 *     - 扇区 14 (0x081E0000-0x081FFFFF): OTA Flag (128KB)
 * ===========================================================================
 */
#define BOOTLOADER_FLASH_ADDR   0x08000000U
#define BOOTLOADER_FLASH_SIZE   0x00020000U   /* 128KB (整个扇区 0) */

#define APP_FLASH_ADDR          0x08020000U   /* App 从扇区 1 开始 */
#define APP_FLASH_SIZE          0x000E0000U   /* 896KB (扇区 1-7) */

#define DOWNLOAD_FLASH_ADDR     0x08100000U   /* Bank2 扇区 8 开始 */
#define DOWNLOAD_FLASH_SIZE     0x000E0000U   /* 896KB (扇区 8-14) */
#define OTA_FLAG_FLASH_ADDR     0x081E0000U   /* Bank2 扇区 15 */

/* OTA Flag */
#define OTA_FLAG_MAGIC          0x4F544155U   /* 'OTAU' */
#define OTA_STATUS_IDLE         0x00000000U
#define OTA_STATUS_PENDING      0x00000001U
#define OTA_STATUS_DONE         0x00000002U

typedef struct
{
  uint32_t magic;
  uint32_t app_size;
  uint32_t app_crc32;
  uint32_t status;
  uint32_t reserved[4];
  uint32_t flag_crc;
  uint32_t reserved2[7];
} OtaFlag_t;

#ifdef __cplusplus
}
#endif

#endif /* __OTA_BL_H */


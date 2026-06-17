#ifndef __OTA_BL_H
#define __OTA_BL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ===========================================================================
 * Flash 分区地址表 (STM32F103CBT6 - 128KB, 页大小1KB)
 * 与 App 的 `zjb/Core/Inc/ota.h` 保持一致
 * ===========================================================================
 */
#define BOOTLOADER_FLASH_ADDR   0x08000000U
#define BOOTLOADER_FLASH_SIZE   0x00002000U   /* 8KB  */

#define APP_FLASH_ADDR          0x08002000U
#define APP_FLASH_SIZE          0x0000E000U   /* 56KB */

#define DOWNLOAD_FLASH_ADDR     0x08010000U
#define DOWNLOAD_FLASH_SIZE     0x0000F000U   /* 60KB */

#define OTA_FLAG_FLASH_ADDR     0x0801F000U   /* page 124 */

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
} OtaFlag_t;

#ifdef __cplusplus
}
#endif

#endif /* __OTA_BL_H */


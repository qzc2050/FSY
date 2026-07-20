#ifndef FLASH_LAYOUT_H
#define FLASH_LAYOUT_H

#include <stdint.h>

/*
 * NeijiBoot / NeiJi 共用片内 Flash 分区（STM32H743，2MB）
 * 与 probe/Neiji/NeiJi/Hardware/storage/flash_layout.h 保持一致。
 */

#define BOOT_FLASH_ADDR           0x08000000U
#define BOOT_FLASH_SIZE           0x00020000U   /* 128KB, 扇区0 */

#define APP_FLASH_ADDR            0x08020000U
#define APP_FLASH_SIZE            0x000E0000U   /* 896KB, 扇区1–7 */

#define APP_DOWNLOAD_FLASH_ADDR   0x08100000U
#define APP_DOWNLOAD_FLASH_SIZE   0x000E0000U   /* 896KB, 扇区8–14 */

#define SET_FLASH_ADDR            0x081E0000U
#define SET_FLASH_SIZE            0x00020000U   /* 128KB, 扇区15 */
#define SET_OTA_FLAG_ADDR         SET_FLASH_ADDR

/* 与 FSY-I ota_bl.h / 后续 App 写 Flag 保持一致 */
#define OTA_FLAG_MAGIC            0x4F544155U   /* 'OTAU' */
#define OTA_STATUS_IDLE           0x00000000U
#define OTA_STATUS_PENDING        0x00000001U
#define OTA_STATUS_DONE           0x00000002U

typedef struct
{
  uint32_t magic;
  uint32_t app_size;
  uint32_t app_crc32;
  uint32_t status;
  uint32_t reserved[4];   /* 共 32B 到此，供 flag_crc 覆盖 */
  uint32_t flag_crc;      /* CRC32(前 32 字节) */
  uint32_t reserved2[7];
} OtaFlag_t;

#endif /* FLASH_LAYOUT_H */

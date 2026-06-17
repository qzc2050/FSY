#ifndef __OTA_H
#define __OTA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ===========================================================================
 * Flash 分区地址表 (STM32F103CBT6 - 128KB, 页大小1KB)
 *
 *  地址范围               大小    页      说明
 *  0x08000000-0x08001FFF  8KB    0-7    Bootloader
 *  0x08002000-0x0800FFFF  56KB   8-63   App (本工程)
 *  0x08010000-0x0801EFFF  60KB   64-123 Download (固件暂存区)
 *  0x0801F000-0x0801F3FF  1KB    124    OTA Flag (升级标记)
 *  0x0801F400-0x0801FBFF  2KB    125-126 保留
 *  0x0801FC00-0x0801FFFF  1KB    127    Config (设备配置, 原有)
 * ===========================================================================
 */
#define BOOTLOADER_FLASH_ADDR   0x08000000U
#define BOOTLOADER_FLASH_SIZE   0x00002000U   /* 8KB  */

#define APP_FLASH_ADDR          0x08002000U
#define APP_FLASH_SIZE          0x0000E000U   /* 56KB */
#define APP_FLASH_PAGES         56U

#define DOWNLOAD_FLASH_ADDR     0x08010000U
#define DOWNLOAD_FLASH_SIZE     0x0000F000U   /* 60KB */
#define DOWNLOAD_FLASH_PAGES    60U

#define OTA_FLAG_FLASH_ADDR     0x0801F000U   /* page 124 */
#define OTA_FLAG_FLASH_SIZE     0x00000400U   /* 1KB  */


/* ===========================================================================
 * OTA 标记结构体 (写在 OTA_FLAG_FLASH_ADDR 页)
 * ===========================================================================
 */
#define OTA_FLAG_MAGIC          0x4F544155U   /* 'OTAU' */
#define OTA_STATUS_IDLE         0x00000000U   /* 无待更新固件 */
#define OTA_STATUS_PENDING      0x00000001U   /* 有待更新固件，Bootloader 执行拷贝 */
#define OTA_STATUS_DONE         0x00000002U   /* Bootloader 已完成更新 (可选回写) */

typedef struct
{
    uint32_t magic;         /* OTA_FLAG_MAGIC */
    uint32_t app_size;      /* Download 区固件字节数 */
    uint32_t app_crc32;     /* 固件 CRC32 */
    uint32_t status;        /* OTA_STATUS_PENDING / IDLE */
    uint32_t reserved[4];
    uint32_t flag_crc;      /* 对前面所有字段的 CRC32（不含 flag_crc 本身） */
} OtaFlag_t;

/* ===========================================================================
 * OTA 状态机 (App 侧)
 * ===========================================================================
 */
typedef enum
{
    OTA_STATE_IDLE    = 0,  /* 空闲，未开始 */
    OTA_STATE_STARTED = 1,  /* 已开始，接收中 */
    OTA_STATE_VERIFY  = 2,  /* 接收完毕，校验中 */
    OTA_STATE_ERROR   = 3,  /* 出错（CRC 不对/越界等） */
    OTA_STATE_DONE    = 4,  /* 标记已写入，等待重启 */
} OtaState_e;

/* ===========================================================================
 * OTA 协议寄存器地址（配合现有 Modbus-like 协议使用）
 *
 * 功能码 0x10 (写多个寄存器):
 *   REG_OTA_START (200, 0x00C8): qty=2, data=total_size(uint32, lo reg first)
 *   REG_OTA_DATA  (208, 0x00D0): qty=1~64, data=固件原始字节 (最多128字节/包)
 *   REG_OTA_DONE  (202, 0x00CA): qty=2, data=app_crc32(uint32, lo reg first)
 *
 * 功能码 0x03 (读多个寄存器):
 *   REG_OTA_STATUS(204, 0x00CC): qty=4, 返回 state(2regs) + written_bytes(2regs)
 * ===========================================================================
 */
#define REG_OTA_START           200U   /* 0x00C8 */
#define REG_OTA_DONE            202U   /* 0x00CA */
#define REG_OTA_STATUS          204U   /* 0x00CC */
#define REG_OTA_DATA            208U   /* 0x00D0 */

/* ===========================================================================
 * API
 * ===========================================================================
 */
void         OTA_Init(void);
OtaState_e   OTA_GetState(void);
uint32_t     OTA_GetWrittenBytes(void);
uint32_t     OTA_GetTotalSize(void);
uint8_t      OTA_IsRealtimeMuted(void);
void         OTA_Service(void);

/* 开始一次 OTA 会话：擦除 Download 区，准备接收 */
int          OTA_StartSession(uint32_t total_size);

/* 追加写入一块固件数据（顺序调用）, 返回 0 成功 */
int          OTA_WriteChunk(const uint8_t *data, uint16_t len);

/* 结束 OTA：校验 CRC，写 OTA Flag，准备重启；返回 0 成功 */
int          OTA_Finish(uint32_t expected_crc32);

/* 放弃当前 OTA 会话，回到 IDLE */
void         OTA_Abort(void);

#ifdef __cplusplus
}
#endif

#endif /* __OTA_H */

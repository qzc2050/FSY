#ifndef FLASH_LAYOUT_H
#define FLASH_LAYOUT_H

#include <stdint.h>

/*
 * NeiJi 片内 Flash 分区（STM32H743IITx，2MB）
 *
 * 与 FSY-I IAP（ota_bl.h）及《辐射报警仪》后续 OTA 规划一致。
 * Boot / App / AppDownload / Set 四区边界固定，后续 OTA 与配置模块只引用本头文件。
 *
 * 扇区：Bank1 + Bank2 各 8 × 128KB
 *
 *   0x0800_0000 ┌─────────────────┐ 扇区0  Boot
 *               │     Boot        │ 128KB  （现阶段空置，留 IAP）
 *   0x0802_0000 ├─────────────────┤ 扇区1
 *               │                 │
 *               │      App        │ 896KB  扇区1–7，当前 NeiJi 程序
 *               │                 │
 *   0x0810_0000 ├─────────────────┤ 扇区8
 *               │                 │
 *               │  App Download   │ 896KB  扇区8–14，OTA 接收区（现不用）
 *               │                 │
 *   0x081E_0000 ├─────────────────┤ 扇区15
 *               │      Set        │ 128KB  OTA 元数据（设备配置已迁 W25Q）
 *   0x0820_0000 └─────────────────┘
 *
 * 注意：
 * - App 链接上限 APP_FLASH_SIZE，禁止覆盖 AppDownload / Set。
 * - Set 内改 OTA 元数据需读-改-擦-写整扇区 15。
 * - 设备配置（SN/型号/地址）存 W25Q 末尾 64KB，见 ext_flash_layout.h。
 */

#define FLASH_BANK_SIZE           (1024U * 1024U)
#define FLASH_SECTOR_SIZE         (128U * 1024U)
#define FLASH_TOTAL_SIZE          (2U * FLASH_BANK_SIZE)

#define FLASH_BANK1_BASE          0x08000000U
#define FLASH_BANK2_BASE          0x08100000U

/* ---------- Boot ---------- */
#define BOOT_FLASH_ADDR           0x08000000U
#define BOOT_FLASH_SIZE           FLASH_SECTOR_SIZE          /* 128KB, 扇区0 */

/* ---------- App（运行区） ---------- */
#define APP_FLASH_ADDR            0x08020000U
#define APP_FLASH_SIZE            (7U * FLASH_SECTOR_SIZE)   /* 896KB, 扇区1–7 */
#define APP_FLASH_END             (APP_FLASH_ADDR + APP_FLASH_SIZE - 1U)

/* ---------- App Download（OTA 暂存，现阶段禁止链接/写常量） ---------- */
#define APP_DOWNLOAD_FLASH_ADDR   0x08100000U
#define APP_DOWNLOAD_FLASH_SIZE   (7U * FLASH_SECTOR_SIZE)   /* 896KB, 扇区8–14 */
#define APP_DOWNLOAD_FLASH_END    (APP_DOWNLOAD_FLASH_ADDR + APP_DOWNLOAD_FLASH_SIZE - 1U)

/* ---------- Set（配置 + OTA 元数据，扇区15 整扇区） ---------- */
#define SET_FLASH_ADDR            0x081E0000U
#define SET_FLASH_SIZE            FLASH_SECTOR_SIZE          /* 128KB, 扇区15 */
#define SET_FLASH_END             (SET_FLASH_ADDR + SET_FLASH_SIZE - 1U)

/* Set 子区（同扇区，写时合并读-modify-erase-program） */
#define SET_OTA_FLAG_OFFSET       0x0000U
#define SET_OTA_FLAG_ADDR         (SET_FLASH_ADDR + SET_OTA_FLAG_OFFSET)

#define SET_DEVICE_CFG_OFFSET     0x0400U
#define SET_DEVICE_CFG_ADDR       (SET_FLASH_ADDR + SET_DEVICE_CFG_OFFSET)
#define SET_DEVICE_CFG_MAX_SIZE   512U

#define SET_BACKUP_CFG_OFFSET     0x0800U
#define SET_BACKUP_CFG_ADDR     (SET_FLASH_ADDR + SET_BACKUP_CFG_OFFSET)
/* 上述 Set 内配置区仅用于从片内 Flash 一次性迁移，新配置写 W25Q */

/* 编译期检查：四区不重叠、不越界 */
#if (APP_FLASH_ADDR + APP_FLASH_SIZE) != APP_DOWNLOAD_FLASH_ADDR
#error "App 区与 AppDownload 区未衔接"
#endif
#if (APP_DOWNLOAD_FLASH_ADDR + APP_DOWNLOAD_FLASH_SIZE) != SET_FLASH_ADDR
#error "AppDownload 区与 Set 区未衔接"
#endif
#if (SET_FLASH_ADDR + SET_FLASH_SIZE) > (FLASH_BANK2_BASE + FLASH_BANK_SIZE)
#error "Set 区超出片内 Flash"
#endif

/* 地址是否落在指定区（供后续 Flash 驱动 / 配置模块使用） */
static inline int Flash_IsInAppRegion(uint32_t addr, uint32_t len)
{
    uint32_t end = addr + len - 1U;
    return (addr >= APP_FLASH_ADDR) && (end <= APP_FLASH_END);
}

static inline int Flash_IsInDownloadRegion(uint32_t addr, uint32_t len)
{
    uint32_t end = addr + len - 1U;
    return (addr >= APP_DOWNLOAD_FLASH_ADDR) && (end <= APP_DOWNLOAD_FLASH_END);
}

static inline int Flash_IsInSetRegion(uint32_t addr, uint32_t len)
{
    uint32_t end = addr + len - 1U;
    return (addr >= SET_FLASH_ADDR) && (end <= SET_FLASH_END);
}

#endif /* FLASH_LAYOUT_H */

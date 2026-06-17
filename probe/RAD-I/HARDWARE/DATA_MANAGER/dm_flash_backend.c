#include "dm_flash_backend.h"
#include "dm_config.h"
#include "w25qxx.h"  /* W25QXX QSPI Flash 驱动 */
#include <string.h>
#include <stdio.h>

/*==========================================================================
 * 静态函数声明
 *==========================================================================*/

static int flash_read(uint32_t addr, uint8_t *buf, size_t len);
static int flash_write_blank(uint32_t addr, const uint8_t *data, size_t len);
static int flash_erase_sector(uint32_t addr);
static size_t flash_get_sector_size(void);
static DM_StorageType_t flash_get_storage_type(void);

/*==========================================================================
 * 全局变量
 *==========================================================================*/

/** Flash 后端接口表 */
static const DM_StorageBackend_t g_flash_backend = {
    .read = flash_read,
    .write_blank = flash_write_blank,
    .erase_sector = flash_erase_sector,
    .get_sector_size = flash_get_sector_size,
    .get_storage_type = flash_get_storage_type,
};

/*==========================================================================
 * API 实现
 *==========================================================================*/

/**
 * 初始化 Flash 后端
 */
int DM_FlashBackend_Init(void)
{
    uint8_t ret;
    
    /* 初始化 W25QXX QSPI Flash */
    ret = W25Qx_QSPI_Init();
    if (ret != QSPI_OK) {
        DM_DEBUG("[DM] Flash 初始化失败：%u\r\n", ret);
        return -1;
    }
    
    /* 验证 Flash ID */
    uint32_t flash_id = W25Qx_QSPI_FLASH_ReadID();
    if (flash_id != sFLASH_ID) {
        DM_DEBUG("[DM] Flash ID 不匹配：0x%06X (期望：0x%06X)\r\n", 
                 (unsigned)flash_id, (unsigned)sFLASH_ID);
        return -1;
    }
    
    DM_DEBUG("[DM] Flash 后端初始化成功（ID: 0x%06X）\r\n", (unsigned)flash_id);
    return 0;
}

/**
 * 获取 Flash 后端接口
 */
const DM_StorageBackend_t* DM_FlashBackend_GetInterface(void)
{
    return &g_flash_backend;
}

/*==========================================================================
 * 底层实现（需要根据实际硬件修改）
 *==========================================================================*/

/**
 * Flash 读取
 */
static int flash_read(uint32_t addr, uint8_t *buf, size_t len)
{
    uint8_t ret;
    
    if (buf == NULL || len == 0) {
        return -1;
    }
    
    /* 使用 W25QXX 驱动读取 Flash */
    ret = W25Qx_QSPI_Read(buf, addr, len);
    if (ret != QSPI_OK) {
        DM_DEBUG("[DM] Flash 读取失败：addr=0x%08X, len=%u, ret=%u\r\n", 
                 addr, (unsigned)len, ret);
        return -1;
    }
    
    return 0;
}

/**
 * Flash 写入（到空白位置）
 */
static int flash_write_blank(uint32_t addr, const uint8_t *data, size_t len)
{
    uint8_t ret;
    uint8_t check_buf[64];
    size_t i;
    
    if (data == NULL || len == 0) {
        return -1;
    }
    
    /* 验证地址对齐 */
    if (addr % 4 != 0) {
        DM_DEBUG("[DM] 错误：写入地址未对齐 addr=0x%08X\r\n", addr);
        return -1;
    }
    
    /* 尝试验证槽位是否为空白 */
    ret = W25Qx_QSPI_Read(check_buf, addr, (len > sizeof(check_buf)) ? sizeof(check_buf) : len);
    if (ret != QSPI_OK) {
        DM_DEBUG("[DM] 错误：验证空白失败 addr=0x%08X\r\n", addr);
        return -1;
    }
    
    for (i = 0; i < len && i < sizeof(check_buf); i++) {
        if (check_buf[i] != 0xFF) {
            DM_DEBUG("[DM] 错误：尝试写入非空白位置 addr=0x%08X\r\n", addr);
            return -1;
        }
    }
    
    /* 使用 W25QXX 驱动写入 Flash */
    ret = W25Qx_QSPI_Write((uint8_t*)data, addr, len);
    if (ret != QSPI_OK) {
        DM_DEBUG("[DM] Flash 写入失败：addr=0x%08X, len=%u, ret=%u\r\n", 
                 addr, (unsigned)len, ret);
        return -1;
    }
    
    /* 验证写入（可选） */
    #if !DM_SKIP_WRITE_VERIFY
    ret = W25Qx_QSPI_Read(check_buf, addr, (len > sizeof(check_buf)) ? sizeof(check_buf) : len);
    if (ret != QSPI_OK) {
        DM_DEBUG("[DM] 错误：写后验证读取失败 addr=0x%08X\r\n", addr);
        return -1;
    }
    
    if (memcmp(data, check_buf, len) != 0) {
        DM_DEBUG("[DM] 错误：写后验证失败 addr=0x%08X\r\n", addr);
        return -1;
    }
    #endif
    
    DM_DEBUG("[DM] Flash 写入成功：addr=0x%08X, len=%u\r\n", addr, (unsigned)len);
    return 0;
}

/**
 * Flash 擦除扇区
 */
static int flash_erase_sector(uint32_t addr)
{
    uint8_t ret;
    
    /* 使用 W25QXX 驱动擦除扇区 */
    ret = W25Qx_QSPI_Erase_Block(addr);
    if (ret != QSPI_OK) {
        DM_DEBUG("[DM] Flash 擦除扇区失败：addr=0x%08X, ret=%u\r\n", addr, ret);
        return -1;
    }
    
    DM_DEBUG("[DM] Flash 擦除扇区：addr=0x%08X\r\n", addr);
    return 0;
}

/**
 * 获取扇区大小
 */
static size_t flash_get_sector_size(void)
{
    return DM_FLASH_SECTOR_SIZE;
}

/**
 * 获取存储类型
 */
static DM_StorageType_t flash_get_storage_type(void)
{
    return DM_STORAGE_FLASH_QSPI;
}

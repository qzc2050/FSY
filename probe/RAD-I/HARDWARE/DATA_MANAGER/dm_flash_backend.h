#ifndef __DM_FLASH_BACKEND_H
#define __DM_FLASH_BACKEND_H

#include "dm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/*==========================================================================
 * 宏定义和常量
 *==========================================================================*/

/** Flash 扇区大小（4KB）- 与 W25QXX 驱动一致 */
#define DM_FLASH_SECTOR_SIZE  QSPI_SECTOR_SIZE  /* 4096 字节 */

/*==========================================================================
 * Flash 存储后端接口
 * 基于 W25Qxx 系列 Flash 的实现
 *==========================================================================*/

/**
 * 初始化 Flash 后端
 * @return 0 成功，其他失败
 */
int DM_FlashBackend_Init(void);

/**
 * 获取 Flash 后端接口
 * @return 后端接口指针
 */
const DM_StorageBackend_t* DM_FlashBackend_GetInterface(void);

/**
 * Flash 读取
 * @param addr 地址
 * @param buf 缓冲区
 * @param len 长度
 * @return 0 成功，其他失败
 */
int DM_FlashBackend_Read(uint32_t addr, uint8_t *buf, size_t len);

/**
 * Flash 写入（到空白位置）
 * @param addr 地址
 * @param data 数据
 * @param len 长度
 * @return 0 成功，其他失败
 */
int DM_FlashBackend_WriteBlank(uint32_t addr, const uint8_t *data, size_t len);

/**
 * Flash 擦除扇区
 * @param addr 扇区地址
 * @return 0 成功，其他失败
 */
int DM_FlashBackend_EraseSector(uint32_t addr);

/**
 * 获取扇区大小
 * @return 扇区大小（字节）
 */
size_t DM_FlashBackend_GetSectorSize(void);

/**
 * 获取存储类型
 * @return 存储类型
 */
DM_StorageType_t DM_FlashBackend_GetStorageType(void);

#ifdef __cplusplus
}
#endif

#endif /* __DM_FLASH_BACKEND_H */

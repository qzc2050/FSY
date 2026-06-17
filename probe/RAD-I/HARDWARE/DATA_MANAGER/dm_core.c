#include "dm_core.h"
#include "dm_config.h"
#include <string.h>
#include <stdio.h>

/*==========================================================================
 * 内部函数声明
 *==========================================================================*/

static DM_Result_t dm_calculate_layout(DM_Manager_t *mgr, const DM_DataFormat_t *format);
static DM_Result_t dm_validate_storage(const DM_DataFormat_t *format);
static int dm_backend_read(const DM_StorageBackend_t *backend, uint32_t addr, uint8_t *buf, size_t len);
static int dm_backend_write(const DM_StorageBackend_t *backend, uint32_t addr, const uint8_t *data, size_t len);
static int dm_backend_erase(const DM_StorageBackend_t *backend, uint32_t addr);

/*==========================================================================
 * 核心层 API 实现
 *==========================================================================*/

/**
 * 注册数据类型
 */
DM_Result_t DM_RegisterDataManager(
    const DM_DataFormat_t *format,
    const DM_StorageBackend_t *backend,
    DM_Manager_t *out_mgr
) {
    DM_Manager_t *mgr;
    DM_Result_t ret;
    
    /* 参数验证 */
    if (format == NULL || backend == NULL || out_mgr == NULL) {
        return DM_ERR_INVALID_PARAM;
    }
    
    if (format->record_size == 0 || format->max_records == 0) {
        return DM_ERR_INVALID_PARAM;
    }
    
    if (format->sector_size == 0 || format->storage_size == 0) {
        return DM_ERR_INVALID_PARAM;
    }
    
    /* 验证存储空间是否足够 */
    ret = dm_validate_storage(format);
    if (ret != DM_OK) {
        return ret;
    }
    
    /* 初始化管理器 */
    mgr = out_mgr;
    memset(mgr, 0, sizeof(DM_Manager_t));
    
    mgr->format = format;
    mgr->backend = backend;
    
    /* 计算扇区布局 */
    ret = dm_calculate_layout(mgr, format);
    if (ret != DM_OK) {
        return ret;
    }
    
    mgr->meta_magic = format->meta_magic;
    mgr->phase = DM_PHASE_PRIMARY;
    
    return DM_OK;
}

/**
 * 初始化数据管理器
 */
DM_Result_t DM_Init(DM_Manager_t *mgr)
{
    if (mgr == NULL) {
        return DM_ERR_INVALID_PARAM;
    }
    
    DM_DEBUG("[DM] 初始化：%s, 正式扇区=%u, 缓存扇区=%u, 每扇区记录=%u\r\n",
             mgr->format->name,
             mgr->formal_sector_count,
             mgr->cache_sector_count,
             mgr->slots_per_sector);
    
    /* TODO: 加载元数据，恢复运行时状态 */
    
    return DM_OK;
}

/**
 * 写入原始数据
 */
DM_Result_t DM_WriteRaw(DM_Manager_t *mgr, const uint8_t *data)
{
    uint32_t addr;
    int ret;
    
    if (mgr == NULL || data == NULL) {
        return DM_ERR_INVALID_PARAM;
    }
    
    /* 根据当前阶段计算写入地址 */
    if (mgr->phase == DM_PHASE_PRIMARY) {
        addr = DM_GetSlotAddress(mgr, DM_SECTOR_TYPE_FORMAL, 0, mgr->write_next);
    } else if (mgr->phase == DM_PHASE_CACHE) {
        addr = DM_GetSlotAddress(mgr, DM_SECTOR_TYPE_CACHE, 0, mgr->cache_write_next);
    } else {
        return DM_ERR_NOT_SUPPORTED;
    }
    
    /* 写入数据 */
    ret = dm_backend_write(mgr->backend, addr, data, mgr->format->record_size);
    if (ret != 0) {
        return DM_ERR_STORAGE_FAILED;
    }
    
    /* 更新写入指针 */
    if (mgr->phase == DM_PHASE_PRIMARY) {
        mgr->write_next++;
        if (mgr->write_next >= mgr->formal_sector_count * mgr->slots_per_sector) {
            mgr->phase = DM_PHASE_CACHE;
        }
    } else {
        mgr->cache_write_next++;
    }
    
    return DM_OK;
}

/**
 * 读取原始数据
 */
DM_Result_t DM_ReadRaw(
    DM_Manager_t *mgr,
    uint16_t logical_index,
    uint8_t *out_data
) {
    uint32_t addr;
    int ret;
    
    if (mgr == NULL || out_data == NULL) {
        return DM_ERR_INVALID_PARAM;
    }
    
    if (logical_index >= mgr->valid_count) {
        return DM_ERR_OUT_OF_RANGE;
    }
    
    /* TODO: 根据逻辑索引计算物理地址（环形缓冲） */
    addr = 0;  /* 待实现 */
    
    ret = dm_backend_read(mgr->backend, addr, out_data, mgr->format->record_size);
    if (ret != 0) {
        return DM_ERR_STORAGE_FAILED;
    }
    
    return DM_OK;
}

/**
 * 获取有效记录数
 */
uint16_t DM_GetValidCount(DM_Manager_t *mgr)
{
    if (mgr == NULL) {
        return 0;
    }
    return mgr->valid_count;
}

/**
 * 清空所有数据
 */
DM_Result_t DM_Clear(DM_Manager_t *mgr)
{
    uint16_t i;
    uint32_t addr;
    
    if (mgr == NULL) {
        return DM_ERR_INVALID_PARAM;
    }
    
    /* 擦除所有正式扇区 */
    for (i = 0; i < mgr->formal_sector_count; i++) {
        addr = DM_GetSectorAddress(mgr, DM_SECTOR_TYPE_FORMAL, i);
        if (dm_backend_erase(mgr->backend, addr) != 0) {
            return DM_ERR_STORAGE_FAILED;
        }
    }
    
    /* 擦除所有缓存扇区 */
    for (i = 0; i < mgr->cache_sector_count; i++) {
        addr = DM_GetSectorAddress(mgr, DM_SECTOR_TYPE_CACHE, i);
        if (dm_backend_erase(mgr->backend, addr) != 0) {
            return DM_ERR_STORAGE_FAILED;
        }
    }
    
    /* 擦除临时扇区 */
    for (i = 0; i < mgr->temp_sector_count; i++) {
        addr = DM_GetSectorAddress(mgr, DM_SECTOR_TYPE_TEMP, i);
        if (dm_backend_erase(mgr->backend, addr) != 0) {
            return DM_ERR_STORAGE_FAILED;
        }
    }
    
    /* 重置运行时状态 */
    mgr->valid_count = 0;
    mgr->write_next = 0;
    mgr->cache_write_next = 0;
    mgr->phase = DM_PHASE_PRIMARY;
    
    return DM_OK;
}

/**
 * 保存元数据
 */
DM_Result_t DM_SaveMeta(DM_Manager_t *mgr)
{
    /* TODO: 将运行时状态保存到元数据扇区 */
    return DM_OK;
}

/**
 * 获取扇区地址
 */
uint32_t DM_GetSectorAddress(
    DM_Manager_t *mgr,
    DM_SectorType_t sector_type,
    uint16_t sector_index
) {
    uint32_t base;
    
    if (mgr == NULL) {
        return 0;
    }
    
    switch (sector_type) {
        case DM_SECTOR_TYPE_FORMAL:
            if (sector_index >= mgr->formal_sector_count) {
                return 0;
            }
            base = mgr->records_base;
            break;
            
        case DM_SECTOR_TYPE_CACHE:
            if (sector_index >= mgr->cache_sector_count) {
                return 0;
            }
            base = mgr->cache_base;
            break;
            
        case DM_SECTOR_TYPE_TEMP:
            if (sector_index >= mgr->temp_sector_count) {
                return 0;
            }
            base = mgr->temp_base;
            break;
            
        default:
            return 0;
    }
    
    return base + (uint32_t)sector_index * mgr->format->sector_size;
}

/**
 * 获取槽位地址
 */
uint32_t DM_GetSlotAddress(
    DM_Manager_t *mgr,
    DM_SectorType_t sector_type,
    uint16_t sector_index,
    uint16_t slot_index
) {
    uint32_t sector_addr;
    uint32_t slot_offset;
    
    if (mgr == NULL) {
        return 0;
    }
    
    if (slot_index >= mgr->slots_per_sector) {
        return 0;
    }
    
    sector_addr = DM_GetSectorAddress(mgr, sector_type, sector_index);
    if (sector_addr == 0) {
        return 0;
    }
    
    slot_offset = (uint32_t)slot_index * mgr->format->record_size;
    
    return sector_addr + slot_offset;
}

/*==========================================================================
 * 工具函数实现
 *==========================================================================*/

/**
 * 检查槽位是否为空白
 */
bool DM_IsSlotBlank(
    const DM_StorageBackend_t *backend,
    uint32_t addr,
    size_t record_size
) {
    uint8_t buf[64];
    size_t i;
    
    if (backend == NULL || record_size == 0 || record_size > sizeof(buf)) {
        return false;
    }
    
    if (dm_backend_read(backend, addr, buf, record_size) != 0) {
        return false;
    }
    
    for (i = 0; i < record_size; i++) {
        if (buf[i] != 0xFF) {
            return false;
        }
    }
    
    return true;
}

/*==========================================================================
 * 内部函数实现
 *==========================================================================*/

/**
 * 计算扇区布局
 */
static DM_Result_t dm_calculate_layout(DM_Manager_t *mgr, const DM_DataFormat_t *format)
{
    uint32_t slots_per_sector;
    uint32_t formal_sectors;
    uint32_t cache_sectors;
    uint32_t temp_sectors;
    uint32_t meta_sectors;
    uint32_t required_size;
    uint32_t offset;
    
    /* 计算每扇区记录数 */
    slots_per_sector = format->sector_size / format->record_size;
    if (slots_per_sector == 0) {
        return DM_ERR_INVALID_PARAM;
    }
    
    /* 计算正式扇区数量 */
    if (format->formal_sector_count > 0) {
        formal_sectors = format->formal_sector_count;
    } else {
        formal_sectors = (format->max_records + slots_per_sector - 1) / slots_per_sector;
    }
    
    /* 计算缓存扇区数量 */
    if (format->cache_sector_count > 0) {
        cache_sectors = format->cache_sector_count;
    } else {
        cache_sectors = formal_sectors;  /* 默认与正式扇区相同 */
    }
    
    /* 计算临时扇区数量 */
    if (format->temp_sector_count > 0) {
        temp_sectors = format->temp_sector_count;
    } else {
        temp_sectors = 1;  /* 默认为 1 */
    }
    
    /* 元数据扇区数量 */
    meta_sectors = (format->meta_sector_count > 0) ? format->meta_sector_count : 1;
    
    /* 计算所需总大小 */
    required_size = (formal_sectors + cache_sectors + temp_sectors + meta_sectors) * format->sector_size;
    
    /* 验证存储空间是否足够 */
    if (required_size > format->storage_size) {
        DM_DEBUG("[DM] 错误：存储空间不足（需要 %u 字节，可用 %u 字节）\r\n",
                 required_size, format->storage_size);
        return DM_ERR_INVALID_PARAM;
    }
    
    /* 填充管理器 */
    mgr->slots_per_sector = (uint16_t)slots_per_sector;
    mgr->formal_sector_count = (uint16_t)formal_sectors;
    mgr->cache_sector_count = (uint16_t)cache_sectors;
    mgr->temp_sector_count = (uint16_t)temp_sectors;
    mgr->meta_sector_count = (uint16_t)meta_sectors;
    
    /* 计算各区域基址 */
    offset = format->storage_base;
    mgr->meta_base = offset;
    offset += meta_sectors * format->sector_size;
    
    mgr->records_base = offset;
    offset += formal_sectors * format->sector_size;
    
    mgr->cache_base = offset;
    offset += cache_sectors * format->sector_size;
    
    mgr->temp_base = offset;
    
    DM_DEBUG("[DM] 布局计算：正式扇区=%u, 缓存扇区=%u, 临时扇区=%u, 每扇区记录=%u\r\n",
             formal_sectors, cache_sectors, temp_sectors, slots_per_sector);
    
    return DM_OK;
}

/**
 * 验证存储空间
 */
static DM_Result_t dm_validate_storage(const DM_DataFormat_t *format)
{
    uint32_t slots_per_sector;
    uint32_t required_sectors;
    uint32_t required_size;
    
    slots_per_sector = format->sector_size / format->record_size;
    if (slots_per_sector == 0) {
        DM_DEBUG("[DM] 错误：记录大小超过扇区大小\r\n");
        return DM_ERR_INVALID_PARAM;
    }
    
    /* 计算最少需要的扇区数（正式 + 缓存 + 临时 + 元数据） */
    required_sectors = 2 * ((format->max_records + slots_per_sector - 1) / slots_per_sector) + 2;
    
    required_size = required_sectors * format->sector_size;
    
    if (required_size > format->storage_size) {
        DM_DEBUG("[DM] 错误：存储空间不足（最少需要 %u 字节）\r\n", required_size);
        return DM_ERR_INVALID_PARAM;
    }
    
    return DM_OK;
}

/**
 * 后端读取包装函数
 */
static int dm_backend_read(const DM_StorageBackend_t *backend, uint32_t addr, uint8_t *buf, size_t len)
{
    if (backend->read == NULL) {
        return -1;
    }
    return backend->read(addr, buf, len);
}

/**
 * 后端写入包装函数
 */
static int dm_backend_write(const DM_StorageBackend_t *backend, uint32_t addr, const uint8_t *data, size_t len)
{
    if (backend->write_blank == NULL) {
        return -1;
    }
    return backend->write_blank(addr, data, len);
}

/**
 * 后端擦除包装函数
 */
static int dm_backend_erase(const DM_StorageBackend_t *backend, uint32_t addr)
{
    if (backend->erase_sector == NULL) {
        return -1;
    }
    return backend->erase_sector(addr);
}

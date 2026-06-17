#include "hist_record_app.h"
#include "dm_flash_backend.h"
#include <string.h>
#include <stdio.h>

/*==========================================================================
 * 全局变量
 *==========================================================================*/

/** 历史记录管理器实例 */
static DM_Manager_t g_hist_mgr;

/** Flash 后端接口 */
static const DM_StorageBackend_t *g_flash_backend = NULL;

/** 当前系统时间戳 */
static uint32_t g_current_system_ts = 0;

/*==========================================================================
 * 内部函数声明
 *==========================================================================*/

static int hist_record_migrate_cache(uint16_t cache_sector_idx);
static uint16_t hist_record_scan_valid(uint32_t reference_ts);
static int hist_record_scan_and_validate(void);
static int hist_record_copy_sector_data(DM_SectorType_t src_type, uint16_t src_idx,
                                         DM_SectorType_t dst_type, uint16_t dst_idx,
                                         uint16_t start_slot, uint16_t slot_count);
static int hist_record_erase_sector_by_type(DM_SectorType_t type, uint16_t idx);
static uint16_t hist_record_count_valid_in_sector(DM_SectorType_t type, uint16_t idx, uint32_t ref_ts);
static int hist_record_read_struct(uint16_t index, HistRecord_t *out);

/*==========================================================================
 * 初始化接口实现
 *==========================================================================*/

/**
 * 初始化历史记录管理
 */
int HistRecord_Init(void)
{
    DM_DataFormat_t format;
    DM_Result_t ret;
    
    /* 初始化 Flash 后端 */
    if (DM_FlashBackend_Init() != 0) {
        HIST_DEBUG("[HIST] Flash 后端初始化失败\r\n");
        return -1;
    }
    
    g_flash_backend = DM_FlashBackend_GetInterface();
    if (g_flash_backend == NULL) {
        HIST_DEBUG("[HIST] 获取 Flash 后端接口失败\r\n");
        return -1;
    }
    
    /* 填充数据格式描述符 */
    memset(&format, 0, sizeof(format));
    format.name = "5min_history";
    format.record_size = HIST_RECORD_SIZE_BYTES;
    format.max_records = HIST_RECORD_MAX_RECORDS;
    format.storage_base = HIST_STORAGE_BASE;
    format.storage_size = HIST_STORAGE_SIZE;
    format.sector_size = HIST_SECTOR_SIZE;
    format.meta_magic = HIST_META_MAGIC;
    format.formal_sector_count = 0;  /* 0 = 自动计算 */
    format.cache_sector_count = 0;   /* 0 = 自动计算 */
    format.temp_sector_count = 0;    /* 0 = 1 */
    format.meta_sector_count = 1;
    format.validate_record = HistFormat_ValidateRecord;
    format.init_record = HistFormat_InitRecord;
    
    /* 注册到核心层 */
    ret = DM_RegisterDataManager(&format, g_flash_backend, &g_hist_mgr);
    if (ret != DM_OK) {
        HIST_DEBUG("[HIST] 注册失败：%d\r\n", ret);
        return -1;
    }
    
    /* 初始化核心层管理器 */
    ret = DM_Init(&g_hist_mgr);
    if (ret != DM_OK) {
        HIST_DEBUG("[HIST] 初始化失败：%d\r\n", ret);
        return -1;
    }
    
    /* 扫描并验证现有数据 */
    ret = hist_record_scan_and_validate();
    if (ret != 0) {
        HIST_DEBUG("[HIST] 扫描验证失败\r\n");
        return -1;
    }
    
    HIST_DEBUG("[HIST] 初始化成功：正式扇区=%u, 缓存扇区=%u, 每扇区记录=%u, 有效记录=%u\r\n",
             g_hist_mgr.formal_sector_count,
             g_hist_mgr.cache_sector_count,
             g_hist_mgr.slots_per_sector,
             g_hist_mgr.valid_count);
    
    return 0;
}

/*==========================================================================
 * 基础读写接口实现
 *==========================================================================*/

/**
 * 写入一条历史记录
 */
int HistRecord_Write(const char *datetime, uint32_t dose_uSv)
{
    HistRecord_t record;
    DM_Result_t ret;
    uint16_t cache_sector_idx;
    
    if (datetime == NULL) {
        return -1;
    }
    
    /* 转换数据格式 */
    record.unix_ts = HistFormat_DatetimeToTimestamp(datetime);
    if (record.unix_ts == 0) {
        HIST_DEBUG("[HIST] 时间戳转换失败：%s\r\n", datetime);
        return -1;
    }
    
    record.dose_uSv = dose_uSv;  /* 直接使用数值，无需转换 */
    
    /* 检查是否需要迁移缓存扇区 */
    if (g_hist_mgr.phase == DM_PHASE_CACHE) {
        cache_sector_idx = g_hist_mgr.cache_write_next / g_hist_mgr.slots_per_sector;
        if (cache_sector_idx < g_hist_mgr.cache_sector_count &&
            g_hist_mgr.cache_write_next >= (cache_sector_idx + 1) * g_hist_mgr.slots_per_sector) {
            /* 缓存扇区已满，需要迁移 */
            if (hist_record_migrate_cache(cache_sector_idx) != 0) {
                return -1;
            }
        }
    }
    
    /* 写入数据 */
    ret = DM_WriteRaw(&g_hist_mgr, (const uint8_t *)&record);
    if (ret != DM_OK) {
        HIST_DEBUG("[HIST] 写入失败：%d\r\n", ret);
        return -1;
    }
    
    /* 更新有效记录数 */
    g_hist_mgr.valid_count++;
    
    /* 定期保存元数据 */
    g_hist_mgr.writes_since_meta_save++;
    if (g_hist_mgr.writes_since_meta_save >= DM_META_SAVE_INTERVAL) {
        DM_SaveMeta(&g_hist_mgr);
        g_hist_mgr.writes_since_meta_save = 0;
    }
    
    return 0;
}

static int hist_record_read_struct(uint16_t index, HistRecord_t *out)
{
    uint32_t addr;
    uint16_t sector_idx, slot_idx;
    DM_SectorType_t sector_type;
    int ret;

    if (out == NULL) {
        return -1;
    }

    if (index >= g_hist_mgr.valid_count) {
        return -1;
    }

    if (g_hist_mgr.oldest_formal_sector < g_hist_mgr.formal_sector_count) {
        uint16_t formal_total_slots = g_hist_mgr.formal_sector_count * g_hist_mgr.slots_per_sector;

        if (index < formal_total_slots) {
            uint16_t total_index = (g_hist_mgr.oldest_formal_sector * g_hist_mgr.slots_per_sector) + index;
            sector_idx = (total_index / g_hist_mgr.slots_per_sector) % g_hist_mgr.formal_sector_count;
            slot_idx = total_index % g_hist_mgr.slots_per_sector;
            sector_type = DM_SECTOR_TYPE_FORMAL;
        } else {
            uint16_t cache_index = index - formal_total_slots;
            sector_idx = cache_index / g_hist_mgr.slots_per_sector;
            slot_idx = cache_index % g_hist_mgr.slots_per_sector;
            sector_type = DM_SECTOR_TYPE_CACHE;
        }
    } else {
        sector_idx = index / g_hist_mgr.slots_per_sector;
        slot_idx = index % g_hist_mgr.slots_per_sector;
        sector_type = DM_SECTOR_TYPE_CACHE;
    }

    addr = DM_GetSlotAddress(&g_hist_mgr, sector_type, sector_idx, slot_idx);
    if (addr == 0) {
        return -1;
    }

    ret = g_flash_backend->read(addr, (uint8_t *)out, sizeof(*out));
    if (ret != 0) {
        return -1;
    }

    if (!HistFormat_IsValid(out)) {
        return -1;
    }

    return 0;
}

/**
 * 读取一条历史记录（完整实现环形缓冲逻辑）
 */
int HistRecord_Read(
    uint16_t index,
    char *out_datetime,
    char *out_dose_value
) {
    HistRecord_t record;

    if (out_datetime == NULL || out_dose_value == NULL) {
        return -1;
    }

    if (hist_record_read_struct(index, &record) != 0) {
        return -1;
    }

    HistFormat_TimestampToDatetime(record.unix_ts, out_datetime);
    HistFormat_DoseToString(record.dose_uSv, out_dose_value);

    return 0;
}

int HistRecord_ReadRecordRaw(uint16_t index, uint32_t *out_unix_ts, float *out_dose_uSv)
{
    HistRecord_t record;

    if (out_unix_ts == NULL || out_dose_uSv == NULL) {
        return -1;
    }

    if (hist_record_read_struct(index, &record) != 0) {
        return -1;
    }

    *out_unix_ts = record.unix_ts;
    *out_dose_uSv = (float)record.dose_uSv;
    return 0;
}

/**
 * 获取有效记录数
 */
uint16_t HistRecord_GetValidCount(void)
{
    return g_hist_mgr.valid_count;
}

/**
 * 读取所有记录并打印
 */
uint16_t HistRecord_ReadAll(void)
{
    uint16_t i;
    char datetime[HIST_DATE_TIME_LEN];
    char dose_value[HIST_DOSE_VALUE_LEN];
    
    HIST_DEBUG("\r\n===== 历史记录（共 %u 条）=====\r\n", g_hist_mgr.valid_count);
    
    for (i = 0; i < g_hist_mgr.valid_count && i < DM_READALL_MAX_LINES; i++) {
        if (HistRecord_Read(i, datetime, dose_value) == 0) {
            HIST_DEBUG("[%3u] %s %s\r\n", i, datetime, dose_value);
        }
    }
    
    if (g_hist_mgr.valid_count > DM_READALL_MAX_LINES) {
        HIST_DEBUG("... 还有 %u 条未显示\r\n", g_hist_mgr.valid_count - DM_READALL_MAX_LINES);
    }
    
    HIST_DEBUG("==============================\r\n");
    
    return g_hist_mgr.valid_count;
}

/**
 * 打印单条记录
 */
int HistRecord_Print(uint16_t index)
{
    char datetime[HIST_DATE_TIME_LEN];
    char dose_value[HIST_DOSE_VALUE_LEN];
    
    if (HistRecord_Read(index, datetime, dose_value) == 0) {
        HIST_DEBUG("[%u] %s %s\r\n", index, datetime, dose_value);
        return 0;
    }
    return -1;
}

/**
 * 清空所有历史记录
 */
int HistRecord_Clear(void)
{
    DM_Result_t ret;
    
    ret = DM_Clear(&g_hist_mgr);
    if (ret != DM_OK) {
        return -1;
    }
    
    g_hist_mgr.valid_count = 0;
    g_current_system_ts = 0;
    
    HIST_DEBUG("[HIST] 已清空所有记录\r\n");
    return 0;
}

/**
 * 获取全局管理器实例
 */
DM_Manager_t* HistRecord_GetManager(void)
{
    return &g_hist_mgr;
}

/*==========================================================================
 * 内部辅助函数实现
 *==========================================================================*/

/**
 * 扇区迁移（缓存扇区满后迁移到正式扇区）
 */
static int hist_record_migrate_cache(uint16_t cache_sector_idx)
{
    uint32_t cache_addr, formal_addr;
    uint8_t buf[DM_SECTOR_SIZE];
    uint16_t i;
    int ret;
    
    if (cache_sector_idx >= g_hist_mgr.cache_sector_count) {
        return -1;
    }
    
    HIST_DEBUG("[HIST] 迁移缓存扇区 %u 到正式扇区\r\n", cache_sector_idx);
    
    /* 获取地址 */
    cache_addr = DM_GetSectorAddress(&g_hist_mgr, DM_SECTOR_TYPE_CACHE, cache_sector_idx);
    formal_addr = DM_GetSectorAddress(&g_hist_mgr, DM_SECTOR_TYPE_FORMAL, cache_sector_idx);
    
    if (cache_addr == 0 || formal_addr == 0) {
        return -1;
    }
    
    /* 读取整个缓存扇区数据 */
    ret = g_flash_backend->read(cache_addr, buf, DM_SECTOR_SIZE);
    if (ret != 0) {
        HIST_DEBUG("[HIST] 读取缓存扇区失败\r\n");
        return -1;
    }
    
    /* 擦除正式扇区 */
    ret = g_flash_backend->erase_sector(formal_addr);
    if (ret != 0) {
        HIST_DEBUG("[HIST] 擦除正式扇区失败\r\n");
        return -1;
    }
    
    /* 写入正式扇区 */
    ret = g_flash_backend->write_blank(formal_addr, buf, DM_SECTOR_SIZE);
    if (ret != 0) {
        HIST_DEBUG("[HIST] 写入正式扇区失败\r\n");
        return -1;
    }
    
    /* 擦除缓存扇区（标记为可重新使用） */
    ret = g_flash_backend->erase_sector(cache_addr);
    if (ret != 0) {
        HIST_DEBUG("[HIST] 擦除缓存扇区失败\r\n");
        return -1;
    }
    
    /* 更新管理器状态 */
    g_hist_mgr.cache_write_next = cache_sector_idx * g_hist_mgr.slots_per_sector;
    
    return 0;
}

/**
 * 擦除指定类型的扇区
 */
static int hist_record_erase_sector_by_type(DM_SectorType_t type, uint16_t idx)
{
    uint32_t addr;
    
    addr = DM_GetSectorAddress(&g_hist_mgr, type, idx);
    if (addr == 0) {
        return -1;
    }
    
    return g_flash_backend->erase_sector(addr);
}

/**
 * 复制扇区数据
 */
static int hist_record_copy_sector_data(DM_SectorType_t src_type, uint16_t src_idx,
                                         DM_SectorType_t dst_type, uint16_t dst_idx,
                                         uint16_t start_slot, uint16_t slot_count)
{
    uint32_t src_addr, dst_addr;
    uint8_t buf[DM_SECTOR_SIZE];
    int ret;
    
    src_addr = DM_GetSectorAddress(&g_hist_mgr, src_type, src_idx);
    dst_addr = DM_GetSectorAddress(&g_hist_mgr, dst_type, dst_idx);
    
    if (src_addr == 0 || dst_addr == 0) {
        return -1;
    }
    
    /* 读取源扇区数据 */
    ret = g_flash_backend->read(src_addr, buf, DM_SECTOR_SIZE);
    if (ret != 0) {
        return -1;
    }
    
    /* 擦除目标扇区 */
    ret = g_flash_backend->erase_sector(dst_addr);
    if (ret != 0) {
        return -1;
    }
    
    /* 写入目标扇区 */
    ret = g_flash_backend->write_blank(dst_addr, buf, DM_SECTOR_SIZE);
    if (ret != 0) {
        return -1;
    }
    
    return 0;
}

/**
 * 统计扇区内的有效记录数
 */
static uint16_t hist_record_count_valid_in_sector(DM_SectorType_t type, uint16_t idx, uint32_t ref_ts)
{
    uint32_t sector_addr, slot_addr;
    HistRecord_t record;
    uint16_t count = 0;
    uint16_t i;
    
    sector_addr = DM_GetSectorAddress(&g_hist_mgr, type, idx);
    if (sector_addr == 0) {
        return 0;
    }
    
    for (i = 0; i < g_hist_mgr.slots_per_sector; i++) {
        slot_addr = sector_addr + i * sizeof(HistRecord_t);
        
        if (g_flash_backend->read(slot_addr, (uint8_t *)&record, sizeof(record)) != 0) {
            continue;
        }
        
        /* 检查记录是否有效 */
        if (HistFormat_IsValid(&record) && record.unix_ts <= ref_ts) {
            count++;
        } else if (!HistFormat_IsValid(&record)) {
            /* 遇到空白记录，后面的都无效 */
            break;
        }
    }
    
    return count;
}

/*==========================================================================
 * 扫描和验证功能实现
 *==========================================================================*/

/**
 * 扫描所有扇区，计算有效记录
 */
static uint16_t hist_record_scan_valid(uint32_t reference_ts)
{
    uint16_t total_valid = 0;
    uint16_t i;
    
    /* 扫描所有正式扇区 */
    for (i = 0; i < g_hist_mgr.formal_sector_count; i++) {
        total_valid += hist_record_count_valid_in_sector(DM_SECTOR_TYPE_FORMAL, i, reference_ts);
    }
    
    /* 扫描所有缓存扇区 */
    for (i = 0; i < g_hist_mgr.cache_sector_count; i++) {
        total_valid += hist_record_count_valid_in_sector(DM_SECTOR_TYPE_CACHE, i, reference_ts);
    }
    
    return total_valid;
}

/**
 * 扫描并验证现有数据
 */
static int hist_record_scan_and_validate(void)
{
    uint16_t valid_count;
    
    /* 使用当前时间戳作为参考（默认所有记录都有效） */
    valid_count = hist_record_scan_valid(0xFFFFFFFF);
    
    g_hist_mgr.valid_count = valid_count;
    g_hist_mgr.oldest_formal_sector = 0;  /* 简化处理，实际需要根据时间戳计算 */
    
    return 0;
}

/*==========================================================================
 * 临时扇区处理功能
 *==========================================================================*/

/**
 * 处理临时扇区（时间回调时使用）
 * 严格按照 md 文档：只有覆盖当前缓存扇区时才使用临时扇区
 */
int HistRecord_ProcessTempSector(DM_SectorType_t source_type, uint16_t source_idx)
{
    uint32_t temp_addr, src_addr;
    int ret;
    
    HIST_DEBUG("[HIST] 处理临时扇区：源类型=%u, 源索引=%u\r\n", source_type, source_idx);
    
    /* 获取临时扇区地址 */
    temp_addr = DM_GetSectorAddress(&g_hist_mgr, DM_SECTOR_TYPE_TEMP, 0);
    if (temp_addr == 0) {
        return -1;
    }
    
    /* 擦除临时扇区 */
    ret = g_flash_backend->erase_sector(temp_addr);
    if (ret != 0) {
        HIST_DEBUG("[HIST] 擦除临时扇区失败\r\n");
        return -1;
    }
    
    /* 如果源扇区有有效数据，复制到临时扇区 */
    if (source_type == DM_SECTOR_TYPE_CACHE && source_idx < g_hist_mgr.cache_sector_count) {
        src_addr = DM_GetSectorAddress(&g_hist_mgr, DM_SECTOR_TYPE_CACHE, source_idx);
        if (src_addr != 0) {
            /* 复制整个扇区数据到临时扇区 */
            ret = hist_record_copy_sector_data(DM_SECTOR_TYPE_CACHE, source_idx,
                                                DM_SECTOR_TYPE_TEMP, 0, 0, 0);
            if (ret != 0) {
                HIST_DEBUG("[HIST] 复制数据到临时扇区失败\r\n");
                return -1;
            }
        }
    }
    
    /* 更新管理器状态 */
    g_hist_mgr.phase = DM_PHASE_TEMP;
    g_hist_mgr.temp_write_next = 0;
    g_hist_mgr.temp_cached_source = source_idx;
    
    HIST_DEBUG("[HIST] 临时扇区处理完成\r\n");
    return 0;
}

/*==========================================================================
 * 时间调整功能实现（严格按照 md 文档）
 *==========================================================================*/

/**
 * 时间回调处理（时间往回调整）
 * 严格按照 md 文档实现
 */
int HistRecord_AdjustTimeBackward(const char *new_datetime)
{
    uint32_t new_ts;
    uint16_t i;
    uint16_t cache_sector_idx;
    DM_SectorType_t target_type;
    uint16_t target_sector_idx;
    uint16_t target_slot;
    
    HIST_DEBUG("[HIST] 时间回调：%s\r\n", new_datetime);
    
    new_ts = HistFormat_DatetimeToTimestamp(new_datetime);
    if (new_ts == 0) {
        return -1;
    }
    
    /* 步骤 1：扫描所有扇区，确定有效范围 */
    uint16_t formal_valid_count = 0;
    uint16_t cache_valid_count = 0;
    
    for (i = 0; i < g_hist_mgr.formal_sector_count; i++) {
        formal_valid_count += hist_record_count_valid_in_sector(DM_SECTOR_TYPE_FORMAL, i, new_ts);
    }
    
    for (i = 0; i < g_hist_mgr.cache_sector_count; i++) {
        cache_valid_count += hist_record_count_valid_in_sector(DM_SECTOR_TYPE_CACHE, i, new_ts);
    }
    
    HIST_DEBUG("[HIST] 扫描结果：正式扇区有效=%u, 缓存扇区有效=%u\r\n", 
               formal_valid_count, cache_valid_count);
    
    /* 步骤 2：处理当前缓存扇区 */
    if (g_hist_mgr.phase == DM_PHASE_CACHE) {
        cache_sector_idx = g_hist_mgr.cache_write_next / g_hist_mgr.slots_per_sector;
        
        /* 检查当前缓存扇区是否有有效记录 */
        uint16_t current_cache_valid = hist_record_count_valid_in_sector(
            DM_SECTOR_TYPE_CACHE, cache_sector_idx, new_ts);
        
        if (current_cache_valid > 0) {
            /* 有有效记录，需要保留 */
            HIST_DEBUG("[HIST] 缓存扇区 %u 有 %u 条有效记录，需要保留\r\n", 
                      cache_sector_idx, current_cache_valid);
            
            /* 判断是否需要使用临时扇区 */
            /* 核心原则：只有覆盖当前缓存扇区时才使用临时扇区 */
            target_type = DM_SECTOR_TYPE_CACHE;
            target_sector_idx = cache_sector_idx;
            target_slot = 0;  /* 从第 0 条开始覆盖 */
            
            /* 使用临时扇区 */
            if (HistRecord_ProcessTempSector(DM_SECTOR_TYPE_CACHE, cache_sector_idx) != 0) {
                return -1;
            }
            
            /* 不擦除缓存扇区，时间调快时可能需要使用 */
            g_hist_mgr.cache_write_next = cache_sector_idx * g_hist_mgr.slots_per_sector;
            
        } else {
            /* 全部无效，不擦除缓存扇区（时间调快时可能需要使用） */
            HIST_DEBUG("[HIST] 缓存扇区 %u 全部无效，保持原样\r\n", cache_sector_idx);
            
            /* 判断需要覆盖的目标扇区 */
            /* 如果正式扇区是空白，实际上是在缓存扇区上覆盖 */
            target_type = DM_SECTOR_TYPE_CACHE;
            target_sector_idx = cache_sector_idx;
            target_slot = 0;
            
            /* 使用临时扇区 */
            if (HistRecord_ProcessTempSector(DM_SECTOR_TYPE_CACHE, cache_sector_idx) != 0) {
                return -1;
            }
            
            g_hist_mgr.cache_write_next = cache_sector_idx * g_hist_mgr.slots_per_sector;
        }
    }
    
    /* 步骤 3：更新有效记录数 */
    g_hist_mgr.valid_count = formal_valid_count;  /* 只计算正式扇区的有效记录 */
    
    /* 步骤 4：更新元数据 */
    g_current_system_ts = new_ts;
    DM_SaveMeta(&g_hist_mgr);
    
    HIST_DEBUG("[HIST] 时间回调完成：有效记录=%u, 当前写入位置=缓存扇区%u 第 0 条\r\n",
               g_hist_mgr.valid_count, cache_sector_idx);
    
    return 0;
}

/**
 * 时间调快处理（时间往前调整）
 * 严格按照 md 文档实现
 */
int HistRecord_AdjustTimeForward(const char *new_datetime)
{
    uint32_t new_ts;
    uint16_t i;
    uint16_t temp_valid_count = 0;
    uint16_t cache_valid_count = 0;
    
    HIST_DEBUG("[HIST] 时间调快：%s\r\n", new_datetime);
    
    new_ts = HistFormat_DatetimeToTimestamp(new_datetime);
    if (new_ts == 0) {
        return -1;
    }
    
    /* 步骤 1：扫描所有扇区 */
    /* 检查临时扇区是否有数据 */
    if (g_hist_mgr.phase == DM_PHASE_TEMP) {
        temp_valid_count = hist_record_count_valid_in_sector(DM_SECTOR_TYPE_TEMP, 0, new_ts);
        HIST_DEBUG("[HIST] 临时扇区有效记录=%u\r\n", temp_valid_count);
    }
    
    /* 检查原缓存扇区（时间调快时恢复使用） */
    if (g_hist_mgr.temp_cached_source < g_hist_mgr.cache_sector_count) {
        cache_valid_count = hist_record_count_valid_in_sector(
            DM_SECTOR_TYPE_CACHE, g_hist_mgr.temp_cached_source, new_ts);
        HIST_DEBUG("[HIST] 缓存扇区 %u 有效记录=%u（恢复使用）\r\n",
                  g_hist_mgr.temp_cached_source, cache_valid_count);
    }
    
    /* 步骤 2：恢复缓存扇区数据 */
    if (g_hist_mgr.phase == DM_PHASE_TEMP && cache_valid_count > 0) {
        /* 丢弃临时扇区数据 */
        uint32_t temp_addr = DM_GetSectorAddress(&g_hist_mgr, DM_SECTOR_TYPE_TEMP, 0);
        if (temp_addr != 0) {
            g_flash_backend->erase_sector(temp_addr);
        }
        
        /* 恢复使用原缓存扇区 */
        g_hist_mgr.phase = DM_PHASE_CACHE;
        g_hist_mgr.cache_write_next = g_hist_mgr.temp_cached_source * g_hist_mgr.slots_per_sector;
        
        /* 计算原缓存扇区的写入位置 */
        uint16_t slot_idx = 0;
        for (i = 0; i < g_hist_mgr.slots_per_sector; i++) {
            uint32_t addr = DM_GetSlotAddress(&g_hist_mgr, DM_SECTOR_TYPE_CACHE, 
                                              g_hist_mgr.temp_cached_source, i);
            HistRecord_t record;
            if (g_flash_backend->read(addr, (uint8_t*)&record, sizeof(record)) == 0) {
                if (HistFormat_IsValid(&record)) {
                    slot_idx = i + 1;
                } else {
                    break;
                }
            }
        }
        g_hist_mgr.cache_write_next = g_hist_mgr.temp_cached_source * g_hist_mgr.slots_per_sector + slot_idx;
        
        HIST_DEBUG("[HIST] 恢复缓存扇区 %u，从第 %u 条开始写入\r\n",
                  g_hist_mgr.temp_cached_source, slot_idx);
    }
    
    /* 步骤 3：重新扫描所有有效记录 */
    g_hist_mgr.valid_count = hist_record_scan_valid(new_ts);
    
    /* 步骤 4：更新元数据 */
    g_current_system_ts = new_ts;
    DM_SaveMeta(&g_hist_mgr);
    
    HIST_DEBUG("[HIST] 时间调快完成：有效记录=%u\r\n", g_hist_mgr.valid_count);
    
    return 0;
}

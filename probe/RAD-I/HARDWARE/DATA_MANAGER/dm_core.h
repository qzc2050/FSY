#ifndef __DM_CORE_H
#define __DM_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*==========================================================================
 * 数据管理核心层 - 抽象接口定义
 * 类似 dev_protocol/core 的架构，提供通用的数据管理框架
 *==========================================================================*/

/*==========================================================================
 * 类型定义
 *==========================================================================*/

/** 存储介质类型 */
typedef enum {
    DM_STORAGE_FLASH_QSPI = 0,    /* QSPI Flash */
    DM_STORAGE_FLASH_SPI,         /* SPI Flash */
    DM_STORAGE_EEPROM,            /* EEPROM */
    DM_STORAGE_FRAM,              /* FRAM */
    DM_STORAGE_CUSTOM,            /* 自定义存储 */
} DM_StorageType_t;

/** 扇区类型 */
typedef enum {
    DM_SECTOR_TYPE_FORMAL = 0,    /* 正式扇区（稳定存储） */
    DM_SECTOR_TYPE_CACHE,         /* 缓存扇区（活动写入） */
    DM_SECTOR_TYPE_TEMP,          /* 临时扇区（时间调整缓冲） */
    DM_SECTOR_TYPE_INVALID,       /* 无效扇区 */
} DM_SectorType_t;

/** 写入阶段 */
typedef enum {
    DM_PHASE_PRIMARY = 0,         /* 主数据区写入 */
    DM_PHASE_CACHE,               /* 缓存区写入 */
    DM_PHASE_TEMP,                /* 临时区写入 */
} DM_WritePhase_t;

/** 操作结果 */
typedef enum {
    DM_OK = 0,
    DM_ERR_INVALID_PARAM = -1,
    DM_ERR_STORAGE_FAILED = -2,
    DM_ERR_NO_BLANK_SLOT = -3,
    DM_ERR_DATA_CORRUPT = -4,
    DM_ERR_NOT_SUPPORTED = -5,
    DM_ERR_OUT_OF_RANGE = -6,
} DM_Result_t;

/*==========================================================================
 * 抽象层接口 - 存储后端必须实现这些接口
 *==========================================================================*/

/**
 * 存储后端接口表
 * 类似 dev_protocol 的底层抽象，支持 Flash/EEPROM/FRAM 等
 */
typedef struct {
    /** 读取数据 */
    int (*read)(uint32_t addr, uint8_t *buf, size_t len);
    
    /** 写入数据（到空白位置） */
    int (*write_blank)(uint32_t addr, const uint8_t *data, size_t len);
    
    /** 擦除扇区 */
    int (*erase_sector)(uint32_t addr);
    
    /** 获取扇区大小 */
    size_t (*get_sector_size)(void);
    
    /** 获取存储类型 */
    DM_StorageType_t (*get_storage_type)(void);
} DM_StorageBackend_t;

/*==========================================================================
 * 数据格式描述 - 用于注册不同数据类型
 *==========================================================================*/

/**
 * 数据格式描述符
 * 用于注册不同格式的数据（历史记录/配置参数/日志等）
 */
typedef struct {
    const char *name;                     /* 数据格式名称 */
    
    /* 数据大小参数 */
    uint32_t record_size;                 /* 单条记录大小（字节） */
    uint32_t max_records;                 /* 需要存储的最大记录数 */
    
    /* 存储空间参数 */
    uint32_t storage_base;                /* 数据存储空间基址 */
    uint32_t storage_size;                /* 数据存储空间总大小（字节） */
    uint32_t sector_size;                 /* 扇区大小（擦除的最小单位） */
    
    /* 扇区分配参数（可选，为 0 时自动计算） */
    uint32_t formal_sector_count;         /* 正式扇区数量（0=自动计算） */
    uint32_t cache_sector_count;          /* 缓存扇区数量（0=自动计算） */
    uint32_t temp_sector_count;           /* 临时扇区数量（0=1） */
    
    /* 元数据配置 */
    uint32_t meta_magic;                  /* 元数据魔数 */
    uint32_t meta_sector_count;           /* 元数据扇区数量（通常为 1） */
    
    /** 数据验证回调（可选） */
    bool (*validate_record)(const uint8_t *data);
    
    /** 数据初始化回调（可选） */
    void (*init_record)(uint8_t *data);
    
    /** 私有数据指针 */
    void *private_data;
} DM_DataFormat_t;

/*==========================================================================
 * 数据管理器实例 - 注册后由核心层管理
 *==========================================================================*/

/**
 * 数据管理器实例
 * 每个注册的数据类型对应一个实例
 */
typedef struct {
    /* 数据格式信息 */
    const DM_DataFormat_t *format;
    
    /* 存储后端 */
    const DM_StorageBackend_t *backend;
    
    /* 计算后的扇区布局（由 Register 自动计算） */
    uint16_t slots_per_sector;            /* 每扇区可存储记录数 */
    uint16_t formal_sector_count;         /* 正式扇区数量 */
    uint16_t cache_sector_count;          /* 缓存扇区数量 */
    uint16_t temp_sector_count;           /* 临时扇区数量 */
    uint16_t meta_sector_count;           /* 元数据扇区数量 */
    
    /* 存储基地址（由 format 中的参数计算得出） */
    uint32_t meta_base;                   /* 元数据扇区基址 */
    uint32_t records_base;                /* 正式扇区基址 */
    uint32_t cache_base;                  /* 缓存扇区基址 */
    uint32_t temp_base;                   /* 临时扇区基址 */
    
    /* 运行时状态 */
    DM_WritePhase_t phase;                /* 当前写入阶段 */
    uint16_t valid_count;                 /* 有效记录数 */
    uint16_t write_next;                  /* 下次写入位置（主区） */
    uint16_t oldest_formal_sector;        /* 最旧正式扇区索引 */
    
    /* 缓存区状态 */
    uint16_t cache_write_next;            /* 下次写入位置（缓存区） */
    uint16_t cache_valid_count;           /* 缓存区有效记录数 */
    
    /* 临时区状态 */
    uint16_t temp_write_next;             /* 下次写入位置（临时区） */
    uint16_t temp_cached_source;          /* 临时区缓存的源扇区索引 */
    
    /* 元数据管理 */
    uint32_t meta_magic;                  /* 元数据魔数 */
    uint16_t writes_since_meta_save;      /* 上次保存后的写入次数 */
    
    /* 私有数据（由应用层使用） */
    void *private_data;
} DM_Manager_t;

/*==========================================================================
 * 核心层 API - 数据管理注册和基础操作
 *==========================================================================*/

/**
 * 注册数据类型
 * @param format 数据格式描述符（包含存储空间、扇区大小、记录数量等参数）
 * @param backend 存储后端接口
 * @param out_mgr 输出的管理器实例
 * @return DM_OK 成功，其他失败
 * 
 * 核心层会自动计算：
 * - 每扇区可存储的记录数 = sector_size / record_size
 * - 需要的正式扇区数 = ceil(max_records / slots_per_sector)
 * - 缓存扇区数 = 正式扇区数（除非特别指定）
 * - 临时扇区数 = 1（除非特别指定）
 * - 验证 storage_size 是否足够 = (formal + cache + temp + meta) * sector_size
 */
DM_Result_t DM_RegisterDataManager(
    const DM_DataFormat_t *format,
    const DM_StorageBackend_t *backend,
    DM_Manager_t *out_mgr
);

/**
 * 初始化数据管理器
 * @param mgr 管理器实例
 * @return DM_OK 成功，其他失败
 */
DM_Result_t DM_Init(DM_Manager_t *mgr);

/**
 * 写入原始数据
 * @param mgr 管理器实例
 * @param data 数据指针
 * @return DM_OK 成功，其他失败
 */
DM_Result_t DM_WriteRaw(DM_Manager_t *mgr, const uint8_t *data);

/**
 * 读取原始数据
 * @param mgr 管理器实例
 * @param logical_index 逻辑索引（从 0 开始）
 * @param out_data 输出数据缓冲区
 * @return DM_OK 成功，其他失败
 */
DM_Result_t DM_ReadRaw(
    DM_Manager_t *mgr,
    uint16_t logical_index,
    uint8_t *out_data
);

/**
 * 获取有效记录数
 * @param mgr 管理器实例
 * @return 有效记录数
 */
uint16_t DM_GetValidCount(DM_Manager_t *mgr);

/**
 * 清空所有数据
 * @param mgr 管理器实例
 * @return DM_OK 成功，其他失败
 */
DM_Result_t DM_Clear(DM_Manager_t *mgr);

/**
 * 保存元数据
 * @param mgr 管理器实例
 * @return DM_OK 成功，其他失败
 */
DM_Result_t DM_SaveMeta(DM_Manager_t *mgr);

/**
 * 获取扇区地址
 * @param mgr 管理器实例
 * @param sector_type 扇区类型
 * @param sector_index 扇区索引
 * @return 扇区基址，0 表示失败
 */
uint32_t DM_GetSectorAddress(
    DM_Manager_t *mgr,
    DM_SectorType_t sector_type,
    uint16_t sector_index
);

/**
 * 获取槽位地址
 * @param mgr 管理器实例
 * @param sector_type 扇区类型
 * @param sector_index 扇区索引
 * @param slot_index 槽位索引（扇区内）
 * @return 槽位地址，0 表示失败
 */
uint32_t DM_GetSlotAddress(
    DM_Manager_t *mgr,
    DM_SectorType_t sector_type,
    uint16_t sector_index,
    uint16_t slot_index
);

/* 注意：时间调整接口属于应用层业务逻辑，不在核心层提供 */
/* 应用层应根据具体数据格式自行实现时间调整功能 */

/*==========================================================================
 * 工具函数
 *==========================================================================*/

/**
 * 检查槽位是否为空白
 * @param backend 存储后端
 * @param addr 槽位地址
 * @param record_size 记录大小
 * @return true 空白，false 非空白
 */
bool DM_IsSlotBlank(
    const DM_StorageBackend_t *backend,
    uint32_t addr,
    size_t record_size
);

/**
 * 计算每扇区槽位数
 * @param sector_size 扇区大小
 * @param record_size 记录大小
 * @return 每扇区槽位数
 */
static inline uint16_t DM_CalcSlotsPerSector(
    size_t sector_size,
    size_t record_size
) {
    return (uint16_t)(sector_size / record_size);
}

#ifdef __cplusplus
}
#endif

#endif /* __DM_CORE_H */

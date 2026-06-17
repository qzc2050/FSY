/**********************************************************************************************************
 * 文件名: dev_malloc.h
 * 概  述: 设备内存管理
 * 创建时间: 2025-08-01
 * 更新时间: 2025-08-22
 * 作  者: LYJ
 * 版  本: 1.0.0
 * Copyright (c) 2025, Mr.Liu All rights reserved.
 **********************************************************************************************************/
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "./core/dev_config.h"


#ifndef NULL
#define NULL 0
#endif

#define MEM_INVAILD_ADDR                0xFFFFFFFF

// 设备内存管理结构体
typedef struct dev_mem
{
    void (*init)(void);                 // 初始化
    float (*usage)(void);               // 内存使用率
    uint8_t *mempl;                     // 内存池
    DEV_DATA_TYPE *memtb;               // 内存管理状态表
    bool memrdy;                        // 内存管理是否就绪
}Dev_Mem_t;


extern bool dev_id[];
extern Dev_Mem_t mem_ctrl;


extern void Dev_Mem_Init(void);     // 设备内存管理初始化
extern void Dev_Mem_Set(void *src, uint8_t val, uint32_t size);    // 设备内存赋值
extern void Dev_Mem_Copy(void *des, void *src, uint32_t size);     // 设备内存复制
extern void Dev_Mem_Release(void *ptr);                    // 设备内存释放
extern void *Dev_Mem_Malloc(uint32_t size);                // 设备内存分配
extern void *Dev_Mem_Realloc(void *ptr, uint32_t size);    // 设备内存重分配
extern float Dev_Mem_Usage(void);   // 获取设备内存使用率
extern uint32_t Dev_Mem_Pool_Addr(void);
extern uint32_t Dev_Mem_Table_Addr(void);

#ifdef __cplusplus
}
#endif

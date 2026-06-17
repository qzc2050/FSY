/**********************************************************************************************************
 * 文件名: dev_config.h
 * 描  述: 设备配置文件
 * 创建时间: 2025-08-13
 * 更新时间: 2025-08-22
 * 作  者: LYJ
 * 版  本: 1.0.0
 * Copyright (c) 2025, Mr.Liu All rights reserved.
 **********************************************************************************************************/
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "./core/dev_protocol.h"


//! ---------------- ↓ 自定义头文件 ↓ ---------------- !//
#include "main.h"
//! ---------------- ↑ 自定义头文件 ↑ ---------------- !//


//! ----------------------- ↓ 设备协议使能 ↓ ----------------------- !//
#define DEV_PROTOCOL_CAN            (false)
#define DEV_PROTOCOL_SPI            (false)
#define DEV_PROTOCOL_NET            (true)
//! ----------------------- ↑ 设备协议使能 ↑ ----------------------- !//


//! ----------------------- ↓ 设备测试功能 ↓ ----------------------- !//
#define DEV_PRINTF(n,args...)       printf(n,##args)
//! ----------------------- ↑ 设备测试功能 ↑ ----------------------- !//


//! ----------------------- ↓ 设备参数配置 ↓ ----------------------- !//
#define DEV_INVAILD_ID              0xFFFF          // 无效设备描述符
#define DEV_ID_MALLOC_CNT           (12)            // 设备描述符可分配数
//! ----------------------- ↑ 设备参数配置 ↑ ----------------------- !//


//! ----------------------- ↓ 设备内存管理 ↓ ----------------------- !//
#define DEV_INSRAM                  0               // 内部SRAM
#define DEV_EXSRAM                  1               // 外扩SRAM
#define DEV_MEM_TYPE                (DEV_EXSRAM)    // 内存池类型

#define DEV_MEM_BLK_SIZE            32              // 内存块大小（单位：字节）
#define DEV_MEM_MALLOC_SIZE         (4 * 1024 * 1024)     // 最大管理内存（单位：字节）
#define DEV_MEM_TB_SIZE             (DEV_MEM_MALLOC_SIZE / DEV_MEM_BLK_SIZE)    // 内存表大小

#if (DEV_MEM_TYPE == DEV_EXSRAM)
//! 当使用EXSRAM / SDRAM时，DEV_DATA_TYPE必须使用uint32_t类型。
// 配置外扩SRAM
#define DEV_DATA_TYPE               uint32_t

    #if (__ARMCC_VERSION < 6010050) //! AC5编译器
        #define DEV_EXSRAM_ADDRESS      (0XC0000000)    // 外扩SRAM起始地址
        #if 0    // 指定地址
        #define REMAP_MEMPL_ADDRESS     __attribute__((aligned(64), at(DEV_EXSRAM_ADDRESS)))    // 内存池地址映射
        #define REMAP_MEMTB_ADDRESS     __attribute__((aligned(64), at(DEV_EXSRAM_ADDRESS + DEV_MEM_MALLOC_SIZE)))     // 内存管理表地址映射
        #else    // 指定分区
        #define REMAP_MEMPL_ADDRESS     __attribute__((section(".RAM_EX_SDRAM"), zero_init))    // 内存池分区
        #define REMAP_MEMTB_ADDRESS     __attribute__((section(".RAM_EX_SDRAM"), zero_init))    // 内存管理表分区
        #endif
    #else   //! AC6编译器（不支持at宏定义表达式）
        #define REMAP_MEMPL_ADDRESS     __attribute__(__ALIGNED(64), (section(".bss.ARM.__at_0XC0000000")))     // 内存池地址映射
        #define REMAP_MEMTB_ADDRESS     __attribute__(__ALIGNED(64), (section(".bss.ARM.__at_0XC0008000")))     // 内存池管理表地址映射
    #endif

#else
// 配置内部SRAM
#define DEV_DATA_TYPE               uint16_t      // 数据类型(uint16_t / uint32_t)（定义uint16_t -> 节省内存占用）
#define REMAP_MEMPL_ADDRESS         __attribute__((aligned(64)))    // 内存池地址映射
#define REMAP_MEMTB_ADDRESS         __attribute__((aligned(64)))    // 内存池管理表地址映射
#endif
//! ----------------------- ↑ 设备内存管理 ↑ ----------------------- !//


//! --------------------- ↓ 设备函数接口配置 ↓ --------------------- !//
// 设备定时器计数函数接口
// 形参：无
// 返回：uint32_t类型 -> 1ms计数器
#define DEV_GET_1MS_TICK_FUN()      HAL_GetTick()
//! --------------------- ↑ 设备函数接口配置 ↑ --------------------- !//


#ifdef __cplusplus
}
#endif

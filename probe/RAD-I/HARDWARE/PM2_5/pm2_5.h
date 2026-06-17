/**********************************************************************************************************
 * 文件名：pm2_5.h
 * 概  述：D5 激光颗粒物检测传感器驱动（通用平台版）
 * 创建时间：2026-04-17
 * 更新时间：2026-04-17
 * 作  者：Mr.Liu
 * 版  本：1.0.0
 * Copyright (c) 2026, Mr.Liu All rights reserved.
 **********************************************************************************************************/
#ifndef __PM2_5_H
#define __PM2_5_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**********************************************************************************************************
 *                                  硬件平台相关配置
 *  用户需要根据实际硬件平台实现以下接口
 **********************************************************************************************************/
/* 波特率：9600bps, 8N1 (8 数据位，无校验，1 停止位) */
#define PM2_5_BAUDRATE              9600

/* 数据帧配置 */
#define PM2_5_FRAME_HEADER_1        0x42    /* 帧头第 1 字节 */
#define PM2_5_FRAME_HEADER_2        0x4D    /* 帧头第 2 字节 */
#define PM2_5_FRAME_LENGTH          32      /* 固定帧长度 32 字节 */

/* 复位引脚延时 (单位：ms) */
#define PM2_5_RST_DELAY             10

/**********************************************************************************************************
 *                                  平台相关接口声明 (需要用户实现)
 **********************************************************************************************************/
/* 串口发送接口 */
int PM2_5_Platform_UartSend(const uint8_t *data, uint16_t length);

/* 串口接收接口 (中断模式) */
int PM2_5_Platform_UartReceive(uint8_t *buffer, uint16_t length);

/* 串口配置接口 */
int PM2_5_Platform_UartConfig(uint32_t baudrate);

/* 复位引脚控制接口 */
void PM2_5_Platform_ResetPin_Set(uint8_t state);  /* state: 0=复位，1=正常 */

/* 延时接口 (单位：ms) */
void PM2_5_Platform_Delay(uint32_t ms);

/* 调试打印接口 (可选) */
void PM2_5_Platform_Printf(const char *format, ...);

/**********************************************************************************************************
 *                                  数据结构定义
 **********************************************************************************************************/
/* PM2.5 数据帧结构 (基于激光散射原理传感器的通用协议) */
typedef struct {
    /* 帧头 */
    uint8_t frame_header1;        /* 帧头第 1 字节 0x42 */
    uint8_t frame_header2;        /* 帧头第 2 字节 0x4D */
    
    /* 帧长度 */
    uint16_t frame_length;        /* 帧长度 (数据 + 校验位) */
    
    /* 浓度数据 (单位：μg/m³) - 标准颗粒物 */
    uint16_t pm1_0_standard;      /* PM1.0 标准颗粒物浓度 */
    uint16_t pm2_5_standard;      /* PM2.5 标准颗粒物浓度 */
    uint16_t pm10_standard;       /* PM10 标准颗粒物浓度 */
    
    /* 大气环境下浓度 */
    uint16_t pm1_0_atmosphere;    /* PM1.0 大气环境浓度 */
    uint16_t pm2_5_atmosphere;    /* PM2.5 大气环境浓度 */
    uint16_t pm10_atmosphere;     /* PM10 大气环境浓度 */
    
    /* 颗粒个数 (单位：个/0.1L 空气) */
    uint16_t particles_0_3um;     /* 0.3μm 颗粒个数 */
    uint16_t particles_0_5um;     /* 0.5μm 颗粒个数 */
    uint16_t particles_1_0um;     /* 1.0μm 颗粒个数 */
    uint16_t particles_2_5um;     /* 2.5μm 颗粒个数 */
    uint16_t particles_5_0um;     /* 5.0μm 颗粒个数 */
    uint16_t particles_10_0um;    /* 10.0μm 颗粒个数 */
    
    /* 版本和错误码 */
    uint8_t version;              /* 版本号 */
    uint8_t error_code;           /* 错误代码 */
    
    /* 校验 */
    uint16_t checksum;            /* 校验和 (从 frame_header1 到 error_code 所有字节的累加和) */
} PM2_5_DataFrame_t;

/* PM2.5 传感器状态 */
typedef struct {
    bool is_initialized;          /* 是否已初始化 */
    bool is_data_valid;           /* 数据是否有效 */
    bool is_frame_synced;         /* 是否已帧同步 */
    
    /* 接收缓冲 */
    uint8_t rx_buffer[PM2_5_FRAME_LENGTH];
    uint16_t rx_index;            /* 当前接收索引 */
    
    /* 解析后的数据 */
    PM2_5_DataFrame_t data;       /* 最新数据帧 */
    
    /* 浓度值 (单位：μg/m³) */
    float pm1_0;                  /* PM1.0 浓度 */
    float pm2_5;                  /* PM2.5 浓度 */
    float pm10;                   /* PM10 浓度 */
    
    /* 颗粒个数 (单位：个/0.1L) */
    uint32_t particles[6];        /* 6 种粒径的颗粒个数 */
    
    /* 错误统计 */
    uint32_t frame_error_count;   /* 帧错误计数 */
    uint32_t checksum_error_count;/* 校验错误计数 */
    uint32_t valid_frame_count;   /* 有效帧计数 */
} PM2_5_Status_t;

/**********************************************************************************************************
 *                                  外部变量声明
 **********************************************************************************************************/
extern PM2_5_Status_t pm2_5_status;

/**********************************************************************************************************
 *                                  函数声明
 **********************************************************************************************************/
/* 初始化函数 */
int PM2_5_Init(void);
void PM2_5_DeInit(void);

/* 数据处理函数 */
void PM2_5_ProcessData(void);
bool PM2_5_ValidateFrame(const uint8_t *data, uint16_t length);
uint16_t PM2_5_CalculateChecksum(const uint8_t *data, uint16_t length);

/* 中断处理函数 (在 USART3_IRQHandler 中调用) */
void PM2_5_ProcessRxByte(uint8_t data);
void PM2_5_WriteRingBuffer(uint8_t data);

/* 数据获取函数 */
float PM2_5_GetPM1_0(void);
float PM2_5_GetPM2_5(void);
float PM2_5_GetPM10(void);
uint32_t PM2_5_GetParticles(uint8_t size_index);
bool PM2_5_IsDataValid(void);

/* 复位控制 */
void PM2_5_Reset(void);
void PM2_5_Enable(void);
void PM2_5_Disable(void);

/* 应用层接口函数 */
int PM2_5_ReadData(float *pm1_0, float *pm2_5, float *pm10);
void PM2_5_PrintStatus(void);

/* 应用层初始化与数据处理 */
void PM2_5_App_Init(void);
void PM2_5_App_Process(void);
float PM2_5_App_GetPM2_5(void);
float PM2_5_App_GetPM1_0(void);
float PM2_5_App_GetPM10(void);

#endif /* __PM2_5_H */

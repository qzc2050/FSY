/**********************************************************************************************************
 * 文件名：pm2_5.c
 * 概  述：D5 激光颗粒物检测传感器驱动程序（通用平台版）
 * 创建时间：2026-04-17
 * 更新时间：2026-04-17
 * 作  者：Mr.Liu
 * 版  本：1.0.0
 * Copyright (c) 2026, Mr.Liu All rights reserved.
 **********************************************************************************************************/
/**
 * @file pm2_5.c
 * @brief 通用平台版 PM2.5 传感器驱动
 * 
 * @note 本驱动已抽象出平台相关接口，用户需要根据实际硬件平台实现以下接口：
 *       - PM2_5_Platform_UartSend()     : 串口发送
 *       - PM2_5_Platform_UartReceive()  : 串口接收 (DMA 模式)
 *       - PM2_5_Platform_UartConfig()   : 串口配置
 *       - PM2_5_Platform_ResetPin_Set() : 复位引脚控制
 *       - PM2_5_Platform_Delay()        : 延时函数
 *       - PM2_5_Platform_Printf()       : 调试打印 (可选)
 *       - PM2_5_Platform_GetDmaCounter(): DMA 计数器 (可选)
 * 
 * @note 参考实现请参考 HARDWARE/PM2_5/pm2_5_bsp.c 文件
 */
#include "pm2_5.h"
#include "main.h"
#include "usart.h"
#include <string.h>
#include <stdio.h>

/**********************************************************************************************************
 *                                  平台相关接口实现 (STM32)
 **********************************************************************************************************/

int PM2_5_Platform_UartSend(const uint8_t *data, uint16_t length)
{
    if (HAL_UART_Transmit(&huart3, (uint8_t *)data, length, 1000) == HAL_OK) {
        return 0;
    }
    return -1;
}

int PM2_5_Platform_UartReceive(uint8_t *buffer, uint16_t length)
{
    /* 中断方式不需要主动启动接收，在 USART3_IRQHandler 中自动接收 */
    (void)buffer;
    (void)length;
    return 0;
}

int PM2_5_Platform_UartConfig(uint32_t baudrate)
{
    printf("[PM2.5] UART Config: %lu bps\r\n", (unsigned long)baudrate);
    
    huart3.Init.BaudRate = baudrate;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    
    if (HAL_UART_Init(&huart3) == HAL_OK) {
        return 0;
    }
    return -1;
}

void PM2_5_Platform_ResetPin_Set(uint8_t state)
{
    /* 复位引脚：PH10，低电平复位，高电平正常工作 */
    if(state == 0) {
        /* 复位状态：拉低 */
        HAL_GPIO_WritePin(PM25_RST_GPIO_Port, PM25_RST_Pin, GPIO_PIN_RESET);
    } else {
        /* 正常工作：拉高 */
        HAL_GPIO_WritePin(PM25_RST_GPIO_Port, PM25_RST_Pin, GPIO_PIN_SET);
    }
}

void PM2_5_Platform_Delay(uint32_t ms)
{
    HAL_Delay(ms);
}

void PM2_5_Platform_Printf(const char *format, ...)
{
    /* 已禁用调试打印 */
    (void)format;
}

uint16_t PM2_5_Platform_GetDmaCounter(void)
{
    /* 中断方式不需要此函数，返回 0 */
    return 0;
}

/**********************************************************************************************************
 *                                  环形缓冲区定义
 **********************************************************************************************************/
#define PM2_5_RING_BUFFER_SIZE      128         /* 环形缓冲区大小 (至少 2 帧) */
static uint8_t pm2_5_ring_buffer[PM2_5_RING_BUFFER_SIZE] = {0};
static volatile uint16_t pm2_5_ring_write_idx = 0;  /* 写索引 (中断中更新) */
static volatile uint16_t pm2_5_ring_read_idx = 0;   /* 读索引 (任务中更新) */

/**********************************************************************************************************
 *                                  环形缓冲区操作函数
 **********************************************************************************************************/
/**
 * @brief 从环形缓冲区读取一个字节
 * @return 读取的字节，缓冲区空时返回 0xFF
 */
static bool PM2_5_ReadRingBuffer(uint8_t *data)
{
    if (pm2_5_ring_read_idx == pm2_5_ring_write_idx) {
        return false;  /* 缓冲区空 */
    }
    
    *data = pm2_5_ring_buffer[pm2_5_ring_read_idx];
    pm2_5_ring_read_idx++;
    if (pm2_5_ring_read_idx >= PM2_5_RING_BUFFER_SIZE) {
        pm2_5_ring_read_idx = 0;
    }
    return true;
}

/**
 * @brief 向环形缓冲区写入一个字节 (中断中调用)
 * @param data: 要写入的字节
 */
void PM2_5_WriteRingBuffer(uint8_t data)
{
    uint16_t next_write_idx = pm2_5_ring_write_idx + 1;
    if (next_write_idx >= PM2_5_RING_BUFFER_SIZE) {
        next_write_idx = 0;
    }
    
    /* 如果缓冲区满，丢弃最旧的数据 */
    if (next_write_idx == pm2_5_ring_read_idx) {
        pm2_5_ring_read_idx++;
        if (pm2_5_ring_read_idx >= PM2_5_RING_BUFFER_SIZE) {
            pm2_5_ring_read_idx = 0;
        }
    }
    
    pm2_5_ring_buffer[pm2_5_ring_write_idx] = data;
    pm2_5_ring_write_idx = next_write_idx;
}

/**
 * @brief PM2.5 串口接收字节处理函数 (在 USART3_IRQHandler 中调用)
 * @param data: 接收到的字节
 */
void PM2_5_ProcessRxByte(uint8_t data)
{
    /* 直接将字节写入环形缓冲区，由 PM2_5_ProcessData() 处理 */
    PM2_5_WriteRingBuffer(data);
}

/**
 * @brief 帧头同步处理 (检测到新帧头时调用)
 * @return 是否需要丢弃当前正在接收的帧
 */
bool PM2_5_CheckFrameSync(uint8_t data)
{
    /* 环形缓冲区模式下不需要此函数 */
    (void)data;
    return false;
}

/**********************************************************************************************************
 *                                  全局变量
 **********************************************************************************************************/
PM2_5_Status_t pm2_5_status = {0};

/* 应用层配置 */
#define PM2_5_HISTORY_SIZE          5           /* 5 点移动平均 */
#define PM2_5_MAX_VALID_VALUE       1000.0f     /* 最大有效值 (μg/m³) */
#define PM2_5_JUMP_THRESHOLD        50.0f       /* 跳变阈值 (μg/m³) */

/* 应用层变量 */
static float pm2_5_history[PM2_5_HISTORY_SIZE] = {0};
static uint8_t pm2_5_history_index = 0;
static bool pm2_5_history_full = false;
static float last_valid_pm2_5 = 0.0f;
static float last_valid_pm1_0 = 0.0f;
static float last_valid_pm10 = 0.0f;
static uint8_t data_quality_flag = 0;

/**********************************************************************************************************
 *                                  内部函数声明
 **********************************************************************************************************/
static void PM2_5_ParseDataFrameFromBuffer(const uint8_t *buf);
static void PM2_5_ResetStateMachine(void);
static float PM2_5_ApplyMovingAverage(float new_value);
static bool PM2_5_ValidateDataQuality(float current, float previous);

/**********************************************************************************************************
 *                                  初始化函数
 **********************************************************************************************************/
/********************************************************************************************
 * 函数名：PM2_5_Init
 * 描  述：PM2.5 传感器初始化
 * 输  入：无
 * 输  出：@retval: 0 -> 成功；-1 -> 失败
 * 调  用：外部调用
 ********************************************************************************************/
int PM2_5_Init(void)
{
    int ret = 0;
    
    MX_USART3_UART_Init();
    
    /* 复位状态机 */
    PM2_5_ResetStateMachine();
    
    /* 复位 PM2.5 传感器硬件 */
    PM2_5_Platform_Printf("[PM2.5] 开始硬件复位...\r\n");
    PM2_5_Reset();
    PM2_5_Platform_Delay(100);  // 复位保持 100ms
    
    /* 退出复位 */
    PM2_5_Platform_ResetPin_Set(1);
    PM2_5_Platform_Delay(100);  // 等待传感器启动
    
//    /* 使能传感器 */
//    PM2_5_Enable();
    
    /* 配置串口波特率为 9600 */
    ret = PM2_5_Platform_UartConfig(PM2_5_BAUDRATE);
    if (ret != 0) {
        PM2_5_Platform_Printf("[PM2.5] 串口配置失败\r\n");
        return -1;
    }
    
    /* 中断方式接收，不需要启动 DMA */
    /* 串口中断已在 usart.c 中使能 */
    
    pm2_5_status.is_initialized = true;
    PM2_5_Platform_Printf("[PM2.5] 初始化成功 (波特率：%d, 帧长：%d 字节)\r\n", 
            PM2_5_BAUDRATE, PM2_5_FRAME_LENGTH);
    
    return 0;
}

/********************************************************************************************
 * 函数名：PM2_5_DeInit
 * 描  述：PM2.5 传感器反初始化
 * 输  入：无
 * 输  出：无
 * 调  用：外部调用
 ********************************************************************************************/
void PM2_5_DeInit(void)
{
    /* 停止 DMA 接收 */
    /* 用户需要在 BSP 中实现停止 DMA 的功能 */
    
    /* 禁用传感器 */
    PM2_5_Disable();
    
    /* 复位状态 */
    PM2_5_ResetStateMachine();
    pm2_5_status.is_initialized = false;
    
    PM2_5_Platform_Printf("[PM2.5] 已反初始化\r\n");
}

/**********************************************************************************************************
 *                                  复位与控制函数
 **********************************************************************************************************/
/********************************************************************************************
 * 函数名：PM2_5_Reset
 * 描  述：复位 PM2.5 传感器
 * 输  入：无
 * 输  出：无
 * 调  用：外部调用
 ********************************************************************************************/
void PM2_5_Reset(void)
{
    /* 拉低复位引脚 */
    PM2_5_Platform_ResetPin_Set(0);
    PM2_5_Platform_Delay(PM2_5_RST_DELAY);
    
    /* 释放复位 */
    PM2_5_Platform_ResetPin_Set(1);
    PM2_5_Platform_Delay(100);
    
    PM2_5_Platform_Printf("[PM2.5] 硬件复位完成\r\n");
}

/********************************************************************************************
 * 函数名：PM2_5_Enable
 * 描  述：使能 PM2.5 传感器
 * 输  入：无
 * 输  出：无
 * 调  用：外部调用
 ********************************************************************************************/
void PM2_5_Enable(void)
{
    /* 某些传感器需要使能引脚，这里根据实际硬件连接实现 */
    /* 如果没有使能引脚，可以留空或控制电源 */
//    PM2_5_Platform_Printf("[PM2.5] 传感器已使能\r\n");
}

/********************************************************************************************
 * 函数名：PM2_5_Disable
 * 描  述：禁用 PM2.5 传感器
 * 输  入：无
 * 输  出：无
 * 调  用：外部调用
 ********************************************************************************************/
void PM2_5_Disable(void)
{
    /* 禁用传感器 (如果需要) */
    PM2_5_Platform_Printf("[PM2.5] 传感器已禁用\r\n");
}

/**********************************************************************************************************
 *                                  数据处理函数
 **********************************************************************************************************/
/********************************************************************************************
 * 函数名：PM2_5_ProcessData
 * 描  述：PM2.5 数据处理主函数 (周期性调用，从环形缓冲区查找并解析帧)
 * 输  入：无
 * 输  出：无
 * 调  用：外部调用
 ********************************************************************************************/
void PM2_5_ProcessData(void)
{
    static uint8_t frame_buffer[PM2_5_FRAME_LENGTH];
    static uint8_t search_state = 0;  /* 0: 查找帧头 1: 已收到 0x42 */
    uint8_t data;
    
    if (!pm2_5_status.is_initialized) {
        return;
    }
    
    /* 从环形缓冲区读取数据并查找帧 */
    while (PM2_5_ReadRingBuffer(&data)) {
        /* 调试打印：显示接收到的每个字节 */
        // printf("%02X ", data);
        switch (search_state) {
            case 0:  /* 查找帧头第一个字节 0x42 */
                if (data == PM2_5_FRAME_HEADER_1) {
                    frame_buffer[0] = data;
                    search_state = 1;
                }
                break;
                
            case 1:  /* 查找帧头第二个字节 0x4D */
                if (data == PM2_5_FRAME_HEADER_2) {
                    frame_buffer[1] = data;
                    search_state = 2;
                } else if (data != PM2_5_FRAME_HEADER_1) {
                    /* 不是帧头，重新查找 */
                    search_state = 0;
                }
                /* 如果又是 0x42，保持在状态 1 */
                break;
                
            default:  /* 接收剩余数据 */
                frame_buffer[search_state++] = data;
                
                /* 检查是否接收完一帧 */
                if (search_state >= PM2_5_FRAME_LENGTH) {
                    /* 调试打印：显示接收到的完整帧 */
                    // printf("\r\n[PM2.5] Frame: ");
                    // for (int i = 0; i < PM2_5_FRAME_LENGTH; i++) {
                    //     printf("%02X ", frame_buffer[i]);
                    // }
                    // printf("\r\n");
                    
                    /* 验证并解析帧 */
                    if (PM2_5_ValidateFrame(frame_buffer, PM2_5_FRAME_LENGTH)) {
                        /* 直接解析 frame_buffer，不复制 */
                        PM2_5_ParseDataFrameFromBuffer(frame_buffer);
                        pm2_5_status.valid_frame_count++;
                        pm2_5_status.is_data_valid = true;
                        
//                        printf("[PM2.5] Valid! raw=%d\r\n", pm2_5_status.data.pm2_5_atmosphere);
                        // printf("    Standard: PM1.0=%d, PM2.5=%d, PM10=%d\r\n",
                        //         pm2_5_status.data.pm1_0_standard, pm2_5_status.data.pm2_5_standard, pm2_5_status.data.pm10_standard);
                        // printf("    Atmosphere: PM1.0=%d, PM2.5=%d, PM10=%d\r\n",
                        //         pm2_5_status.data.pm1_0_atmosphere, pm2_5_status.data.pm2_5_atmosphere, pm2_5_status.data.pm10_atmosphere);
                        // printf("    Particles: 0.3=%d, 0.5=%d, 1.0=%d, 2.5=%d, 5.0=%d, 10.0=%d\r\n",
                        //         pm2_5_status.data.particles_0_3um, pm2_5_status.data.particles_0_5um, pm2_5_status.data.particles_1_0um,
                        //         pm2_5_status.data.particles_2_5um, pm2_5_status.data.particles_5_0um, pm2_5_status.data.particles_10_0um);
                        // printf("    Final PM2.5=%.1f ug/m3\r\n", pm2_5_status.pm2_5);
                    } else {
                        pm2_5_status.frame_error_count++;
                        pm2_5_status.is_data_valid = false;
                        printf("[PM2.5] Invalid frame!\r\n");
                    }
                    search_state = 0;  /* 重新查找下一帧 */
                }
                break;
        }
    }
}

/********************************************************************************************
 * 函数名：PM2_5_ParseDataFrameFromBuffer
 * 描  述：直接从缓冲区解析数据帧
 * 输  入：@param: *buf -> 数据缓冲区
 * 输  出：无
 * 调  用：内部调用
 ********************************************************************************************/
static void PM2_5_ParseDataFrameFromBuffer(const uint8_t *buf)
{
    PM2_5_DataFrame_t *data = &pm2_5_status.data;
    
    /* 按大端格式解析数据 (网络字节序) */
    data->frame_header1 = buf[0];
    data->frame_header2 = buf[1];
    data->frame_length = (uint16_t)((buf[2] << 8) | buf[3]);
    
    /* 浓度数据 (标准颗粒物) */
    data->pm1_0_standard = (uint16_t)((buf[4] << 8) | buf[5]);
    data->pm2_5_standard = (uint16_t)((buf[6] << 8) | buf[7]);
    data->pm10_standard = (uint16_t)((buf[8] << 8) | buf[9]);
    
    /* 大气环境下浓度 */
    data->pm1_0_atmosphere = (uint16_t)((buf[10] << 8) | buf[11]);
    data->pm2_5_atmosphere = (uint16_t)((buf[12] << 8) | buf[13]);
    data->pm10_atmosphere = (uint16_t)((buf[14] << 8) | buf[15]);
    
    /* 颗粒个数 */
    data->particles_0_3um = (uint16_t)((buf[16] << 8) | buf[17]);
    data->particles_0_5um = (uint16_t)((buf[18] << 8) | buf[19]);
    data->particles_1_0um = (uint16_t)((buf[20] << 8) | buf[21]);
    data->particles_2_5um = (uint16_t)((buf[22] << 8) | buf[23]);
    data->particles_5_0um = (uint16_t)((buf[24] << 8) | buf[25]);
    data->particles_10_0um = (uint16_t)((buf[26] << 8) | buf[27]);
    
    /* 版本和错误码 */
    data->version = buf[28];
    data->error_code = buf[29];
    
    /* 校验和 */
    data->checksum = (uint16_t)((buf[30] << 8) | buf[31]);
    
    /* 更新状态变量 - 使用大气环境数据 */
    pm2_5_status.pm1_0 = (float)data->pm1_0_atmosphere;
    pm2_5_status.pm2_5 = (float)data->pm2_5_atmosphere;
    pm2_5_status.pm10 = (float)data->pm10_atmosphere;
    
    pm2_5_status.particles[0] = data->particles_0_3um;
    pm2_5_status.particles[1] = data->particles_0_5um;
    pm2_5_status.particles[2] = data->particles_1_0um;
    pm2_5_status.particles[3] = data->particles_2_5um;
    pm2_5_status.particles[4] = data->particles_5_0um;
    pm2_5_status.particles[5] = data->particles_10_0um;
}

/********************************************************************************************
 * 函数名：PM2_5_ValidateFrame
 * 描  述：验证数据帧
 * 输  入：@param: *data -> 数据指针；@param: length -> 数据长度
 * 输  出：@retval: true -> 验证通过；false -> 验证失败
 * 调  用：内部调用
 ********************************************************************************************/
bool PM2_5_ValidateFrame(const uint8_t *data, uint16_t length)
{
    if ((data == NULL) || (length != PM2_5_FRAME_LENGTH)) {
        return false;
    }
    
    /* 检查帧头 (必须是 0x42 0x4D) */
    if ((data[0] != PM2_5_FRAME_HEADER_1) || (data[1] != PM2_5_FRAME_HEADER_2)) {
        return false;
    }
    
    /* 检查帧长度 */
    uint16_t frame_len = (uint16_t)((data[2] << 8) | data[3]);
    if (frame_len != (PM2_5_FRAME_LENGTH - 4)) {
        return false;
    }
    
    /* 计算校验和 */
    uint16_t calc_checksum = PM2_5_CalculateChecksum(data, length - 2);
    uint16_t recv_checksum = (uint16_t)((data[length - 2] << 8) | data[length - 1]);
    
    if (calc_checksum != recv_checksum) {
        pm2_5_status.checksum_error_count++;
        return false;
    }
    
    return true;
}

/********************************************************************************************
 * 函数名：PM2_5_CalculateChecksum
 * 描  述：计算校验和
 * 输  入：@param: *data -> 数据指针；@param: length -> 数据长度
 * 输  出：@retval: 校验和
 * 调  用：内部调用
 ********************************************************************************************/
uint16_t PM2_5_CalculateChecksum(const uint8_t *data, uint16_t length)
{
    uint16_t checksum = 0;
    
    for (uint16_t i = 0; i < length; i++) {
        checksum += data[i];
    }
    
    return checksum;
}

/********************************************************************************************
 * 函数名：PM2_5_ResetStateMachine
 * 描  述：复位状态机
 * 输  入：无
 * 输  出：无
 * 调  用：内部调用
 ********************************************************************************************/
static void PM2_5_ResetStateMachine(void)
{
    memset(&pm2_5_status, 0, sizeof(PM2_5_Status_t));
    pm2_5_status.is_frame_synced = false;
    pm2_5_status.is_data_valid = false;
}

/**********************************************************************************************************
 *                                  数据获取函数
 **********************************************************************************************************/
/********************************************************************************************
 * 函数名：PM2_5_GetPM1_0
 * 描  述：获取 PM1.0 浓度
 * 输  入：无
 * 输  出：@retval: PM1.0 浓度值 (μg/m³)
 * 调  用：外部调用
 ********************************************************************************************/
float PM2_5_GetPM1_0(void)
{
    if (!pm2_5_status.is_data_valid) {
        return -1.0f;
    }
    return pm2_5_status.pm1_0;
}

/********************************************************************************************
 * 函数名：PM2_5_GetPM2_5
 * 描  述：获取 PM2.5 浓度
 * 输  入：无
 * 输  出：@retval: PM2.5 浓度值 (μg/m³)
 * 调  用：外部调用
 ********************************************************************************************/
float PM2_5_GetPM2_5(void)
{
    if (!pm2_5_status.is_data_valid) {
        return -1.0f;
    }
    return pm2_5_status.pm2_5;
}

/********************************************************************************************
 * 函数名：PM2_5_GetPM10
 * 描  述：获取 PM10 浓度
 * 输  入：无
 * 输  出：@retval: PM10 浓度值 (μg/m³)
 * 调  用：外部调用
 ********************************************************************************************/
float PM2_5_GetPM10(void)
{
    if (!pm2_5_status.is_data_valid) {
        return -1.0f;
    }
    return pm2_5_status.pm10;
}

/********************************************************************************************
 * 函数名：PM2_5_GetParticles
 * 描  述：获取指定粒径的颗粒个数
 * 输  入：@param: size_index -> 粒径索引 (0:0.3μm, 1:0.5μm, ..., 5:10.0μm)
 * 输  出：@retval: 颗粒个数 (个/0.1L)
 * 调  用：外部调用
 ********************************************************************************************/
uint32_t PM2_5_GetParticles(uint8_t size_index)
{
    if (!pm2_5_status.is_data_valid || (size_index >= 6)) {
        return 0;
    }
    return pm2_5_status.particles[size_index];
}

/********************************************************************************************
 * 函数名：PM2_5_IsDataValid
 * 描  述：检查数据是否有效
 * 输  入：无
 * 输  出：@retval: true -> 数据有效；false -> 数据无效
 * 调  用：外部调用
 ********************************************************************************************/
bool PM2_5_IsDataValid(void)
{
    return pm2_5_status.is_data_valid;
}

/********************************************************************************************
 * 函数名：PM2_5_DMA_RX_Callback
 * 描  述：DMA 接收完成回调
 * 输  入：无
 * 输  出：无
 * 调  用：外部回调
 ********************************************************************************************/
void PM2_5_DMA_RX_Callback(void)
{
    /* 空实现，数据已在 PM2_5_ProcessData 中通过 DMA 计数器处理 */
    /* 用户可以在这里添加自定义处理逻辑 */
}

/**********************************************************************************************************
 *                                  应用层接口函数
 **********************************************************************************************************/
/********************************************************************************************
 * 函数名：PM2_5_ReadData
 * 描  述：PM2.5 数据读取 (应用层调用)
 * 输  入：@param: *pm1_0 -> PM1.0 浓度指针；@param: *pm2_5 -> PM2.5 浓度指针；
 *         @param: *pm10 -> PM10 浓度指针
 * 输  出：@retval: 0 -> 成功；-1 -> 失败
 * 调  用：外部调用
 ********************************************************************************************/
int PM2_5_ReadData(float *pm1_0, float *pm2_5, float *pm10)
{
    if (!pm2_5_status.is_initialized || !pm2_5_status.is_data_valid) {
        return -1;
    }
    
    if (pm1_0 != NULL) {
        *pm1_0 = pm2_5_status.pm1_0;
    }
    if (pm2_5 != NULL) {
        *pm2_5 = pm2_5_status.pm2_5;
    }
    if (pm10 != NULL) {
        *pm10 = pm2_5_status.pm10;
    }
    
    return 0;
}

/********************************************************************************************
 * 函数名：PM2_5_PrintStatus
 * 描  述：PM2.5 状态信息打印
 * 输  入：无
 * 输  出：无
 * 调  用：外部调用
 ********************************************************************************************/
void PM2_5_PrintStatus(void)
{
    if (!pm2_5_status.is_initialized) {
        PM2_5_Platform_Printf("[PM2.5] 传感器未初始化\r\n");
        return;
    }
    
    PM2_5_Platform_Printf("\r\n========== PM2.5 传感器状态 ==========\r\n");
    PM2_5_Platform_Printf("初始化状态：%s\r\n", pm2_5_status.is_initialized ? "已初始化" : "未初始化");
    PM2_5_Platform_Printf("数据有效性：%s\r\n", pm2_5_status.is_data_valid ? "有效" : "无效");
    PM2_5_Platform_Printf("有效帧计数：%lu\r\n", (unsigned long)pm2_5_status.valid_frame_count);
    PM2_5_Platform_Printf("帧错误计数：%lu\r\n", (unsigned long)pm2_5_status.frame_error_count);
    PM2_5_Platform_Printf("校验错误计数：%lu\r\n", (unsigned long)pm2_5_status.checksum_error_count);
    
    if (pm2_5_status.is_data_valid) {
        PM2_5_Platform_Printf("\r\n--- 实时数据 ---\r\n");
        PM2_5_Platform_Printf("PM1.0:  %.1f μg/m³\r\n", (double)pm2_5_status.pm1_0);
        PM2_5_Platform_Printf("PM2.5:  %.1f μg/m³\r\n", (double)pm2_5_status.pm2_5);
        PM2_5_Platform_Printf("PM10:   %.1f μg/m³\r\n", (double)pm2_5_status.pm10);
        
        PM2_5_Platform_Printf("\r\n--- 颗粒个数 (个/0.1L) ---\r\n");
        PM2_5_Platform_Printf("≥0.3μm: %lu\r\n", (unsigned long)pm2_5_status.particles[0]);
        PM2_5_Platform_Printf("≥0.5μm: %lu\r\n", (unsigned long)pm2_5_status.particles[1]);
        PM2_5_Platform_Printf("≥1.0μm: %lu\r\n", (unsigned long)pm2_5_status.particles[2]);
        PM2_5_Platform_Printf("≥2.5μm: %lu\r\n", (unsigned long)pm2_5_status.particles[3]);
        PM2_5_Platform_Printf("≥5.0μm: %lu\r\n", (unsigned long)pm2_5_status.particles[4]);
        PM2_5_Platform_Printf("≥10.0μm:%lu\r\n", (unsigned long)pm2_5_status.particles[5]);
    }
    PM2_5_Platform_Printf("======================================\r\n\r\n");
}

/**********************************************************************************************************
 *                                  应用层功能函数
 **********************************************************************************************************/
/********************************************************************************************
 * 函数名：PM2_5_ApplyMovingAverage
 * 描  述：应用移动平均滤波
 * 输  入：@param: new_value -> 新测量值
 * 输  出：@retval: 滤波后的值
 * 调  用：内部调用
 ********************************************************************************************/
static float PM2_5_ApplyMovingAverage(float new_value)
{
    static float sum = 0.0f;
    
    /* 移除旧值 */
    if (pm2_5_history_full) {
        sum -= pm2_5_history[pm2_5_history_index];
    }
    
    /* 添加新值 */
    pm2_5_history[pm2_5_history_index] = new_value;
    sum += new_value;
    
    /* 更新索引 */
    pm2_5_history_index++;
    if (pm2_5_history_index >= PM2_5_HISTORY_SIZE) {
        pm2_5_history_index = 0;
        pm2_5_history_full = true;
    }
    
    /* 计算平均值 */
    if (pm2_5_history_full) {
        return sum / PM2_5_HISTORY_SIZE;
    } else {
        return sum / pm2_5_history_index;
    }
}

/********************************************************************************************
 * 函数名：PM2_5_ValidateDataQuality
 * 描  述：验证数据质量
 * 输  入：@param: current -> 当前值；@param: previous -> 上次值
 * 输  出：@retval: true -> 数据有效；false -> 数据无效
 * 调  用：内部调用
 ********************************************************************************************/
static bool PM2_5_ValidateDataQuality(float current, float previous)
{
    float diff;
    
    /* 检查是否超出合理范围 */
    if ((current < 0.0f) || (current > PM2_5_MAX_VALID_VALUE)) {
        return false;
    }
    
    /* 检查跳变 */
    diff = (current > previous) ? (current - previous) : (previous - current);
    if (diff > PM2_5_JUMP_THRESHOLD) {
        data_quality_flag++;
        /* 连续多次跳变过大才判定为无效 */
        if (data_quality_flag < 3) {
            return true;
        }
        return false;
    }
    
    data_quality_flag = 0;
    return true;
}

/********************************************************************************************
 * 函数名：PM2_5_App_Init
 * 描  述：PM2.5 应用层初始化
 * 输  入：无
 * 输  出：无
 * 调  用：外部调用
 ********************************************************************************************/
void PM2_5_App_Init(void)
{
    /* 初始化底层驱动 */
    PM2_5_Init();
    
    /* 初始化历史数据 */
    for (uint8_t i = 0; i < PM2_5_HISTORY_SIZE; i++) {
        pm2_5_history[i] = 0.0f;
    }
    pm2_5_history_index = 0;
    pm2_5_history_full = false;
    
    PM2_5_Platform_Printf("[PM2.5-APP] 应用层初始化完成\r\n");
}

/********************************************************************************************
 * 函数名：PM2_5_App_Process
 * 描  述：PM2.5 数据处理 (应用层周期性调用)
 * 输  入：无
 * 输  出：无
 * 调  用：外部调用
 ********************************************************************************************/
void PM2_5_App_Process(void)
{
    float pm1_0, pm2_5, pm10;
    
    /* 处理 DMA 接收到的数据 */
    PM2_5_ProcessData();
    
    /* 如果数据有效，更新环境变量 */
    if (PM2_5_IsDataValid()) {
        /* 读取数据 */
        if (PM2_5_ReadData(&pm1_0, &pm2_5, &pm10) == 0) {
            /* 应用移动平均滤波 */
            pm2_5 = PM2_5_ApplyMovingAverage(pm2_5);
            
            /* 数据质量验证 */
            if (PM2_5_ValidateDataQuality(pm2_5, last_valid_pm2_5)) {
                /* 更新最后有效值 */
                last_valid_pm2_5 = pm2_5;
                last_valid_pm1_0 = pm1_0;
                last_valid_pm10 = pm10;
            }
        }
    }
}

/********************************************************************************************
 * 函数名：PM2_5_App_GetPM2_5
 * 描  述：获取 PM2.5 浓度 (应用层调用)
 * 输  入：无
 * 输  出：@retval: PM2.5 浓度值 (μg/m³)，无效返回 -1
 * 调  用：外部调用
 ********************************************************************************************/
float PM2_5_App_GetPM2_5(void)
{
    if (!PM2_5_IsDataValid()) {
        return -1.0f;
    }
    return last_valid_pm2_5;
}

/********************************************************************************************
 * 函数名：PM2_5_App_GetPM1_0
 * 描  述：获取 PM1.0 浓度 (应用层调用)
 * 输  入：无
 * 输  出：@retval: PM1.0 浓度值 (μg/m³)，无效返回 -1
 * 调  用：外部调用
 ********************************************************************************************/
float PM2_5_App_GetPM1_0(void)
{
    if (!PM2_5_IsDataValid()) {
        return -1.0f;
    }
    return last_valid_pm1_0;
}

/********************************************************************************************
 * 函数名：PM2_5_App_GetPM10
 * 描  述：获取 PM10 浓度 (应用层调用)
 * 输  入：无
 * 输  出：@retval: PM10 浓度值 (μg/m³)，无效返回 -1
 * 调  用：外部调用
 ********************************************************************************************/
float PM2_5_App_GetPM10(void)
{
    if (!PM2_5_IsDataValid()) {
        return -1.0f;
    }
    return last_valid_pm10;
}

/********************************************************************************************
 * 函数名：PM2_5_App_GetAirQualityLevel
 * 描  述：获取空气质量等级 (基于 PM2.5)
 * 输  入：无
 * 输  出：@retval: 空气质量等级 (1-6 级，0 表示无效)
 * 调  用：外部调用
 ********************************************************************************************/
uint8_t PM2_5_App_GetAirQualityLevel(void)
{
    float pm2_5 = PM2_5_App_GetPM2_5();
    
    if (pm2_5 < 0.0f) {
        return 0;
    }
    
    if (pm2_5 <= 35.0f) {
        return 1;  /* 优 */
    } else if (pm2_5 <= 75.0f) {
        return 2;  /* 良 */
    } else if (pm2_5 <= 115.0f) {
        return 3;  /* 轻度污染 */
    } else if (pm2_5 <= 150.0f) {
        return 4;  /* 中度污染 */
    } else if (pm2_5 <= 250.0f) {
        return 5;  /* 重度污染 */
    } else {
        return 6;  /* 严重污染 */
    }
}

/********************************************************************************************
 * 函数名：PM2_5_App_GetAirQualityString
 * 描  述：获取空气质量等级描述
 * 输  入：无
 * 输  出：@retval: 等级描述字符串
 * 调  用：外部调用
 ********************************************************************************************/
const char* PM2_5_App_GetAirQualityString(void)
{
    uint8_t level = PM2_5_App_GetAirQualityLevel();
    
    switch (level) {
        case 1: return "优";
        case 2: return "良";
        case 3: return "轻度污染";
        case 4: return "中度污染";
        case 5: return "重度污染";
        case 6: return "严重污染";
        default: return "无效";
    }
}

/********************************************************************************************
 * 函数名：PM2_5_App_Diagnostics
 * 描  述：PM2.5 应用层诊断
 * 输  入：无
 * 输  出：无
 * 调  用：外部调用
 ********************************************************************************************/
void PM2_5_App_Diagnostics(void)
{
    PM2_5_Platform_Printf("\r\n========== PM2.5 应用层诊断 ==========\r\n");
    
    /* 打印底层状态 */
    PM2_5_PrintStatus();
    
    /* 打印应用层数据 */
    PM2_5_Platform_Printf("\r\n--- 应用层数据 ---\r\n");
    PM2_5_Platform_Printf("PM1.0:  %.1f μg/m³\r\n", (double)PM2_5_App_GetPM1_0());
    PM2_5_Platform_Printf("PM2.5:  %.1f μg/m³\r\n", (double)PM2_5_App_GetPM2_5());
    PM2_5_Platform_Printf("PM10:   %.1f μg/m³\r\n", (double)PM2_5_App_GetPM10());
    PM2_5_Platform_Printf("空气质量：%s (等级 %d)\r\n", 
           PM2_5_App_GetAirQualityString(), 
           PM2_5_App_GetAirQualityLevel());
    
    PM2_5_Platform_Printf("======================================\r\n\r\n");
}

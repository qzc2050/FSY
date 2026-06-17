/**********************************************************************************************************
 * 文件名: dev_protocol.c
 * 概  述: 设备协议
 * 创建时间: 2025-08-14
 * 更新时间: 2025-08-22
 * 作  者: LYJ
 * 版  本: 1.0.0
 * Copyright (c) 2025, Mr.Liu All rights reserved.
 **********************************************************************************************************/
#include "./core/dev_queue.h"
#include "./core/dev_malloc.h"
#include "./core/dev_protocol.h"


#if DEV_PROTOCOL_CAN
#include "./can/can_protocol.h"
#endif

#if DEV_PROTOCOL_SPI
#include "./spi/spi_protocol.h"
#endif

#if DEV_PROTOCOL_NET
#include "./net_raw/net_raw_protocol.h"
#endif


bool dev_id[DEV_ID_MALLOC_CNT] = {0};    // 设备描述符


/********************************************************************************************
* 函数名：Dev_Protocol_init
* 描  述：设备协议初始化
* 输  入: 无
* 输  出：无
* 调  用：外部调用
********************************************************************************************/
void Dev_Protocol_init(void)
{
    Dev_Mem_Init();

#if DEV_PROTOCOL_CAN
    // candrv.init();
#endif

#if DEV_PROTOCOL_SPI
    spictrl.init();
#endif

#if DEV_PROTOCOL_NET
    netctrl.init();
#endif

    DEV_PRINTF("内存管理 -> 内存池使用率：%.2f %% \r\n", Dev_Mem_Usage());
}

/********************************************************************************************
* 函数名：Dev_Calculate_CRC
* 描  述：设备CRC校验码计算
* 输  入：@param: *data -> 数据指针
*         @param: size -> 数据大小
* 输  出：@retval: 16位CRC校验码
* 调  用：内部调用
********************************************************************************************/
uint16_t Dev_Calculate_CRC(uint8_t *data, uint32_t size)
{
    uint16_t crc = 0xFFFF;
    uint16_t poly = 0x8005;
    
    for(uint32_t i = 0; i < size; i++)
    {
        crc ^=(uint16_t)data[i] << 8;
        for(uint8_t bit = 0; bit < 8; bit++)
        {
            if (crc & 0x8000)
                crc = (crc << 1) ^ poly;
            else
                crc <<= 1;
        }
    }
    return crc;
}

/********************************************************************************************
* 函数名：Dev_Id_alloc
* 描  述：设备描述符分配
* 输  入: 无
* 输  出：@retval: 设备描述符
*         @retval: DEV_INVAILD_ID -> 可分配设备描述符不足
* 调  用：外部调用
********************************************************************************************/
uint16_t Dev_Id_alloc(void)
{
    for(uint16_t id = 0; id < DEV_ID_MALLOC_CNT; id++)
        if(!dev_id[id])
            return id;

    return DEV_INVAILD_ID;
}

/********************************************************************************************
* 函数名：Dev_Id_Release
* 描  述：设备描述符释放
* 输  入: @param: id -> 设备描述符
* 输  出：无
* 调  用：外部调用
********************************************************************************************/
void Dev_Id_Release(uint16_t id)
{
    dev_id[id] = false;
}

/********************************************************************************************
* 函数名：Dev_Tk_Init
* 描  述：设备计时初始化
* 输  入: @param: *time_tick -> 时间节点指针
* 输  出：无
* 调  用：外部调用
********************************************************************************************/
void Dev_Tk_Init(uint32_t *time_tk)
{
    *time_tk = DEV_GET_1MS_TICK_FUN();
}

/********************************************************************************************
* 函数名：Dev_Tk_Wait
* 描  述：设备计时等待
* 输  入: @param: time -> 定时时间（单位：ms）
*         @param: time_tk -> 时间节点
* 输  出：@retval: true -> 等待完成，false -> 等待中
* 调  用：外部调用
********************************************************************************************/
bool Dev_Tk_Wait(uint32_t time, uint32_t time_tk)
{
    return ((DEV_GET_1MS_TICK_FUN() - time_tk) < time) ? false : true;
}













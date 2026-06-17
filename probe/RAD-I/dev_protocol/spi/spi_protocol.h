/**********************************************************************************************************
 * 文件名: spi_protocol.h
 * 概  述: SPI协议（接收、发送、应答）
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

// SPI协议头文件
#include "./spi/spi_config.h"
#include "./core/dev_queue.h"


// SPI协议宏定义
#define SPI_HEAD_LENTH              4     // 帧头长度（单位：字节）
#define SPI_LEN_LENTH               2     // 数据长度（单位：字节）
#define SPI_CRC_LENTH               2     // CRC长度（单位：字节）
#define SPI_TAIL_LENTH              4     // 帧尾长度（单位：字节）
#define SPI_FRAME_EXT_LENTH         12    // (SPI_HEAD_LENTH + SPI_LEN_LENTH + SPI_CRC_LENTH + SPI_TAIL_LENTH)
#define SPI_ACK_LENTH               12    // (SPI_HEAD_LENTH + SPI_LEN_LENTH + SPI_CRC_LENTH + SPI_TAIL_LENTH)
#define SPI_FRAME_HEAD              {SPI_HEAD_1, SPI_HEAD_2, SPI_HEAD_3, SPI_HEAD_4}    // 帧头定义
#define SPI_FRAME_TAIL              {SPI_TAIL_1, SPI_TAIL_2, SPI_TAIL_3, SPI_TAIL_4}    // 帧尾定义


typedef enum{
    SPI_ACK_OK,         // 应答成功
    SPI_ACK_ERR,        // 应答错误
    SPI_ACK_WAITING,    // 等待应答
    SPI_ACK_NOP,        // 无应答请求
}Spi_Ack_State_t;

typedef enum{
    SPI_STEP_HEAD,      // 帧头校验
    SPI_STEP_LENTH,     // 获取数据长度
    SPI_STEP_DATA,      // 获取数据
    SPI_STEP_CRC,       // 获取CRC校验码
    SPI_STEP_TAIL,      // 帧尾校验
}Spi_Frame_Step_t;

typedef struct{
    uint16_t size;      // 数据大小
    uint8_t *buf;       // 缓存地址
}Spi_TxData_t;

typedef struct{
    uint16_t size;      // 数据大小
    uint8_t *buf;       // 缓存地址
}Spi_RxData_t;

typedef struct{
    uint16_t depth;     // 队列深度
    uint16_t count;     // 元素个数
    uint16_t head;      // 队头索引
    uint16_t tail;      // 队尾索引
}Spi_Queue_t;

// SPI应答联合体
typedef union{
    uint8_t pack[SPI_ACK_LENTH];       // 应答数据

    struct{
        uint8_t hd[SPI_HEAD_LENTH];    // 帧头
        uint8_t len[SPI_LEN_LENTH];    // 0x0000
        uint8_t crc[SPI_CRC_LENTH];    // CRC校验
        uint8_t tl[SPI_TAIL_LENTH];    // 帧尾
    }frame;
}Spi_Ack_Pack_t;

// SPI设备结构体
typedef struct{
    void (*csl)(void);          // CS引脚拉低函数（设备使能）
    void (*csh)(void);          // CS引脚拉高函数（设备失能）
    bool (*transmit)(uint8_t*, uint8_t*, uint16_t);    // 数据传输函数

    uint16_t id;                // 设备描述符
    uint16_t period;            // 设备轮询周期
    uint32_t peri_tk;           // 设备轮询时间节点
    char name[SPI_NAME_LEN];    // 设备名称
    Dev_Queue_Config_t qcfg;    // 设备队列配置
    Spi_Queue_t *txq;           // 发送队列指针
    Spi_Queue_t *rxq;           // 接收队列指针
    Spi_TxData_t *txd;          // 发送节点指针
    Spi_RxData_t *rxd;          // 接收队列指针
}Spi_Device_t;

// SPI传输结构体
typedef struct{
    bool txbusy;                // 本地 -> 目标，发送忙
    bool ackbusy;               // 本地 -> ACK，应答忙
    Spi_Frame_Step_t step;      // 当前数据解析状态

#if SPI_TRANSMIT_ACK
    uint32_t ack_time;          // ACK -> 本地，应答等待时间
    Spi_Ack_Pack_t ack;         // 本地 -> ACK，应答数据
    Spi_Ack_State_t ack_sta;    // ACK -> 本地，应答等待状态
#endif

    uint32_t tx_tk;             // 发送结束时间节点
    uint16_t xsize;             // 数据传输大小 -> 数据传输函数
    uint16_t crc;               // 本地 -> 目标，CRC校验码
    uint16_t txsize;            // 目标 -> 本地，当前数据发送大小
    uint16_t rxlen;             // 目标 -> 本地，目标发送数据大小/本地接收数据大小
    uint8_t retx_times;         // 本地 -> 目标，当前重发次数
    uint8_t *send;              // 本地 -> 目标，发送缓存区
    uint8_t *recv;              // 目标 -> 本地，接收缓存区
    uint8_t *data;              // 目标 -> 本地，数据缓存区
    uint8_t *ackp;              // 本地 -> 目标，应答缓存区
}Spi_Xfer_t;

// SPI协议结构体
typedef struct{
    bool (*init)(void);         // 外设初始化函数
    void (*deinit)(void);       // 外设反初始化函数
    bool (*transmit)(uint8_t*, uint8_t*, uint16_t);    // 数据传输函数

    uint8_t num;                // 外设 -> 设备数
    uint8_t crt_dev;            // 当前轮询设备
    char name[SPI_NAME_LEN];    // 外设名称
    Spi_Device_t *dev[SPI_MAX_DEV_CNT];    // 设备信息、缓存区
    Spi_Xfer_t xfer;            // 外设传输管理
}Spi_Periph_t;

// SPI协议控制结构体
typedef struct{
    void (*init)(void);         // 协议初始化函数

    uint8_t periph_cnt;         // 当前外设注册数量
    Spi_Periph_t *periph[SPI_MAX_PERIPH_CNT];    // 外设句柄
}Spi_Ctrl_t;


#define Spi_Transmit_Data(sdata, rdata, size)   {                                                                   \
                                                    crt_dev->csl();                                                 \
                                                    if(!crt_dev->transmit((void *)sdata, (uint8_t *)rdata, size))   \
                                                    {                                                               \
                                                        crt_dev->csh();                                             \
                                                        return;                                                     \
                                                    }                                                               \
                                                    crt_dev->csh();                                                 \
                                                }


extern Spi_Ctrl_t spictrl;


static void Spi_Data_Printf(uint8_t *sdata, uint16_t len);
static bool Spi_RxQueue_Push(uint8_t *rdata, uint16_t len);
static void Spi_Txbuf_Clear(void);
static bool Spi_Txbuf_Write(uint8_t *data, uint16_t size);
static void Spi_Protocol_Resolve(void);
static void Spi_Ack_Cancel_Wait(void);
static void Spi_Ack_Write(uint16_t crc);
static bool Spi_Ack_Detect(Spi_Ack_Pack_t *data);
static void Spi_Ack_Task(void);
static void Spi_Transmit_Task(void);
static void Spi_Resolve_Task(void);


extern Spi_Periph_t *Spi_Periph_Register(char *name, bool(*init)(void), void(*deinit)(void), \
                                            bool(*transmit)(uint8_t*, uint8_t*, uint16_t));
extern void Spi_Periph_Unregister(Spi_Periph_t *ph);
extern Spi_Device_t *Spi_Device_Register(Spi_Periph_t *ph, char *name, void (*csl)(void), \
                                    void (*csh)(void), Dev_Queue_Config_t qcfg, uint16_t period);
extern void Spi_Device_Unregister(Spi_Periph_t *ph, Spi_Device_t *dev);
extern bool Spi_TxQueue_Push(Spi_Device_t *dev, uint8_t *sdata, uint16_t len);
extern void Spi_Thread_Task(void);


#ifdef __cplusplus
}
#endif

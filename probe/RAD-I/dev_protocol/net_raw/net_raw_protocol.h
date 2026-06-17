/**********************************************************************************************************
 * 文件名: net_raw_protocol.h
 * 概  述: 数据协议（接收、发送、应答）
 * 创建时间: 2026-03-30
 * 更新时间: 2026-03-30
 * 作  者: LYJ
 * 版  本: 1.0.0
 * Copyright (c) 2025, Mr.Liu All rights reserved.
 **********************************************************************************************************/
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

// 数据协议头文件
#include "./net_raw/net_raw_config.h"
#include "./core/dev_queue.h"


// 协议功能码 
#define NET_FC_EXCEPTION_MASK         (0x80u)

/* 读保持寄存器 */
#define NET_FC_READ_HOLDING_REQ       (0x03u)
#define NET_FC_READ_HOLDING_RESP      (0x13u)
#define NET_FC_READ_HOLDING_ERR       (0x83u)

/* 写单个寄存器 */
#define NET_FC_WRITE_SINGLE_REQ       (0x06u)
#define NET_FC_WRITE_SINGLE_RESP      (0x16u)
#define NET_FC_WRITE_SINGLE_ERR       (0x86u)

/* 写多个寄存器 */
#define NET_FC_WRITE_MULTI_REQ        (0x10u)
#define NET_FC_WRITE_MULTI_RESP       (0x20u)
#define NET_FC_WRITE_MULTI_ERR        (0x90u)

/* 读单个寄存器 */
#define NET_FC_READ_SINGLE_REQ        (0x05u)
#define NET_FC_READ_SINGLE_RESP       (0x15u)
#define NET_FC_READ_SINGLE_ERR        (0x85u)

/* 设备主动上传 */
#define NET_FC_ACTIVE_UPLOAD          (0x23u)
#define NET_FC_ACTIVE_UPLOAD_SINGLE   (0x25u)

/* 异常码 */
#define NET_EXC_ILLEGAL_FUNCTION      (0x01u)
#define NET_EXC_ILLEGAL_ADDRESS       (0x02u)
#define NET_EXC_ILLEGAL_VALUE         (0x03u)
#define NET_EXC_SLAVE_FAILURE         (0x04u)

// 设备应答枚举
typedef enum{
    NET_ACK_OK,
    NET_ACK_ERR,
    NET_ACK_WAITING,
    NET_ACK_NOP,
}Net_Ack_State_t;

typedef struct{
    uint16_t size;      // 数据大小
    uint8_t *buf;       // 缓存地址
}Net_TxData_t;

typedef struct{
    uint16_t size;      // 数据大小
    uint8_t *buf;       // 缓存地址
}Net_RxData_t;

typedef struct{
    uint16_t depth;     // 队列深度
    uint16_t count;     // 元素个数
    uint16_t head;      // 队头索引
    uint16_t tail;      // 队尾索引
}Net_Queue_t;

/* 设备注册参数 */
typedef struct{
    Dev_Queue_Config_t qcfg;     // 队列/缓冲配置
    uint8_t *reg_tb;             // 寄存器表（可为 NULL，内部自动分配 reg_sz 大小的寄存器表）
    uint32_t reg_sz;             // 寄存器表字节长度（偶数）
    uint16_t reasm_sz;           // 重组缓存大小（注册时由用户指定）
    uint16_t period;             // 轮询周期
    uint8_t addr;                // 设备地址

}Net_Device_Base_Config_t;

// 协议应答联合体
typedef union{
    uint8_t pack[8];

    struct{
        uint8_t hd[4];
        uint8_t len[2];
        uint8_t crc[2];
    }frame;
}Net_Ack_Pack_t;

// NET 设备结构体
typedef struct{
    void (*net_begin)(void);    // 发送前使能（可为 NULL，如 RS485 DE）
    void (*net_end)(void);      // 发送后释放（可为 NULL）

    bool (*transmit)(uint8_t*, uint16_t);    // 发送函数
    uint16_t (*receive)(uint8_t *);          // 接收函数

    uint16_t id;                // 设备描述符
    uint16_t period;            // 设备轮询周期
    uint32_t peri_tk;           // 设备轮询时间节点
    char name[NET_NAME_LEN];    // 设备名称
    Dev_Queue_Config_t qcfg;    // 设备队列配置
    Net_Queue_t *txq;           // 发送队列指针
    Net_Queue_t *rxq;           // 接收队列指针
    Net_TxData_t *txd;          // 发送节点指针
    Net_RxData_t *rxd;          // 接收节点指针

    uint8_t addr;               // 设备地址
    uint8_t reg_tb_owned;       // 1：系统创建（注销时释放） 0：用户创建
    uint8_t *reg_tb;            // 保持寄存器表字节区（小端排列）
    uint32_t reg_sz;            // 寄存器表字节长度（须为偶数）
    
    uint8_t *reasm_buf;         // 重组缓存区
    uint16_t reasm_size;        // 重组缓存已接收字节数
    uint16_t reasm_sz;          // 重组缓存大小
}Net_Device_t;

// NET 传输结构体
typedef struct{
    bool txbusy;                // 本地 -> 目标，发送忙
    bool ackbusy;               // 本地 -> ACK，应答忙

#if NET_TRANSMIT_ACK
    uint32_t ack_time;
    Net_Ack_Pack_t ack;         // 保留占位
    Net_Ack_State_t ack_sta;
    uint8_t last_req_addr;      // 设备地址
    uint8_t last_req_fc;        // 功能码
    uint16_t last_req_reg;      // 请求的寄存器地址
#endif

    uint32_t tx_tk;             // 发送结束时间节点
    uint16_t xsize;             // 单帧缓冲最大字节数
    uint16_t txsize;            // 当前待发字节数
    uint16_t rxlen;             // 本次接收有效字节数
    uint8_t retx_times;         // 重发次数
    uint8_t *send;              // 发送缓存区
    uint8_t *recv;              // 接收缓存区
    uint8_t *data;              // 数据缓存区
#if NET_TRANSMIT_ACK
    uint8_t *ackp;              // 应答缓存区
#endif
}Net_Xfer_t;

// NET 协议结构体
typedef struct{
    bool (*init)(void);         // 外设初始化函数
    void (*deinit)(void);       // 外设反初始化函数
    bool (*transmit)(uint8_t*, uint16_t);    // 发送函数
    uint16_t (*receive)(uint8_t *);          // 接收函数

    uint8_t num;                // 外设 -> 设备数
    uint8_t crt_dev;            // 当前轮询设备
    char name[NET_NAME_LEN];    // 外设名称
    Net_Device_t *dev[NET_MAX_DEV_CNT];    // 设备信息、缓存区
    Net_Xfer_t xfer;            // 外设传输管理
}Net_Periph_t;

// NET 协议控制结构体
typedef struct{
    void (*init)(void);         // 协议初始化函数

    uint8_t periph_cnt;         // 当前外设注册数量
    Net_Periph_t *periph[NET_MAX_PERIPH_CNT];    // 外设句柄
}Net_Ctrl_t;


#define Net_Transmit_Data(sdata, rdata, txb)   {                                                                            \
                                                    if(crt_dev->net_begin) crt_dev->net_begin();                            \
                                                    {                                                                       \
                                                        uint16_t __off = 0u;                                                \
                                                        while(__off < (txb))                                                \
                                                        {                                                                   \
                                                            uint16_t __n = (uint16_t)((txb) - __off);                       \
                                                            if(__n > crt_dev->qcfg.txb_size) __n = crt_dev->qcfg.txb_size;  \
                                                            if(__n == 0u)                                                   \
                                                            {                                                               \
                                                                crt_xfer->rxlen = 0u;                                       \
                                                                if(crt_dev->net_end) crt_dev->net_end();                    \
                                                                return;                                                     \
                                                            }                                                               \
                                                            if(!crt_dev->transmit((void *)((sdata) + __off), __n))          \
                                                            {                                                               \
                                                                crt_xfer->rxlen = 0u;                                       \
                                                                if(crt_dev->net_end) crt_dev->net_end();                    \
                                                                return;                                                     \
                                                            }                                                               \
                                                            __off = (uint16_t)(__off + __n);                                \
                                                        }                                                                   \
                                                    }                                                                       \
                                                    crt_xfer->rxlen = (crt_ph->receive ? crt_ph->receive((uint8_t *)(rdata)) : 0u); \
                                                    if(crt_dev->net_end) crt_dev->net_end();                                \
                                                }


extern Net_Ctrl_t netctrl;


extern uint16_t Net_Modbus_Crc16(const uint8_t *data, uint16_t len);
extern bool Net_Raw_FrameCrcOk(const uint8_t *frame, uint16_t len);
extern Net_Periph_t *Net_Periph_Register(char *name, bool(*init)(void), void(*deinit)(void), \
                                            bool(*transmit)(uint8_t*, uint16_t), uint16_t(*receive)(uint8_t *));
extern void Net_Periph_Unregister(Net_Periph_t *ph);
extern Net_Device_t *Net_Device_Register(Net_Periph_t *ph, char *name, void (*net_begin)(void), \
                                    void (*net_end)(void), const Net_Device_Base_Config_t *cfg);
extern void Net_Device_Unregister(Net_Periph_t *ph, Net_Device_t *dev);
extern bool Net_TxQueue_Push(Net_Device_t *dev, uint8_t *sdata, uint16_t len);
extern uint16_t Net_Reg_Holding_Read_U16(Net_Device_t *dev, uint16_t reg_addr);
extern uint32_t Net_Reg_Holding_Read_U32(Net_Device_t *dev, uint16_t reg_addr);
extern void Net_Reg_Holding_Write_U16(Net_Device_t *dev, uint16_t reg_addr, uint16_t val);
extern void Net_Reg_Holding_Write_U32(Net_Device_t *dev, uint16_t reg_addr, uint32_t val);
extern void Net_Protocol_Master_OnResponse(const uint8_t *pdu, uint16_t pdu_len);
extern void Net_Protocol_HandlePdu(Net_Device_t *dev, uint8_t *pdu, uint16_t len);
extern uint16_t Net_Modbus_Slave_Process(Net_Device_t *dev, uint8_t *wire, uint16_t wire_len, uint8_t *out, uint16_t out_max);
extern void Net_Thread_Task(void);


#ifdef __cplusplus
}
#endif

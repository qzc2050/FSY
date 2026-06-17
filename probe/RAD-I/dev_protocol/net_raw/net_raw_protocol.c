/**********************************************************************************************************
 * 文件名: net_raw_protocol.c
 * 概  述: 网络/串口裸协议（接收、发送）——辐射报警仪 Modbus RTU
 * 创建时间: 2026-03-30
 * 更新时间: 2026-03-30
 * 作  者: LYJ
 * 版  本: 1.0.0
 * Copyright (c) 2025, Mr.Liu All rights reserved.
 **********************************************************************************************************/
/********************************** 辐射报警仪 Modbus RTU ***************************************
 * 帧：地址 | 功能码 | 数据 | CRC_Lo | CRC_Hi
 * CRC：Modbus RTU（0xFFFF 初值，0xA001 反序多项式），先发低字节
*************************************************************************************/
#include "./net_raw/net_raw_protocol.h"
#include "./core/dev_malloc.h"
#include "./net_raw/net_raw_app.h"
#include "./net_raw/net_raw_bsp.h"
#include <string.h>


Net_Periph_t *crt_ph = NULL;
Net_Xfer_t *crt_xfer = NULL;
Net_Device_t *crt_dev = NULL;

Net_Ctrl_t netctrl = {
    .init = Net_Device_Init,
    .periph = {0},
    .periph_cnt = 0,
};

/********************************************************************************************
* 函数名：Net_Modbus_Crc16
* 描  述：Modbus RTU CRC16（初值 0xFFFF，多项式 0xA001，低字节先发）
* 输  入：@param: *data -> 数据指针；@param: len -> 参与计算的字节数
* 输  出：@retval: 16 位 CRC
* 调  用：内部/外部调用
********************************************************************************************/
uint16_t Net_Modbus_Crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;

    for(uint16_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for(uint8_t b = 0; b < 8; b++)
        {
            if(crc & 0x01)
                crc = (uint16_t)((crc >> 1) ^ 0xA001u);
            else
                crc = (uint16_t)(crc >> 1);
        }
    }
    return crc;
}

/********************************************************************************************
* 函数名：Net_Raw_FrameCrcOk
* 描  述：校验一帧 Modbus RTU（最后两字节为 CRC，低字节在前）
* 输  入：@param: *frame -> 含 CRC 的完整帧；@param: len -> 帧总长度
* 输  出：@retval: true -> CRC 正确；false -> 长度不足或 CRC 错误
* 调  用：内部调用
********************************************************************************************/
bool Net_Raw_FrameCrcOk(const uint8_t *frame, uint16_t len)
{
    if(len < 4)
        return false;
    uint16_t c = Net_Modbus_Crc16(frame, (uint16_t)(len - 2));
    return (frame[len - 2] == (uint8_t)(c & 0xFFu)) && (frame[len - 1] == (uint8_t)(c >> 8));
}

/** 小端 u16 安全读取：volatile 逐字节 + noinline，防止 ARMCC O2 内联后合成未对齐 LDRH/LDR */
#if defined(__GNUC__) || defined(__ARMCC_VERSION) || defined(__CC_ARM)
__attribute__((noinline))
#endif
static uint16_t net_load_u16_le(const uint8_t *p)
{
    volatile const uint8_t *b;
    uint8_t lo;
    uint8_t hi;

    if(p == NULL)
        return 0U;
    b = p;
    lo = b[0];
    hi = b[1];
    return (uint16_t)lo | ((uint16_t)hi << 8);
}

/********************************************************************************************
* 函数名：Net_Fc_IsCandidate
* 描  述：滑窗前快速功能码过滤，减少无效 CRC 计算次数
* 输  入：@param: fc -> 功能码
* 输  出：@retval: true -> 属于候选集合（含异常码 0x80）
* 调  用：内部调用（Net_Frame_Reasm_Process）
********************************************************************************************/
static inline bool Net_Fc_IsCandidate(uint8_t fc)
{
    if((fc & 0x80u) != 0u)
        return true;

    switch(fc)
    {
        case NET_FC_READ_HOLDING_REQ:
        case NET_FC_WRITE_SINGLE_REQ:
        case NET_FC_WRITE_MULTI_REQ:
        case NET_FC_READ_SINGLE_REQ:
        case NET_FC_READ_HOLDING_RESP:
        case NET_FC_WRITE_SINGLE_RESP:
        case NET_FC_WRITE_MULTI_RESP:
        case NET_FC_READ_SINGLE_RESP:
        case NET_FC_ACTIVE_UPLOAD:
        case NET_FC_ACTIVE_UPLOAD_SINGLE:
            return true;
        default:
            return false;
    }
}

/********************************************************************************************
* 函数名：Net_Fc_PredictFrameLen
* 描  述：预测最可能的完整帧长度（含 CRC），未知返回 0；仅作快路径优化
*         调用方在预测失败时必须回退全窗口扫描，确保不丢有效帧
* 输  入：@param: p -> 帧起始；@param: avail -> 可用字节数
* 输  出：@retval: 预测帧长；0 表示不预测
* 调  用：内部调用（Net_Frame_Reasm_Process）
********************************************************************************************/
static inline uint16_t Net_Fc_PredictFrameLen(const uint8_t *p, uint16_t avail)
{
    uint8_t fc;
    uint16_t len = 0U;

    if(!p || avail < 4U)
        return 0U;

    fc = p[1];
    switch(fc)
    {
        case NET_FC_READ_HOLDING_REQ:   /* addr fc start_lo start_hi qty_lo qty_hi crc_lo crc_hi */
        case NET_FC_WRITE_SINGLE_REQ:
        case NET_FC_WRITE_SINGLE_RESP:
        case NET_FC_WRITE_MULTI_RESP:
        case NET_FC_READ_SINGLE_REQ:
        case NET_FC_READ_SINGLE_RESP:
            len = 8U;
            break;

        case NET_FC_WRITE_MULTI_REQ:    /* 9 + byte_count */
            if(avail >= 7U)
                len = (uint16_t)(9U + p[6]);
            break;

        case NET_FC_READ_HOLDING_RESP:  /* 7 + byte_count */
            if(avail >= 3U)
                len = (uint16_t)(7U + p[2]);
            break;

        default:
            /* 异常帧/主动上报等长度不固定，不做预测 */
            break;
    }

    if((len >= 4U) && (len <= avail))
        return len;
    return 0U;
}

/********************************************************************************************
* 函数名：Net_FrameLen_FormatOk
* 描  述：CRC 命中后按功能码校验帧长，避免短帧/伪帧误判
* 输  入：@param: p -> 帧起始；@param: len -> 帧总长度（含 CRC）
* 输  出：@retval: true -> 长度与功能码一致
* 调  用：内部调用（重组/ACK/解析路径）
********************************************************************************************/
static inline bool Net_FrameLen_FormatOk(const uint8_t *p, uint16_t len)
{
    if(!p || (len < 4U))
        return false;

    if((p[1] & 0x80u) != 0u)
        return (len == 8U);

    switch(p[1])
    {
        case NET_FC_READ_HOLDING_REQ:
        case NET_FC_WRITE_SINGLE_REQ:
        case NET_FC_WRITE_SINGLE_RESP:
        case NET_FC_WRITE_MULTI_RESP:
        case NET_FC_READ_SINGLE_REQ:
        case NET_FC_READ_SINGLE_RESP:
            return (len == 8U);

        case NET_FC_WRITE_MULTI_REQ:
            if(len < 9U)
                return false;
            if((uint32_t)p[6] != (uint32_t)net_load_u16_le(&p[4]) * 2U)
                return false;
            return (len == (uint16_t)(9U + p[6]));

        case NET_FC_READ_HOLDING_RESP:
        case NET_FC_ACTIVE_UPLOAD:
            if(len < 7U)
                return false;
            return (len == (uint16_t)(7U + p[2]));

        default:
            return false;
    }
}

/********************************************************************************************
* 函数名：Net_GetCrcSearchMaxTry
* 描  述：统一计算 CRC 滑窗最大尝试长度（剩余字节与 rxb_size+CRC 上限取小）
* 输  入：@param: remain -> 当前位置起剩余字节；@param: rxb_size -> 设备单包接收上限
* 输  出：@retval: 本次滑窗允许的最大 try 长度
* 调  用：内部调用
********************************************************************************************/
static inline uint16_t Net_GetCrcSearchMaxTry(uint16_t remain, uint16_t rxb_size)
{
    uint16_t maxtry = remain;

    if(rxb_size > 0U)
    {
        uint16_t frame_cap = (uint16_t)(rxb_size + NET_CRC_LENTH);
        if((frame_cap >= 4U) && (maxtry > frame_cap))
            maxtry = frame_cap;
    }
    return maxtry;
}


#if (NET_SEND_DEBUG | NET_RECV_DEBUG)
/********************************************************************************************
* 函数名：Net_Data_Printf
* 描  述：NET 调试打印（发送/接收十六进制）
* 输  入：@param: *sdata -> 数据指针；@param: size -> 字节数
* 输  出：无
* 调  用：内部调用
********************************************************************************************/
static void Net_Data_Printf(uint8_t *sdata, uint16_t size)
{
    for(uint16_t i = 0; i < size; i++)
        DEV_PRINTF("%02X ", sdata[i]);
    DEV_PRINTF(" t: %u\r\n", DEV_GET_1MS_TICK_FUN());
}
#endif

/********************************************************************************************
* 函数名：Net_Periph_Register
* 描  述：NET 外设注册（UART 等）
* 输  入：@param: *name -> 外设名称
*         @param: *init -> 初始化（无参，成功返回 true）
*         @param: *deinit -> 反初始化
*         @param: *transmit -> 仅发送（TX 指针，待发字节数），成功返回 true
* 输  出：@retval: 外设句柄；失败返回 NULL
* 调  用：外部调用（如 Net_Device_Init 中注册）
********************************************************************************************/
Net_Periph_t *Net_Periph_Register(char *name, bool(*init)(void), void(*deinit)(void), \
                                    bool(*transmit)(uint8_t*, uint16_t), uint16_t(*receive)(uint8_t *))
{
    if(!name || !init || !deinit || !transmit || !receive)
    {
        DEV_PRINTF("%s -> 注册失败 -> 无效指针！\r\n", name);
        return NULL;
    }
    if(netctrl.periph_cnt >= NET_MAX_PERIPH_CNT)
        DEV_PRINTF("%s -> 注册失败 -> 可注册外设数不足！\r\n", name);
    else if(!init())
        DEV_PRINTF("%s -> 外设注册 -> 外设初始化失败！\r\n", name);
    else
    {
        Net_Periph_t *new_ph = (Net_Periph_t *)Dev_Mem_Malloc(sizeof(Net_Periph_t));
        if(!new_ph)
        {
            DEV_PRINTF("%s -> 外设注册 -> 外设句柄分配失败！\r\n", name);
            return NULL;
        }

        new_ph->num = 0;
        memset((void *)&new_ph->xfer, 0, sizeof(Net_Xfer_t));

        strcpy(new_ph->name, name);
        new_ph->init = init;
        new_ph->deinit = deinit;
        new_ph->transmit = transmit;
        new_ph->receive = receive;
        netctrl.periph[netctrl.periph_cnt++] = new_ph;
#if NET_TRANSMIT_ACK
        new_ph->xfer.ack_sta = NET_ACK_NOP;
        new_ph->xfer.ackbusy = false;
#endif
        DEV_PRINTF("%s -> 外设注册 -> 注册成功！\r\n", name);
        return new_ph;
    }
    return NULL;
}

/********************************************************************************************
* 函数名：Net_Periph_Unregister
* 描  述：NET 外设注销（释放其下设备与 xfer 缓冲）
* 输  入：@param: *ph -> 外设句柄
* 输  出：无
* 调  用：外部调用
********************************************************************************************/
void Net_Periph_Unregister(Net_Periph_t *ph)
{
    uint8_t idx;

    if(!netctrl.periph_cnt)
    {
        DEV_PRINTF("%s -> 注销失败 -> 未注册任何外设！\r\n", ph->name);
        return;
    }

    if(!ph)
    {
        DEV_PRINTF("外设注销 -> 注销失败 -> 未注册该外设！\r\n");
        return;
    }

    for(idx = 0; idx < netctrl.periph_cnt; idx++)
        if(ph == netctrl.periph[idx])
            break;

    if(idx == netctrl.periph_cnt)
    {
        DEV_PRINTF("%s -> 注销失败 -> 未注册该外设！\r\n", ph->name);
        return;
    }

    ph->deinit();
    for(uint8_t num = 0; num < ph->num; num++)
        Net_Device_Unregister(ph, ph->dev[num]);

#if NET_TRANSMIT_ACK
    Dev_Mem_Release(ph->xfer.ackp);
#endif

    Dev_Mem_Release(ph->xfer.send);
    Dev_Mem_Release(ph->xfer.recv);
    Dev_Mem_Release(ph->xfer.data);

    DEV_PRINTF("%s -> 外设注销 -> 注销成功！\r\n", ph->name);
    netctrl.periph[--netctrl.periph_cnt] = NULL;
    Dev_Mem_Release(ph);
}

/********************************************************************************************
* 函数名：Net_Device_RegTb_Init
* 描  述：保持寄存器表初始化；reg_tb 非 NULL 时引用用户缓冲，为 NULL 时由本模块分配 reg_sz 字节
* 输  入：@param: *dh -> 设备句柄；@param: reg_tb -> 用户表或 NULL；@param: reg_sz -> 字节长度（须为 >=2 的偶数）
* 输  出：@retval: true -> 成功；false -> 参数非法或分配失败
* 调  用：内部调用（Net_Device_Register）
********************************************************************************************/
static inline bool Net_Device_RegTb_Init(Net_Device_t *dh, uint8_t *reg_tb, uint32_t reg_sz)
{
    dh->reg_sz = 0;
    dh->reg_tb = NULL;
    dh->reg_tb_owned = 0;

    if(reg_sz < 2 || (reg_sz & 1))
        return false;

    if(reg_tb != NULL)
    {
        dh->reg_tb = reg_tb;
        dh->reg_sz = reg_sz;
        return true;
    }

    dh->reg_tb = (uint8_t *)Dev_Mem_Malloc(reg_sz);
    if(!dh->reg_tb)
        return false;
    memset(dh->reg_tb, 0, reg_sz);
    dh->reg_sz = reg_sz;
    dh->reg_tb_owned = 1;
    return true;
}

/********************************************************************************************
* 函数名：Net_Device_Register
* 描  述：NET 设备注册（队列、xfer、保持寄存器表）
* 输  入：@param: *ph -> 已注册外设；@param: *name -> 设备名
*         @param: net_begin / net_end -> 发送前后钩子（可为 NULL）
*         @param: cfg -> 设备注册参数（队列、周期、地址、寄存器表、重组缓存等）
* 输  出：@retval: 设备句柄；失败返回 NULL
* 调  用：外部调用
********************************************************************************************/
Net_Device_t *Net_Device_Register(Net_Periph_t *ph, char *name, void (*net_begin)(void), \
                                    void (*net_end)(void), const Net_Device_Base_Config_t *cfg)
{
    uint8_t idx;
    Dev_Queue_Config_t qcfg_local;

    if(!cfg)
    {
        DEV_PRINTF("%s -> 注册失败 -> 参数配置结构体指针不能为空！\r\n", name);
        return NULL;
    }

    qcfg_local = cfg->qcfg;
    
    if(!cfg->reg_sz || (cfg->reg_sz & 1))
    {
        DEV_PRINTF("%s -> 注册失败 -> 寄存器表字节长度要求：非0偶数！\r\n", name);
        return NULL;
    }

    for(idx = 0; idx < netctrl.periph_cnt; idx++)
        if((void *)netctrl.periph[idx] == (void *)ph)
            break;

    if(idx == netctrl.periph_cnt)
    {
        DEV_PRINTF("%s -> 注册失败 -> 未注册外设！\r\n", name);
        return NULL;
    }

    if(ph->num >= NET_MAX_DEV_CNT)
    {
        DEV_PRINTF("%s -> 注册失败 -> 可注册设备数不足！\r\n", name);
        return NULL;
    }

    if((qcfg_local.txb_size > cfg->reasm_sz) || (qcfg_local.rxb_size > cfg->reasm_sz))
    {
        DEV_PRINTF("%s -> 注册失败 -> reasm_sz=%u 小于 txb_size/rxb_size！\r\n",
                    name, (unsigned)cfg->reasm_sz);
        return NULL;
    }
    
    if(cfg->reasm_sz == 0U)
    {
        DEV_PRINTF("%s -> 注册失败 -> 重组缓存大小设置异常：%u！\r\n", name, cfg->reasm_sz);
        return NULL;
    }

    if(cfg->reasm_sz < qcfg_local.rxb_size)
    {
        DEV_PRINTF("%s -> 注册失败 -> reasm_sz=%u 小于 rxb_size=%u！\r\n",
                    name, (unsigned)cfg->reasm_sz, (unsigned)qcfg_local.rxb_size);
        return NULL;
    }

    Net_Device_t *new_dh = (Net_Device_t *)Dev_Mem_Malloc(sizeof(Net_Device_t));
    if(!new_dh)
    {
        DEV_PRINTF("%s -> 设备注册 -> 设备句柄分配失败！\r\n", name);
        return NULL;
    }

    if((new_dh->id = Dev_Id_alloc()) == DEV_INVAILD_ID)
    {
        DEV_PRINTF("%s -> 注册失败 -> 可分配设备描述符不足！\r\n", name);
        Dev_Mem_Release(new_dh);
        return NULL;
    }

    ph->dev[ph->num++] = new_dh;
    new_dh->txq = (Net_Queue_t *)Dev_Mem_Malloc(sizeof(Net_Queue_t));
    new_dh->rxq = (Net_Queue_t *)Dev_Mem_Malloc(sizeof(Net_Queue_t));
    if(!new_dh->txq || !new_dh->rxq)
    {
        DEV_PRINTF("%s -> 设备注册 -> TX/RX 队列分配失败！\r\n", name);
        Net_Device_Unregister(ph, new_dh);
        return NULL;
    }

    new_dh->txq->count = 0;
    new_dh->rxq->count = 0;
    Dev_Queue_Clear((void *)new_dh->txq);
    Dev_Queue_Clear((void *)new_dh->rxq);
    new_dh->txq->depth = qcfg_local.txq_depth;
    new_dh->rxq->depth = qcfg_local.rxq_depth;
    new_dh->txd = (Net_TxData_t *)Dev_Mem_Malloc(sizeof(Net_TxData_t) * qcfg_local.txq_depth);
    new_dh->rxd = (Net_RxData_t *)Dev_Mem_Malloc(sizeof(Net_RxData_t) * qcfg_local.rxq_depth);
    if(!new_dh->txd || !new_dh->rxd)
    {
        DEV_PRINTF("%s -> 设备注册 -> TX/RX 指针分配失败！\r\n", name);
        Net_Device_Unregister(ph, new_dh);
        return NULL;
    }

    Net_TxData_t *txd = new_dh->txd;
    for(uint16_t i = 0; i < qcfg_local.txq_depth; i++)
    {
        if((txd->buf = (uint8_t *)Dev_Mem_Malloc(qcfg_local.txb_size)) == NULL)
        {
            DEV_PRINTF("%s -> 设备注册 -> TX 内存分配失败！\r\n", name);
            Net_Device_Unregister(ph, new_dh);
            return NULL;
        }
        memset(txd->buf, 0, qcfg_local.txb_size);
        txd->size = 0;
        txd++;
    }

    Net_RxData_t *rxd = new_dh->rxd;
    for(uint16_t i = 0; i < qcfg_local.rxq_depth; i++)
    {
        if((rxd->buf = (uint8_t *)Dev_Mem_Malloc(qcfg_local.rxb_size)) == NULL)
        {
            DEV_PRINTF("%s -> 设备注册 -> RX 内存分配失败！\r\n", name);
            Net_Device_Unregister(ph, new_dh);
            return NULL;
        }
        memset(rxd->buf, 0, qcfg_local.rxb_size);
        rxd->size = 0;
        rxd++;
    }

    new_dh->reasm_sz = cfg->reasm_sz;
    new_dh->reasm_buf = (uint8_t *)Dev_Mem_Malloc(new_dh->reasm_sz);
    new_dh->reasm_size = 0;

    if(!new_dh->reasm_buf)
    {
        DEV_PRINTF("%s -> 设备注册 -> 重组缓冲区分配失败！\r\n", name);
        Net_Device_Unregister(ph, new_dh);
        return NULL;
    }
    DEV_PRINTF("%s -> 重组缓冲区已分配 (size=%u)\r\n", name, (unsigned)new_dh->reasm_sz);

    if(!Net_Device_RegTb_Init(new_dh, cfg->reg_tb, cfg->reg_sz))
    {
        DEV_PRINTF("%s -> 设备注册 -> 保持寄存器表初始化失败！\r\n", name);
        Net_Device_Unregister(ph, new_dh);
        return NULL;
    }

    uint16_t temp = (qcfg_local.txb_size > qcfg_local.rxb_size) ? qcfg_local.txb_size : qcfg_local.rxb_size;

    if(ph->xfer.xsize < (temp + NET_CRC_LENTH))
    {
        ph->xfer.xsize = (uint16_t)(temp + NET_CRC_LENTH);
        ph->xfer.send = Dev_Mem_Realloc(ph->xfer.send, ph->xfer.xsize);
        ph->xfer.recv = Dev_Mem_Realloc(ph->xfer.recv, ph->xfer.xsize);
        ph->xfer.data = Dev_Mem_Realloc(ph->xfer.data, ph->xfer.xsize);

    #if NET_TRANSMIT_ACK
        ph->xfer.ackp = Dev_Mem_Realloc(ph->xfer.ackp, ph->xfer.xsize);
        if(!ph->xfer.send || !ph->xfer.recv || !ph->xfer.data || !ph->xfer.ackp)
        {
            DEV_PRINTF("%s -> 设备注册 -> xfer TX/RX/DATA/ACKP 内存重分配失败！\r\n", name);
            Net_Device_Unregister(ph, new_dh);
            return NULL;
        }
        memset(ph->xfer.ackp, 0, ph->xfer.xsize);
    #else
        if(!ph->xfer.send || !ph->xfer.recv || !ph->xfer.data)
        {
            DEV_PRINTF("%s -> 设备注册 -> xfer TX/RX/DATA 内存分配失败！\r\n", name);
            Net_Device_Unregister(ph, new_dh);
            return NULL;
        }
    #endif

        memset(ph->xfer.send, 0, ph->xfer.xsize);
        memset(ph->xfer.recv, 0, ph->xfer.xsize);
        memset(ph->xfer.data, 0, ph->xfer.xsize);
    }

    strcpy(new_dh->name, name);
    new_dh->transmit = ph->transmit;
    new_dh->net_begin = net_begin;
    new_dh->net_end = net_end;
    memcpy(&new_dh->qcfg, &qcfg_local, sizeof(Dev_Queue_Config_t));
    new_dh->period = cfg->period;
    new_dh->addr = cfg->addr;
    DEV_PRINTF("%s -> 设备注册 -> 注册成功！(txb=%u, rxb=%u, reasm=%u)\r\n",
                name,
                (unsigned)new_dh->qcfg.txb_size,
                (unsigned)new_dh->qcfg.rxb_size,
                (unsigned)new_dh->reasm_sz);
    return new_dh;
}

/********************************************************************************************
* 函数名：Net_Device_Unregister
* 描  述：NET 设备注销（释放队列、寄存器表及句柄，必要时收缩外设 xfer 缓冲）
* 输  入：@param: *ph -> 外设；@param: *dev -> 设备句柄
* 输  出：无
* 调  用：外部调用
********************************************************************************************/
void Net_Device_Unregister(Net_Periph_t *ph, Net_Device_t *dev)
{
    uint8_t idx;

    for(idx = 0; idx < netctrl.periph_cnt; idx++)
        if(netctrl.periph[idx] == ph)
            break;

    if(idx == netctrl.periph_cnt)
    {
        DEV_PRINTF("%s -> 设备注销 -> 未注册该外设！\r\n", dev->name);
        return;
    }

    for(idx = 0; idx < ph->num; idx++)
        if(ph->dev[idx] == dev)
            break;

    if(idx == ph->num)
    {
        DEV_PRINTF("%s -> 设备注销 -> 未注册该设备！\r\n", dev->name);
        return;
    }

    Net_TxData_t *txd = dev->txd;
    for(uint16_t i = 0; i < dev->qcfg.txq_depth; i++)
    {
        Dev_Mem_Release(txd->buf);
        txd++;
    }

    Net_RxData_t *rxd = dev->rxd;
    for(uint16_t i = 0; i < dev->qcfg.rxq_depth; i++)
    {
        Dev_Mem_Release(rxd->buf);
        rxd++;
    }

    Dev_Id_Release(dev->id);

    if(dev->reg_tb_owned && dev->reg_tb)
    {
        Dev_Mem_Release(dev->reg_tb);
        dev->reg_tb = NULL;
        dev->reg_sz = 0;
        dev->reg_tb_owned = 0;
    }

    Dev_Mem_Release(dev->txd);
    Dev_Mem_Release(dev->rxd);
    Dev_Mem_Release(dev->txq);
    Dev_Mem_Release(dev->rxq);
    
    /* 释放统一重组缓冲区 */
    if(dev->reasm_buf)
    {
        Dev_Mem_Release(dev->reasm_buf);
        dev->reasm_buf = NULL;
    }

    DEV_PRINTF("%s -> 设备注销 -> 注销成功！\r\n", dev->name);
    ph->dev[--ph->num] = NULL;
    Dev_Mem_Release(dev);

    uint16_t temp = 0;
    for(uint16_t i = 0; i < ph->num; i++)
    {
        if(temp < ph->dev[i]->qcfg.txb_size)
            temp = ph->dev[i]->qcfg.txb_size;
        if(temp < ph->dev[i]->qcfg.rxb_size)
            temp = ph->dev[i]->qcfg.rxb_size;
    }

    if(!temp)
    {
        temp += NET_CRC_LENTH;
        if(ph->xfer.xsize <= temp)
            return;

        ph->xfer.xsize = temp;
        ph->xfer.send = Dev_Mem_Realloc(ph->xfer.send, ph->xfer.xsize);
        ph->xfer.recv = Dev_Mem_Realloc(ph->xfer.recv, ph->xfer.xsize);
        ph->xfer.data = Dev_Mem_Realloc(ph->xfer.data, ph->xfer.xsize);

#if NET_TRANSMIT_ACK
        ph->xfer.ackp = Dev_Mem_Realloc(ph->xfer.ackp, ph->xfer.xsize);
        memset(ph->xfer.ackp, 0, ph->xfer.xsize);
#endif

        memset(ph->xfer.send, 0, ph->xfer.xsize);
        memset(ph->xfer.recv, 0, ph->xfer.xsize);
        memset(ph->xfer.data, 0, ph->xfer.xsize);
    }
}

/********************************************************************************************
* 函数名：Net_TxQueue_Push
* 描  述：发送数据入队（待 Net_Transmit_Task 组帧发出）
* 输  入：@param: *dev -> 设备；@param: *sdata -> 待发 PDU 数据；@param: size -> 字节数
* 输  出：@retval: true -> 入队成功；false -> 失败
* 调  用：外部调用
********************************************************************************************/
bool Net_TxQueue_Push(Net_Device_t *dev, uint8_t *sdata, uint16_t size)
{
    if(!dev->transmit)
        DEV_PRINTF("设备未注册！\r\n");
    else if(!size)
        DEV_PRINTF("%s -> 发送数据入队失败，数据大小不能为0！ t: %u\r\n", crt_dev->name, DEV_GET_1MS_TICK_FUN());
    else if(size <= dev->qcfg.txb_size)
    {
        uint16_t push = Dev_Get_Queue_Idle((void *)dev->txq);
        if(push != QUEUE_INVALID_IDX)
        {
            Net_TxData_t *txd = dev->txd;
            txd += push;
            txd->size = size;
            memcpy(txd->buf, sdata, size);
            Dev_Queue_Push((void *)dev->txq);
            return true;
        }
        DEV_PRINTF("%s -> 发送数据入队失败，缓存已满！ t: %u\r\n", crt_dev->name, DEV_GET_1MS_TICK_FUN());
    }
    else
        DEV_PRINTF("%s -> 发送数据入队失败，数据溢出！ t: %u\r\n", crt_dev->name, DEV_GET_1MS_TICK_FUN());

    return false;
}

/********************************************************************************************
* 函数名：Net_RxQueue_Push
* 描  述：解析后的接收 PDU 入队（供 Net_Resolve_Task 回调应用层）
* 输  入：@param: *rdata -> 数据；@param: size -> 字节数
* 输  出：@retval: true -> 入队成功；false -> 失败
* 调  用：内部调用
********************************************************************************************/
static bool Net_RxQueue_Push(uint8_t *rdata, uint16_t size)
{
    if(!size)
        DEV_PRINTF("%s -> 接收数据入队失败，数据长度为：0！ t: %u\r\n", crt_dev->name, DEV_GET_1MS_TICK_FUN());
    else if(size <= crt_dev->qcfg.rxb_size)
    {
        uint16_t push = Dev_Get_Queue_Idle((void *)crt_dev->rxq);
        if(push != QUEUE_INVALID_IDX)
        {
            Net_RxData_t *rxd = crt_dev->rxd;
            rxd += push;
            memcpy(rxd->buf, rdata, size);
            rxd->size = size;
            Dev_Queue_Push((void *)crt_dev->rxq);
            return true;
        }
        DEV_PRINTF("%s -> 接收数据入队失败，缓存已满！ t: %u\r\n", crt_dev->name, DEV_GET_1MS_TICK_FUN());
    }
    else
        DEV_PRINTF("%s -> 接收数据入队失败，数据溢出！ t: %u\r\n", crt_dev->name, DEV_GET_1MS_TICK_FUN());

    return false;
}

/********************************************************************************************
* 函数名：Net_Txbuf_Clear
* 描  述：清空当前外设发送缓冲（crt_xfer->send）与 txsize
* 输  入：无（使用 crt_xfer）
* 输  出：无
* 调  用：内部调用
********************************************************************************************/
static void Net_Txbuf_Clear(void)
{
    crt_xfer->txsize = 0;
    memset((void *)crt_xfer->send, 0, crt_xfer->xsize);
}

/********************************************************************************************
* 函数名：Net_Txbuf_Write
* 描  述：将 PDU 填入发送缓冲并追加 Modbus RTU CRC（首字节置为本机 addr）
* 输  入：@param: *data -> PDU；@param: size -> PDU 字节数
* 输  出：@retval: true -> 成功；false -> 指针/长度非法或缓冲溢出
* 调  用：内部调用
********************************************************************************************/
static bool Net_Txbuf_Write(uint8_t *data, uint16_t size)
{
    if((data == NULL) || !size)
        DEV_PRINTF("TXB -> 数据指针/大小不能为空！ t: %u\r\n", DEV_GET_1MS_TICK_FUN());
    /* 正常路径：上一帧已发出后 Net_Txbuf_Clear，此处 txsize==0。本式=「已有占用 + 本段PDU + CRC2」须<=send 容量；非 0 时仅表示可在缓冲内连续排布多帧（每帧仍自含 CRC） */
    else if((crt_xfer->txsize + NET_CRC_LENTH + size) <= crt_xfer->xsize)
    {
        uint16_t pdu_start = crt_xfer->txsize;
        memcpy((void *)&crt_xfer->send[pdu_start], (void *)data, size);
        crt_xfer->send[pdu_start] = crt_dev->addr;
        crt_xfer->txsize += size;

        uint16_t c = Net_Modbus_Crc16(crt_xfer->send + pdu_start, size);
        crt_xfer->send[crt_xfer->txsize++] = (uint8_t)(c & 0xFF);
        crt_xfer->send[crt_xfer->txsize++] = (uint8_t)(c >> 8);

        return true;
    }
    else if(size > crt_xfer->xsize)
        DEV_PRINTF("TXB -> 数据溢出！ t: %u\r\n", DEV_GET_1MS_TICK_FUN());

    return false;
}

/********************************************************************************************
* 函数名：Net_Dev_Reg_Count
* 描  述：计算设备保持寄存器个数（reg_sz/2）
* 输  入：@param: *dev -> 设备
* 输  出：@retval: 寄存器个数；无效为 0
* 调  用：内部调用
********************************************************************************************/
static uint32_t Net_Dev_Reg_Count(const Net_Device_t *dev)
{
    if(!dev || !dev->reg_tb || dev->reg_sz < 2)
        return 0;
    return (uint32_t)(dev->reg_sz / 2);
}

/********************************************************************************************
* 函数名：Net_Reg_Holding_Read_U16
* 描  述：按寄存器地址读取保持寄存器（16 位，小端表布局）
* 输  入：@param: *dev -> 设备；@param: reg_addr -> 逻辑寄存器地址（按个数取模）
* 输  出：@retval: 寄存器值
* 调  用：内部/外部调用
********************************************************************************************/
uint16_t Net_Reg_Holding_Read_U16(Net_Device_t *dev, uint16_t reg_addr)
{
    uint32_t rc = Net_Dev_Reg_Count(dev);
    if(!rc)
        return 0;
    uint32_t idx = (uint32_t)reg_addr % rc;
    return ((uint16_t *)(void *)dev->reg_tb)[idx];
}

/********************************************************************************************
* 函数名：Net_Reg_Holding_Read_U32
* 描  述：按寄存器地址读取保持寄存器（32 位，占用 2 个连续寄存器，小端序）
* 输  入：@param: *dev -> 设备；@param: reg_addr -> 起始逻辑寄存器地址
* 输  出：@retval: 32 位寄存器值
* 调  用：内部/外部调用
********************************************************************************************/
uint32_t Net_Reg_Holding_Read_U32(Net_Device_t *dev, uint16_t reg_addr)
{
    uint32_t rc = Net_Dev_Reg_Count(dev);
    if(!rc)
        return 0;
    
    /* 读取低 16 位（第一个寄存器） */
    uint32_t idx = (uint32_t)reg_addr % rc;
    uint32_t low_word = ((uint16_t *)(void *)dev->reg_tb)[idx];
    
    /* 读取高 16 位（第二个寄存器） */
    idx = (uint32_t)(reg_addr + 1U) % rc;
    uint32_t high_word = ((uint16_t *)(void *)dev->reg_tb)[idx];
    
    /* 组合成 32 位值（小端序） */
    return (low_word & 0xFFFFU) | ((high_word & 0xFFFFU) << 16U);
}

/********************************************************************************************
* 函数名：Net_Reg_Holding_Write_U16
* 描  述：按寄存器地址写入保持寄存器（16 位）
* 输  入：@param: *dev -> 设备；@param: reg_addr -> 逻辑地址；@param: val -> 写入值
* 输  出：无
* 调  用：内部/外部调用
********************************************************************************************/
void Net_Reg_Holding_Write_U16(Net_Device_t *dev, uint16_t reg_addr, uint16_t val)
{
    uint32_t rc = Net_Dev_Reg_Count(dev);
    if(!rc)
        return;
    uint32_t idx = (uint32_t)reg_addr % rc;
    ((uint16_t *)(void *)dev->reg_tb)[idx] = val;
}

/********************************************************************************************
* 函数名：Net_Reg_Holding_Write_U32
* 描  述：按寄存器地址写入保持寄存器（32 位，占用 2 个连续寄存器，小端序）
* 输  入：@param: *dev -> 设备；@param: reg_addr -> 起始逻辑地址；@param: val -> 写入值
* 输  出：无
* 调  用：内部/外部调用
********************************************************************************************/
void Net_Reg_Holding_Write_U32(Net_Device_t *dev, uint16_t reg_addr, uint32_t val)
{
    uint32_t rc = Net_Dev_Reg_Count(dev);
    if(!rc)
        return;
    /* 先写低 16 位（第一个寄存器） */
    uint32_t idx = (uint32_t)reg_addr % rc;
    ((uint16_t *)(void *)dev->reg_tb)[idx] = (uint16_t)(val & 0xFFFFU);
    /* 再写高 16 位（第二个寄存器） */
    idx = (uint32_t)(reg_addr + 1U) % rc;
    ((uint16_t *)(void *)dev->reg_tb)[idx] = (uint16_t)((val >> 16U) & 0xFFFFU);
}

/********************************************************************************************
* 函数名：Net_Slave_Apply_Write06
* 描  述：解析写单个寄存器请求（功能码 0x06）并更新保持寄存器
* 输  入：@param: *dev -> 设备；@param: *pdu -> PDU；@param: pdu_len -> 长度
* 输  出：@retval: true -> 成功
* 调  用：内部调用
********************************************************************************************/
static bool Net_Slave_Apply_Write06(Net_Device_t *dev, const uint8_t *pdu, uint16_t pdu_len)
{
    if(pdu_len < 6)
        return false;
    uint16_t reg = net_load_u16_le(&pdu[2]);
    uint16_t val = net_load_u16_le(&pdu[4]);
    Net_Reg_Holding_Write_U16(dev, reg, val);

    Net_Resolve_Handle(dev, NET_FC_WRITE_SINGLE_REQ, reg, 1U);
    return true;
}

/********************************************************************************************
* 函数名：Net_Slave_Apply_Write10
* 描  述：解析写多个寄存器请求（功能码 0x10）并更新保持寄存器
* 输  入：@param: *dev -> 设备；@param: *pdu -> PDU；@param: pdu_len -> 长度
* 输  出：@retval: true -> 成功
* 调  用：内部调用
********************************************************************************************/
static bool Net_Slave_Apply_Write10(Net_Device_t *dev, const uint8_t *pdu, uint16_t pdu_len)
{
    if(pdu_len < 7)
        return false;
    uint16_t start = net_load_u16_le(&pdu[2]);
    uint16_t qreg = net_load_u16_le(&pdu[4]);
    uint8_t bc = pdu[6];
    if(pdu_len < (uint16_t)(7 + bc))
        return false;
    if((uint32_t)qreg * 2 != (uint32_t)bc)
        return false;
    
    /* 普通寄存器写入 */
    for(uint16_t i = 0; i < bc / 2; i++)
    {
        uint16_t v = net_load_u16_le(&pdu[7 + i * 2]);
        Net_Reg_Holding_Write_U16(dev, (uint16_t)(start + i), v);
    }

    /* 历史查询 reg108/112、时间同步 reg94 等依赖应用层回调，不可仅入 RX 队列 */
    Net_Resolve_Handle(dev, NET_FC_WRITE_MULTI_REQ, start, qreg);

    return true;
}

/********************************************************************************************
* 函数名：Net_Slave_RegRangeOk
* 描  述：从机读请求等场景下检查起始地址与数量是否合法
* 输  入：@param: *dev -> 设备；@param: start -> 起始寄存器；@param: qty -> 数量
* 输  出：@retval: true -> 合法
* 调  用：内部调用
********************************************************************************************/
static bool Net_Slave_RegRangeOk(const Net_Device_t *dev, uint16_t start, uint16_t qty)
{
    uint32_t reg_count = Net_Dev_Reg_Count(dev);
    if(!reg_count)
        return false;
    if(!qty || ((uint32_t)start + (uint32_t)qty) > 0xFFFF)
        return false;
    if((uint32_t)start >= reg_count)
        return false;
    if((uint32_t)start + (uint32_t)qty > reg_count)
        return false;
    return true;
}

/********************************************************************************************
* 函数名：Net_Protocol_Master_OnResponse
* 描  述：本机作 Modbus 主站时，对合法应答 PDU 做调试打印（异常/读/写应答）
* 输  入：@param: *pdu -> PDU；@param: pdu_len -> 长度
* 输  出：无
* 调  用：内部调用
********************************************************************************************/
void Net_Protocol_Master_OnResponse(const uint8_t *pdu, uint16_t pdu_len)
{
    if(pdu_len < 2u)
        return;

    uint8_t fc = pdu[1];

    if(fc & NET_FC_EXCEPTION_MASK)
    {
        if(pdu_len < 3)
        {
            NET_MASTER_RESP_PRINTF("NET master RX: exc len=%u\r\n", (unsigned)pdu_len);
            return;
        }
        NET_MASTER_RESP_PRINTF("NET master RX: exc fc=%02X base=%02X code=%02X\r\n",
                                fc, (uint8_t)(fc & (uint8_t)~NET_FC_EXCEPTION_MASK), exc);
        return;
    }

    switch(fc)
    {
        case NET_FC_READ_HOLDING_RESP:
            if(pdu_len >= 3)
            {
                uint8_t bc = pdu[2];
                if((pdu_len >= (uint16_t)(3 + bc)) && ((bc % 2) == 0))
                    NET_MASTER_RESP_PRINTF("NET master RX 0x13: byte_count=%u\r\n", (unsigned)bc);
            }
            break;

        case NET_FC_WRITE_SINGLE_RESP:
        case NET_FC_WRITE_MULTI_RESP:
            if(pdu_len >= 6)
            {
                uint16_t reg = net_load_u16_le(&pdu[2]);
                NET_MASTER_RESP_PRINTF("NET master RX: fc=%02X start=%04X\r\n", (unsigned)fc, (unsigned)reg);
            }
            break;

        case NET_FC_READ_SINGLE_RESP:
            if(pdu_len >= 6)
            {
                uint16_t val = net_load_u16_le(&pdu[4]);
                NET_MASTER_RESP_PRINTF("NET master RX 0x15: val=%04X\r\n", (unsigned)val);
            }
            break;

        default:
            break;
    }
}

/********************************************************************************************
* 函数名：Net_IsHostUploadAckFc
* 描  述：上位机对 0x23/0x25 主动上传回写的确认/异常功能码
********************************************************************************************/
static inline bool Net_IsHostUploadAckFc(uint8_t fc)
{
    return (fc == NET_FC_WRITE_MULTI_RESP) || (fc == NET_FC_WRITE_SINGLE_RESP) ||
            (fc == NET_FC_WRITE_MULTI_ERR) || (fc == NET_FC_WRITE_SINGLE_ERR);
}

/********************************************************************************************
* 函数名：Net_Protocol_HandlePdu
* 描  述：应用层解析 PDU（广播/从机请求处理、主站应答分支）
* 输  入：@param: *dev -> 设备；@param: *pdu -> PDU；@param: len -> 长度
* 输  出：无
* 调  用：内部调用（Net_Resolve_Handle 等）
********************************************************************************************/
void Net_Protocol_HandlePdu(Net_Device_t *dev, uint8_t *pdu, uint16_t len)
{
    if(!dev)
        return;
    if(len < 2)
        return;
    if(pdu[0] != dev->addr)
        return;

    uint8_t fc = pdu[1];

    /* 上位机对主动上传的 0x20/0x16（及 0x90/0x86）确认：非应用层业务帧，静默忽略 */
    if(Net_IsHostUploadAckFc(fc))
        return;

    if(fc & NET_FC_EXCEPTION_MASK)
    {
        Net_Protocol_Master_OnResponse(pdu, len);
        return;
    }

    if((fc == NET_FC_READ_HOLDING_RESP) || (fc == NET_FC_READ_SINGLE_RESP))
    {
        Net_Protocol_Master_OnResponse(pdu, len);
        return;
    }

    switch(fc)
    {
        case NET_FC_READ_HOLDING_REQ:
            if(len >= 6)
            {
                uint16_t start = net_load_u16_le(&pdu[2]);
                uint16_t qty = net_load_u16_le(&pdu[4]);
                if(!Net_Slave_RegRangeOk(dev, start, qty))
                    return;
                
                // printf("start: %d, qty: %d\r\n\r\n", start, qty);
                // for(uint16_t i = 0; i < qty; i++)
                // {
                //     printf("reg: %d, val: %d\r\n", start + i, Net_Reg_Holding_Read_U16(dev, start + i));
                // }
            }
            break;

        case NET_FC_WRITE_SINGLE_REQ:
        case NET_FC_WRITE_MULTI_REQ:
            /* 从机写寄存器已在 Net_Modbus_Slave_Process -> Apply 中完成（含应用层回调） */
            break;

        case NET_FC_READ_SINGLE_REQ:
            // printf("start: %d, qty: %d\r\n\r\n", (uint16_t)pdu[2] | ((uint16_t)pdu[3] << 8), (uint16_t)pdu[4] | ((uint16_t)pdu[5] << 8));
            // for(uint16_t i = 0; i < qty; i++)
            // {
            //     printf("reg: %d, val: %d\r\n", start + i, Net_Reg_Holding_Read_U16(dev, start + i));
            // }
            break;

        case NET_FC_ACTIVE_UPLOAD:
            if(len >= 6)
            {
                uint16_t reg = net_load_u16_le(&pdu[3]);
                (void)reg;
            }
            break;

        default:
            break;
    }
}

/********************************************************************************************
* 函数名：Net_Slave_AppendCrc
* 描  述：对 out[0..len_before_crc-1] 计算 Modbus CRC 并追加低字节、高字节
* 输  入：@param: out -> 缓冲；@param: len_before_crc -> 参与 CRC 的字节数
* 输  出：@retval: 追加 CRC 后的总长度（len_before_crc + 2）
* 调  用：内部调用（Net_Modbus_Slave_Process）
********************************************************************************************/
static uint16_t Net_Slave_AppendCrc(uint8_t *out, uint16_t len_before_crc)
{
    uint16_t c = Net_Modbus_Crc16(out, len_before_crc);
    out[len_before_crc] = (uint8_t)(c & 0xFF);
    out[len_before_crc + 1] = (uint8_t)(c >> 8);
    return (uint16_t)(len_before_crc + 2);
}

/********************************************************************************************
* 函数名：Net_Modbus_Slave_Process
* 描  述：从机侧：校验 CRC 后按功能码生成应答帧
*         读保持(0x03)应答长度随寄存器数量变化；写单/写多/读单正常应答均为 6 字节 PDU + CRC = 8 字节
* 输  入：@param: *dev -> 设备；@param: *wire -> 线上一帧；@param: wire_len -> 线长
*         @param: *out -> 应答缓冲；@param: out_max -> 缓冲容量
* 输  出：@retval: 应答字节数；0 表示不应答或错误
* 调  用：外部调用（如 BSP 收满一帧后）
********************************************************************************************/
uint16_t Net_Modbus_Slave_Process(Net_Device_t *dev, uint8_t *wire, uint16_t wire_len, uint8_t *out, uint16_t out_max)
{
    if(!dev)
        return 0;
    if(wire_len < 8U || out_max < 8U)
        return 0;
    if(!Net_Raw_FrameCrcOk(wire, wire_len))
        return 0;

    uint16_t pdu_len = (uint16_t)(wire_len - 2);
    uint8_t addr = wire[0];
    uint8_t fc = wire[1];

    if(addr != dev->addr)
        return 0;

    if(fc == NET_FC_READ_HOLDING_REQ && pdu_len >= 6)
    {
        uint16_t start = net_load_u16_le(&wire[2]);
        uint16_t qty = net_load_u16_le(&wire[4]);
        
        if(!Net_Slave_RegRangeOk(dev, start, qty))
        {
            if(out_max < 6)
                return 0;
            out[0] = addr;
            out[1] = NET_FC_READ_HOLDING_ERR;
            out[2] = wire[2];
            out[3] = wire[3];
            out[4] = 0x00;
            out[5] = 0x02;
            return Net_Slave_AppendCrc(out, 6);
        }
        uint32_t bytes = (uint32_t)qty * 2;
        if(bytes > (out_max - 5) || bytes > 252)
            return 0;
        out[0] = addr;
        out[1] = NET_FC_READ_HOLDING_RESP;
        out[2] = (uint8_t)bytes;
        out[3] = (uint8_t)(start & 0xFF);
        out[4] = (uint8_t)((start >> 8) & 0xFF);
        for(uint16_t i = 0; i < qty; i++)
        {
            uint16_t v = Net_Reg_Holding_Read_U16(dev, (uint16_t)(start + i));
            out[5 + i * 2] = (uint8_t)(v & 0xFF);
            out[6 + i * 2] = (uint8_t)(v >> 8);
        }
        return Net_Slave_AppendCrc(out, (uint16_t)(5 + bytes));
    }

    if(fc == NET_FC_WRITE_MULTI_REQ && pdu_len >= 7)
    {
        if(!Net_Slave_Apply_Write10(dev, wire, pdu_len))
            return 0;
        if(out_max < 8)
            return 0;
        out[0] = addr;
        out[1] = NET_FC_WRITE_MULTI_RESP;
        
        for(uint16_t i = 0; i < 4; i++)
            out[2 + i] = wire[2 + i];
        
        return Net_Slave_AppendCrc(out, 6);
    }

    if(fc == NET_FC_WRITE_SINGLE_REQ && pdu_len >= 6)
    {
        if(!Net_Slave_Apply_Write06(dev, wire, pdu_len))
            return 0;
        if(out_max < 8)
            return 0;
        out[0] = addr;
        out[1] = NET_FC_WRITE_SINGLE_RESP;
        
        //        memcpy(&out[2], &wire[2], 4);  // 可能未字节对齐，导致程序崩溃
        for(uint16_t i = 0; i < 4; i++)
            out[2 + i] = wire[2 + i];
        
        return Net_Slave_AppendCrc(out, 6);
    }

    if(fc == NET_FC_READ_SINGLE_REQ && pdu_len >= 6)
    {
        if(out_max < 8)
            return 0;
        uint16_t reg = net_load_u16_le(&wire[2]);
        uint16_t qty = net_load_u16_le(&wire[4]);
        if(!Net_Slave_RegRangeOk(dev, reg, qty))
        {
            if(out_max < 6)
                return 0;
            out[0] = addr;
            out[1] = NET_FC_READ_SINGLE_ERR;
            out[2] = wire[2];
            out[3] = wire[3];
            out[4] = 0x00;
            out[5] = 0x02;
            return Net_Slave_AppendCrc(out, 6);
        }
        uint16_t v = Net_Reg_Holding_Read_U16(dev, reg);
        out[0] = addr;
        out[1] = NET_FC_READ_SINGLE_RESP;
        out[2] = wire[2];
        out[3] = wire[3];
        out[4] = (uint8_t)(v & 0xFF);
        out[5] = (uint8_t)(v >> 8);
        return Net_Slave_AppendCrc(out, 6);
    }

    return 0;
}

#if NET_TRANSMIT_ACK
/********************************************************************************************
* 函数名：Net_IsRespFcMatch
* 描  述：判断应答功能码是否与请求功能码匹配（含异常码 功能码|0x80）
* 输  入：@param: req_fc -> 请求功能码；@param: resp_fc -> 应答功能码
* 输  出：@retval: true -> 匹配
* 调  用：内部调用（Net_Ack_TryConsumeResponse）
********************************************************************************************/
static inline bool Net_IsRespFcMatch(uint8_t req_fc, uint8_t resp_fc)
{
    if(resp_fc == (uint8_t)(req_fc | NET_FC_EXCEPTION_MASK))
        return true;
    switch(req_fc)
    {
        case NET_FC_READ_HOLDING_REQ: return resp_fc == NET_FC_READ_HOLDING_RESP;
        case NET_FC_WRITE_SINGLE_REQ: return resp_fc == NET_FC_WRITE_SINGLE_RESP;
        case NET_FC_WRITE_MULTI_REQ: return resp_fc == NET_FC_WRITE_MULTI_RESP;
        case NET_FC_READ_SINGLE_REQ: return resp_fc == NET_FC_READ_SINGLE_RESP;
        case NET_FC_ACTIVE_UPLOAD:
            return (resp_fc == NET_FC_WRITE_MULTI_RESP) ||
                    (resp_fc == NET_FC_WRITE_MULTI_ERR);
        case NET_FC_ACTIVE_UPLOAD_SINGLE:
            return (resp_fc == NET_FC_WRITE_SINGLE_RESP) ||
                    (resp_fc == NET_FC_WRITE_SINGLE_ERR);
        default: return false;
    }
}

/********************************************************************************************
* 函数名：Net_IsRespParamMatch
* 描  述：核对应答 PDU 中的寄存器地址/数量是否与本次待发请求一致
*         避免 0x23 等 ACK 被其它主动上传（传感器/阈值）的 0x20 误消费
* 输  入：@param: req_fc -> 请求功能码；@param: resp_fc -> 应答功能码
*         @param: req -> 本次 send 缓冲；@param: req_len -> send 长度（含 CRC）
*         @param: resp -> 应答 PDU；@param: resp_len -> 应答 PDU 长度（不含 CRC）
* 输  出：@retval: true -> 参数匹配
* 调  用：内部调用（Net_Ack_TryConsumeResponse）
********************************************************************************************/
static bool Net_IsRespParamMatch(uint8_t req_fc, uint8_t resp_fc,
                                  const uint8_t *req, uint16_t req_len,
                                  const uint8_t *resp, uint16_t resp_len)
{
    uint16_t req_reg;
    uint16_t req_param;
    uint16_t resp_reg;
    uint16_t resp_param;

    if(!req || !resp)
        return false;

    if(resp_fc == (uint8_t)(req_fc | NET_FC_EXCEPTION_MASK))
    {
        if(resp_len < 4U)
            return false;
        resp_reg = net_load_u16_le(&resp[2]);
        switch(req_fc)
        {
            case NET_FC_ACTIVE_UPLOAD:
                if(req_len < 5U)
                    return false;
                req_reg = net_load_u16_le(&req[3]);
                break;
            case NET_FC_WRITE_MULTI_REQ:
                if(req_len < 4U)
                    return false;
                req_reg = net_load_u16_le(&req[2]);
                break;
            case NET_FC_WRITE_SINGLE_REQ:
            case NET_FC_ACTIVE_UPLOAD_SINGLE:
                if(req_len < 4U)
                    return false;
                req_reg = net_load_u16_le(&req[2]);
                break;
            default:
                return true;
        }
        return (resp_reg == req_reg);
    }

    if(resp_len < 6U)
        return false;

    resp_reg = net_load_u16_le(&resp[2]);
    resp_param = net_load_u16_le(&resp[4]);

    switch(req_fc)
    {
        case NET_FC_ACTIVE_UPLOAD:
            if(req_len < 5U)
                return false;
            req_reg = net_load_u16_le(&req[3]);
            req_param = (uint16_t)(req[2] / 2U);
            return (resp_reg == req_reg) && (resp_param == req_param);

        case NET_FC_WRITE_MULTI_REQ:
            if(!Net_FrameLen_FormatOk(req, req_len))
                return false;
            req_reg = net_load_u16_le(&req[2]);
            req_param = net_load_u16_le(&req[4]);
            return (resp_reg == req_reg) && (req_param == resp_param);

        case NET_FC_WRITE_SINGLE_REQ:
        case NET_FC_ACTIVE_UPLOAD_SINGLE:
            if(req_len < 6U)
                return false;
            req_reg = net_load_u16_le(&req[2]);
            req_param = net_load_u16_le(&req[4]);
            return (resp_reg == req_reg) && (req_param == resp_param);

        default:
            return true;
    }
}

/********************************************************************************************
* 函数名：Net_Ack_Cancel_Wait
* 描  述：取消应答等待（清 ACK 状态与重发计数）
* 输  入：无
* 输  出：无
* 调  用：内部调用
********************************************************************************************/
static void Net_Ack_Cancel_Wait(void)
{
    crt_xfer->ack_sta = NET_ACK_NOP;
    crt_xfer->ackbusy = false;
    crt_xfer->retx_times = 0;
}

/********************************************************************************************
* 函数名：Net_Ack_TryConsumeResponse
* 描  述：在等待应答状态下，从接收缓冲中匹配与本次请求对应的合法应答 PDU（含异常帧）
* 输  入：无（使用 crt_xfer->recv、rxlen、last_req_*）
* 输  出：@retval: true -> 已匹配并入队；false -> 未匹配
* 调  用：内部调用（Net_Protocol_Resolve）
********************************************************************************************/
static bool Net_Ack_TryConsumeResponse(void)
{
    uint16_t n = crt_xfer->rxlen;
    if(!n)
        n = crt_xfer->xsize;

    uint16_t pos = 0;
    
#if NET_RECV_DEBUG
    DEV_PRINTF("[ACK] 功能码=0x%02X, 从机地址=0x%02X, 请求寄存器=0x%04X, t=%u\r\n", 
                crt_xfer->last_req_fc, crt_xfer->last_req_addr, crt_xfer->last_req_reg, DEV_GET_1MS_TICK_FUN());
    // DEV_PRINTF("[ACK] 接收数据：");
    // for(uint16_t i = 0; i < n && i < 32; i++)
    // {
    //     DEV_PRINTF("%02X ", crt_xfer->recv[i]);
    // }
    // DEV_PRINTF("\r\n");
#endif
    
    while(pos + 4 <= n)
    {
        while(pos < n && crt_xfer->recv[pos] == 0)
            pos++;

        if(pos + 4 > n)
            break;

        uint16_t flen = 0;
        uint16_t maxtry = Net_GetCrcSearchMaxTry((uint16_t)(n - pos),
                            (crt_dev != NULL) ? crt_dev->qcfg.rxb_size : 0U);

        for(uint16_t try = maxtry; try >= 4U; try--)
        {
            if(Net_Raw_FrameCrcOk(crt_xfer->recv + pos, try) &&
                Net_FrameLen_FormatOk(crt_xfer->recv + pos, try))
            {
                flen = try;
                break;
            }
        }

        if(!flen)
        {
            pos++;
            continue;
        }

        uint16_t pdu_len = (uint16_t)(flen - 2);
        uint8_t *p = crt_xfer->recv + pos;
        
        if((p[0] == crt_xfer->last_req_addr) &&
            Net_IsRespFcMatch(crt_xfer->last_req_fc, p[1]) &&
            Net_IsRespParamMatch(crt_xfer->last_req_fc, p[1],
                                crt_xfer->send, crt_xfer->txsize, p, pdu_len))
        {
#if NET_RECV_DEBUG
            DEV_PRINTF("ACK -> ");
            Net_Data_Printf((uint8_t *)p, pdu_len);
            DEV_PRINTF("\r\n");
#endif
            if(pdu_len > crt_xfer->xsize)
                pdu_len = crt_xfer->xsize;
            memcpy(crt_xfer->data, p, pdu_len);
// #if NET_RECV_DEBUG
//                 NET_RECV_PRINTF("RX ACK -> ");
//                 Net_Data_Printf((uint8_t *)crt_xfer->data, pdu_len);
// #endif
            /* 发送侧 ACK 已在协议层消费，不入 rxq，避免应用层误报未知功能码 */
            crt_xfer->txbusy = false;
            Net_Txbuf_Clear();
            Net_Ack_Cancel_Wait();
            memset(crt_xfer->recv, 0, crt_xfer->xsize);
            crt_xfer->rxlen = 0;
            return true;
        }
        pos += flen;
    }
    return false;
}

/********************************************************************************************
* 函数名：Net_Ack_Task
* 描  述：应答超时与重发（风格同 SPI 协议层）
* 输  入：无
* 输  出：无
* 调  用：内部调用（Net_Thread_Task）
********************************************************************************************/
static void Net_Ack_Task(void)
{
    switch(crt_xfer->ack_sta)
    {
        case NET_ACK_NOP:
        case NET_ACK_OK:
            break;
        case NET_ACK_WAITING:
            if(Dev_Tk_Wait(NET_ACK_WAITING_TIME, crt_xfer->ack_time))
            {
                if(++crt_xfer->retx_times <= NET_RETX_TIMES)
                {
                    crt_xfer->ack_sta = NET_ACK_ERR;
                    DEV_PRINTF("RX -> %s -> 应答超时 -> 重发次数：%d t: %u\r\n", crt_dev->name, crt_xfer->retx_times, DEV_GET_1MS_TICK_FUN());
                }
                else
                {
                    Net_Ack_Cancel_Wait();
                    crt_xfer->txbusy = false;
                    DEV_PRINTF("RX -> %s -> 应答失败！ t: %u\r\n", crt_dev->name, DEV_GET_1MS_TICK_FUN());
                    Net_Send_Err_Handle(crt_dev->id, crt_xfer->send, crt_xfer->txsize);
                    Net_Txbuf_Clear();
                }
            }
            break;
        case NET_ACK_ERR:
            Dev_Tk_Init((uint32_t *)&crt_xfer->ack_time);
            break;
        default:
            DEV_PRINTF("RX -> %s -> 异常状态 %d！ t: %u\r\n\r\n", crt_dev->name, (int)crt_xfer->ack_sta, DEV_GET_1MS_TICK_FUN());
            break;
    }
}
#endif

/********************************************************************************************
* 函数名：Net_Frame_Reasm_Process
* 描  述：通用帧重组处理：处理网络分包/粘包，累积到完整帧
* 输  入：@param: dev -> 设备句柄
*         @param: frame -> 接收到的帧数据（frame_len=0 时表示仅搜索重组缓冲）
*         @param: frame_len -> 帧长度（0 表示不累积新数据）
*         @param: out_pdu -> 输出完整 PDU 的缓冲区
*         @param: out_pdu_size -> 输出 PDU 长度
* 输  出：成功返回 true（out_pdu 有有效数据），false 表示数据不完整
* 调  用：内部调用（Net_Protocol_Resolve）
********************************************************************************************/
static bool Net_Frame_Reasm_Process(Net_Device_t *dev, uint8_t *frame, uint16_t frame_len,
                                     uint8_t *out_pdu, uint16_t *out_pdu_size)
{
    uint16_t max_reasm_size;
    
    /* 严格检查指针有效性 */
    if(!dev || !out_pdu || !out_pdu_size)
    {
        if(out_pdu_size)
            *out_pdu_size = 0;
        return false;
    }
    
    if(!dev->reasm_buf)
    {
        *out_pdu_size = 0;
        return false;
    }
    
    max_reasm_size = dev->reasm_sz;
    if(max_reasm_size == 0)
    {
        DEV_PRINTF("%s -> 重组缓冲配置错误！(rxb_size=%u)\r\n", 
                    dev->name, (unsigned)max_reasm_size);
        *out_pdu_size = 0;
        return false;
    }
    
    /* 累积新数据到重组缓冲（如果有新数据） */
    if(frame_len > 0)
    {
        /* 检查 frame 指针有效性 */
        if(!frame)
        {
            *out_pdu_size = 0;
            return false;
        }
        
        /* 检查是否会溢出
         * 策略：溢出时丢弃旧缓存，优先保留“最新数据窗口”，可显著缩短从坏流恢复时间。
         */
        if((dev->reasm_size + frame_len) > max_reasm_size)
        {
            DEV_PRINTF("%s -> 重组缓冲溢出！(size=%u, frame=%u) -> 丢旧保新\r\n", 
                        dev->name, (unsigned)dev->reasm_size, (unsigned)frame_len);

            if(frame_len >= max_reasm_size)
            {
                uint16_t off = (uint16_t)(frame_len - max_reasm_size);
                for(uint16_t i = 0; i < max_reasm_size; i++)
                    dev->reasm_buf[i] = frame[off + i];
                dev->reasm_size = max_reasm_size;
            }
            else
            {
                for(uint16_t i = 0; i < frame_len; i++)
                    dev->reasm_buf[i] = frame[i];
                dev->reasm_size = frame_len;
            }
        }
        else
        {
            /* 累积数据到重组缓冲 - 使用 for 循环避免 memcpy 可能的对齐问题 */
            uint16_t dst_idx = dev->reasm_size;
            for(uint16_t i = 0; i < frame_len; i++)
                dev->reasm_buf[dst_idx + i] = frame[i];
            dev->reasm_size += frame_len;
        }
    }
    
    /* 如果重组缓冲为空，直接返回 */
    if(dev->reasm_size == 0)
    {
        *out_pdu_size = 0;
        return false;
    }
    
    /* 严格检查重组缓冲大小，防止越界 */
    if(dev->reasm_size > max_reasm_size)
    {
        DEV_PRINTF("%s -> 重组缓冲大小异常！(size=%u, max=%u)\r\n", 
                    dev->name, (unsigned)dev->reasm_size, (unsigned)max_reasm_size);
        dev->reasm_size = 0;
        *out_pdu_size = 0;
        return false;
    }
    
    /* 尝试在重组缓冲中搜索完整帧（滑动窗口 CRC 识别）
     * 核心思想：
     * 1. 从缓冲起始位置搜索所有可能的帧边界
     * 2. 找到第一个 CRC 合法的帧即处理
     * 3. 处理完后将剩余数据移到缓冲起始位置
     * 4. 如果缓冲中有错误帧，新数据累积后可能形成完整帧，自动识别
     */
    uint16_t n = dev->reasm_size;
    uint16_t pos = 0;
    
    while(pos + 4 <= n)
    {
        /* 严格边界检查 */
        if(pos >= max_reasm_size)
        {
            DEV_PRINTF("%s -> 重组缓冲 pos 越界！(pos=%u, max=%u)\r\n", 
                        dev->name, (unsigned)pos, (unsigned)max_reasm_size);
            dev->reasm_size = 0;
            *out_pdu_size = 0;
            return false;
        }
        
        /* 跳过填充零 */
        while(pos < n && pos < max_reasm_size && dev->reasm_buf[pos] == 0)
            pos++;
        
        if(pos + 4 > n)
            break;

        /* 快速过滤：地址不匹配直接跳过，可显著降低坏流下的误判/耗时 */
        if(dev->reasm_buf[pos] != dev->addr)
        {
            pos++;
            continue;
        }

        /* 快速过滤：功能码不在协议集合内时跳过 */
        if(!Net_Fc_IsCandidate(dev->reasm_buf[pos + 1]))
        {
            pos++;
            continue;
        }
        
        /* 滑动窗口搜索 CRC 匹配 */
        uint16_t flen = 0;
        uint16_t pred_len;
        uint16_t maxtry = Net_GetCrcSearchMaxTry((uint16_t)(n - pos), dev->qcfg.rxb_size);
        
        /* 限制最大尝试长度不超过缓冲剩余空间 */
        if(pos + maxtry > max_reasm_size)
            maxtry = max_reasm_size - pos;

        /* 快路径：优先校验预测长度，命中可省掉大部分窗口扫描 */
        pred_len = Net_Fc_PredictFrameLen(dev->reasm_buf + pos, maxtry);
        if((pred_len >= 4U) &&
            Net_Raw_FrameCrcOk(dev->reasm_buf + pos, pred_len) &&
            Net_FrameLen_FormatOk(dev->reasm_buf + pos, pred_len))
            flen = pred_len;
        
        for(uint16_t try_len = 4U; (flen == 0U) && (try_len <= maxtry); try_len++)
        {
            if(try_len == pred_len)
                continue;
            /* 边界检查 */
            if(pos + try_len > max_reasm_size)
                break;
            
            if(Net_Raw_FrameCrcOk(dev->reasm_buf + pos, try_len) &&
                Net_FrameLen_FormatOk(dev->reasm_buf + pos, try_len))
            {
                flen = try_len;
                break;
            }
        }
        
        if(flen)
        {
            /* 找到完整帧，输出 */
            if(pos + flen > max_reasm_size)
            {
                DEV_PRINTF("%s -> 输出帧越界！(pos=%u, flen=%u, max=%u)\r\n", 
                            dev->name, (unsigned)pos, (unsigned)flen, (unsigned)max_reasm_size);
                dev->reasm_size = 0;
                *out_pdu_size = 0;
                return false;
            }
            
            /* 避免未对齐地址上的 memcpy 导致硬件异常 */
            for(uint16_t i = 0; i < flen; i++)
                out_pdu[i] = dev->reasm_buf[pos + i];
            *out_pdu_size = flen;
            
            /* 移除已处理的帧和之前的无效数据，保留剩余数据 */
            uint16_t remain = n - (pos + flen);
            if(remain > 0)
            {
                if(remain > max_reasm_size)
                {
                    DEV_PRINTF("%s -> 剩余数据越界！(remain=%u, max=%u)\r\n", 
                                dev->name, (unsigned)remain, (unsigned)max_reasm_size);
                    dev->reasm_size = 0;
                    *out_pdu_size = 0;
                    return false;
                }
                /* 关键：将剩余数据移到缓冲起始位置，丢弃 pos 之前的所有数据（包括错误帧） */
                for(uint16_t i = 0; i < remain; i++)
                    dev->reasm_buf[i] = dev->reasm_buf[pos + flen + i];
            }
            dev->reasm_size = remain;
            
// #if NET_RECV_DEBUG
//             if((*out_pdu_size < 2U) || !Net_IsHostUploadAckFc(out_pdu[1]))
//             {
//                 NET_RECV_PRINTF("RX(reasm) -> ");
//                 Net_Data_Printf(out_pdu, *out_pdu_size);
//             }
// #endif
            
            return true;
        }
        
        /* 无合法帧，前进 1 字节继续搜索 */
        pos++;
    }
    
    /* 数据不完整，继续累积。
     * 若缓冲已逼近上限且仍无合法帧，执行快速重同步：仅保留末尾少量字节，
     * 以支持“跨包拼接最小帧”并避免后续反复全量 CRC 滑窗导致恢复变慢。
     */
    if((max_reasm_size > 32U) && (n >= (uint16_t)(max_reasm_size - 32U)) && (n > 16U))
    {
        uint16_t start = (uint16_t)(n - 16U);
        for(uint16_t i = 0; i < 16U; i++)
            dev->reasm_buf[i] = dev->reasm_buf[start + i];
        dev->reasm_size = 16U;
    }

    *out_pdu_size = 0;
    return false;
}

/********************************************************************************************
* 函数名：Net_Protocol_Resolve
* 描  述：接收解析入口（类比 CAN 的 Can_Recv_Handle）：在 crt_xfer->recv 上切帧、CRC 校验；
*         满足协议时先尝试从机应答（Net_Modbus_Slave_Process + Ph_Net_Transmit），否则将 PDU 入接收队列供 Net_Resolve_Task 处理
* 输  入：无（使用 crt_xfer、crt_dev）
* 输  出：无
* 调  用：内部调用（Net_Thread_Task）
********************************************************************************************/
static void Net_Protocol_Resolve(void)
{
    uint16_t pdu_len = 0;

    /* 严格检查指针有效性 */
    if(!crt_dev || !crt_xfer)
    {
        DEV_PRINTF("Net_Protocol_Resolve: crt_dev=%p, crt_xfer=%p\r\n", 
                    (void*)crt_dev, (void*)crt_xfer);
        return;
    }

#if NET_TRANSMIT_ACK
    if(crt_xfer->ack_sta == NET_ACK_WAITING)
    {
        /* ACK 等待态：循环提取帧并尝试匹配 ACK 应答
         * 粘包场景下 reasm_buf 可能含多帧，须逐帧提取直到命中 ACK 或耗尽
         */
        if(crt_xfer->rxlen > 0)
        {
            if(crt_xfer->rxlen > crt_xfer->xsize)
            {
                DEV_PRINTF("Net_Protocol_Resolve: drop invalid ack-rxlen=%u (xsize=%u)\r\n",
                            (unsigned)crt_xfer->rxlen, (unsigned)crt_xfer->xsize);
                crt_xfer->rxlen = 0;
                crt_dev->reasm_size = 0;
                return;
            }

            bool ack_first = true;
            bool ack_reasm;
            do {
                ack_reasm = ack_first
                    ? Net_Frame_Reasm_Process(crt_dev, crt_xfer->recv, crt_xfer->rxlen,
                                                crt_xfer->recv, &pdu_len)
                    : Net_Frame_Reasm_Process(crt_dev, NULL, 0,
                                                crt_xfer->recv, &pdu_len);
                ack_first = false;
                if(!ack_reasm || pdu_len == 0)
                    break;
                crt_xfer->rxlen = pdu_len;
                if((crt_xfer->rxlen >= 4U) && Net_Ack_TryConsumeResponse())
                    return;
            } while(1);
            crt_xfer->rxlen = 0;
        }
        return;
    }
#endif

    /* 帧重组 + 处理循环
     * 核心思想：
     *   1. 将新接收数据累积到重组缓冲（第一次调用传入 recv）
     *   2. 循环提取完整帧（后续调用传 NULL/0 纯从 reasm_buf 提取）
     *   3. 每提取一帧立即处理（从机应答或入队）
     *   4. 直到重组缓冲中无更多完整帧
     * 这样既支持跨包分片重组（一帧拆成两包），也正确处理 TCP 粘包（多帧一包到达）
     */
    if(crt_xfer->rxlen > 0)
    {
        if(crt_xfer->rxlen > crt_xfer->xsize)
        {
            DEV_PRINTF("Net_Protocol_Resolve: drop invalid rxlen=%u (xsize=%u)\r\n",
                        (unsigned)crt_xfer->rxlen, (unsigned)crt_xfer->xsize);
            crt_xfer->rxlen = 0;
            crt_dev->reasm_size = 0;
            return;
        }

        bool first_pass = true;
        bool reasm_ok;

        do {
            reasm_ok = first_pass
                ? Net_Frame_Reasm_Process(crt_dev, crt_xfer->recv, crt_xfer->rxlen,
                                            crt_xfer->recv, &pdu_len)
                : Net_Frame_Reasm_Process(crt_dev, NULL, 0,
                                            crt_xfer->recv, &pdu_len);
            first_pass = false;

            if(!reasm_ok || pdu_len == 0)
                break;

            /* 上位机对主动上传的写应答：发送侧 ACK 等待态由 TryConsume 处理；其余情况静默丢弃 */
            if((pdu_len >= 2U) && Net_IsHostUploadAckFc(crt_xfer->recv[1]))
                continue;

            /* 处理提取的合法帧（已通过 CRC + 格式校验） */
            {
                uint16_t olen = Net_Modbus_Slave_Process(crt_dev, crt_xfer->recv, pdu_len,
                                                            crt_xfer->data, crt_xfer->xsize);
                if(olen > 0)
                    crt_dev->transmit(crt_xfer->data, olen);
                
                /* 无论是否有应答，都要调用应用层处理（用于 OTA 等特殊处理） */
                // 先调用 Net_Protocol_HandlePdu 操作寄存器（主站侧处理）
                uint16_t pdu_no_crc = (pdu_len > 2U) ? (uint16_t)(pdu_len - 2U) : pdu_len;
                if(pdu_no_crc > crt_xfer->xsize)
                    pdu_no_crc = crt_xfer->xsize;
                for(uint16_t i = 0; i < pdu_no_crc; i++)
                    crt_xfer->data[i] = crt_xfer->recv[i];
                
                // 调用协议层处理函数（操作寄存器）
                Net_Protocol_HandlePdu(crt_dev, crt_xfer->data, pdu_no_crc);
                
// #if NET_RECV_DEBUG
//                 NET_RECV_PRINTF("RX -> ");
//                 Net_Data_Printf((uint8_t *)crt_xfer->data, pdu_no_crc);
// #endif
                /* 0x06/0x10 已在 Apply 中同步 Net_Resolve_Handle，避免入队失败或重复处理 */
                if(crt_xfer->data[1] != NET_FC_WRITE_SINGLE_REQ &&
                   crt_xfer->data[1] != NET_FC_WRITE_MULTI_REQ)
                {
                    Net_RxQueue_Push((uint8_t *)crt_xfer->data, pdu_no_crc);
                }
            }
        } while(1);
    }
    crt_xfer->rxlen = 0;
}

/********************************************************************************************
* 函数名：Net_Transmit_Task
* 描  述：从发送队列取 PDU 组帧，调用 Net_Transmit_Data（transmit + periph receive）；无待发时仅收
*         发送按 qcfg.txb_size 分片（如 CAN 设 8、网口设较大值）
* 输  入：无（使用 crt_dev、crt_xfer）
* 输  出：无
* 调  用：内部调用（Net_Thread_Task）
********************************************************************************************/
static void Net_Transmit_Task(void)
{
    uint16_t push;

#if NET_TRANSMIT_ACK
    {
        uint16_t size = 0;
        uint8_t *pdata = NULL;

        if(crt_xfer->ackbusy)
            crt_xfer->ackbusy = false;

        if(crt_xfer->ack_sta != NET_ACK_WAITING)
        {
            if(!crt_xfer->txbusy)
            {
                while(1)
                {
                    push = Dev_Get_Queue_Occupied((void *)crt_dev->txq);
                    if(push == QUEUE_INVALID_IDX)
                        break;

                    Net_TxData_t *txd = crt_dev->txd;
                    txd += push;
                    pdata = txd->buf;
                    size = txd->size;
                    Dev_Queue_Pop((void *)crt_dev->txq);

                    if(Net_Txbuf_Write(pdata, size))
                    {
                        crt_xfer->txbusy = true;
                        break;
                    }
                }
            }

            if(crt_xfer->txsize)
            {
                Net_Transmit_Data(crt_xfer->send, crt_xfer->recv, crt_xfer->txsize);
                crt_xfer->last_req_addr = crt_xfer->send[0];
                crt_xfer->last_req_fc = crt_xfer->send[1];
                /* 获取请求的寄存器地址（根据功能码解析） */
                crt_xfer->last_req_reg = 0;  /* 默认值为 0 */
                if(crt_xfer->txsize >= 4)
                {
                    /* 根据功能码确定寄存器地址位置：
                     * 0x03/0x04/0x06/0x10: 地址在 [2:3]
                     * 0x23 (主动上传): 字节数在 [2]，地址在 [3:4]
                     */
                    if(crt_xfer->last_req_fc == 0x23)
                    {
                        /* 功能码 0x23：字节数在第 2 字节，地址在第 3-4 字节 */
                        if(crt_xfer->txsize >= 5)  /* 0x23 需要至少 5 字节 */
                        {
                            /* 使用 net_load_u16_le 安全读取，避免 O2 优化导致的未对齐访问 */
                            crt_xfer->last_req_reg = net_load_u16_le(&crt_xfer->send[3]);
                        }
                    }
                    else
                    {
                        /* 其他功能码：地址在 [2:3] */
                        crt_xfer->last_req_reg = net_load_u16_le(&crt_xfer->send[2]);
                    }
                }
            }
            else
            {
                /* 无待发时仍需调用外设 receive，保证被动入站帧可更新 rxlen */
                crt_xfer->rxlen = (crt_ph->receive ? crt_ph->receive(crt_xfer->recv) : 0u);
#if NET_RECV_DEBUG
                uint16_t p = 0;
                for(p = 0; p < crt_xfer->rxlen; p++)
                    if(crt_xfer->recv[p] != '\0')
                        break;
                if(p < crt_xfer->rxlen)
                {
                    NET_RECV_PRINTF("RX -> ");
                    Net_Data_Printf(crt_xfer->recv, crt_xfer->rxlen);
                }
#endif
            }

            if(crt_xfer->txbusy)
            {
                crt_xfer->ack_sta = NET_ACK_WAITING;
                Dev_Tk_Init(&crt_xfer->ack_time);
            }

#if NET_SEND_DEBUG
            {
                uint16_t p = 0;
                for(p = 0; p < crt_xfer->txsize; p++)
                    if(crt_xfer->send[p] != '\0')
                        break;
                if(p < crt_xfer->txsize)
                {
                    NET_SEND_PRINTF("TX -> ");
                    Net_Data_Printf(crt_xfer->send, crt_xfer->txsize);
                }
            }
#endif
            Net_Protocol_Resolve();
        }
        else
        {
            crt_xfer->rxlen = (crt_ph->receive ? crt_ph->receive(crt_xfer->recv) : 0u);
#if NET_RECV_DEBUG
            {
                uint16_t p = 0;
                for(p = 0; p < crt_xfer->rxlen; p++)
                    if(crt_xfer->recv[p] != '\0')
                        break;
                if(p < crt_xfer->rxlen)
                {
                    NET_RECV_PRINTF("RX -> ");
                    Net_Data_Printf(crt_xfer->recv, crt_xfer->rxlen);
                }
            }
#endif
            Net_Protocol_Resolve();
        }
    }
#else

    static uint16_t size = 0;
    static uint8_t *pdata = NULL;

    while(((push = Dev_Get_Queue_Occupied((void *)crt_dev->txq)) != QUEUE_INVALID_IDX) || pdata)
    {
        if(!pdata)
        {
            Net_TxData_t *txd = crt_dev->txd;
            txd += push;
            pdata = txd->buf;
            size = txd->size;
        }

        if(!Net_Txbuf_Write(pdata, size))
            break;

        size = 0;
        pdata = NULL;
        crt_xfer->txbusy = true;
        Dev_Queue_Pop((void *)crt_dev->txq);
    }

    if(crt_xfer->txsize)
    {
        Net_Transmit_Data(crt_xfer->send, crt_xfer->recv, crt_xfer->txsize);
#if NET_SEND_DEBUG
        uint16_t p = 0;
        for(p = 0; p < crt_xfer->txsize; p++)
            if(crt_xfer->send[p] != '\0')
                break;
        if(p < crt_xfer->txsize)
        {
            NET_SEND_PRINTF("TX -> ");
            Net_Data_Printf(crt_xfer->send, crt_xfer->txsize);
        }
#endif
    }
    else
    {
        crt_xfer->rxlen = (crt_ph->receive ? crt_ph->receive(crt_xfer->recv) : 0u);
#if NET_RECV_DEBUG
        uint16_t p = 0;
        for(p = 0; p < crt_xfer->rxlen; p++)
            if(crt_xfer->recv[p] != '\0')
                break;
        if(p < crt_xfer->rxlen)
        {
            NET_RECV_PRINTF("RX -> ");
            Net_Data_Printf(crt_xfer->recv, crt_xfer->rxlen);
        }
#endif
    }

    Net_Txbuf_Clear();
    Net_Protocol_Resolve();
#endif
}

/********************************************************************************************
* 函数名：Net_Resolve_Task
* 描  述：从接收队列取出 PDU，调用 Net_Resolve_Handle
* 输  入：无
* 输  出：无
* 调  用：内部调用（Net_Thread_Task）
********************************************************************************************/
static void Net_Resolve_Task(void)
{
    uint16_t pop;

    while((pop = Dev_Get_Queue_Occupied((void *)crt_dev->rxq)) != QUEUE_INVALID_IDX)
    {
        Net_RxData_t *rxd = crt_dev->rxd;
        rxd += pop;
        
        Net_Resolve_Handle(crt_dev, rxd->buf[1], net_load_u16_le(&rxd->buf[2]), \
                            net_load_u16_le(&rxd->buf[4]));
        
        Dev_Queue_Pop((void *)crt_dev->rxq);
    }
}

/********************************************************************************************
* 函数名：Net_Thread_Task
* 描  述：NET 协议线程任务：轮询各外设/设备，执行 ACK、发送、解析
* 输  入：无
* 输  出：无
* 调  用：外部调用（周期调度）
********************************************************************************************/
void Net_Thread_Task(void)
{
    for(uint8_t idx = 0; idx < netctrl.periph_cnt; idx++)
    {
        crt_ph = netctrl.periph[idx];
        crt_xfer = &crt_ph->xfer;

        for(uint8_t i = 0; i < crt_ph->num; i++)
        {
            if(++crt_ph->crt_dev >= crt_ph->num)
                crt_ph->crt_dev = 0;
            crt_dev = crt_ph->dev[crt_ph->crt_dev];

            if(!Dev_Tk_Wait(crt_dev->period, crt_dev->peri_tk))
                return;

            Dev_Tk_Init(&crt_dev->peri_tk);
            Net_Transmit_Task();
#if NET_TRANSMIT_ACK
            Net_Ack_Task();
#endif
            Net_Resolve_Task();
        }
    }
}

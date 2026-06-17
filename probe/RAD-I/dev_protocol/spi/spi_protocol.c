/**********************************************************************************************************
 * 文件名: spi_protocol.c
 * 概  述: SPI协议（接收、发送、应答）
 * 创建时间: 2025-08-01
 * 更新时间: 2025-08-22
 * 作  者: LYJ
 * 版  本: 1.0.0
 * Copyright (c) 2025, Mr.Liu All rights reserved.
 **********************************************************************************************************/
/********************************** SPI数据传输协议（无ACK） **************************
 * 数据格式: （帧头 + 数据长度 + 数据 + CRC校验 + 帧尾）* n
*************************************************************************************/
/********************************** SPI数据传输协议（ACK） ****************************
 * 数据格式: 帧头 + 数据长度 + 数据 + CRC校验 + 帧尾
 * 应答格式：帧头 + 0x0000 + CRC校验 + 帧尾
*************************************************************************************/
#include "./spi/spi_protocol.h"
#include "./core/dev_malloc.h"
#include "./spi/spi_app.h"


Spi_Periph_t *crt_ph = NULL;    // 当前轮询外设
Spi_Xfer_t *crt_xfer = NULL;    // 当前数据传输控制
Spi_Device_t *crt_dev = NULL;   // 当前轮询设备

Spi_Ctrl_t spictrl = {
    .init = Spi_Device_Init,    // 设备初始化函数
    .periph = {0},
    .periph_cnt = 0,
};


static const uint8_t frame_hd[SPI_HEAD_LENTH] = SPI_FRAME_HEAD;
static const uint8_t frame_tl[SPI_TAIL_LENTH] = SPI_FRAME_TAIL;

#if (SPI_SEND_DEBUG | SPI_RECV_DEBUG | SPI_ACK_INFO)
/********************************************************************************************
* 函数名：Spi_Data_Printf
* 描  述：SPI 测试信息打印（发送）
* 输  入：@param: *sdata -> 数据指针
*         @param: size -> 数据大小
* 输  出：无
* 调  用：内部调用
********************************************************************************************/
static void Spi_Data_Printf(uint8_t *sdata, uint16_t size)
{
    for(uint16_t i = 0; i < size; i++)
        DEV_PRINTF("%02X ", sdata[i]);
    DEV_PRINTF(" t: %u\r\n", DEV_GET_1MS_TICK_FUN());
}
#endif

/********************************************************************************************
* 函数名：Spi_Periph_Register
* 描  述：SPI 外设注册
* 输  入：@param: *name -> 外设名称
*         @param: *init -> 初始化函数指针（输入：无  输出：true -> 初始化成功，false -> 初始化失败）
*         @param: *deinit -> 反初始化函数指针（输入：无  输出：无）
*         @param: *transmit -> 数据传输函数指针（输入：TX数据指针，RX数据指针，数据大小）
*                                            （输出：true -> 发送成功，false -> 发送失败）
* 输  出：@retval: 外设句柄
* 调  用：外部调用
********************************************************************************************/
Spi_Periph_t *Spi_Periph_Register(char *name, bool(*init)(void), void(*deinit)(void), \
                                    bool(*transmit)(uint8_t*, uint8_t*, uint16_t))
{
    if(spictrl.periph_cnt >= SPI_MAX_PERIPH_CNT)
        DEV_PRINTF("%s -> 注册失败 -> 可注册外设数不足！\r\n", name);
    else if(!init())
        DEV_PRINTF("%s -> 注册失败 -> 外设初始化失败！\r\n", name);
    else
    {
        Spi_Periph_t *new_ph = (Spi_Periph_t *)Dev_Mem_Malloc(sizeof(Spi_Periph_t));
        if(!new_ph)
        {
            DEV_PRINTF("%s -> 注册失败 -> 外设句柄分配失败！\r\n", name);
            return NULL;
        }

        new_ph->num = 0;    // 初始设备数0
        memset((void *)&new_ph->xfer, 0, sizeof(Spi_Xfer_t));    // 清空数据传输控制

#if SPI_TRANSMIT_ACK
        memcpy(new_ph->xfer.ack.frame.hd, frame_hd, SPI_HEAD_LENTH);
        memcpy(new_ph->xfer.ack.frame.tl, frame_tl, SPI_TAIL_LENTH);
#endif

        strcpy(new_ph->name, name);    // 外设注册名称
        new_ph->init = init;           // 外设初始化函数
        new_ph->deinit = deinit;       // 外设反初始化函数
        new_ph->transmit = transmit;   // 外设传输函数
        spictrl.periph[spictrl.periph_cnt++] = new_ph;    // 外设注册
        DEV_PRINTF("%s -> 外设注册 -> 注册成功！\r\n", name);
        return new_ph;    // 外设句柄
    }
    return NULL;    // 注册失败
}

/********************************************************************************************
* 函数名：Spi_Periph_Unregister
* 描  述：SPI 外设注销
* 输  入：@param: *ph -> 外设句柄
* 输  出：无
* 调  用：外部调用
********************************************************************************************/
void Spi_Periph_Unregister(Spi_Periph_t *ph)
{
    uint8_t idx;

    if(!spictrl.periph_cnt)
    {
        DEV_PRINTF("%s -> 注销失败 -> 未注册任何外设！\r\n", ph->name);
        return;
    }
    
    if(!ph)
    {
        DEV_PRINTF("外设注销 -> 注销失败 -> 未注册该外设！\r\n");
        return; 
    }
    
    for(idx = 0; idx < spictrl.periph_cnt; idx++)
        if(ph == spictrl.periph[idx])
            break;

    if(idx == spictrl.periph_cnt)
    {
        DEV_PRINTF("%s -> 外设注销 -> 未注册该外设！\r\n", ph->name);
        return;
    }

    ph->deinit();    // 外设反初始化
    for(uint8_t num = 0; num < ph->num; num++)
        Spi_Device_Unregister(ph, ph->dev[num]);    // 注销设备

#if SPI_TRANSMIT_ACK
    Dev_Mem_Release(ph->xfer.ackp);    // 内存释放
#endif

    Dev_Mem_Release(ph->xfer.send);    // 内存释放
    Dev_Mem_Release(ph->xfer.recv);    // 内存释放
    Dev_Mem_Release(ph->xfer.data);    // 内存释放
    
    DEV_PRINTF("%s -> 外设注销 -> 注销成功！\r\n", ph->name);
    spictrl.periph[--spictrl.periph_cnt] = NULL;    // 外设注销
    Dev_Mem_Release(ph);    // 内存释放
}

/********************************************************************************************
* 函数名：Spi_Device_Register
* 描  述：SPI 设备注册
* 输  入：@param: *ph -> 外设句柄
*         @param: *name -> 设备名称
*         @param: *csl -> CS拉低/设备使能函数指针（输入：无  输出：无）
*         @param: *csh -> CS拉高/设备失能函数指针（输入：无  输出：无）
*         @param: qcfg -> 设备队列配置
*         @param: period -> 设备轮询周期
* 输  出：@retval: 设备句柄
* 调  用：外部调用
********************************************************************************************/
Spi_Device_t *Spi_Device_Register(Spi_Periph_t *ph, char *name, void (*csl)(void), \
                                    void (*csh)(void), Dev_Queue_Config_t qcfg, uint16_t period)
{
    uint8_t idx;

    for(idx = 0;idx < spictrl.periph_cnt;idx++)          // 外设检索
        if((void *)spictrl.periph[idx] == (void *)ph)    // 目标外设
            break;    // 检索成功
    
    if(idx == spictrl.periph_cnt)    // 检索失败
    {
        DEV_PRINTF("%s -> 设备注册 -> 未注册外设！\r\n", name);
        return NULL;
    }

    if(ph->num >= SPI_MAX_DEV_CNT)
    {
        DEV_PRINTF("%s -> 注册失败 -> 可注册设备数不足！\r\n", name);
        return NULL;
    }

    Spi_Device_t *new_dh = (Spi_Device_t *)Dev_Mem_Malloc(sizeof(Spi_Device_t));
    if(!new_dh)
    {
        DEV_PRINTF("%s -> 注册失败 -> 设备句柄分配失败！\r\n", name);
        return NULL;
    }

    if((new_dh->id = Dev_Id_alloc()) == DEV_INVAILD_ID)    // 设备描述符分配
    {
        DEV_PRINTF("%s -> 注册失败 -> 可分配设备描述符不足！\r\n", name);
        Dev_Mem_Release(new_dh);
        return NULL;
    }

    ph->dev[ph->num++] = new_dh;    // 设备注册
    new_dh->txq = (Spi_Queue_t *)Dev_Mem_Malloc(sizeof(Spi_Queue_t));
    new_dh->rxq = (Spi_Queue_t *)Dev_Mem_Malloc(sizeof(Spi_Queue_t));
    if(!new_dh->txq || !new_dh->rxq)
    {
        DEV_PRINTF("%s -> 设备注册 -> TX/RX 队列分配失败！\r\n", name);
        Spi_Device_Unregister(ph, new_dh);
        return NULL;
    }

    new_dh->txq->count = 0;                  // 初始元素个数0
    new_dh->rxq->count = 0;                  // 初始元素个数0
    Dev_Queue_Clear((void *)new_dh->txq);    // 清空队列
    Dev_Queue_Clear((void *)new_dh->rxq);    // 清空队列
    new_dh->txq->depth = qcfg.txq_depth;     // 获取队列深度
    new_dh->rxq->depth = qcfg.rxq_depth;     // 获取队列深度
    new_dh->txd = (Spi_TxData_t *)Dev_Mem_Malloc(sizeof(Spi_TxData_t) * qcfg.txq_depth);
    new_dh->rxd = (Spi_RxData_t *)Dev_Mem_Malloc(sizeof(Spi_RxData_t) * qcfg.rxq_depth);
    if(!new_dh->txd || !new_dh->rxd)
    {
        DEV_PRINTF("%s -> 设备注册 -> TX/RX 指针分配失败！\r\n", name);
        Spi_Device_Unregister(ph, new_dh);
        return NULL;
    }

    // 发送节点内存分配
    Spi_TxData_t *txd = new_dh->txd;
    for(uint16_t i = 0; i < qcfg.txq_depth; i++)
    {
        if((txd->buf = (uint8_t *)Dev_Mem_Malloc(qcfg.txb_size)) == NULL)
        {
            DEV_PRINTF("%s -> 设备注册 -> TX 内存分配失败！\r\n", name);
            Spi_Device_Unregister(ph, new_dh);
            return false;
        }
        memset(txd->buf, 0, qcfg.txb_size);
        txd->size = 0;
        txd++;
    }
    
    // 接收节点内存分配
    Spi_RxData_t *rxd = new_dh->rxd;
    for(uint16_t i = 0; i < qcfg.rxq_depth; i++)
    {
        if((rxd->buf = (uint8_t *)Dev_Mem_Malloc(qcfg.rxb_size)) == NULL)
        {
            DEV_PRINTF("%s -> 设备注册 -> RX 内存分配失败！\r\n", name);
            Spi_Device_Unregister(ph, new_dh);
            return false;
        }
        memset(rxd->buf, 0, qcfg.rxb_size);
        rxd->size = 0;
        rxd++;
    }

    // 更新数据传输参数
    uint16_t temp = 0;
    for(uint16_t i = 0; i < ph->num; i++)
    {
        if(temp < qcfg.txb_size)
            temp = qcfg.txb_size;
        if(temp < qcfg.rxb_size)
            temp = qcfg.rxb_size;
    }
    
    if(ph->xfer.xsize >= (temp + SPI_FRAME_EXT_LENTH))
    {
        DEV_PRINTF("%s -> 设备注册 -> 注册成功！\r\n", name);
        return new_dh;
    }

    ph->xfer.xsize = temp + SPI_FRAME_EXT_LENTH;    // 单帧数据传输大小
    ph->xfer.send = Dev_Mem_Realloc(ph->xfer.send, ph->xfer.xsize);    // 内存重分配
    ph->xfer.recv = Dev_Mem_Realloc(ph->xfer.recv, ph->xfer.xsize);    // 内存重分配
    ph->xfer.data = Dev_Mem_Realloc(ph->xfer.data, ph->xfer.xsize);    // 内存重分配

#if SPI_TRANSMIT_ACK
    ph->xfer.ackp = Dev_Mem_Realloc(ph->xfer.ackp, ph->xfer.xsize);    // 内存重分配
    if(!ph->xfer.send || !ph->xfer.recv || !ph->xfer.data || !ph->xfer.ackp)
    {
        DEV_PRINTF("%s -> 设备注册 -> xfer TX/RX/DATA/ACKP内存重分配失败！\r\n", name);
        Spi_Device_Unregister(ph, new_dh);
        return false;
    }
    memset(ph->xfer.ackp, 0, ph->xfer.xsize);    // 清空缓存
#else
    if(!ph->xfer.send || !ph->xfer.recv || !ph->xfer.data)
    {
        DEV_PRINTF("%s -> 设备注册 -> xfer TX/RX/DATA内存分配失败！\r\n", name);
        Spi_Device_Unregister(ph, new_dh);
        return false;
    }
#endif

    memset(ph->xfer.send, 0, ph->xfer.xsize);    // 清空缓存
    memset(ph->xfer.recv, 0, ph->xfer.xsize);    // 清空缓存
    memset(ph->xfer.data, 0, ph->xfer.xsize);    // 清空缓存

    strcpy(new_dh->name, name);         // 设备注册名称
    new_dh->transmit = ph->transmit;    // 获取外设传输函数
    new_dh->csl = csl;                  // CS引脚拉低/设备使能函数
    new_dh->csh = csh;                  // CS引脚拉低/设备失能函数
    memcpy(&new_dh->qcfg, &qcfg, sizeof(Dev_Queue_Config_t));
    new_dh->period = period;            // 获取设备轮询周期
    DEV_PRINTF("%s -> 设备注册 -> 注册成功！\r\n", name);
    return new_dh;
}

/********************************************************************************************
* 函数名：Spi_Device_Unregister
* 描  述：SPI 设备注销
* 输  入：@param: *ph -> 外设句柄
*         @param: *dev -> 设备句柄
* 输  出：无
* 调  用：外部调用
********************************************************************************************/
void Spi_Device_Unregister(Spi_Periph_t *ph, Spi_Device_t *dev)
{
    uint8_t idx;

    for(idx = 0; idx < spictrl.periph_cnt; idx++)    // 外设检索
        if(spictrl.periph[idx] == ph)    // 目标外设
            break;        // 检索成功
    
    if(idx == spictrl.periph_cnt)
    {
        DEV_PRINTF("%s -> 设备注销 -> 未注册该外设！\r\n", dev->name);
        return;
    }

    for(idx = 0; idx < ph->num; idx++)    // 设备检索
        if(ph->dev[idx] == dev)    // 目标设备
            break;        // 检索成功

    if(idx == ph->num)
    {
        DEV_PRINTF("%s -> 设备注销 -> 未注册该设备！\r\n", dev->name);
        return;
    }
    
    // 发送节点内存释放
    Spi_TxData_t *txd = dev->txd;
    for(uint16_t i = 0; i < dev->qcfg.txq_depth; i++)
    {
        Dev_Mem_Release(txd->buf);
        txd++;
    }
    
    // 接收节点内存释放
    Spi_RxData_t *rxd = dev->rxd;
    for(uint16_t i = 0; i < dev->qcfg.rxq_depth; i++)
    {
        Dev_Mem_Release(rxd->buf);
        rxd++;
    }

    Dev_Id_Release(dev->id);      // 设备描述符释放

    Dev_Mem_Release(dev->txd);    // 内存释放
    Dev_Mem_Release(dev->rxd);    // 内存释放
    Dev_Mem_Release(dev->txq);    // 内存释放
    Dev_Mem_Release(dev->rxq);    // 内存释放
    
    DEV_PRINTF("%s -> 设备注销 -> 注销成功！\r\n", dev->name);
    ph->dev[--ph->num] = NULL;    // 设备注销
    Dev_Mem_Release(dev);         // 内存释放
    
    // 更新数据传输参数
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
        temp += SPI_FRAME_EXT_LENTH;
        if(ph->xfer.xsize <= temp)    // 单帧数据传输大小变化
            return;

        ph->xfer.xsize = temp;        // 单帧数据传输大小
        ph->xfer.send = Dev_Mem_Realloc(ph->xfer.send, ph->xfer.xsize);    // 内存重分配
        ph->xfer.recv = Dev_Mem_Realloc(ph->xfer.recv, ph->xfer.xsize);    // 内存重分配
        ph->xfer.data = Dev_Mem_Realloc(ph->xfer.data, ph->xfer.xsize);    // 内存重分配

#if SPI_TRANSMIT_ACK
        ph->xfer.ackp = Dev_Mem_Realloc(ph->xfer.ackp, ph->xfer.xsize);    // 内存重分配
        memset(ph->xfer.ackp, 0, ph->xfer.xsize);    // 清空缓存
#endif

        memset(ph->xfer.send, 0, ph->xfer.xsize);    // 清空缓存
        memset(ph->xfer.recv, 0, ph->xfer.xsize);    // 清空缓存
        memset(ph->xfer.data, 0, ph->xfer.xsize);    // 清空缓存
    }
}

/********************************************************************************************
* 函数名：Spi_TxQueue_Push
* 描  述：SPI 发送数据入队
* 输  入：@param: *dev -> 设备句柄
*         @param: *sdata -> 数据指针
*         @param: size -> 数据大小
* 输  出：@retval: true -> 入队成功，false -> 入队失败
* 调  用：外部调用
********************************************************************************************/
bool Spi_TxQueue_Push(Spi_Device_t *dev, uint8_t *sdata, uint16_t size)
{
    if(!dev->transmit)
        DEV_PRINTF("设备未注册！\r\n");
    else if(!size)
        DEV_PRINTF("%s -> 发送数据入队失败，数据大小不能为0！ t: %u\r\n", crt_dev->name , DEV_GET_1MS_TICK_FUN());
    else if(size <= dev->qcfg.txb_size)
    {
        uint16_t push = Dev_Get_Queue_Idle((void *)dev->txq);
        if(push != QUEUE_INVALID_IDX)
        {
            // 发送数据入队
            Spi_TxData_t *txd = dev->txd;
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
* 函数名：Spi_RxQueue_Push
* 描  述：SPI 接收数据入队
* 输  入：@param: *rdata -> 数据指针
*         @param: size -> 数据大小
* 输  出：@retval: true -> 入队成功，false -> 入队失败
* 调  用：内部调用
********************************************************************************************/
static bool Spi_RxQueue_Push(uint8_t *rdata, uint16_t size)
{
    if(!size)
        DEV_PRINTF("%s -> 接收数据入队失败，数据长度为：0！ t: %u\r\n", crt_dev->name, DEV_GET_1MS_TICK_FUN());
    else if(size <= crt_dev->qcfg.rxb_size)
    {
        uint16_t push = Dev_Get_Queue_Idle((void *)crt_dev->rxq);
        if(push != QUEUE_INVALID_IDX)
        {
            // 接收数据入队
            Spi_RxData_t *rxd = crt_dev->rxd;
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
* 函数名：Spi_Txbuf_Clear
* 描  述：SPI 发送缓存清空
* 输  入：无
* 输  出：无
* 调  用：内部调用
********************************************************************************************/
static void Spi_Txbuf_Clear(void)
{
    crt_xfer->txsize = 0;
    memset((void *)crt_xfer->send, 0, crt_xfer->xsize);
}

/********************************************************************************************
* 函数名：Spi_Txbuf_Write
* 描  述：SPI 发送缓存填充
* 输  入：@param: *data -> 数据指针
*         @param: size -> 数据大小
* 输  出：@retval: true -> 填充成功， false -> 填充失败
* 调  用：内部调用
********************************************************************************************/
static bool Spi_Txbuf_Write(uint8_t *data, uint16_t size)
{
    if((data == NULL) || !size)
        DEV_PRINTF("TXB -> 数据指针/大小不能为空！ t: %u\r\n", DEV_GET_1MS_TICK_FUN());
    else if((crt_xfer->txsize + SPI_FRAME_EXT_LENTH + size) <= crt_xfer->xsize)
    {
        memcpy((void *)&crt_xfer->send[crt_xfer->txsize], frame_hd, SPI_HEAD_LENTH);    // 填充帧头
        crt_xfer->txsize += SPI_HEAD_LENTH;
        crt_xfer->send[crt_xfer->txsize++] = size >> 8;        // 填充数据大小
        crt_xfer->send[crt_xfer->txsize++] = (size & 0xff);    // 填充数据大小
        memcpy((void *)&crt_xfer->send[crt_xfer->txsize], (void *)data, size);    // 填充数据
        crt_xfer->txsize += size;
        crt_xfer->crc = Dev_Calculate_CRC(data, size);         // 获取CRC校验码
#if SPI_ACK_INFO
        DEV_PRINTF("TX -> %s -> Wait ACK(CRC) -> %02X %02X t: %u\r\n", crt_dev->name, \
        crt_xfer->crc >> 8, crt_xfer->crc & 0Xff, DEV_GET_1MS_TICK_FUN());
#endif
        crt_xfer->send[crt_xfer->txsize++] = crt_xfer->crc >> 8;      // 填充CRC校验码
        crt_xfer->send[crt_xfer->txsize++] = crt_xfer->crc & 0Xff;    // 填充CRC校验码
        memcpy((void *)&crt_xfer->send[crt_xfer->txsize], frame_tl, SPI_TAIL_LENTH);    // 填充帧尾
        crt_xfer->txsize += SPI_TAIL_LENTH;
        return true;
    }
    else if(size > crt_xfer->xsize)    // 超过数据最大传输长度
        DEV_PRINTF("TXB -> 数据溢出！ t: %u\r\n", DEV_GET_1MS_TICK_FUN());
    else if((crt_xfer->txsize + SPI_FRAME_EXT_LENTH + size) > crt_xfer->xsize)
        ;    // DEV_PRINTF("Send the data next time!");

    return false;
}

/********************************************************************************************
* 函数名：Spi_Protocol_Resolve
* 描  述：SPI 协议解析
* 输  入：无
* 输  出：无
* 调  用：内部调用
********************************************************************************************/
static void Spi_Protocol_Resolve(void)
{
    uint16_t pos = 0, match = 0;
    static uint16_t crc = 0;

    for(pos = 0; pos < crt_xfer->xsize; pos++)
        if(crt_xfer->recv[pos] != '\0')
            break;    // 存在有效数据

    if(pos == crt_xfer->xsize)    // 无效数据包
        return;

    while(pos < crt_xfer->xsize)
    {
        switch(crt_xfer->step)
        {
            case SPI_STEP_HEAD:
                if(crt_xfer->recv[pos] == frame_hd[match])    // 帧头校验
                {
                    if(++match == SPI_HEAD_LENTH)    // 校验成功
                    {
                        crt_xfer->step = SPI_STEP_LENTH;
                        match = 0;
                    }
                }
                else if(match)    // 校验失败
                {
                    DEV_PRINTF("RX -> 帧头%d错误！ t: %u\r\n", match + 1, DEV_GET_1MS_TICK_FUN());
                    match = 0;
                }
                break;

            case SPI_STEP_LENTH:
                crt_xfer->rxlen = (uint16_t)crt_xfer->recv[pos++] << 8;    // 获取数据大小
                crt_xfer->rxlen |= crt_xfer->recv[pos++];                  // 获取数据大小
                if(crt_xfer->rxlen > crt_xfer->xsize)    // 目标发送数据大小 > 数据传输大小
                {
                    DEV_PRINTF("RX -> 数据溢出！ t: %u\r\n", DEV_GET_1MS_TICK_FUN());
                    crt_xfer->step = SPI_STEP_HEAD;
                    break;
                }
                else if(!crt_xfer->rxlen)    // 接收设备应答
                {
#if SPI_TRANSMIT_ACK
                    if(Spi_Ack_Detect((Spi_Ack_Pack_t *)&crt_xfer->recv[pos - SPI_LEN_LENTH - SPI_HEAD_LENTH]))
                    {
                        crt_xfer->txbusy = false;    // 数据传输空闲
                        Spi_Txbuf_Clear();           // 清空发送缓存
                        Spi_Ack_Cancel_Wait();       // 取消应答请求
                        crt_xfer->step = SPI_STEP_HEAD;
                        return;
                    }
#endif
                    crt_xfer->step = SPI_STEP_HEAD;
                    break;
                }

            case SPI_STEP_DATA:
                memcpy(crt_xfer->data, &crt_xfer->recv[pos], crt_xfer->rxlen);
                pos += crt_xfer->rxlen;

            case SPI_STEP_CRC:
                crc = (uint16_t)crt_xfer->recv[pos++] << 8;
                crc |= crt_xfer->recv[pos++];
                crt_xfer->step = SPI_STEP_TAIL;

            case SPI_STEP_TAIL:
                if(crt_xfer->recv[pos] == frame_tl[match])    // 帧尾校验
                {
                    if(++match == SPI_TAIL_LENTH)    // 校验成功
                    {
                        match = 0;
                        crt_xfer->step = SPI_STEP_HEAD;
                        if(crc == Dev_Calculate_CRC((uint8_t *)crt_xfer->data, crt_xfer->rxlen))    // CRC校验
                        {
#if SPI_RECV_DEBUG
                            SPI_RECV_PRINTF("RX -> ");
                            Spi_Data_Printf((uint8_t *)crt_xfer->data, crt_xfer->rxlen);
#endif

                            Spi_RxQueue_Push((uint8_t *)crt_xfer->data, crt_xfer->rxlen);    // 接收数据入队
#if SPI_TRANSMIT_ACK
                            Spi_Ack_Write(crc);    // 填充应答数据
#endif
                        }
                        else
                            DEV_PRINTF("RX -> CRC校验错误：%02X %02X！ t: %u\r\n", crc >> 8, crc & 0xFF, DEV_GET_1MS_TICK_FUN());
                    }
                }
                else if(match)    // 校验失败
                {
                    DEV_PRINTF("RX -> 帧尾%d错误！ t: %u\r\n", match + 1, DEV_GET_1MS_TICK_FUN());
                    crt_xfer->step = SPI_STEP_HEAD;
                    match = 0;
                }
                break;

            default: break;
        }
        pos++;
    }
}

#if SPI_TRANSMIT_ACK
/********************************************************************************************
* 函数名：Spi_Ack_Cancel_Wait
* 描  述：SPI 取消应答等待
* 输  入：无
* 输  出：无
* 调  用：内部调用
********************************************************************************************/
static void Spi_Ack_Cancel_Wait(void)
{
    crt_xfer->ack_sta = SPI_ACK_NOP;    // 无应答请求
    crt_xfer->ackbusy = false;          // 应答空闲
    crt_xfer->retx_times = 0;           // 重置重发次数
}

/********************************************************************************************
* 函数名：Spi_Ack_Write
* 描  述：SPI 应答填充
* 输  入：@param: crc - > 应答CRC校验码
* 输  出：无
* 调  用：内部调用
********************************************************************************************/
static void Spi_Ack_Write(uint16_t crc)
{
    crt_xfer->ack.frame.crc[1] = crc & 0xFF;
    crt_xfer->ack.frame.crc[0] = crc >> 8;
    crt_xfer->ackbusy = true;    // 应答忙
#if SPI_ACK_INFO
    DEV_PRINTF("RX -> REQ ACK -> %02X %02X t: %u\r\n", crt_xfer->ack.frame.crc[0], crt_xfer->ack.frame.crc[1], DEV_GET_1MS_TICK_FUN());
#endif
}

/********************************************************************************************
* 函数名：Spi_Ack_Detect
* 描  述：SPI 应答检测
* 输  入：@param: *data -> 数据指针
* 输  出：@retval: true -> 应答成功，false -> 应答失败
* 调  用：内部调用
********************************************************************************************/
static bool Spi_Ack_Detect(Spi_Ack_Pack_t *data)
{
    if(memcmp(data->frame.tl, frame_tl, SPI_TAIL_LENTH))    // 帧尾校验
        return false;
    else if(crt_xfer->crc != ((((uint16_t)data->frame.crc[0]) << 8) | data->frame.crc[1]))
    {
#if SPI_ACK_INFO
        DEV_PRINTF("RX -> %s -> 无效应答 -> ", crt_dev->name);
        Spi_Data_Printf(data->pack, SPI_ACK_LENTH);
#endif

        crt_xfer->ack_sta = SPI_ACK_ERR;    // 应答错误
        return false;
    }

#if SPI_ACK_INFO
    DEV_PRINTF("RX -> %s -> 有效应答(CRC) -> %02X %02X t: %u\r\n", crt_dev->name, \
        data->frame.crc[0], data->frame.crc[1], DEV_GET_1MS_TICK_FUN());
#endif
    
    crt_xfer->ack_sta = SPI_ACK_OK;    // 应答成功
    return true;
}

/********************************************************************************************
* 函数名：Spi_Ack_Task
* 描  述：SPI 应答任务
* 输  入：无
* 输  出：无
* 调  用：内部调用
********************************************************************************************/
static void Spi_Ack_Task(void)
{
    switch(crt_xfer->ack_sta)
    {
        case SPI_ACK_NOP:
            break;
        case SPI_ACK_OK:
            break;
        case SPI_ACK_WAITING:
            if(Dev_Tk_Wait(SPI_ACK_WAITING_TIME, crt_xfer->ack_time))    // 应答超时
            {
                if(++crt_xfer->retx_times <= SPI_RETX_TIMES)
                {
                    crt_xfer->ack_sta = SPI_ACK_ERR;    // 应答错误
                    DEV_PRINTF("RX -> %s -> 应答超时 -> 重发次数：%d t: %u\r\n", crt_dev->name, crt_xfer->retx_times, DEV_GET_1MS_TICK_FUN());
                }
                else
                {
#if SPI_TRANSMIT_ACK
                    Spi_Ack_Cancel_Wait();       // 取消应答请求
#endif
                    crt_xfer->txbusy = false;    // 数据传输空闲
                    DEV_PRINTF("RX -> %s -> 应答失败！ t: %u\r\n", crt_dev->name, DEV_GET_1MS_TICK_FUN());
                    Spi_Send_Err_Handle(crt_dev->id, crt_xfer->send, crt_xfer->txsize);    // 数据传输失败处理
                    Spi_Txbuf_Clear();           // 清空数据发送缓存
                }
            }
            break;
        case SPI_ACK_ERR:
            Dev_Tk_Init((uint32_t *)&crt_xfer->ack_time);    // 重置应答计时
            break;
        default:
            DEV_PRINTF("RX -> %s -> 异常状态 %d！ t: %u\r\n\r\n", \
                crt_dev->name, crt_xfer->ack_sta, DEV_GET_1MS_TICK_FUN());
            break;
    }
}
#endif

/********************************************************************************************
* 函数名：Spi_Transmit_Task
* 描  述：SPI 数据传输任务
* 输  入：无
* 输  出：无
* 调  用：内部调用
********************************************************************************************/
static void Spi_Transmit_Task(void)
{
    uint16_t push;    // 队列索引

#if SPI_TRANSMIT_ACK
    uint16_t size = 0;        // 数据大小
    uint8_t *pdata = NULL;    // 数据指针

    if(crt_xfer->ackbusy)     // 目标应答请求
    {
        memcpy(crt_xfer->ackp, (void *)crt_xfer->ack.pack, SPI_ACK_LENTH);
        Spi_Transmit_Data(crt_xfer->ackp, crt_xfer->recv, crt_xfer->xsize);

        crt_xfer->ackbusy = false;    // 应答成功
#if SPI_ACK_INFO
        DEV_PRINTF("TX -> ACK -> ");
        Spi_Data_Printf((void *)crt_xfer->ack.pack, SPI_ACK_LENTH);
#endif
        Spi_Protocol_Resolve();       // 数据解析
        return;
    }
    
    if(crt_xfer->ack_sta != SPI_ACK_WAITING)     // 应答等待中
    {
        if(!crt_xfer->txbusy)
        {
            while(1)
            {
                push = Dev_Get_Queue_Occupied((void *)crt_dev->txq);
                if(push == QUEUE_INVALID_IDX)    // 队列空
                    break;
                
                Spi_TxData_t *txd = crt_dev->txd;
                txd += push;
                pdata = txd->buf;
                size = txd->size;
                Dev_Queue_Pop((void *)crt_dev->txq);    // 数据出队

                if(Spi_Txbuf_Write(pdata, size))    // 写入成功 -> 退出
                {
                    crt_xfer->txbusy = true;    // 数据传输中
                    break;
                }


                // if(!pdata)
                // {
                //     push = Dev_Get_Queue_Occupied((void *)crt_dev->txq);
                //     if(push == QUEUE_INVALID_IDX)    // 队列空
                //     {
                //         pdata = NULL;
                //         size = 0;
                //         break;
                //     }
                //     else
                //     {
                //         Spi_TxData_t *txd = crt_dev->txd;
                //         txd += push;
                //         pdata = txd->buf;
                //         size = txd->size;
                //     }
                // }

                // if(!Spi_Txbuf_Write(pdata, size))    // 写入失败 -> 退出
                //     break;

                // size = 0;
                // pdata = NULL;
                // crt_xfer->txbusy = true;    // 数据传输中
                // Dev_Queue_Pop((void *)crt_dev->txq);    // 数据出队
            }
        }

        Spi_Transmit_Data(crt_xfer->send, crt_xfer->recv, crt_xfer->xsize);

        if(crt_xfer->txbusy)    // 应答忙
            crt_xfer->ack_sta = SPI_ACK_WAITING;    // 应答等待
        
        Dev_Tk_Init((uint32_t *)&crt_xfer->ack_time);

#if SPI_SEND_DEBUG
        uint16_t pos = 0;
        for(pos = 0; pos < crt_xfer->xsize; pos++)
            if(crt_xfer->send[pos] != '\0')
                break;
        if(pos < crt_xfer->xsize)
        {
            SPI_SEND_PRINTF("TX -> ");
            Spi_Data_Printf((uint8_t *)crt_xfer->send, crt_xfer->xsize);
        }
#endif

    }
    else
    {
        // 发送空数据，等待目标应答
        memset(crt_xfer->ackp, 0, SPI_ACK_LENTH);
        Spi_Transmit_Data(crt_xfer->ackp, crt_xfer->recv, crt_xfer->xsize);
    }
    Spi_Protocol_Resolve();    // 数据解析

#else
    
    static uint16_t size = 0;    // 数据大小
    static uint8_t *pdata = NULL;    // 数据指针

    // 填充帧数据至缓存满
    while(((push = Dev_Get_Queue_Occupied((void *)crt_dev->txq)) != QUEUE_INVALID_IDX) || pdata)
    {
        if(!pdata)
        {
            Spi_TxData_t *txd = crt_dev->txd;
            txd += push;
            pdata = txd->buf;
            size = txd->size;
        }

        if(!Spi_Txbuf_Write(pdata, size))    // 填充帧数据
            break;
        
        size = 0;
        pdata = NULL;
        crt_xfer->txbusy = true;    // 发送忙
        Dev_Queue_Pop((void *)crt_dev->txq);    // 数据出队
    }

    Spi_Transmit_Data(crt_xfer->send, crt_xfer->recv, crt_xfer->xsize);

#if SPI_SEND_DEBUG
    uint16_t pos = 0;
    
    for(pos = 0; pos < crt_xfer->xsize; pos++)
        if(crt_xfer->send[pos] != '\0')
            break;

    if(pos != crt_xfer->xsize)
    {
        SPI_SEND_PRINTF("TX -> ");
        Spi_Data_Printf(crt_xfer->send, crt_xfer->xsize);
    }
#endif
    Spi_Txbuf_Clear();    // 清空发送缓存

    Spi_Protocol_Resolve();    // 数据解析
#endif
}

/********************************************************************************************
* 函数名：Spi_Resolve_Task
* 描  述：SPI 数据解析任务
* 输  入：无
* 输  出：无
* 调  用：内部调用
********************************************************************************************/
static void Spi_Resolve_Task(void)
{
    uint16_t pop;

    while((pop = Dev_Get_Queue_Occupied((void *)crt_dev->rxq)) != QUEUE_INVALID_IDX)
    {
        Spi_RxData_t *rxd = crt_dev->rxd;
        rxd += pop;
        Spi_Resolve_Handle(crt_dev->id, rxd->buf, rxd->size);    // 数据处理函数
        Dev_Queue_Pop((void *)crt_dev->rxq);    // 数据出队
    }
}

/********************************************************************************************
* 函数名：Spi_Thread_Task
* 描  述：SPI 线程任务
* 输  入：无
* 输  出：无
* 说  明: //!置于主循环一直运行
* 调  用：外部调用
********************************************************************************************/
void Spi_Thread_Task(void)
{
    for(uint8_t idx = 0; idx < spictrl.periph_cnt; idx++)    // 外设轮询
    {
        crt_ph = spictrl.periph[idx];    // 当前外设
        crt_xfer = &crt_ph->xfer;        // 当前数据传输控制

        for(uint8_t i = 0; i < crt_ph->num; i++)       // 设备轮询
        {
            if(++crt_ph->crt_dev >= crt_ph->num)
                crt_ph->crt_dev = 0;
            crt_dev = crt_ph->dev[crt_ph->crt_dev];    // 当前设备

            if(!Dev_Tk_Wait(crt_dev->period, crt_dev->peri_tk))    // 设备轮询周期
                return;

            Dev_Tk_Init(&crt_dev->peri_tk);    // 记录设备轮询时间节点
#if SPI_TRANSMIT_ACK
            Spi_Ack_Task();         // 应答任务
#endif
            Spi_Transmit_Task();    // 数据传输任务
            Spi_Resolve_Task();     // 数据解析任务
        }
    }
}



/**********************************************************************************************************
 * 文件名：net_raw_bsp.c
 * 概  述：网络/串口裸协议底层接口（W5500 TCP Server）
 * 创建时间：2026-03-30
 * 更新时间：2026-05-20
 * 作  者：LYJ
 * 版  本：1.0.0
 * Copyright (c) 2025, Mr.Liu All rights reserved.
 **********************************************************************************************************/
#include "./net_raw/net_raw_bsp.h"
#include "./net_raw/net_raw_protocol.h"
#include "./net_raw/net_raw_app.h"


//! ---------------- ↓ 自定义头文件 ↓ ---------------- !//
#include <stdbool.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "usart.h"
#include "fdcan.h"
#include "geiger.h"
#include "main.h"
#include "socket.h"
#include "lora.h"
#include "usart.h"
#include "w5500_dhcp.h"
//! ---------------- ↑ 自定义头文件 ↑ ---------------- !//


//! ----------------- ↓ 自定义变量 ↓ ----------------- !//
// W5500 Socket 相关变量（使用已初始化的 Socket）
Net_Periph_t *net_w5500_ph = NULL;
Net_Device_t *net_w5500_dh = NULL;

static SemaphoreHandle_t s_net_tx_mutex = NULL;

static void net_tx_mutex_init(void)
{
    if(s_net_tx_mutex == NULL)
        s_net_tx_mutex = xSemaphoreCreateMutex();
}

Net_Periph_t *net_can_ph = NULL;
Net_Device_t *net_can_dh = NULL;

Net_Periph_t *net_lora_ph = NULL;
Net_Device_t *net_lora_dh = NULL;

FDCAN_TxHeaderTypeDef can_txh;
FDCAN_RxHeaderTypeDef can_rxh;

uint8_t g_tx_sock_ready = 0;
uint8_t g_rx_sock_ready = 0;
static uint8_t s_phy_link_up = 1;     /* PHY 链路状态，用于断线后重建 listen */
static uint32_t s_sock_con_ms[2] = {0, 0};
static uint8_t s_sock_prev_st[2] = {SOCK_CLOSED, SOCK_CLOSED};

typedef struct {
    uint8_t  sock;
    uint16_t port;
    uint8_t *ready;
} Net_ProtoSock_t;

#if (SETTING_SOCKET_PORT == DATA_UPLOAD_SOCKET_PORT)
static Net_ProtoSock_t s_proto_sock[] = {
    { SETTING_SOCKET_NUM, SETTING_SOCKET_PORT, &g_tx_sock_ready },
};
#define NET_PROTO_SOCK_CNT  1U
#else
static Net_ProtoSock_t s_proto_sock[] = {
    { SETTING_SOCKET_NUM, SETTING_SOCKET_PORT, &g_tx_sock_ready },
    { DATA_UPLOAD_SOCKET_NUM, DATA_UPLOAD_SOCKET_PORT, &g_rx_sock_ready },
};
#define NET_PROTO_SOCK_CNT  2U
#endif


//! ----------------- ↑ 自定义变量 ↑ ----------------- !//


//! ----------------- ↓ 自定义函数 ↓ ----------------- !//
static bool Net_Tcp_Listen(uint8_t sock, uint16_t port, bool reopen);
static void Net_Tcp_RecoverListen(uint8_t sock, uint16_t port);
static void Net_Tcp_Socket_Maintain(uint8_t sock, uint16_t port);
static void Net_Phy_Link_Maintain(void);
static bool Net_LocalIpValid(void);
static uint16_t Net_Tcp_Socket_Recv(uint8_t sock, uint8_t *rdata, uint16_t cap);
#if (SETTING_SOCKET_PORT != DATA_UPLOAD_SOCKET_PORT)
static void Net_Tcp_Socket_ResetStalePeer(uint8_t peer_sock, uint16_t peer_port,
                                            uint32_t peer_con_ms, uint32_t now);
#endif
//! ----------------- ↑ 自定义函数 ↑ ----------------- !//


/********************************************************************************************
* 函数名：Net_Device_Init
* 描  述：NET 设备初始化
* 输  入：无
* 输  出：无
* 调  用：外部调用
********************************************************************************************/
void Net_Device_Init(void)
{
    Net_Device_Base_Config_t cfg = {
        .qcfg = {
            .txb_size = 1024,
            .txq_depth = 12,
            .rxb_size = 1024,
            .rxq_depth = 12,
        },
        .period = 2,
        .addr = sys_cfg.dev_addr,  // 从 sys_cfg 读取设备地址
        .reg_tb = NULL,
        .reg_sz = (uint32_t)(512 * sizeof(uint16_t)),
        .reasm_sz = 1024,
    };

    net_w5500_ph = Net_Periph_Register("NET_W5500", 
                                        Ph_Net_Init, 
                                        Ph_Net_DeInit, 
                                        Ph_Net_Transmit, 
                                        Ph_Net_Receive);
    if(!net_w5500_ph)
        return;

    net_w5500_dh = Net_Device_Register(net_w5500_ph,
                                    "W5500",
                                    NULL,
                                    NULL,
                                    &cfg);
    if(!net_w5500_dh)
        return;

    if(!net_w5500_dh->reg_tb)
    {
        DEV_PRINTF("协议寄存器表分配失败，CAN设备注册取消！\r\n");
        return;
    }

    Net_Device_Base_Config_t cfg_can = {
        .qcfg = {
            .txb_size = 1024,
            .txq_depth = 12,
            .rxb_size = 16,
            .rxq_depth = 32,
        },
        .period = 2,
        .addr = sys_cfg.dev_addr,  // 从 sys_cfg 读取设备地址
        .reg_tb = net_w5500_dh->reg_tb,
        .reg_sz = net_w5500_dh->reg_sz,
        .reasm_sz = 1024,
    };

    net_can_ph = Net_Periph_Register("NET_CAN", 
                                    Ph_Can_Init, 
                                    Ph_Can_DeInit, 
                                    Ph_Can_Transmit, 
                                    Ph_Can_Receive);
    if(!net_can_ph)
        return;

    net_can_dh = Net_Device_Register(net_can_ph,
                                    "CAN",
                                    NULL,
                                    NULL,
                                    &cfg_can);
    if(!net_can_dh)
        return;

    Net_Device_Base_Config_t cfg_lora = {
        .qcfg = {
            .txb_size = 1024,
            .txq_depth = 12,
            .rxb_size = LORA_REC_LEN,
            .rxq_depth = 12,
        },
        .period = 2,
        .addr = sys_cfg.dev_addr,
        .reg_tb = net_w5500_dh->reg_tb,
        .reg_sz = net_w5500_dh->reg_sz,
        .reasm_sz = 1024,
    };

    net_lora_ph = Net_Periph_Register("NET_LORA",
                                    Ph_Lora_Init,
                                    Ph_Lora_DeInit,
                                    Ph_Lora_Transmit,
                                    Ph_Lora_Receive);
    if(!net_lora_ph)
        return;

    net_lora_dh = Net_Device_Register(net_lora_ph,
                                    "LORA",
                                    NULL,
                                    NULL,
                                    &cfg_lora);
    if(!net_lora_dh)
        return;
}

/********************************************************************************************
* 函数名：Ph_Net_Init
* 描  述：NET 外设初始化（按 SETTING_SOCKET_PORT / DATA_UPLOAD_SOCKET_PORT listen，同口或异口均可）
* 输  入：无
* 输  出：@retval: true -> 初始化成功
* 调  用：外部调用
********************************************************************************************/
bool Ph_Net_Init(void)
{
    uint8_t i;
    bool ok = true;

    net_tx_mutex_init();
    g_tx_sock_ready = 0;
    g_rx_sock_ready = 0;

    for(i = 0; i < NET_PROTO_SOCK_CNT; i++)
    {
        if(Net_Tcp_Listen(s_proto_sock[i].sock, s_proto_sock[i].port, false))
            *s_proto_sock[i].ready = 1;
        else
            ok = false;
    }

    s_phy_link_up = (getPHYCFGR() & LINK) ? 1u : 0u;
    memset(s_sock_con_ms, 0, sizeof(s_sock_con_ms));
    memset(s_sock_prev_st, SOCK_CLOSED, sizeof(s_sock_prev_st));

#if (SETTING_SOCKET_PORT == DATA_UPLOAD_SOCKET_PORT)
    if(ok)
        DEV_PRINTF("TCP listen: TX/RX port %u (Socket %u)\r\n",
                   (unsigned)SETTING_SOCKET_PORT, (unsigned)SETTING_SOCKET_NUM);
    else
        DEV_PRINTF("TCP listen failed on port %u\r\n", (unsigned)SETTING_SOCKET_PORT);
    return ok;
#else
    if(g_tx_sock_ready && g_rx_sock_ready)
    {
        DEV_PRINTF("TCP listen: RX ctrl port %u (Socket %u), TX data port %u (Socket %u)\r\n",
                   (unsigned)SETTING_SOCKET_PORT, (unsigned)SETTING_SOCKET_NUM,
                   (unsigned)DATA_UPLOAD_SOCKET_PORT, (unsigned)DATA_UPLOAD_SOCKET_NUM);
        return true;
    }
    DEV_PRINTF("TCP listen partial: RX ctrl(port %u)=%s, TX data(port %u)=%s\r\n",
               (unsigned)SETTING_SOCKET_PORT, g_tx_sock_ready ? "OK" : "FAIL",
               (unsigned)DATA_UPLOAD_SOCKET_PORT, g_rx_sock_ready ? "OK" : "FAIL");
    return (g_tx_sock_ready || g_rx_sock_ready);
#endif
}

void Ph_Net_RebindOnIpChange(void)
{
    uint8_t i;

    if(!Net_LocalIpValid())
        return;

    for(i = 0; i < NET_PROTO_SOCK_CNT; i++)
    {
        if(Net_Tcp_Listen(s_proto_sock[i].sock, s_proto_sock[i].port, true))
            *s_proto_sock[i].ready = 1;
        else
            *s_proto_sock[i].ready = 0;
    }

    memset(s_sock_con_ms, 0, sizeof(s_sock_con_ms));
    memset(s_sock_prev_st, SOCK_CLOSED, sizeof(s_sock_prev_st));
    DEV_PRINTF("TCP listen rebound after IP update\r\n");
}

void Net_Tcp_PeriodicMaintain(void)
{
    uint8_t i;

    if(W5500_Is_Network_Recovering())
        return;

    Net_Phy_Link_Maintain();

    if(!Net_LocalIpValid())
        return;

    for(i = 0; i < NET_PROTO_SOCK_CNT; i++)
    {
        if(!*s_proto_sock[i].ready)
            continue;

        Net_Tcp_Socket_Maintain(s_proto_sock[i].sock, s_proto_sock[i].port);
    }
}

/********************************************************************************************
* 函数名：Ph_Net_DeInit
* 描  述：NET 外设反初始化（关闭 W5500 Socket）
* 输  入：无
* 输  出：无
* 调  用：外部调用
********************************************************************************************/
void Ph_Net_DeInit(void)
{
    uint8_t i;

    for(i = 0; i < NET_PROTO_SOCK_CNT; i++)
    {
        if(!*s_proto_sock[i].ready)
            continue;
        close(s_proto_sock[i].sock);
        *s_proto_sock[i].ready = 0;
        DEV_PRINTF("W5500 Socket %u (port %u) closed\r\n",
                   (unsigned)s_proto_sock[i].sock, (unsigned)s_proto_sock[i].port);
    }
}

bool Net_Tcp_DataSendReady(void)
{
#if (SETTING_SOCKET_PORT == DATA_UPLOAD_SOCKET_PORT)
    return g_tx_sock_ready && (getSn_SR(SETTING_SOCKET_NUM) == SOCK_ESTABLISHED);
#else
    return g_rx_sock_ready && (getSn_SR(DATA_UPLOAD_SOCKET_NUM) == SOCK_ESTABLISHED);
#endif
}

/********************************************************************************************
* 函数名：Ph_Net_Transmit
* 描  述：经数据口（5000 / 同口时为 SETTING 口）发送协议数据
* 输  入：@param: sdata -> 待发数据；@param: tx_len -> 字节数
* 输  出：@retval: true -> 发送成功；false -> 发送口未连接或发送不完整
* 调  用：协议层调用
********************************************************************************************/
bool Ph_Net_Transmit(uint8_t *sdata, uint16_t tx_len)
{
    uint16_t sent;
    bool ok = false;

    if(!sdata || tx_len == 0)
        return false;

    if(W5500_Is_Network_Recovering())
        return false;

    net_tx_mutex_init();
    if(s_net_tx_mutex != NULL)
    {
        if(xSemaphoreTake(s_net_tx_mutex, pdMS_TO_TICKS(500)) != pdTRUE)
            return false;
    }

#if (SETTING_SOCKET_PORT == DATA_UPLOAD_SOCKET_PORT)
    if(!g_tx_sock_ready || getSn_SR(SETTING_SOCKET_NUM) != SOCK_ESTABLISHED)
        goto tx_done;

    sent = send(SETTING_SOCKET_NUM, sdata, tx_len);
    if(sent == tx_len)
    {
        ok = true;
        goto tx_done;
    }

    Net_Tcp_Socket_Maintain(SETTING_SOCKET_NUM, SETTING_SOCKET_PORT);
    DEV_PRINTF("TX Socket %u (port %u) send fail: expect %u, sent %u\r\n",
               (unsigned)SETTING_SOCKET_NUM, (unsigned)SETTING_SOCKET_PORT,
               (unsigned)tx_len, (unsigned)sent);
#else
    if(!g_rx_sock_ready || getSn_SR(DATA_UPLOAD_SOCKET_NUM) != SOCK_ESTABLISHED)
        goto tx_done;

    sent = send(DATA_UPLOAD_SOCKET_NUM, sdata, tx_len);
    if(sent == tx_len)
    {
        ok = true;
        goto tx_done;
    }

    Net_Tcp_Socket_Maintain(DATA_UPLOAD_SOCKET_NUM, DATA_UPLOAD_SOCKET_PORT);
    DEV_PRINTF("TX Socket %u (port %u) send fail: expect %u, sent %u\r\n",
               (unsigned)DATA_UPLOAD_SOCKET_NUM, (unsigned)DATA_UPLOAD_SOCKET_PORT,
               (unsigned)tx_len, (unsigned)sent);
#endif

tx_done:
    if(s_net_tx_mutex != NULL)
        (void)xSemaphoreGive(s_net_tx_mutex);
    return ok;
}

/********************************************************************************************
* 函数名：Ph_Net_Receive
* 描  述：从控制口（5001 / 同口时为 SETTING 口）接收协议数据；异口时数据口上的误入数据丢弃
* 输  入：@param: rdata -> 协议层 crt_xfer->recv，容量为 qcfg.rxb_size
* 输  出：@retval: 本次收到字节数；无数据或未连接为 0
* 调  用：协议层调用
********************************************************************************************/
uint16_t Ph_Net_Receive(uint8_t *rdata)
{
    uint16_t cap, n;

    if(W5500_Is_Network_Recovering())
        return 0;

    if(!net_w5500_dh || !rdata)
        return 0;
    cap = net_w5500_dh->qcfg.rxb_size;
    if(!cap)
        return 0;

    Net_Tcp_PeriodicMaintain();

#if (SETTING_SOCKET_PORT == DATA_UPLOAD_SOCKET_PORT)
    if(g_tx_sock_ready && getSn_SR(SETTING_SOCKET_NUM) == SOCK_ESTABLISHED)
#else
    if((g_tx_sock_ready && getSn_SR(SETTING_SOCKET_NUM) == SOCK_ESTABLISHED) ||
       (g_rx_sock_ready && getSn_SR(DATA_UPLOAD_SOCKET_NUM) == SOCK_ESTABLISHED))
#endif
    {
        tx_inft.tcp = true;
        if(tx_inft.crt != INFT_TCP)
            Net_TxInft_UpdateCrt();
    }
    else
    {
        tx_inft.tcp = false;
        if(tx_inft.crt == INFT_TCP)
            Net_TxInft_UpdateCrt();
    }

    if(!g_tx_sock_ready || getSn_SR(SETTING_SOCKET_NUM) != SOCK_ESTABLISHED)
        return 0;

#if (SETTING_SOCKET_PORT != DATA_UPLOAD_SOCKET_PORT)
    if(g_rx_sock_ready && getSn_SR(DATA_UPLOAD_SOCKET_NUM) == SOCK_ESTABLISHED)
    {
        uint16_t stray = Net_Tcp_Socket_Recv(DATA_UPLOAD_SOCKET_NUM, rdata, cap);
        if(stray > 0)
        {
            DEV_PRINTF("Socket %u (port %u) stray RX %u bytes discarded\r\n",
                       (unsigned)DATA_UPLOAD_SOCKET_NUM, (unsigned)DATA_UPLOAD_SOCKET_PORT,
                       (unsigned)stray);
        }
    }
#endif

    n = Net_Tcp_Socket_Recv(SETTING_SOCKET_NUM, rdata, cap);
    if(n > 0)
        return n;

    if(getSn_IR(SETTING_SOCKET_NUM) & Sn_IR_TIMEOUT)
    {
        setSn_IR(SETTING_SOCKET_NUM, Sn_IR_TIMEOUT);
        Net_Tcp_Socket_Maintain(SETTING_SOCKET_NUM, SETTING_SOCKET_PORT);
    }
    return 0;
}

/********************************************************************************************
* 函数名：Ph_Can_Init
* 描  述：CAN 外设初始化（CAN 已在 freertos_task 中初始化，此处打开协议端口）
* 输  入：无
* 输  出：@retval: true -> 初始化成功；false -> 初始化失败
* 调  用：外部调用
********************************************************************************************/
bool Ph_Can_Init(void)
{
    MX_FDCAN2_Init();
    if(!CAN_Drive_Filter_Init())
        return false;
    if(HAL_FDCAN_Start(&hfdcan2) != HAL_OK)
        return false;
    return true;
}

/********************************************************************************************
* 函数名：Ph_Can_DeInit
* 描  述：CAN 外设反初始化（关闭 CAN）
* 输  入：无
* 输  出：无
* 调  用：外部调用
********************************************************************************************/
void Ph_Can_DeInit(void)
{
    
}

/********************************************************************************************
* 函数名：Ph_Can_Transmit
* 描  述：通过 CAN 发送数据
* 输  入：@param: sdata -> 待发数据；@param: tx_len -> 字节数
* 输  出：@retval: true -> 发送成功；false -> 未连接或发送不完整
* 调  用：协议层调用
********************************************************************************************/
bool Ph_Can_Transmit(uint8_t *sdata, uint16_t tx_len)
{
    uint32_t dlc_bytes = 0;
    uint8_t txb[8] = {0};
    uint8_t tx_byte = 0;
    
    do{
        if(tx_len > 8)
            tx_byte = 8;
        else
            tx_byte = tx_len;

        memset(txb, 0, sizeof(txb));
        for(uint8_t i = 0; i < tx_byte; i++)
            txb[i] = sdata[i];

        sdata += tx_byte;
        tx_len -= tx_byte;

        switch(tx_byte){
            case 0: dlc_bytes = FDCAN_DLC_BYTES_0; break;
            case 1: dlc_bytes = FDCAN_DLC_BYTES_1; break;
            case 2: dlc_bytes = FDCAN_DLC_BYTES_2; break;
            case 3: dlc_bytes = FDCAN_DLC_BYTES_3; break;
            case 4: dlc_bytes = FDCAN_DLC_BYTES_4; break;
            case 5: dlc_bytes = FDCAN_DLC_BYTES_5; break;
            case 6: dlc_bytes = FDCAN_DLC_BYTES_6; break;
            case 7: dlc_bytes = FDCAN_DLC_BYTES_7; break;
            case 8: dlc_bytes = FDCAN_DLC_BYTES_8; break;
            default: 
                DEV_PRINTF("ID：%08x -> 数据发送长度错误！\r\n\r\n", net_w5500_dh->addr);
                return false;
        }
        
        can_txh.Identifier = net_can_dh->addr;               // 32位ID
        can_txh.IdType = FDCAN_STANDARD_ID;                  // ID类型
        can_txh.TxFrameType = FDCAN_DATA_FRAME;              // 数据帧
        can_txh.DataLength = dlc_bytes;                      // 数据长度
        can_txh.ErrorStateIndicator = FDCAN_ESI_ACTIVE;      // 错误状态指示
        can_txh.FDFormat = FDCAN_CLASSIC_CAN;                // 发送帧格式
        can_txh.BitRateSwitch = FDCAN_BRS_OFF;               // 位速率切换
        can_txh.TxEventFifoControl = FDCAN_NO_TX_EVENTS;     // 关闭发送事件处理
        can_txh.MessageMarker = 0;                           // 消息标志
        
        if(HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan2,&can_txh,txb) != HAL_OK) 
            return false;   // 发送失败
    } while(tx_len);
    
    return true;	// 发送成功
}

/********************************************************************************************
* 函数名：Ph_Can_Receive
* 描  述：通过 CAN 接收数据（非阻塞轮询）
*         【重要】不清空缓冲区，由协议层的重组缓冲处理分包/粘包
* 输  入：@param: rdata -> 与协议层 crt_xfer->recv 一致，可为 NULL（仅返回字节数；由调用方写入 crt_xfer->rxlen）
* 输  出：@retval: 本次收到字节数；无数据或失败为 0（失败时打印区分）
********************************************************************************************/
uint16_t Ph_Can_Receive(uint8_t *rdata)
{
    static uint32_t rx_tk = 0;

    if(Dev_Tk_Wait(5000, rx_tk))    // 5s 未接收心跳/数据 = 外机未连接
    {
        tx_inft.can = false;
        if(tx_inft.crt == INFT_CAN)
            Net_TxInft_UpdateCrt();
    }

    if(HAL_FDCAN_GetRxMessage(&hfdcan2,FDCAN_RX_FIFO0,&can_rxh,rdata) != HAL_OK)  // 是否有数据接收
        return 0;

    if(can_rxh.RxFrameType == FDCAN_REMOTE_FRAME)    /* 远程帧 */
        return 0;

    if(can_rxh.Identifier != net_can_dh->addr)     // 非本机ID，忽略
        return 0;

    Dev_Tk_Init(&rx_tk);
    tx_inft.can = true;
    if((tx_inft.crt != INFT_TCP) && (tx_inft.crt != INFT_CAN))
        Net_TxInft_UpdateCrt();
//    return can_rxh.DataLength >> 16;
    return can_rxh.DataLength;
}

/********************************************************************************************
* 函数名：Ph_Lora_Init
* 描  述：LORA 外设初始化（UART5 / E32 已在 start_task 中可选同步，此处确保正常模式就绪）
* 输  入：无
* 输  出：@retval: true -> 初始化成功；false -> 初始化失败
* 调  用：Net_Periph_Register
********************************************************************************************/
bool Ph_Lora_Init(void)
{
    return LORA_Init();
}

/********************************************************************************************
* 函数名：Ph_Lora_DeInit
* 描  述：LORA 外设反初始化
* 输  入：无
* 输  出：无
* 调  用：Net_Periph_Unregister
********************************************************************************************/
void Ph_Lora_DeInit(void)
{

}

/********************************************************************************************
* 函数名：Ph_Lora_Transmit
* 描  述：通过 LORA UART 发送协议数据（配置会话期间 LORA_Transmit 内部丢弃）
* 输  入：@param: sdata -> 待发数据；@param: tx_len -> 字节数
* 输  出：@retval: true -> 发送成功；false -> 未发送或失败
* 调  用：协议层调用
********************************************************************************************/
bool Ph_Lora_Transmit(uint8_t *sdata, uint16_t tx_len)
{
    if(!sdata || tx_len == 0U)
        return false;

    return LORA_Transmit(sdata, tx_len);
}

/********************************************************************************************
* 函数名：Ph_Lora_Receive
* 描  述：通过 LORA 接收协议数据（非阻塞轮询；配置模式应答由 LORA_Receive 吸收并返回 0）
* 输  入：@param: rdata -> 协议层 crt_xfer->recv
* 输  出：@retval: 本次收到字节数；无数据为 0
* 调  用：协议层调用
********************************************************************************************/
uint16_t Ph_Lora_Receive(uint8_t *rdata)
{
    static uint32_t rx_tk = 0;
    uint16_t n;

    if(LORA_IsCfgBusy())
        return 0;

    if(Dev_Tk_Wait(5000, rx_tk))
    {
        tx_inft.lora = false;
        if(tx_inft.crt == INFT_LORA)
            Net_TxInft_UpdateCrt();
    }

    n = LORA_Receive(rdata, 0, 0);
    if(n == 0U)
        return 0;

    Dev_Tk_Init(&rx_tk);
    tx_inft.lora = true;
    if((tx_inft.crt != INFT_TCP) && (tx_inft.crt != INFT_CAN) && (tx_inft.crt != INFT_LORA))
        Net_TxInft_UpdateCrt();

    return n;
}

/********************************************************************************************
* 函数名：Net_Device_Update_Addr
* 描  述：更新协议设备地址（当 sys_cfg.dev_addr 改变时调用）
* 输  入：无
* 输  出：无
* 调  用：外部调用（当设备地址改变时）
********************************************************************************************/
void Net_Device_Update_Addr(void)
{
    if(net_w5500_dh)
        net_w5500_dh->addr = sys_cfg.dev_addr;
    
    if(net_can_dh)
        net_can_dh->addr = sys_cfg.dev_addr;

    if(net_lora_dh)
        net_lora_dh->addr = sys_cfg.dev_addr;
}


static bool Net_Tcp_Listen(uint8_t sock, uint16_t port, bool reopen)
{
    if(reopen)
        close(sock);
    else
        DEV_PRINTF("Opening Socket %u as TCP Server on port %u...\r\n",
                    (unsigned)sock, (unsigned)port);

    if(socket(sock, Sn_MR_TCP, port, 0x00) != 1 || !listen(sock))
    {
        DEV_PRINTF("Socket %u listen failed on port %u\r\n",
                    (unsigned)sock, (unsigned)port);
        return false;
    }

    if(!reopen)
    {
        DEV_PRINTF("W5500 Socket %u listen OK, port %u, status=0x%02X\r\n",
                    (unsigned)sock, (unsigned)port, (unsigned)getSn_SR(sock));
    }
    return true;
}

static void Net_Tcp_RecoverListen(uint8_t sock, uint16_t port)
{
    uint8_t st = getSn_SR(sock);

    if(st == SOCK_ESTABLISHED || st == SOCK_CLOSE_WAIT)
        disconnect(sock);
    if(st != SOCK_CLOSED && st != SOCK_INIT && st != SOCK_LISTEN)
        close(sock);

    if(getSn_SR(sock) != SOCK_LISTEN)
    {
        if(!Net_Tcp_Listen(sock, port, true))
        {
            // DEV_PRINTF("Socket %u recover listen failed on port %u (sr=0x%02X)\r\n",
            //             (unsigned)sock, (unsigned)port, (unsigned)getSn_SR(sock));
        }
    }

    if(sock < 2)
        s_sock_prev_st[sock] = getSn_SR(sock);
}

static void Net_Phy_Link_Maintain(void)
{
    uint8_t link_up = (getPHYCFGR() & LINK) ? 1u : 0u;
    uint8_t i;

    if(link_up == s_phy_link_up)
        return;

    s_phy_link_up = link_up;
    DEV_PRINTF("PHY link %s\r\n", link_up ? "up, restore TCP listen" : "down, close TCP sockets");

    for(i = 0; i < NET_PROTO_SOCK_CNT; i++)
    {
        if(!*s_proto_sock[i].ready)
            continue;
        if(link_up)
            Net_Tcp_Listen(s_proto_sock[i].sock, s_proto_sock[i].port, true);
        else
            close(s_proto_sock[i].sock);
    }
}

static bool Net_LocalIpValid(void)
{
    uint8_t ip[4];

    getSIPR(ip);
    if((ip[0] | ip[1] | ip[2] | ip[3]) == 0U)
        return false;
    if(ip[0] == 255U && ip[1] == 255U && ip[2] == 255U && ip[3] == 255U)
        return false;
    return true;
}

#if (SETTING_SOCKET_PORT != DATA_UPLOAD_SOCKET_PORT)
static void Net_Tcp_Socket_ResetStalePeer(uint8_t peer_sock, uint16_t peer_port,
                                            uint32_t peer_con_ms, uint32_t now)
{
    uint8_t peer_st = getSn_SR(peer_sock);
    uint32_t age = now - peer_con_ms;

    if(peer_st != SOCK_ESTABLISHED && peer_st != SOCK_CLOSE_WAIT)
        return;
    if(age <= NET_TCP_STALE_MS)
        return;

    // DEV_PRINTF("Reset stale peer Socket %u (port %u), CON age=%lu ms\r\n",
    //             (unsigned)peer_sock, (unsigned)peer_port, (unsigned long)age);

    if(peer_st == SOCK_CLOSE_WAIT)
        disconnect(peer_sock);
    close(peer_sock);
    Net_Tcp_Listen(peer_sock, peer_port, false);
}
#endif

static void Net_Tcp_Socket_Maintain(uint8_t sock, uint16_t port)
{
    uint8_t status = getSn_SR(sock);
    uint8_t ir = getSn_IR(sock);
    uint32_t now = HAL_GetTick();
    uint8_t prev_st = (sock < 2) ? s_sock_prev_st[sock] : SOCK_CLOSED;

    if((ir & Sn_IR_CON) && status == SOCK_ESTABLISHED && prev_st != SOCK_ESTABLISHED)
    {
        setSn_IR(sock, Sn_IR_CON);
        if(sock < 2)
            s_sock_con_ms[sock] = now;
#if (SETTING_SOCKET_PORT != DATA_UPLOAD_SOCKET_PORT)
        if(sock == s_proto_sock[0].sock)
            Net_Tcp_Socket_ResetStalePeer(s_proto_sock[1].sock, s_proto_sock[1].port,
                                            s_sock_con_ms[s_proto_sock[1].sock], now);
        else if(sock == s_proto_sock[1].sock)
            Net_Tcp_Socket_ResetStalePeer(s_proto_sock[0].sock, s_proto_sock[0].port,
                                            s_sock_con_ms[s_proto_sock[0].sock], now);
#endif
    }
    else if(ir & Sn_IR_CON)
    {
        setSn_IR(sock, Sn_IR_CON);
    }

    if(ir & (Sn_IR_DISCON | Sn_IR_TIMEOUT))
    {
        setSn_IR(sock, ir & (Sn_IR_DISCON | Sn_IR_TIMEOUT));
        Net_Tcp_RecoverListen(sock, port);
        return;
    }

    status = getSn_SR(sock);
    switch(status)
    {
        case SOCK_LISTEN:
        case SOCK_ESTABLISHED:
            break;

        /* 客户端断开后：仅首次 disconnect/close，若仍卡在关闭流程则强制 recover */
        case SOCK_CLOSE_WAIT:
            if(prev_st == SOCK_ESTABLISHED)
                disconnect(sock);
            else if(prev_st == SOCK_CLOSE_WAIT)
                Net_Tcp_RecoverListen(sock, port);
            break;

        case SOCK_CLOSING:
        case SOCK_FIN_WAIT:
        case SOCK_TIME_WAIT:
        case SOCK_LAST_ACK:
            if(prev_st != status)
                close(sock);
            else
                Net_Tcp_RecoverListen(sock, port);
            break;

        case SOCK_CLOSED:
        case SOCK_INIT:
            Net_Tcp_Listen(sock, port, true);
            break;

        case SOCK_SYNSENT:
        case SOCK_SYNRECV:
            if(sock < 2)
            {
                if(prev_st != status)
                    s_sock_con_ms[sock] = now;
                else if((now - s_sock_con_ms[sock]) >= NET_TCP_SYN_HOLD_MS)
                    Net_Tcp_RecoverListen(sock, port);
            }
            break;

        default:
            break;
    }

    if(sock < 2)
        s_sock_prev_st[sock] = getSn_SR(sock);
}

static uint16_t Net_Tcp_Socket_Recv(uint8_t sock, uint8_t *rdata, uint16_t cap)
{
    uint16_t rx_size, drop_len, got, n;

    if(!rdata || cap == 0 || getSn_SR(sock) != SOCK_ESTABLISHED)
        return 0;

    rx_size = getSn_RX_RSR(sock);
    if(rx_size == 0)
        return 0;

    if(rx_size > cap)
    {
        drop_len = rx_size;
        while(drop_len > 0)
        {
            got = recv(sock, rdata, (drop_len > cap) ? cap : drop_len);
            if(got == 0)
                break;
            drop_len = (got >= drop_len) ? 0u : (uint16_t)(drop_len - got);
        }
        // DEV_PRINTF("Socket %u RX backlog=%u, flush done -> resync\r\n",
        //             (unsigned)sock, (unsigned)rx_size);
        return 0;
    }

    n = recv(sock, rdata, rx_size);
    
// #if NET_RECV_DEBUG
//     if(n > 0)
//     {
//         DEV_PRINTF("[TCP 接收] 原始数据：");
//         for(uint16_t i = 0; i < n; i++)
//         {
//             DEV_PRINTF("%02X ", rdata[i]);
//         }
//         DEV_PRINTF("(共 %d 字节，t=%u)\r\n", n, DEV_GET_1MS_TICK_FUN());
//     }
// #endif
    
    return (n > cap) ? cap : n;
}


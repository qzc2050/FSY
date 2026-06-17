/**********************************************************************************************************
 * 文件名：net_raw_app.c
 * 概  述：网络/串口裸协议应用（设备绑定与发送失败回调；PDU 解析在 net_raw_protocol.c）
 * 创建时间：2026-03-30
 * 更新时间：2026-03-30
 * 作  者：LYJ
 * 版  本：1.0.0
 * Copyright (c) 2025, Mr.Liu All rights reserved.
 **********************************************************************************************************/
#include "./net_raw/net_raw_app.h"
#include "./net_raw/net_raw_protocol.h"
#include "./net_raw/net_raw_bsp.h"
#include "./core/dev_queue.h"

#include "main.h"
#include "socket.h"
#include "device_config.h"
#include "w5500_dhcp.h"
#include "geiger.h"  // 用于 sys_cfg 结构体
#include "env_monitor.h"  // 用于 env_data 结构体
#include "pcf8563.h"
#include "reg_flash.h"  // Flash 读写函数
#include "lora.h"
#include "hist_record_app.h"  // 历史记录管理

#include "FreeRTOS.h"
#include "task.h"

/* 包含重启函数 */
#include "core_cm7.h"  // NVIC_SystemReset


/* 设备状态变量（bit0-14 对应协议定义的各个状态标志） */
static uint32_t g_device_status = 0;  // 初始化为 0，后续根据硬件状态更新

/* OTA 状态变量 */
static Ota_State_t g_ota_state = OTA_STATE_IDLE;  // 初始为空闲状态
static uint32_t g_ota_file_size = 0;              // OTA 文件大小
static uint32_t g_ota_written_bytes = 0;          // 已写入字节数
static uint32_t g_ota_file_crc = 0;               // 文件 CRC32

/* 重启定时器变量 */
static uint32_t g_ota_done_time = 0;              // 进入 DONE 状态的时间戳
static uint8_t g_ota_reboot_pending = 0;          // 重启等待标志
static uint32_t g_ota_last_activity_ms = 0;     // 最近一次 OTA 活动（收包/开始/结束）

/* 写寄存器接收侧进度掩码（time_cfg / hist / thr，见 Net_RegWriteMask_t） */
static Net_RegWriteMask_t g_reg_write_mask;

/* 5 分钟历史记录上下文（Net_5MinHistCtx_t） */
static Net_5MinHistCtx_t g_5min_hist = {
    NET_5MIN_HIST_IDLE,
    NULL,
    0U,
    0U,
    0,
    0U,
    0U
};

/* 函数声明 */
static void net_reg_write_mask_reset_hist(void);
static void ota_touch_activity(void);
static void ota_reset_idle_session(void);
static void net_hist_ctx_reset(void);
static void net_hist_on_reg_written(Net_Device_t *dev, uint16_t reg, uint16_t qreg);
static void net_time_cfg_on_reg_written(Net_Device_t *dev, uint16_t reg, uint16_t qreg);
static void net_thr_on_reg_written(Net_Device_t *dev, uint16_t reg, uint16_t qreg);
static void net_alarm_biten_on_reg_written(Net_Device_t *dev, uint16_t reg, uint16_t qreg);
static void net_alarm_biten_apply(Net_Device_t *dev, uint32_t biten);
static void net_dev_ctrl_on_reg_written(Net_Device_t *dev, uint16_t reg, uint16_t qreg);
static bool net_cfg_try_save_flash(void);

extern uint8_t USB_MSC_IsStarted(void);

/* 设备接口优先级 */
Dev_Inft_Struct tx_inft = {0};

extern Net_Device_t *net_w5500_dh;
extern Net_Device_t *net_can_dh;
extern Net_Device_t *net_lora_dh;

extern volatile Sys_Cfg_Struct sys_cfg;  // 设备配置结构体（定义在 geiger.h）
extern Environment_Data_t env_data;  // 传感器数据结构体（定义在 env_monitor.c）
extern Data_Var_Struct data_var;  // 剂量数据变量（定义在 geiger.c）

/***************************************************************************************************
* 函数实现
***************************************************************************************************/

static void net_reg_write_mask_reset_hist(void)
{
    g_reg_write_mask.hist_start = 0U;
    g_reg_write_mask.hist_end = 0U;
}


static void ota_touch_activity(void)
{
    g_ota_last_activity_ms = HAL_GetTick();
}

static void ota_reset_idle_session(void)
{
    DEV_PRINTF("[OTA] 会话超时（%u ms 无数据），恢复 IDLE\r\n",
                (unsigned)OTA_SESSION_IDLE_MS);
    g_ota_state = OTA_STATE_IDLE;
    g_ota_written_bytes = 0;
    g_ota_file_size = 0;
    g_ota_file_crc = 0;
    g_ota_last_activity_ms = 0;
}

bool Ota_IsHeartbeatPaused(void)
{
    return (g_ota_state == OTA_STATE_STARTED || g_ota_state == OTA_STATE_VERIFY);
}

static void net_hist_ctx_reset(void)
{
    g_5min_hist.state = NET_5MIN_HIST_IDLE;
    g_5min_hist.dev = NULL;
    g_5min_hist.ts_start = 0U;
    g_5min_hist.ts_end = 0U;
    g_5min_hist.scan_logical = 0;
    g_5min_hist.queued = 0U;
    g_5min_hist.drain_since = 0U;
    net_reg_write_mask_reset_hist();
}



/********************************************************************************************
* 函数名：Net_TxInft_UpdateCrt
* 描  述：根据 tx_inft 各接口连接状态更新当前传输接口（优先级：TCP > CAN > LORA）
* 输  入：无
* 输  出：无
* 调  用：外部调用
********************************************************************************************/
void Net_TxInft_UpdateCrt(void)
{
    if(tx_inft.tcp)
    {
        tx_inft.crt = INFT_TCP;
        DEV_PRINTF("Tx 优先级: TCP传输模式！\r\n");
    }
    else if(tx_inft.can)
    {
        tx_inft.crt = INFT_CAN;
        DEV_PRINTF("Tx 优先级: CAN传输模式！\r\n");
    }
    else if(tx_inft.lora)
    {
        tx_inft.crt = INFT_LORA;
        DEV_PRINTF("Tx 优先级: LORA传输模式！\r\n");
    }
    else
    {
        if(tx_inft.crt != INFT_NULL)
        {
            tx_inft.crt = INFT_NULL;
            DEV_PRINTF("Tx 优先级: 停止传输！\r\n");
        }
    }
}



/********************************************************************************************
* 函数名：Net_Resolve_Handle
* 描  述：应用层 PDU 解析入口（无 CRC 的 PDU）；先解析功能码和寄存器地址，然后调用 Net_Protocol_HandlePdu
*         注意：Net_Protocol_HandlePdu 会先操作寄存器，用户不作处理才会调用此函数
* 输  入：@param: dev -> 设备句柄；@param: fc -> 功能码；@param: addr -> 寄存器地址；@param: len -> 数据长度
* 输  出：无
* 调  用：内部调用（Net_Resolve_Task）
********************************************************************************************/
void Net_Resolve_Handle(Net_Device_t *dev, uint8_t fc, uint16_t reg, uint16_t len)
{
    if(!dev || ((dev != net_w5500_dh) && (dev != net_can_dh) && (dev != net_lora_dh)))
    {
        DEV_PRINTF("未解析设备描述符：%u！\r\n", dev ? (unsigned)dev->id : 0u);
        return;
    }
    
    // 根据功能码进行相应的应用层处理
    switch(fc)
    {
        case NET_FC_READ_HOLDING_REQ:  // 读多寄存器 (0x03)
            Net_App_HandleReadMulti(dev, reg, len);
            break;
            
        case NET_FC_READ_SINGLE_REQ:  // 读单寄存器 (0x05)
            Net_App_HandleReadSingle(dev, reg, len);
            break;
            
        case NET_FC_WRITE_SINGLE_REQ:  // 写单寄存器 (0x06)
            Net_App_HandleWriteSingle(dev, reg, len);
            break;
            
        case NET_FC_WRITE_MULTI_REQ:  // 写多寄存器 (0x10)
            Net_App_HandleWriteMulti(dev, reg, len);
            break;
            
        case NET_FC_READ_HOLDING_RESP:  // 读多应答 (0x13)
            Net_App_HandleReadMultiResp(dev, reg, len);
            break;
            
        case NET_FC_READ_SINGLE_RESP:  // 读单应答 (0x15)
            Net_App_HandleReadSingleResp(dev, reg, len);
            break;

        case NET_FC_WRITE_MULTI_RESP:   // 写多应答 (0x20)，上位机确认 0x23
        case NET_FC_WRITE_SINGLE_RESP:  // 写单应答 (0x16)，上位机确认 0x25
            break;

        case NET_FC_ACTIVE_UPLOAD:  // 主动上传 (多) (0x23)
            Net_App_HandleActiveUploadMulti(dev, reg, len);
            break;
            
        case NET_FC_ACTIVE_UPLOAD_SINGLE:
            Net_App_HandleActiveUploadSingle(dev, reg, len);
            break;
            
        default:
            DEV_PRINTF("应用层解析：未知功能码 0x%02X, 地址=%u, 长度=%u\r\n", fc, (unsigned)reg, (unsigned)len);
            break;
    }
}

/********************************************************************************************
* 函数名：Net_App_HandleReadMulti
* 描  述：应用层处理函数 - 读多寄存器 (0x03)
*         特殊处理 OTA 状态寄存器（204-207）
* 输  入：@param: dev -> 设备句柄；@param: addr -> 寄存器地址；@param: len -> 数据长度
* 输  出：无
* 调  用：外部调用（Net_Resolve_Handle）
********************************************************************************************/
__weak void Net_App_HandleReadMulti(Net_Device_t *dev, uint16_t reg, uint16_t len)
{
    /* 其他普通寄存器读取已在协议层处理 */
}

/********************************************************************************************
* 函数名：Net_App_HandleReadSingle
* 描  述：应用层处理函数 - 读单寄存器 (0x05)
* 输  入：@param: dev -> 设备句柄；@param: addr -> 寄存器地址；@param: len -> 数据长度
* 输  出：无
* 调  用：外部调用（Net_Resolve_Handle）
********************************************************************************************/
__weak void Net_App_HandleReadSingle(Net_Device_t *dev, uint16_t reg, uint16_t len)
{
    // 用户可在此添加自定义处理逻辑
}

/********************************************************************************************
* 函数名：Net_App_HandleWriteSingle
* 描  述：应用层处理函数 - 写单寄存器 (0x06)
* 输  入：@param: dev -> 设备句柄；@param: addr -> 寄存器地址；@param: len -> 数据长度
* 输  出：无
* 调  用：外部调用（Net_Resolve_Handle）
********************************************************************************************/
void Net_App_HandleWriteSingle(Net_Device_t *dev, uint16_t reg, uint16_t len)
{
    (void)len;
    net_thr_on_reg_written(dev, reg, 1U);
    net_alarm_biten_on_reg_written(dev, reg, 1U);
    net_dev_ctrl_on_reg_written(dev, reg, 1U);
    net_time_cfg_on_reg_written(dev, reg, 1U);
    net_hist_on_reg_written(dev, reg, 1U);
}

/********************************************************************************************
* 函数名：Net_App_HandleWriteMulti
* 描  述：应用层处理函数 - 写多寄存器 (0x10)
*         特殊处理 OTA 升级寄存器（200-203, 208+）
* 输  入：@param: dev -> 设备句柄；@param: addr -> 寄存器地址；@param: len -> 数据长度
* 输  出：无
* 调  用：外部调用（Net_Resolve_Handle）
********************************************************************************************/
__weak void Net_App_HandleWriteMulti(Net_Device_t *dev, uint16_t reg, uint16_t len)
{
    switch(reg)
    {
        /* OTA 开始指令（写入寄存器 200-201） */
        case NET_REG_OTA_FILE_SIZE:
            if(len == 2)
            {
                uint32_t file_size = 0;
                
                /* 从寄存器表读取文件大小（寄存器 200-201，4 字节） */
                /* 使用 Net_Reg_Holding_Read_U32 正确读取 32 位数据 */
                file_size = Net_Reg_Holding_Read_U32(dev, NET_REG_OTA_FILE_SIZE);
                
                DEV_PRINTF("[OTA] 开始指令：file_size=%lu, len=%d\r\n", (unsigned long)file_size, len);
                
                /* 保存 OTA 参数 */
                g_ota_file_size = file_size;
                g_ota_written_bytes = 0;
                g_ota_state = OTA_STATE_STARTED;
                ota_touch_activity();
                
                /* 调用 OTA 准备函数（擦除 Flash） */
                if(!Ota_PrepareDownload(file_size))
                {
                    g_ota_state = OTA_STATE_ERROR;
                }
                
                /* 主动发送一次当前 OTA 状态（寄存器 204-207） */
                Net_Active_Upload_OtaStatus(dev, g_ota_state, g_ota_written_bytes);
            }
            break;
        
        /* OTA 数据写入（寄存器 208-271，最多 128 字节） */
        case NET_REG_OTA_DATA:
            // DEV_PRINTF("[OTA] reg=%u, len=%u\r\n", (unsigned)reg, (unsigned)len);
            /* 调用 OTA 数据处理函数 */
            Ota_ProcessPacket(dev, len);
            break;
        
        /* OTA 结束指令（写入寄存器 202-203，CRC32） */
        case NET_REG_OTA_CRC32:
            if(len == 2)
            {
                Ota_HandleFinishCommand(dev);
            }
            break;
        
        default:
            /* 其他寄存器写入已在协议层处理 */
            break;
    }

    net_thr_on_reg_written(dev, reg, len);
    net_alarm_biten_on_reg_written(dev, reg, len);
    net_dev_ctrl_on_reg_written(dev, reg, len);
    net_time_cfg_on_reg_written(dev, reg, len);
    net_hist_on_reg_written(dev, reg, len);
}

/********************************************************************************************
* 函数名：Net_App_HandleReadMultiResp
* 描  述：应用层处理函数 - 读多应答 (0x13)
* 输  入：@param: dev -> 设备句柄；@param: addr -> 寄存器地址；@param: len -> 数据长度
* 输  出：无
* 调  用：外部调用（Net_Resolve_Handle）
********************************************************************************************/
__weak void Net_App_HandleReadMultiResp(Net_Device_t *dev, uint16_t reg, uint16_t len)
{
    // 用户可在此添加自定义处理逻辑
    // 例如：解析返回的传感器数据
}

/********************************************************************************************
* 函数名：Net_App_HandleReadSingleResp
* 描  述：应用层处理函数 - 读单应答 (0x15)
* 输  入：@param: dev -> 设备句柄；@param: addr -> 寄存器地址；@param: len -> 数据长度
* 输  出：无
* 调  用：外部调用（Net_Resolve_Handle）
********************************************************************************************/
__weak void Net_App_HandleReadSingleResp(Net_Device_t *dev, uint16_t reg, uint16_t len)
{
    // 用户可在此添加自定义处理逻辑
}

/********************************************************************************************
* 函数名：Net_App_HandleActiveUploadMulti
* 描  述：应用层处理函数 - 主动上传多 (0x23)
* 输  入：@param: dev -> 设备句柄；@param: addr -> 寄存器地址；@param: len -> 数据长度
* 输  出：无
* 调  用：外部调用（Net_Resolve_Handle）
********************************************************************************************/
__weak void Net_App_HandleActiveUploadMulti(Net_Device_t *dev, uint16_t reg, uint16_t len)
{
    // 用户可在此添加自定义处理逻辑
    // 例如：处理定时上传的传感器数据
}

/********************************************************************************************
* 函数名：Net_App_HandleActiveUploadSingle
* 描  述：应用层处理函数 - 主动上传单 (0x25)
* 输  入：@param: dev -> 设备句柄；@param: addr -> 寄存器地址；@param: len -> 数据长度
* 输  出：无
* 调  用：外部调用（Net_Resolve_Handle）
********************************************************************************************/
__weak void Net_App_HandleActiveUploadSingle(Net_Device_t *dev, uint16_t reg, uint16_t len)
{
    // 用户可在此添加自定义处理逻辑
}

/********************************************************************************************
* 函数名：Net_Send_Err_Handle
* 描  述：NET 指令发送失败处理
* 输  入：@param: id -> 设备描述符
*         @param: *sdata -> 数据指针
*         @param: size -> 数据大小
* 输  出：无
* 调  用：外部调用
********************************************************************************************/
void Net_Send_Err_Handle(uint16_t id, uint8_t *sdata, uint16_t size)
{
#if NET_SEND_ERR_HANDLE
    if(net_w5500_dh && id == net_w5500_dh->id)
    {
        DEV_PRINTF("TX -> FAIL -> ");
        for(uint16_t i = 0; i < size; i++)
            DEV_PRINTF("%02X ", sdata[i]);
        DEV_PRINTF("\r\n");
    }
    else
        DEV_PRINTF("未解析设备描述符：%d！\r\n", id);
#else
    (void)id;
    (void)sdata;
    (void)size;
#endif
}

/********************************************************************************************
* 函数名：Net_User_Registers_Init
* 描  述：用户自定义寄存器初始化接口
*         通过 Net_Reg_Holding_Write_U16 函数初始化寄存器值（从 0 开始自增）
*         需在 Dev_Protocol_init 完成后调用（确保 net_w5500_dh 已注册）
* 输  入：无
* 输  出：无
* 调  用：外部调用（freertos_app.c）
********************************************************************************************/
void Net_User_Registers_Init(void)
{
    uint8_t ip[4] = {0};
    uint16_t reg_value = 0;
    
    if(!net_w5500_dh)
    {
        DEV_PRINTF("[寄存器] net_w5500_dh 未注册，跳过初始化\r\n");
        return;
    }
    
    DEV_PRINTF("[寄存器] 开始初始化用户寄存器...\r\n");
    
    /* 1. 初始化序列号寄存器（地址 0-5，共 6 个寄存器，12 字节）
     *    使用 DEVICE_CFG_DEFAULT_SN 的前 12 个字符
     */
    const char *sn = DEVICE_CFG_DEFAULT_SN;
    uint16_t sn_len = strlen(sn);
    for(uint8_t i = 0; i < 6; i++)
    {
        uint16_t char1 = (i * 2 < sn_len) ? (uint8_t)sn[i * 2] : 0;
        uint16_t char2 = (i * 2 + 1 < sn_len) ? (uint8_t)sn[i * 2 + 1] : 0;
        reg_value = (char1 << 8) | char2;
        
        Net_Reg_Holding_Write_U16(net_w5500_dh, i, reg_value);
    }
    DEV_PRINTF("[寄存器] 序列号写入: %s\r\n", sn);
    
    /* 2. 初始化 IP 地址寄存器（地址 6-7，共 2 个寄存器，4 字节）
     *    从 W5500 获取当前 IP 地址
     */
    W5500_Get_IP(ip);
    
    /* 寄存器 6: IP[0] | IP[1] */
    reg_value = ((uint16_t)ip[0] << 8) | ip[1];
    Net_Reg_Holding_Write_U16(net_w5500_dh, 6, reg_value);
    
    /* 寄存器 7: IP[2] | IP[3] */
    reg_value = ((uint16_t)ip[2] << 8) | ip[3];
    Net_Reg_Holding_Write_U16(net_w5500_dh, 7, reg_value);
    
    DEV_PRINTF("[寄存器] IP 地址写入 : %d.%d.%d.%d\r\n", 
            ip[0], ip[1], ip[2], ip[3]);
    
    /* 3. 初始化产品型号寄存器（地址 8-13，共 6 个寄存器，12 字节）
     *    使用 DEVICE_PRODUCT_MODEL
     */
    const char *model = DEVICE_PRODUCT_MODEL;
    uint16_t model_len = strlen(model);
    for(uint8_t i = 0; i < 6; i++)
    {
        uint16_t char1 = (i * 2 < model_len) ? (uint8_t)model[i * 2] : 0;
        uint16_t char2 = (i * 2 + 1 < model_len) ? (uint8_t)model[i * 2 + 1] : 0;
        reg_value = (char1 << 8) | char2;
        
        Net_Reg_Holding_Write_U16(net_w5500_dh, 8 + i, reg_value);
    }
    DEV_PRINTF("[寄存器] 产品型号: %s\r\n", model);
    
    /* 4. 初始化协议类型寄存器（地址 14-17，共 4 个寄存器，8 字节）
     *    使用 DEVICE_PROTOCOL_TYPE
     *    注意：不要覆盖地址 13（报警状态）和地址 15（设备状态）
     */
    const char *protocol = DEVICE_PROTOCOL_TYPE;
    uint16_t proto_len = strlen(protocol);
    for(uint8_t i = 0; i < 4; i++)
    {
        uint16_t char1 = (i * 2 < proto_len) ? (uint8_t)protocol[i * 2] : 0;
        uint16_t char2 = (i * 2 + 1 < proto_len) ? (uint8_t)protocol[i * 2 + 1] : 0;
        reg_value = (char1 << 8) | char2;
        
        Net_Reg_Holding_Write_U16(net_w5500_dh, 14 + i, reg_value);
    }
    DEV_PRINTF("[寄存器] 协议类型写入: %s\r\n", protocol);
    
    /* 5. 初始化报警状态寄存器（地址 13）为 0
     *    确保初始状态下无报警
     */
    Net_Reg_Holding_Write_U32(net_w5500_dh, NET_REG_ALARM_BIT1, 0);
    DEV_PRINTF("[寄存器] 报警状态已清零\r\n");
    
    /* 6. 阈值 / 序列号 / 设备参数（来自 Flash sys_cfg） */
    Net_Config_Sync_To_Registers(CFG_IDX_RATE);
    Net_Config_Sync_To_Registers(CFG_IDX_ENV);
    Net_Config_Sync_To_Registers(CFG_IDX_DEVICE_INFO);
    Net_Config_Sync_To_Registers(CFG_IDX_ALARM_EN);
    Net_Config_Sync_To_Registers(CFG_IDX_DEVICE_ADDR);
    
    /* 7. 根据 Flash 中的阈值初始化 shadow 标志（上电时可能阈值=0） */
    if(sys_cfg.th_rh_rate <= 0.0f)
        sys_cfg.dose_th_shadow_flags |= NET_DOSE_SHADOW_HI_VALID;
    if(sys_cfg.th_rl_rate <= 0.0f)
        sys_cfg.dose_th_shadow_flags |= NET_DOSE_SHADOW_LO_VALID;
    
    DEV_PRINTF("[寄存器] 初始化完成！shadow_flags=0x%02X\r\n", 
               (unsigned)sys_cfg.dose_th_shadow_flags);
}

/********************************************************************************************
* 函数名：Net_Active_Upload_SensorData
* 描  述：从机主动上传传感器数据（功能码 0x23）
*         协议格式：[地址][0x23][字节数][数据...][CRC]
*         上传数据包括：辐射量、温度、气压、湿度、CO2、PM2.5、报警状态、设备状态
* 输  入：@param: *dev -> 设备句柄
*         @param: upload_type -> 上传类型 (0x23=定期上传，0x25=事件触发上传)
* 输  出：@retval: true -> 上传成功；false -> 上传失败
* 调  用：外部调用（周期任务或事件触发）
********************************************************************************************/
bool Net_Active_Upload_SensorData(Net_Device_t *dev, uint8_t upload_type)
{
    uint8_t frame_buf[64] = {0};
    uint16_t frame_idx = 0;
    uint16_t crc;
    
    if(!dev || !dev->reg_tb || dev->reg_sz < 2)
        return false;
    
    if(Ota_IsHeartbeatPaused())
        return false;
    
    /* 数据发送链路未连接时不上传（避免发送队列堆积） */
    if(dev == net_w5500_dh)
        if(!Net_Tcp_DataSendReady())
            return false;
    
    /* 构建主动上传完整帧
     * 协议格式：[地址][功能码][字节数][寄存器起始地址][数据...][CRC]
     * 传感器数据上传 8 个传感器数据（从地址 1 开始，每个 4 字节）
     */
    
    /* 1. 设备地址 */
    frame_buf[frame_idx++] = dev->addr;
    
    /* 2. 功能码 */
    frame_buf[frame_idx++] = upload_type;  // 0x23 或 0x25
    
    /* 3. 数据字节数（8 个传感器 * 4 字节 = 32 字节） */
    uint8_t byte_count = 44;  // 8 个传感器数据，每个 4 字节
    frame_buf[frame_idx++] = byte_count;
    
    /* 4. 寄存器起始地址（从地址 1 开始，小端序） */
    frame_buf[frame_idx++] = (uint8_t)(NET_REG_DOSE_RATE & 0xFF);
    frame_buf[frame_idx++] = (uint8_t)((NET_REG_DOSE_RATE >> 8) & 0xFF);
    
    /* 5. 读取寄存器数据并填入（小端序）
     * 传感器数据是 32 位，每个占用 2 个寄存器
     * 寄存器地址：1(辐射量), 3(温度), 5(气压), 7(湿度), 9(CO2), 11(PM2.5), 13(报警), 15(状态)
     */
    uint16_t reg_addrs[] = {
        NET_REG_DOSE_RATE,      // 1 - 辐射量
        NET_REG_TEMP,           // 3 - 温度
        NET_REG_PRESS,          // 5 - 气压
        NET_REG_HUM,            // 7 - 湿度
        NET_REG_CO2,            // 9 - CO2
        NET_REG_PM2D5,          // 11 - PM2.5
        NET_REG_ALARM_BIT1,     // 13 - 报警状态
        NET_REG_STATUS_BIT      // 15 - 设备状态
    };
    
    for(uint8_t i = 0; i < 8; i++)
    {
        /* 读取 32 位数据（占用 2 个寄存器） */
        uint32_t value = Net_Reg_Holding_Read_U32(dev, reg_addrs[i]);
        /* 小端序：先低字节，后高字节 */
        frame_buf[frame_idx++] = (uint8_t)(value & 0xFF);
        frame_buf[frame_idx++] = (uint8_t)((value >> 8) & 0xFF);
        frame_buf[frame_idx++] = (uint8_t)((value >> 16) & 0xFF);
        frame_buf[frame_idx++] = (uint8_t)((value >> 24) & 0xFF);
    }
    for(uint8_t i = 0; i < 3; i++)
    {
        /* 读取 32 位数据（占用 2 个寄存器） */
        frame_buf[frame_idx++] = 0;
        frame_buf[frame_idx++] = 0;
        frame_buf[frame_idx++] = 0;
        frame_buf[frame_idx++] = 0;
    }
    
    /* 6. 计算并添加 CRC（对地址到数据的所有字节） */
    crc = Net_Modbus_Crc16(frame_buf, frame_idx);
    frame_buf[frame_idx++] = (uint8_t)(crc & 0xFF);
    frame_buf[frame_idx++] = (uint8_t)(crc >> 8);
    
    /* 打印发送的帧数据用于调试 */
    // printf("[主动上传] 发送帧：");
    // for(uint16_t i = 0; i < frame_idx; i++)
    // {
    //     printf("%02X ", frame_buf[i]);
    // }
    // printf("\r\n");
    
    if(dev->transmit(frame_buf, frame_idx))
    {
        // DEV_PRINTF("[主动上传] 传感器数据上传成功，类型：0x%02X, 帧长度：%d\r\n", upload_type, frame_idx);
        return true;
    }
    else
    {
        DEV_PRINTF("[主动上传] 传感器数据上传失败！\r\n");
        return false;
    }


    // uint8_t frame_buf[64];
    // uint16_t frame_idx = 0;
    // uint16_t crc;
    
    // if(!dev || !dev->reg_tb || dev->reg_sz < 2)
    //     return false;
    
    // if(Ota_IsHeartbeatPaused())
    //     return false;
    
    // /* 数据发送链路未连接时不上传（避免发送队列堆积） */
    // if(dev == net_w5500_dh)
    //     if(!Net_Tcp_DataSendReady())
    //         return false;
    
    // /* 构建主动上传完整帧
    //  * 协议格式：[地址][功能码][字节数][寄存器起始地址][数据...][CRC]
    //  * 传感器数据上传 8 个传感器数据（从地址 1 开始，每个 4 字节）
    //  */
    
    // /* 1. 设备地址 */
    // frame_buf[frame_idx++] = dev->addr;
    
    // /* 2. 功能码 */
    // frame_buf[frame_idx++] = upload_type;  // 0x23 或 0x25
    
    // /* 3. 数据字节数（8 个传感器 * 4 字节 = 32 字节） */
    // uint8_t byte_count = 32;  // 8 个传感器数据，每个 4 字节
    // frame_buf[frame_idx++] = byte_count;
    
    // /* 4. 寄存器起始地址（从地址 1 开始，小端序） */
    // frame_buf[frame_idx++] = (uint8_t)(NET_REG_DOSE_RATE & 0xFF);
    // frame_buf[frame_idx++] = (uint8_t)((NET_REG_DOSE_RATE >> 8) & 0xFF);
    
    // /* 5. 读取寄存器数据并填入（小端序）
    //  * 传感器数据是 32 位，每个占用 2 个寄存器
    //  * 寄存器地址：1(辐射量), 3(温度), 5(气压), 7(湿度), 9(CO2), 11(PM2.5), 13(报警), 15(状态)
    //  */
    // uint16_t reg_addrs[] = {
    //     NET_REG_DOSE_RATE,      // 1 - 辐射量
    //     NET_REG_TEMP,           // 3 - 温度
    //     NET_REG_PRESS,          // 5 - 气压
    //     NET_REG_HUM,            // 7 - 湿度
    //     NET_REG_CO2,            // 9 - CO2
    //     NET_REG_PM2D5,          // 11 - PM2.5
    //     NET_REG_ALARM_BIT1,     // 13 - 报警状态
    //     NET_REG_STATUS_BIT      // 15 - 设备状态
    // };
    
    // for(uint8_t i = 0; i < 8; i++)
    // {
    //     /* 读取 32 位数据（占用 2 个寄存器） */
    //     uint32_t value = Net_Reg_Holding_Read_U32(dev, reg_addrs[i]);
    //     /* 小端序：先低字节，后高字节 */
    //     frame_buf[frame_idx++] = (uint8_t)(value & 0xFF);
    //     frame_buf[frame_idx++] = (uint8_t)((value >> 8) & 0xFF);
    //     frame_buf[frame_idx++] = (uint8_t)((value >> 16) & 0xFF);
    //     frame_buf[frame_idx++] = (uint8_t)((value >> 24) & 0xFF);
    // }
    
    // /* 6. 计算并添加 CRC（对地址到数据的所有字节） */
    // crc = Net_Modbus_Crc16(frame_buf, frame_idx);
    // frame_buf[frame_idx++] = (uint8_t)(crc & 0xFF);
    // frame_buf[frame_idx++] = (uint8_t)(crc >> 8);
    
    // /* 打印发送的帧数据用于调试 */
    // // printf("[主动上传] 发送帧：");
    // // for(uint16_t i = 0; i < frame_idx; i++)
    // // {
    // //     printf("%02X ", frame_buf[i]);
    // // }
    // // printf("\r\n");
    
    // if(dev->transmit(frame_buf, frame_idx))
    // {
    //     // DEV_PRINTF("[主动上传] 传感器数据上传成功，类型：0x%02X, 帧长度：%d\r\n", upload_type, frame_idx);
    //     return true;
    // }
    // else
    // {
    //     DEV_PRINTF("[主动上传] 传感器数据上传失败！\r\n");
    //     return false;
    // }
}

/********************************************************************************************
* 函数名：Net_Active_Upload_AlarmStatus
* 描  述：从机主动上传报警状态（事件触发）
*         当报警状态寄存器（地址 13）发生变化时调用
* 输  入：@param: *dev -> 设备句柄
*         @param: alarm_status -> 当前报警状态（32 位）
* 输  出：@retval: true -> 上传成功；false -> 上传失败
* 调  用：外部调用（报警事件触发）
********************************************************************************************/
bool Net_Active_Upload_AlarmStatus(Net_Device_t *dev, uint32_t alarm_status)
{
    uint8_t frame_buf[16];
    uint16_t frame_idx = 0;
    uint16_t crc;
    
    if(!dev)
        return false;
    
    if(Ota_IsHeartbeatPaused())
        return false;
    
    /* 构建报警状态上传完整帧
     * 协议格式：[地址][功能码][字节数][寄存器起始地址][数据...][CRC]
     * 报警状态占用 2 个连续寄存器（地址 13-14）
     */
    
    /* 1. 设备地址 */
    frame_buf[frame_idx++] = dev->addr;
    
    /* 2. 功能码 */
    frame_buf[frame_idx++] = NET_FC_ACTIVE_UPLOAD;  // 0x23
    
    /* 3. 数据字节数（2 个寄存器 * 2 字节 = 4 字节） */
    frame_buf[frame_idx++] = 4;
    
    /* 4. 寄存器起始地址（报警状态寄存器地址 = 13，小端序） */
    frame_buf[frame_idx++] = (uint8_t)(NET_REG_ALARM_BIT1 & 0xFF);
    frame_buf[frame_idx++] = (uint8_t)((NET_REG_ALARM_BIT1 >> 8) & 0xFF);
    
    /* 5. 报警状态数据（小端序，2 个寄存器共 4 字节） */
    /* 寄存器 13（低 16 位） */
    uint16_t alarm_lo = (uint16_t)(alarm_status & 0xFFFF);
    frame_buf[frame_idx++] = (uint8_t)(alarm_lo & 0xFF);
    frame_buf[frame_idx++] = (uint8_t)((alarm_lo >> 8) & 0xFF);
    /* 寄存器 14（高 16 位） */
    uint16_t alarm_hi = (uint16_t)((alarm_status >> 16) & 0xFFFF);
    frame_buf[frame_idx++] = (uint8_t)(alarm_hi & 0xFF);
    frame_buf[frame_idx++] = (uint8_t)((alarm_hi >> 8) & 0xFF);
    
    /* 6. 计算并添加 CRC */
    crc = Net_Modbus_Crc16(frame_buf, frame_idx);
    frame_buf[frame_idx++] = (uint8_t)(crc & 0xFF);
    frame_buf[frame_idx++] = (uint8_t)(crc >> 8);
    
    /* 7. 直接通过 Ph_Net_Transmit 发送 */
    if(Ph_Net_Transmit(frame_buf, frame_idx))
    {
        DEV_PRINTF("[主动上传] 报警状态上传成功，状态：0x%08lX\r\n", (unsigned long)alarm_status);
        return true;
    }
    else
    {
        DEV_PRINTF("[主动上传] 报警状态上传失败！\r\n");
        return false;
    }
}

/********************************************************************************************
* 函数名：Net_Active_Upload_DeviceStatus
* 描  述：从机主动上传设备状态（事件触发）
*         当设备状态寄存器（地址 15）发生变化时调用
* 输  入：@param: *dev -> 设备句柄
*         @param: device_status -> 当前设备状态（32 位）
* 输  出：@retval: true -> 上传成功；false -> 上传失败
* 调  用：外部调用（设备状态变化触发）
********************************************************************************************/
bool Net_Active_Upload_DeviceStatus(Net_Device_t *dev, uint32_t device_status)
{
    uint8_t frame_buf[16];
    uint16_t frame_idx = 0;
    uint16_t crc;
    
    if(!dev)
        return false;
    
    if(Ota_IsHeartbeatPaused())
        return false;
    
    /* 构建设备状态上传完整帧
     * 协议格式：[地址][功能码][字节数][寄存器起始地址][数据...][CRC]
     * 设备状态占用 2 个连续寄存器（地址 15-16）
     */
    
    /* 1. 设备地址 */
    frame_buf[frame_idx++] = dev->addr;
    
    /* 2. 功能码 */
    frame_buf[frame_idx++] = NET_FC_ACTIVE_UPLOAD;  // 0x23
    
    /* 3. 数据字节数（2 个寄存器 * 2 字节 = 4 字节） */
    frame_buf[frame_idx++] = 4;
    
    /* 4. 寄存器起始地址（设备状态寄存器地址 = 15，小端序） */
    frame_buf[frame_idx++] = (uint8_t)(NET_REG_STATUS_BIT & 0xFF);
    frame_buf[frame_idx++] = (uint8_t)((NET_REG_STATUS_BIT >> 8) & 0xFF);
    
    /* 5. 设备状态数据（小端序，2 个寄存器共 4 字节） */
    /* 寄存器 15（低 16 位） */
    uint16_t status_lo = (uint16_t)(device_status & 0xFFFF);
    frame_buf[frame_idx++] = (uint8_t)(status_lo & 0xFF);
    frame_buf[frame_idx++] = (uint8_t)((status_lo >> 8) & 0xFF);
    /* 寄存器 16（高 16 位） */
    uint16_t status_hi = (uint16_t)((device_status >> 16) & 0xFFFF);
    frame_buf[frame_idx++] = (uint8_t)(status_hi & 0xFF);
    frame_buf[frame_idx++] = (uint8_t)((status_hi >> 8) & 0xFF);
    
    /* 6. 计算并添加 CRC */
    crc = Net_Modbus_Crc16(frame_buf, frame_idx);
    frame_buf[frame_idx++] = (uint8_t)(crc & 0xFF);
    frame_buf[frame_idx++] = (uint8_t)(crc >> 8);
    
    /* 7. 直接通过 Ph_Net_Transmit 发送 */
    if(!Ph_Net_Transmit(frame_buf, frame_idx))
    {
        DEV_PRINTF("[主动上传] 设备状态上传失败！\r\n");
        return false;
    }
    // else
    // {
    //     DEV_PRINTF("[OTA] 设备状态上传成功，状态：0x%08lX\r\n", (unsigned long)device_status);
    //     return true;
    // }
    return true;
}

/********************************************************************************************
* 函数名：Net_Active_Upload_OtaStatus
* 描  述：从机主动上传 OTA 状态（事件触发）
*         当 OTA 状态寄存器（地址 204-207）发生变化时调用
* 输  入：@param: *dev -> 设备句柄
*         @param: state -> 当前 OTA 状态（32 位）
*         @param: written_bytes -> 已写入字节数（32 位）
* 输  出：@retval: true -> 上传成功；false -> 上传失败
* 调  用：外部调用（OTA 状态变化触发）
********************************************************************************************/
bool Net_Active_Upload_OtaStatus(Net_Device_t *dev, uint32_t state, uint32_t written_bytes)
{
    uint8_t frame_buf[20];
    uint16_t frame_idx = 0;
    uint16_t crc;
    
    if(!dev)
        return false;
    
    /* 构建 OTA 状态上传完整帧
     * 协议格式：[地址][功能码][字节数][寄存器起始地址][数据...][CRC]
     * OTA 状态占用 4 个连续寄存器（地址 204-207），共 8 字节
     * 数据格式：state（32 位，低地址）+ written_bytes（32 位，高地址）
     */
    
    /* 1. 设备地址 */
    frame_buf[frame_idx++] = dev->addr;
    
    /* 2. 功能码 */
    frame_buf[frame_idx++] = NET_FC_ACTIVE_UPLOAD;  // 0x23
    
    /* 3. 数据字节数（4 个寄存器 * 2 字节 = 8 字节） */
    frame_buf[frame_idx++] = 8;
    
    /* 4. 寄存器起始地址（OTA 状态寄存器地址 = 204，小端序） */
    frame_buf[frame_idx++] = (uint8_t)(NET_REG_OTA_STATE & 0xFF);
    frame_buf[frame_idx++] = (uint8_t)((NET_REG_OTA_STATE >> 8) & 0xFF);
    
    /* 5. OTA 状态数据（小端序，4 个寄存器共 8 字节） */
    /* 寄存器 204-205（低 32 位：state） */
    frame_buf[frame_idx++] = (uint8_t)(state & 0xFF);
    frame_buf[frame_idx++] = (uint8_t)((state >> 8) & 0xFF);
    frame_buf[frame_idx++] = (uint8_t)((state >> 16) & 0xFF);
    frame_buf[frame_idx++] = (uint8_t)((state >> 24) & 0xFF);
    
    /* 寄存器 206-207（高 32 位：written_bytes） */
    frame_buf[frame_idx++] = (uint8_t)(written_bytes & 0xFF);
    frame_buf[frame_idx++] = (uint8_t)((written_bytes >> 8) & 0xFF);
    frame_buf[frame_idx++] = (uint8_t)((written_bytes >> 16) & 0xFF);
    frame_buf[frame_idx++] = (uint8_t)((written_bytes >> 24) & 0xFF);
    
    /* 6. 计算并添加 CRC */
    crc = Net_Modbus_Crc16(frame_buf, frame_idx);
    frame_buf[frame_idx++] = (uint8_t)(crc & 0xFF);
    frame_buf[frame_idx++] = (uint8_t)(crc >> 8);
    
    /* 7. 直接通过 Ph_Net_Transmit 发送 */
    if(!Ph_Net_Transmit(frame_buf, frame_idx))
    {
        DEV_PRINTF("[OTA] 状态上传失败:state=%d, written=%d\r\n", state, written_bytes);
        return false;
    }
    else
    {
        // DEV_PRINTF("[主动上传] OTA 状态上传成功：state=%lu, written=%lu\r\n", 
        //             (unsigned long)state, (unsigned long)written_bytes);
        return true;
    }
}

/********************************************************************************************
* 函数名：Net_Active_Upload_Periodic
* 描  述：从机定期主动上传所有传感器数据（心跳包）
*         建议调用周期：1 秒 ~ 10 秒（根据应用需求）
* 输  入：@param: *dev -> 设备句柄
* 输  出：无
* 调  用：外部调用（周期任务）
********************************************************************************************/
void Net_Active_Upload_Periodic(Net_Device_t *dev)
{
    if(!dev)
        return;
    
    /* 使用功能码 0x23 定期上传传感器数据 */
    Net_Active_Upload_SensorData(dev, 0x23);
}

/********************************************************************************************
* 函数名：Net_Active_Upload_Thresholds
* 描  述：从机定期主动上传所有阈值参数（配置心跳包）
*         上传地址 50-72 的所有阈值参数（辐射、温度、气压、湿度、CO2、PM2.5）
* 输  入：@param: *dev -> 设备句柄
* 输  出：无
* 调  用：外部调用（周期任务，建议周期 5-10 秒）
********************************************************************************************/
void Net_Active_Upload_Thresholds(Net_Device_t *dev)
{
    uint8_t frame_buf[128];
    uint16_t frame_idx = 0;
    uint16_t crc;
    
    if(!dev || !dev->reg_tb || dev->reg_sz < 2)
        return;
    
    if(Ota_IsHeartbeatPaused())
        return;
    
    /* 数据发送链路未连接时不上传 */
    if((dev == net_w5500_dh) && !Net_Tcp_DataSendReady())
        return;
    
    /* 构建阈值参数上传完整帧
     * 协议格式：[地址][功能码][字节数][寄存器起始地址][数据...][CRC]
     * 阈值参数从地址 50 开始，到地址 72 结束
     * 所有阈值参数都是 32 位（占用 2 个寄存器 = 4 字节）
     * 总共：12 个 32 位参数 = 12*4 = 48 字节
     */
    
    /* 1. 设备地址 */
    frame_buf[frame_idx++] = dev->addr;
    
    /* 2. 功能码 */
    frame_buf[frame_idx++] = 0x23;  // 使用 0x23 功能码
    
    /* 3. 数据字节数（48 字节） */
    uint8_t byte_count = 48;
    frame_buf[frame_idx++] = byte_count;
    
    /* 4. 寄存器起始地址（从地址 50 开始，小端序） */
    frame_buf[frame_idx++] = (uint8_t)(NET_REG_ALERT_THRESHOLD1 & 0xFF);
    frame_buf[frame_idx++] = (uint8_t)((NET_REG_ALERT_THRESHOLD1 >> 8) & 0xFF);
    
    /* 5. 读取寄存器数据并填入（小端序）
     * 地址 50-72 的阈值参数（全部是 32 位）
     */
    uint16_t threshold_addrs[] = {
        NET_REG_ALERT_THRESHOLD1,   // 50 - 辐射上阈值 (32 位)
        NET_REG_ALERT_THRESHOLD2,   // 52 - 辐射下阈值 (32 位)
        NET_REG_ALERT_THRESHOLD3,   // 54 - 温度上阈值 (32 位)
        NET_REG_ALERT_THRESHOLD4,   // 56 - 温度下阈值 (32 位)
        NET_REG_ALERT_THRESHOLD5,   // 58 - 气压上阈值 (32 位)
        NET_REG_ALERT_THRESHOLD6,   // 60 - 气压下阈值 (32 位)
        NET_REG_ALERT_THRESHOLD7,   // 62 - 湿度上阈值 (32 位)
        NET_REG_ALERT_THRESHOLD8,   // 64 - 湿度下阈值 (32 位)
        NET_REG_ALERT_THRESHOLD9,   // 66 - CO2 上阈值 (32 位)
        NET_REG_ALERT_THRESHOLD10,  // 68 - CO2 下阈值 (32 位)
        NET_REG_ALERT_THRESHOLD11,  // 70 - PM2.5 上阈值 (32 位)
        NET_REG_ALERT_THRESHOLD12   // 72 - PM2.5 下阈值 (32 位)
    };
    
    for(uint8_t i = 0; i < 12; i++)
    {
        /* 所有阈值参数都是 32 位 */
        uint32_t value = Net_Reg_Holding_Read_U32(dev, threshold_addrs[i]);
        frame_buf[frame_idx++] = (uint8_t)(value & 0xFF);
        frame_buf[frame_idx++] = (uint8_t)((value >> 8) & 0xFF);
        frame_buf[frame_idx++] = (uint8_t)((value >> 16) & 0xFF);
        frame_buf[frame_idx++] = (uint8_t)((value >> 24) & 0xFF);
    }
    
    /* 6. 计算并添加 CRC */
    crc = Net_Modbus_Crc16(frame_buf, frame_idx);
    frame_buf[frame_idx++] = (uint8_t)(crc & 0xFF);
    frame_buf[frame_idx++] = (uint8_t)(crc >> 8);
    
    /* 打印发送的帧数据用于调试 */
    // printf("[阈值上传] 发送帧：");
    // for(uint16_t i = 0; i < frame_idx; i++)
    // {
    //     printf("%02X ", frame_buf[i]);
    // }
    // printf("\r\n");
    
    /* 7. 直接发送 */
    if(!dev->transmit(frame_buf, frame_idx))
        DEV_PRINTF("[阈值参数] 上传失败！\r\n");
}

/********************************************************************************************
* 函数名：net_active_upload_transmit_block
* 描  述：主动上传指定起始寄存器块（0x23，reg_qty 为偶数，按 32 位成对读取）
********************************************************************************************/
static bool net_active_upload_transmit_block(Net_Device_t *dev, uint16_t start_reg, uint16_t reg_qty)
{
    uint8_t frame_buf[32];
    uint16_t frame_idx = 0;
    uint16_t crc;
    uint8_t byte_count;
    uint16_t i;

    if(!dev || !dev->reg_tb || reg_qty == 0U || (reg_qty & 1U))
        return false;
    if(Ota_IsHeartbeatPaused())
        return false;
    if((dev == net_w5500_dh) && !Net_Tcp_DataSendReady())
        return false;

    byte_count = (uint8_t)(reg_qty * 2U);
    frame_buf[frame_idx++] = dev->addr;
    frame_buf[frame_idx++] = NET_FC_ACTIVE_UPLOAD;
    frame_buf[frame_idx++] = byte_count;
    frame_buf[frame_idx++] = (uint8_t)(start_reg & 0xFF);
    frame_buf[frame_idx++] = (uint8_t)((start_reg >> 8) & 0xFF);

    for(i = 0U; i < reg_qty; i += 2U)
    {
        uint32_t value = Net_Reg_Holding_Read_U32(dev, (uint16_t)(start_reg + i));
        frame_buf[frame_idx++] = (uint8_t)(value & 0xFF);
        frame_buf[frame_idx++] = (uint8_t)((value >> 8) & 0xFF);
        frame_buf[frame_idx++] = (uint8_t)((value >> 16) & 0xFF);
        frame_buf[frame_idx++] = (uint8_t)((value >> 24) & 0xFF);
    }

    crc = Net_Modbus_Crc16(frame_buf, frame_idx);
    frame_buf[frame_idx++] = (uint8_t)(crc & 0xFF);
    frame_buf[frame_idx++] = (uint8_t)(crc >> 8);
    return dev->transmit(frame_buf, frame_idx);
}

/********************************************************************************************
* 函数名：Net_Active_Upload_DeviceParams
* 描  述：定时主动上传 reg82 报警使能、reg121 设备地址、reg122 报警音量、reg123 声/光/屏控制
********************************************************************************************/
void Net_Active_Upload_DeviceParams(Net_Device_t *dev)
{
    if(!dev)
        return;

    if(!net_active_upload_transmit_block(dev, NET_REG_ALARM_BITEN, 2U))
        DEV_PRINTF("[设备参数] 报警使能上传失败！\r\n");
    if(!net_active_upload_transmit_block(dev, NET_REG_ADDRESS, 2U))
        DEV_PRINTF("[设备参数] 设备地址、报警音量上传失败！\r\n");
    if(!net_active_upload_transmit_block(dev, NET_REG_CONTROL_BIT2, 2U))
        DEV_PRINTF("[设备参数] 声报警/光报警/屏幕控制上传失败！\r\n");
}

/********************************************************************************************
* 函数名：Net_Active_Upload_Scheduled
* 描  述：周期主动上传统一入口（按 tx_inft 当前链路）
*         顺序：传感器 reg1-16 → 阈值 reg50-73 → 设备参数（报警使能/地址/音量/控制）→ 序列号 reg86-93(CAN/LoRa)
* 输  入：无
* 输  出：无
* 调  用：外部调用（freertos 周期任务，NET_ACTIVE_UPLOAD_PERIOD_MS）
********************************************************************************************/
void Net_Active_Upload_Scheduled(void)
{
    Net_Device_t *dev = NULL;

    if(tx_inft.crt == INFT_TCP)
        dev = net_w5500_dh;
    else if(tx_inft.crt == INFT_CAN)
        dev = net_can_dh;
    else if(tx_inft.crt == INFT_LORA)
        dev = net_lora_dh;

    if(!dev)
        return;

    Net_Active_Upload_Periodic(dev);
    vTaskDelay(100);
    Net_Active_Upload_Thresholds(dev);
    vTaskDelay(100);
    Net_Active_Upload_DeviceParams(dev);

    // if(net_can_dh)
    //     Net_Active_Upload_SerialNum(net_can_dh);
    // if(net_lora_dh)
    //     Net_Active_Upload_SerialNum(net_lora_dh);
}

/********************************************************************************************
* 函数名：Net_Active_Upload_SerialNum
* 描  述：从机主动上传寄存器表序列号（功能码 0x23，地址 86，8 寄存器 = 16 字节 ASCII）
* 输  入：@param: *dev -> 设备句柄（如 net_can_dh）
* 输  出：无
* 调  用：外部调用（周期任务）
********************************************************************************************/
void Net_Active_Upload_SerialNum(Net_Device_t *dev)
{
    uint8_t frame_buf[32];
    uint16_t frame_idx = 0;
    uint16_t crc;

    uint8_t sn_buf[17];

    if(!dev || !dev->reg_tb || dev->reg_sz < 2)
        return;

    if((dev == net_w5500_dh) && !Net_Tcp_DataSendReady())
        return;

    memset(sn_buf, 0, sizeof(sn_buf));
    for(uint8_t i = 0; i < 8; i++)
    {
        uint16_t word = Net_Reg_Holding_Read_U16(dev, (uint16_t)(NET_REG_SERIALNUM + i));
        sn_buf[i * 2U] = (uint8_t)(word & 0xFFU);
        if(i * 2U + 1U < 16U)
            sn_buf[i * 2U + 1U] = (uint8_t)(word >> 8);
    }

    frame_buf[frame_idx++] = dev->addr;
    frame_buf[frame_idx++] = 0x23;
    frame_buf[frame_idx++] = 16;
    frame_buf[frame_idx++] = (uint8_t)(NET_REG_SERIALNUM & 0xFF);
    frame_buf[frame_idx++] = (uint8_t)((NET_REG_SERIALNUM >> 8) & 0xFF);

    for(uint8_t i = 0; i < 8; i++)
    {
        uint16_t word = Net_Reg_Holding_Read_U16(dev, (uint16_t)(NET_REG_SERIALNUM + i));
        frame_buf[frame_idx++] = (uint8_t)(word & 0xFF);
        frame_buf[frame_idx++] = (uint8_t)((word >> 8) & 0xFF);
    }

    crc = Net_Modbus_Crc16(frame_buf, frame_idx);
    frame_buf[frame_idx++] = (uint8_t)(crc & 0xFF);
    frame_buf[frame_idx++] = (uint8_t)(crc >> 8);

    if(!dev->transmit(frame_buf, frame_idx))
        DEV_PRINTF("%s -> 序列号上传失败：%.16s\r\n", dev->name, sn_buf);
}

/********************************************************************************************
* 函数名：Net_Config_Sync_To_Registers
* 描  述：将设备配置（阈值、开关等）同步到协议寄存器
*         调用此函数前需确保 sys_cfg 已更新（通过串口指令或其他方式）
* 输  入：无
* 输  出：无
* 调  用：外部调用（配置更新后）
********************************************************************************************/
void Net_Config_Sync_To_Registers(Config_Index_t idx)
{
    uint32_t dose_u32;
    uint32_t temp_u32;
    uint32_t press_u32;
    uint32_t hum_u32;
    uint32_t co2_u32;
    uint32_t pm25_u32;
    uint32_t alarm_biten;
    
    /* 报警状态同步优化：记录上次同步的报警状态，避免重复刷新寄存器 */
    static uint32_t last_sync_alarm_status = 0;

    
    if(!net_w5500_dh)
    {
        DEV_PRINTF("[寄存器同步] net_w5500_dh 未注册，跳过同步\r\n");
        return;
    }
    
    switch(idx)
    {
        case CFG_IDX_RATE:
            /* 同步剂量率阈值到寄存器时，保持 reg82 的禁止状态 */
            /* 只有 reg82 允许报警（无 shadow 标志）且阈值>0 时，才清除 shadow 标志 */
            if((sys_cfg.dose_th_shadow_flags & NET_DOSE_SHADOW_HI_VALID) == 0U &&
               sys_cfg.th_rh_rate > 0.0f)
            {
                /* 正常状态，无需操作 */
            }
            else if(sys_cfg.th_rh_rate > 0.0f)
            {
                /* reg82 允许报警且阈值>0，清除 shadow 标志 */
                sys_cfg.dose_th_shadow_flags &= (uint8_t)~NET_DOSE_SHADOW_HI_VALID;
            }
            
            if((sys_cfg.dose_th_shadow_flags & NET_DOSE_SHADOW_LO_VALID) == 0U &&
               sys_cfg.th_rl_rate > 0.0f)
            {
                /* 正常状态，无需操作 */
            }
            else if(sys_cfg.th_rl_rate > 0.0f)
            {
                /* reg82 允许报警且阈值>0，清除 shadow 标志 */
                sys_cfg.dose_th_shadow_flags &= (uint8_t)~NET_DOSE_SHADOW_LO_VALID;
            }
            
            /* 1. 辐射剂量率阈值（uSv/h -> uSv/h*100，2 寄存器 = 4 字节） */
            dose_u32 = (uint32_t)(sys_cfg.th_rh_rate * 100.0f);
            Net_Reg_Holding_Write_U32(net_w5500_dh, NET_REG_ALERT_THRESHOLD1, dose_u32);
            
            dose_u32 = (uint32_t)(sys_cfg.th_rl_rate * 100.0f);
            Net_Reg_Holding_Write_U32(net_w5500_dh, NET_REG_ALERT_THRESHOLD2, dose_u32);
            DEV_PRINTF("[寄存器同步] 辐射剂量率阈值：HI=%.2f uSv/h, LO=%.2f uSv/h, shadow_flags=0x%02X\r\n", 
                    (double)sys_cfg.th_rh_rate, (double)sys_cfg.th_rl_rate,
                    (unsigned)sys_cfg.dose_th_shadow_flags);
            break;
        
        case CFG_IDX_ENV:
            /* 2. 温度阈值（℃ -> ℃*10，2 寄存器 = 4 字节） */
            temp_u32 = (uint32_t)(sys_cfg.temp_th_hi * 10.0f);
            Net_Reg_Holding_Write_U32(net_w5500_dh, NET_REG_ALERT_THRESHOLD3, temp_u32);
            
            temp_u32 = (uint32_t)(sys_cfg.temp_th_lo * 10.0f);
            Net_Reg_Holding_Write_U32(net_w5500_dh, NET_REG_ALERT_THRESHOLD4, temp_u32);
            DEV_PRINTF("[寄存器同步] 温度阈值：HI=%.2f C, LO=%.2f C\r\n", 
                    (double)sys_cfg.temp_th_hi, (double)sys_cfg.temp_th_lo);
            
            /* 3. 气压阈值（hPa -> Pa，2 寄存器 = 4 字节） */
            press_u32 = (uint32_t)(sys_cfg.press_th_hi * 100.0f);  // press_th_hi 单位是 hPa，转换为 Pa 上传
            Net_Reg_Holding_Write_U32(net_w5500_dh, NET_REG_ALERT_THRESHOLD5, press_u32);
            
            press_u32 = (uint32_t)(sys_cfg.press_th_lo * 100.0f);  // press_th_lo 单位是 hPa，转换为 Pa 上传
            Net_Reg_Holding_Write_U32(net_w5500_dh, NET_REG_ALERT_THRESHOLD6, press_u32);
            DEV_PRINTF("[寄存器同步] 气压阈值：HI=%.1f hPa (%u Pa), LO=%.1f hPa (%u Pa)\r\n", 
                    (double)sys_cfg.press_th_hi, (uint32_t)(sys_cfg.press_th_hi * 100.0f),
                    (double)sys_cfg.press_th_lo, (uint32_t)(sys_cfg.press_th_lo * 100.0f));
            
            /* 4. 湿度阈值（%RH -> %*1，2 寄存器 = 4 字节） */
            hum_u32 = (uint32_t)(sys_cfg.hum_th_hi * 10.0f);
            Net_Reg_Holding_Write_U32(net_w5500_dh, NET_REG_ALERT_THRESHOLD7, hum_u32);
            
            hum_u32 = (uint32_t)(sys_cfg.hum_th_lo * 10.0f);
            Net_Reg_Holding_Write_U32(net_w5500_dh, NET_REG_ALERT_THRESHOLD8, hum_u32);
            DEV_PRINTF("[寄存器同步] 湿度阈值：HI=%.1f %%, LO=%.1f %%\r\n", 
                    (double)sys_cfg.hum_th_hi, (double)sys_cfg.hum_th_lo);
            
            /* 5. CO2 阈值（ppm -> ppm*100，2 寄存器 = 4 字节） */
            co2_u32 = (uint32_t)(sys_cfg.co2_th_hi * 100.0f);
            Net_Reg_Holding_Write_U32(net_w5500_dh, NET_REG_ALERT_THRESHOLD9, co2_u32);
            
            co2_u32 = (uint32_t)(sys_cfg.co2_th_lo * 100.0f);
            Net_Reg_Holding_Write_U32(net_w5500_dh, NET_REG_ALERT_THRESHOLD10, co2_u32);
            DEV_PRINTF("[寄存器同步] CO2 阈值：HI=%u ppm, LO=%u ppm\r\n", 
                    (unsigned)sys_cfg.co2_th_hi, (unsigned)sys_cfg.co2_th_lo);
            
            /* 6. PM2.5 阈值（ug/m³ -> ug/m³*100，2 寄存器 = 4 字节） */
            pm25_u32 = (uint32_t)(sys_cfg.pm25_th_hi * 100.0f);
            Net_Reg_Holding_Write_U32(net_w5500_dh, NET_REG_ALERT_THRESHOLD11, pm25_u32);
            
            pm25_u32 = (uint32_t)(sys_cfg.pm25_th_lo * 100.0f);
            Net_Reg_Holding_Write_U32(net_w5500_dh, NET_REG_ALERT_THRESHOLD12, pm25_u32);
            DEV_PRINTF("[寄存器同步] PM2.5 阈值：HI=%u, LO=%u\r\n", 
                    (unsigned)sys_cfg.pm25_th_hi, (unsigned)sys_cfg.pm25_th_lo);
            break;
        
        case CFG_IDX_ALARM_STATE:
            /* 12. 报警状态寄存器（32 位标志，2 寄存器 = 4 字节） */
            /* 只有报警状态真正变化时才写入寄存器，避免一直刷新 */
            if(sys_cfg.alarm_status != last_sync_alarm_status)
            {
                Net_Reg_Holding_Write_U32(net_w5500_dh, NET_REG_ALARM_BIT1, sys_cfg.alarm_status);
                last_sync_alarm_status = sys_cfg.alarm_status;
                DEV_PRINTF("[寄存器同步] 报警状态：0x%08lX\r\n", (unsigned long)sys_cfg.alarm_status);
            }
            else
            {
                DEV_PRINTF("[寄存器同步] 报警状态无变化，跳过同步：0x%08lX\r\n", (unsigned long)sys_cfg.alarm_status);
            }
            break;
        
        case CFG_IDX_ALARM_EN:
            /* bit=1 表示禁止；阈值=0 或 alarm_sound=0 视为该路报警已禁止 */
            alarm_biten = 0U;
            if(sys_cfg.th_rh_rate <= 0.0f)
                alarm_biten |= (1U << NET_REG_ALARM_BITEN_DOSE_HI_BIT);
            if(sys_cfg.th_rl_rate <= 0.0f)
                alarm_biten |= (1U << NET_REG_ALARM_BITEN_DOSE_LO_BIT);
            /* 声报警：alarm_sound=0 或音量=0 时禁止 */
            if(sys_cfg.alarm_sound == 0U || sys_cfg.alarm_volume == 0U)
                alarm_biten |= (1U << NET_REG_ALARM_BITEN_SOUND_BIT);
            
            Net_Reg_Holding_Write_U32(net_w5500_dh, NET_REG_ALARM_BITEN, alarm_biten);
            DEV_PRINTF("[寄存器同步] 报警禁止掩码：0x%08lX (辐射 HI=%s LO=%s, 声=%s)\r\n",
                    (unsigned long)alarm_biten,
                    (sys_cfg.th_rh_rate <= 0.0f ? "禁" : "允"),
                    (sys_cfg.th_rl_rate <= 0.0f ? "禁" : "允"),
                    ((sys_cfg.alarm_sound == 0U || sys_cfg.alarm_volume == 0U) ? "禁" : "允"));
            
            /* 注意：不在这里调用 net_alarm_biten_apply()，避免循环触发 */
            /* net_alarm_biten_apply() 只在 net_alarm_biten_on_reg_written() 中调用 */
            break;
        
        case CFG_IDX_DEVICE_ADDR:
            /* 8. 设备地址寄存器（1 寄存器 = 2 字节） */
            Net_Reg_Holding_Write_U16(net_w5500_dh, NET_REG_ADDRESS, (uint16_t)sys_cfg.dev_addr);
            DEV_PRINTF("[寄存器同步] 设备地址：%d\r\n", (unsigned)sys_cfg.dev_addr);
            
            /* 9. 报警音量寄存器（1 寄存器 = 2 字节，0-100 百分比） */
            Net_Reg_Holding_Write_U16(net_w5500_dh, NET_REG_ALARM_VOLUME, (uint16_t)sys_cfg.alarm_volume);
            DEV_PRINTF("[寄存器同步] 报警音量：%d%%\r\n", (unsigned)sys_cfg.alarm_volume);
            
            /* 10. 设备状态寄存器（地址 15，2 寄存器 = 4 字节）
             * bit0-11: 硬件状态（目前未实现，保持 0）
             * bit12: 是否启用声音报警
             * bit13: 是否启用光报警
             * bit14: 是否启用屏幕
             */
            g_device_status = 0;  // 清零
            if(sys_cfg.alarm_sound) g_device_status |= (1U << 12);
            if(sys_cfg.alarm_light) g_device_status |= (1U << 13);
            if(sys_cfg.display_enable) g_device_status |= (1U << 14);
            
            Net_Reg_Holding_Write_U32(net_w5500_dh, NET_REG_STATUS_BIT, g_device_status);
            DEV_PRINTF("[寄存器同步] 设备状态：0x%08lX (声=%d, 光=%d, 屏=%d)\r\n", 
                    (unsigned long)g_device_status, 
                    (int)sys_cfg.alarm_sound, 
                    (int)sys_cfg.alarm_light, 
                    (int)sys_cfg.display_enable);

            /* 11. reg123 声/光/屏控制位（与 sys_cfg / Flash 一致） */
            {
                uint16_t ctrl = 0U;
                if(sys_cfg.alarm_sound)
                    ctrl |= (1U << NET_REG_CTRL2_SOUND_BIT);
                if(sys_cfg.alarm_light)
                    ctrl |= (1U << NET_REG_CTRL2_LIGHT_BIT);
                if(sys_cfg.display_enable)
                    ctrl |= (1U << NET_REG_CTRL2_DISPLAY_BIT);
                Net_Reg_Holding_Write_U16(net_w5500_dh, NET_REG_CONTROL_BIT2, ctrl);
                DEV_PRINTF("[寄存器同步] 声/光/屏控制：0x%04X\r\n", (unsigned)ctrl);
            }
            break;
        
        case CFG_IDX_DEVICE_INFO:
        {
            /* 10. 序列号寄存器（8 寄存器 = 16 字节 ASCII 字符串）
             * 寄存器地址：86-93（共 16 字节）
             */
            uint8_t sn_buf[16];
            memset(sn_buf, 0, sizeof(sn_buf));
            /* 复制 sys_cfg.SN 到 sn_buf，最多 16 字节 */
            size_t sn_len = strlen((void *)sys_cfg.SN);
            if(sn_len > 16U) sn_len = 16U;
            memcpy(sn_buf, (void *)sys_cfg.SN, sn_len);
            /* 逐字节写入 8 个寄存器（每个寄存器 2 字节） */
            for(int i = 0; i < 8; i++)
            {
                uint16_t sn_word = (uint16_t)sn_buf[i * 2];
                if(i * 2 + 1 < 16)
                    sn_word |= ((uint16_t)sn_buf[i * 2 + 1] << 8);
                Net_Reg_Holding_Write_U16(net_w5500_dh, (uint16_t)(NET_REG_SERIALNUM + i), sn_word);
            }
            DEV_PRINTF("[寄存器同步] 序列号：%.16s\r\n", sn_buf);
        }
        {
            /* 11. 软件版本寄存器（10 寄存器 = 20 字节 ASCII 字符串）
             * 寄存器地址：98-107（共 20 字节）
             * 软件版本通过宏定义获取，不保存到 Flash
             */
            uint8_t sw_buf[20];
            memset(sw_buf, 0, sizeof(sw_buf));
            /* 从宏定义获取软件版本 */
            const char *sw_ver = DEVICE_SOFTWARE_VERSION;
            size_t sw_len = strlen(sw_ver);
            if(sw_len > 20U) sw_len = 20U;
            memcpy(sw_buf, sw_ver, sw_len);
            /* 逐字节写入 10 个寄存器（每个寄存器 2 字节） */
            for(int i = 0; i < 10; i++)
            {
                uint16_t sw_word = (uint16_t)sw_buf[i * 2];
                if(i * 2 + 1 < 20)
                    sw_word |= ((uint16_t)sw_buf[i * 2 + 1] << 8);
                Net_Reg_Holding_Write_U16(net_w5500_dh, (uint16_t)(NET_REG_SW_VERSION + i), sw_word);
            }
            DEV_PRINTF("[寄存器同步] 软件版本：%.20s\r\n", sw_buf);
        }
            break;
        
        default:
            DEV_PRINTF("[寄存器同步] 未知的配置索引：%d\r\n", (int)idx);
            break;
    }
    /* 注意：硬件版本、灵敏度不在协议寄存器表中，仅保存在 sys_cfg 中 */
}

/********************************************************************************************
* 函数名：Net_Config_Sync_All
* 描  述：遍历所有配置索引，依次同步到协议寄存器
*         用于初始化或恢复出厂设置后批量更新所有配置
* 输  入：无
* 输  出：无
* 调  用：外部调用（需要全量更新时）
********************************************************************************************/
void Net_Config_Sync_All(void)
{
    Config_Index_t i;
    
    DEV_PRINTF("[寄存器同步] ========== 开始全量同步所有配置 ==========\r\n");
    
    for(i = CFG_IDX_RATE; i < CFG_IDX_MAX; i++)
    {
        Net_Config_Sync_To_Registers(i);
    }
    
    DEV_PRINTF("[寄存器同步] ========== 全量同步完成！ ==========\r\n");
}

static bool net_cfg_try_save_flash(void)
{
    if(!DeviceConfig_IsReady())
    {
        DEV_PRINTF("[NET] 配置已更新 RAM，配置未就绪，跳过 Flash\r\n");
        return false;
    }
    if(USB_MSC_IsStarted())
    {
        DEV_PRINTF("[NET] 配置已更新 RAM，USB 占用 Flash，跳过保存\r\n");
        return false;
    }
    if(DeviceConfig_WriteFromSysCfg() != 0)
    {
        DEV_PRINTF("[NET] 配置写入 Flash 失败\r\n");
        return false;
    }
    DEV_PRINTF("[NET] 配置已保存到 Flash\r\n");
    return true;
}

/********************************************************************************************
* reg82 报警禁止掩码接收（32 位写满后处理）
* bit=1：禁止对应剂量报警；禁止时暂存非 0 阈值至 sys_cfg/Flash 并置 0，允许时恢复
* 若阈值已为 0 或用户已通过 reg50/52 自行打开，则不再额外处理
********************************************************************************************/
static bool net_alarm_biten_write_touched(uint16_t reg, uint16_t qreg)
{
    uint32_t end_reg;

    if(qreg == 0U)
        return false;
    end_reg = (uint32_t)reg + (uint32_t)qreg;
    return (reg <= (NET_REG_ALARM_BITEN + 1U)) && (end_reg > NET_REG_ALARM_BITEN);
}

static void net_alarm_biten_mark_written(uint16_t reg, uint16_t qreg)
{
    uint32_t end_reg;
    uint16_t i;

    if(qreg == 0U)
        return;

    end_reg = (uint32_t)reg + (uint32_t)qreg;
    for(i = reg; (uint32_t)i < end_reg; i++)
    {
        if(i == NET_REG_ALARM_BITEN)
            g_reg_write_mask.alarm_biten |= 1U;
        else if(i == (NET_REG_ALARM_BITEN + 1U))
            g_reg_write_mask.alarm_biten |= 2U;
    }
}

static bool net_alarm_biten_apply_dose_hi(uint32_t biten)
{
    bool prohibit = ((biten >> NET_REG_ALARM_BITEN_DOSE_HI_BIT) & 1U) != 0U;

    if(prohibit)
    {
        if(sys_cfg.th_rh_rate > 0.0f)
        {
            sys_cfg.th_rh_rate_saved = sys_cfg.th_rh_rate;
            sys_cfg.dose_th_shadow_flags |= NET_DOSE_SHADOW_HI_VALID;
            sys_cfg.th_rh_rate = 0.0f;
            return true;
        }
        return false;
    }

    if(sys_cfg.th_rh_rate > 0.0f)
        return false;

    if(sys_cfg.dose_th_shadow_flags & NET_DOSE_SHADOW_HI_VALID)
    {
        sys_cfg.th_rh_rate = sys_cfg.th_rh_rate_saved;
        sys_cfg.dose_th_shadow_flags &= (uint8_t)~NET_DOSE_SHADOW_HI_VALID;
        return true;
    }
    return false;
}

static bool net_alarm_biten_apply_dose_lo(uint32_t biten)
{
    bool prohibit = ((biten >> NET_REG_ALARM_BITEN_DOSE_LO_BIT) & 1U) != 0U;

    if(prohibit)
    {
        if(sys_cfg.th_rl_rate > 0.0f)
        {
            sys_cfg.th_rl_rate_saved = sys_cfg.th_rl_rate;
            sys_cfg.dose_th_shadow_flags |= NET_DOSE_SHADOW_LO_VALID;
            sys_cfg.th_rl_rate = 0.0f;
            return true;
        }
        return false;
    }

    if(sys_cfg.th_rl_rate > 0.0f)
        return false;

    if(sys_cfg.dose_th_shadow_flags & NET_DOSE_SHADOW_LO_VALID)
    {
        sys_cfg.th_rl_rate = sys_cfg.th_rl_rate_saved;
        sys_cfg.dose_th_shadow_flags &= (uint8_t)~NET_DOSE_SHADOW_LO_VALID;
        return true;
    }
    return false;
}

static void net_alarm_biten_apply(Net_Device_t *dev, uint32_t biten)
{
    bool cfg_changed = false;
    bool need_clear_alarm = false;
    uint32_t old_alarm_status = sys_cfg.alarm_status;

    (void)dev;

    /* 1. 应用上阈值报警禁止 */
    if(net_alarm_biten_apply_dose_hi(biten))
        cfg_changed = true;
    
    /* 2. 应用下阈值报警禁止 */
    if(net_alarm_biten_apply_dose_lo(biten))
        cfg_changed = true;
    
    /* 3. 如果禁止报警，清除对应的报警状态（不能使能关了还在报警） */
    if((biten & (1U << NET_REG_ALARM_BITEN_DOSE_HI_BIT)) != 0U)
    {
        /* 上阈值报警被禁止，清除上阈值报警状态 */
        if((sys_cfg.alarm_status & (1U << RATE_HIGH_ALARM_BIT)) != 0U)
        {
            sys_cfg.alarm_status &= ~(1U << RATE_HIGH_ALARM_BIT);
            need_clear_alarm = true;
            DEV_PRINTF("[报警使能] 上阈值禁止，清除报警状态 HI=0\r\n");
        }
    }
    if((biten & (1U << NET_REG_ALARM_BITEN_DOSE_LO_BIT)) != 0U)
    {
        /* 下阈值报警被禁止，清除下阈值报警状态 */
        if((sys_cfg.alarm_status & (1U << RATE_LOW_ALARM_BIT)) != 0U)
        {
            sys_cfg.alarm_status &= ~(1U << RATE_LOW_ALARM_BIT);
            need_clear_alarm = true;
            DEV_PRINTF("[报警使能] 下阈值禁止，清除报警状态 LO=0\r\n");
        }
    }
    
    /* 4. 同步配置和报警状态 */
    if(cfg_changed)
    {
        Net_Config_Sync_To_Registers(CFG_IDX_RATE);
        (void)net_cfg_try_save_flash();
    }
    
    /* 5. 同步报警使能寄存器 */
    Net_Config_Sync_To_Registers(CFG_IDX_ALARM_EN);
    
    /* 6. 如果清除了报警状态，同步到寄存器表 */
    if(need_clear_alarm)
    {
        Net_Config_Sync_To_Registers(CFG_IDX_ALARM_STATE);
        DEV_PRINTF("[报警使能] 报警状态从 0x%08lX -> 0x%08lX\r\n", 
                   (unsigned long)old_alarm_status, (unsigned long)sys_cfg.alarm_status);
    }
}

static void net_alarm_biten_on_reg_written(Net_Device_t *dev, uint16_t reg, uint16_t qreg)
{
    uint32_t biten;

    if(!dev || !net_alarm_biten_write_touched(reg, qreg))
        return;

    net_alarm_biten_mark_written(reg, qreg);
    if(g_reg_write_mask.alarm_biten != NET_REG_ALARM_BITEN_WRITTEN_ALL)
        return;

    g_reg_write_mask.alarm_biten = 0U;
    biten = Net_Reg_Holding_Read_U32(dev, NET_REG_ALARM_BITEN);
    DEV_PRINTF("[NET] 报警禁止掩码=0x%08lX\r\n", (unsigned long)biten);
    net_alarm_biten_apply(dev, biten);
}

/********************************************************************************************
* 阈值寄存器 50～72 接收：32 位参数需 2 寄存器写满后写 sys_cfg 并落 Flash
* 缩放与 Net_Config_Sync_To_Registers / Net_Active_Upload_Thresholds 一致
********************************************************************************************/
static bool net_thr_write_touched(uint16_t reg, uint16_t qreg)
{
    uint32_t end_reg;

    if(qreg == 0U)
        return false;
    end_reg = (uint32_t)reg + (uint32_t)qreg;
    return (reg <= (NET_REG_ALERT_THRESHOLD12 + 1U)) && (end_reg > NET_REG_ALERT_THRESHOLD1);
}

static void net_thr_mark_written(uint16_t reg, uint16_t qreg)
{
    uint32_t end_reg;
    uint16_t i;

    if(qreg == 0U)
        return;

    end_reg = (uint32_t)reg + (uint32_t)qreg;
    for(i = reg; (uint32_t)i < end_reg; i++)
    {
        uint16_t off;

        if(i < NET_REG_ALERT_THRESHOLD1 || i > (NET_REG_ALERT_THRESHOLD12 + 1U))
            continue;
        off = (uint16_t)(i - NET_REG_ALERT_THRESHOLD1);
        g_reg_write_mask.thr_regs |= (1UL << off);
    }
}

static bool net_thr_pair_ready(uint8_t pair)
{
    uint32_t bits = 3UL << (pair * 2U);

    if(pair >= NET_REG_THR_PAIR_CNT)
        return false;
    return ((g_reg_write_mask.thr_regs & bits) == bits);
}

static void net_thr_clear_pair(uint8_t pair)
{
    g_reg_write_mask.thr_regs &= ~(3UL << (pair * 2U));
}

static bool net_thr_apply_pair(Net_Device_t *dev, uint16_t reg_base)
{
    uint32_t raw;

    if(!dev)
        return false;

    raw = Net_Reg_Holding_Read_U32(dev, reg_base);

    switch(reg_base)
    {
        case NET_REG_ALERT_THRESHOLD1:
            sys_cfg.th_rh_rate = (float)raw / 100.0f;
            DEV_PRINTF("[NET] 剂量率上阈值=%.2f uSv/h\r\n", (double)sys_cfg.th_rh_rate);
            return true;
        case NET_REG_ALERT_THRESHOLD2:
            sys_cfg.th_rl_rate = (float)raw / 100.0f;
            DEV_PRINTF("[NET] 剂量率下阈值=%.2f uSv/h\r\n", (double)sys_cfg.th_rl_rate);
            return true;
        case NET_REG_ALERT_THRESHOLD3:
            sys_cfg.temp_th_hi = (float)raw / 10.0f;
            DEV_PRINTF("[NET] 温度上阈值=%.1f C\r\n", (double)sys_cfg.temp_th_hi);
            return true;
        case NET_REG_ALERT_THRESHOLD4:
            sys_cfg.temp_th_lo = (float)raw / 10.0f;
            DEV_PRINTF("[NET] 温度下阈值=%.1f C\r\n", (double)sys_cfg.temp_th_lo);
            return true;
        case NET_REG_ALERT_THRESHOLD5:
            sys_cfg.press_th_hi = (float)raw / 100.0f;  // 寄存器是 Pa，转换为 hPa 存储
            DEV_PRINTF("[NET] 气压上阈值=%.1f hPa (%u Pa)\r\n", (double)sys_cfg.press_th_hi, (uint32_t)raw);
            return true;
        case NET_REG_ALERT_THRESHOLD6:
            sys_cfg.press_th_lo = (float)raw / 100.0f;  // 寄存器是 Pa，转换为 hPa 存储
            DEV_PRINTF("[NET] 气压下阈值=%.1f hPa (%u Pa)\r\n", (double)sys_cfg.press_th_lo, (uint32_t)raw);
            return true;
        case NET_REG_ALERT_THRESHOLD7:
            sys_cfg.hum_th_hi = (float)raw / 10.0f;
            DEV_PRINTF("[NET] 湿度上阈值=%.1f %%\r\n", (double)sys_cfg.hum_th_hi);
            return true;
        case NET_REG_ALERT_THRESHOLD8:
            sys_cfg.hum_th_lo = (float)raw / 10.0f;
            DEV_PRINTF("[NET] 湿度下阈值=%.1f %%\r\n", (double)sys_cfg.hum_th_lo);
            return true;
        case NET_REG_ALERT_THRESHOLD9:
            sys_cfg.co2_th_hi = raw / 100U;
            DEV_PRINTF("[NET] CO2 上阈值=%u ppm\r\n", (unsigned)sys_cfg.co2_th_hi);
            return true;
        case NET_REG_ALERT_THRESHOLD10:
            sys_cfg.co2_th_lo = raw / 100U;
            DEV_PRINTF("[NET] CO2 下阈值=%u ppm\r\n", (unsigned)sys_cfg.co2_th_lo);
            return true;
        case NET_REG_ALERT_THRESHOLD11:
            sys_cfg.pm25_th_hi = (uint16_t)((raw / 100U > 0xFFFFU) ? 0xFFFFU : (raw / 100U));
            DEV_PRINTF("[NET] PM2.5 上阈值=%u\r\n", (unsigned)sys_cfg.pm25_th_hi);
            return true;
        case NET_REG_ALERT_THRESHOLD12:
            sys_cfg.pm25_th_lo = (uint16_t)((raw / 100U > 0xFFFFU) ? 0xFFFFU : (raw / 100U));
            DEV_PRINTF("[NET] PM2.5 下阈值=%u\r\n", (unsigned)sys_cfg.pm25_th_lo);
            return true;
        default:
            return false;
    }
}

static void net_thr_on_reg_written(Net_Device_t *dev, uint16_t reg, uint16_t qreg)
{
    uint8_t pair;
    bool updated = false;
    bool rate_changed = false;

    if(!dev || !net_thr_write_touched(reg, qreg))
        return;

    net_thr_mark_written(reg, qreg);

    for(pair = 0U; pair < NET_REG_THR_PAIR_CNT; pair++)
    {
        uint16_t reg_base;

        if(!net_thr_pair_ready(pair))
            continue;

        net_thr_clear_pair(pair);
        reg_base = (uint16_t)(NET_REG_ALERT_THRESHOLD1 + (uint16_t)pair * 2U);
        if(net_thr_apply_pair(dev, reg_base))
        {
            updated = true;
            if(pair == 0U)
            {
                /* 上阈值写入：根据阈值是否为 0 设置/清除 shadow 标志 */
                if(sys_cfg.th_rh_rate <= 0.0f)
                {
                    /* 阈值=0，设置 shadow 标志（禁止报警） */
                    sys_cfg.dose_th_shadow_flags |= NET_DOSE_SHADOW_HI_VALID;
                    DEV_PRINTF("[阈值写入] 上阈值=0，设置 shadow 标志 HI=1\r\n");
                }
                else
                {
                    /* 阈值>0，清除 shadow 标志（允许报警） */
                    sys_cfg.dose_th_shadow_flags &= (uint8_t)~NET_DOSE_SHADOW_HI_VALID;
                    DEV_PRINTF("[阈值写入] 上阈值>0，清除 shadow 标志 HI=0\r\n");
                }
                rate_changed = true;
            }
            else if(pair == 1U)
            {
                /* 下阈值写入：根据阈值是否为 0 设置/清除 shadow 标志 */
                if(sys_cfg.th_rl_rate <= 0.0f)
                {
                    /* 阈值=0，设置 shadow 标志（禁止报警） */
                    sys_cfg.dose_th_shadow_flags |= NET_DOSE_SHADOW_LO_VALID;
                    DEV_PRINTF("[阈值写入] 下阈值=0，设置 shadow 标志 LO=1\r\n");
                }
                else
                {
                    /* 阈值>0，清除 shadow 标志（允许报警） */
                    sys_cfg.dose_th_shadow_flags &= (uint8_t)~NET_DOSE_SHADOW_LO_VALID;
                    DEV_PRINTF("[阈值写入] 下阈值>0，清除 shadow 标志 LO=0\r\n");
                }
                rate_changed = true;
            }
        }
    }

    if(!updated)
        return;

    (void)net_cfg_try_save_flash();

    if(rate_changed)
        Net_Config_Sync_To_Registers(CFG_IDX_ALARM_EN);
}

/********************************************************************************************
* 设备控制寄存器 120/122/123 接收
* 120：写 0x0001 重启（不落 Flash）
* 121：设备地址 0～255，落 Flash
* 122：报警音量 0～100，落 Flash
* 123：bit0 声 / bit1 光 / bit2 屏，落 Flash 并同步 reg15 状态位
********************************************************************************************/
static bool net_dev_ctrl_write_touched(uint16_t reg, uint16_t qreg)
{
    uint32_t end_reg;

    if(qreg == 0U)
        return false;
    end_reg = (uint32_t)reg + (uint32_t)qreg;
    return (reg <= NET_REG_CONTROL_BIT2) && (end_reg > NET_REG_REBOOT);
}

static void net_dev_ctrl_on_reg_written(Net_Device_t *dev, uint16_t reg, uint16_t qreg)
{
    uint32_t end_reg;
    uint16_t i;
    bool cfg_updated = false;
    bool need_dev_sync = false;
    bool addr_changed = false;

    if(!dev || !net_dev_ctrl_write_touched(reg, qreg))
        return;

    end_reg = (uint32_t)reg + (uint32_t)qreg;
    for(i = reg; (uint32_t)i < end_reg; i++)
    {
        if(i == NET_REG_REBOOT)
        {
            uint16_t reboot_val = Net_Reg_Holding_Read_U16(dev, NET_REG_REBOOT);

            if(reboot_val == NET_REG_REBOOT_CMD)
            {
                DEV_PRINTF("[NET] 设备重启，即将复位\r\n");
                vTaskDelay(pdMS_TO_TICKS(100));
                NVIC_SystemReset();
            }
        }
        else if(i == NET_REG_ADDRESS)
        {
            uint16_t addr_val = Net_Reg_Holding_Read_U16(dev, NET_REG_ADDRESS);

            if(addr_val > 255U)
                addr_val = 255U;
            sys_cfg.dev_addr = (uint8_t)addr_val;
            DEV_PRINTF("[NET] 设备地址=%u\r\n", (unsigned)sys_cfg.dev_addr);
            cfg_updated = true;
            need_dev_sync = true;
            addr_changed = true;
        }
        else if(i == NET_REG_ALARM_VOLUME)
        {
            uint16_t vol = Net_Reg_Holding_Read_U16(dev, NET_REG_ALARM_VOLUME);

            if(vol > 100U)
                vol = 100U;
            sys_cfg.alarm_volume = (uint8_t)vol;
            
            /* 需求 1&2：根据音量值自动设置声报警使能 */
            if(sys_cfg.alarm_volume > 0U)
            {
                /* 需求 1：音量>0，打开声报警使能 */
                sys_cfg.alarm_sound = 1U;
            }
            else
            {
                /* 需求 2：音量=0，关闭声报警使能 */
                sys_cfg.alarm_sound = 0U;
            }
            
            DEV_PRINTF("[NET] 报警音量=%u%%, 声报警=%u\r\n", 
                        (unsigned)sys_cfg.alarm_volume, (unsigned)sys_cfg.alarm_sound);
            cfg_updated = true;
            need_dev_sync = true;
        }
        else if(i == NET_REG_CONTROL_BIT2)
        {
            uint16_t ctrl = Net_Reg_Holding_Read_U16(dev, NET_REG_CONTROL_BIT2);
            uint8_t new_sound = (uint8_t)((ctrl >> NET_REG_CTRL2_SOUND_BIT) & 1U);
            uint8_t old_sound = sys_cfg.alarm_sound;

            if(new_sound != old_sound)
            {
                if(new_sound != 0U)
                {
                    /* 需求 4：打开声报警使能，恢复报警音量为原先不为 0 的数值 */
                    sys_cfg.alarm_sound = 1U;
                    if(sys_cfg.alarm_volume == 0U && sys_cfg.alarm_volume_saved > 0U)
                    {
                        sys_cfg.alarm_volume = sys_cfg.alarm_volume_saved;
                        DEV_PRINTF("[声报警开启] 音量已恢复=%u%%\r\n", (unsigned)sys_cfg.alarm_volume);
                    }
                }
                else
                {
                    /* 需求 3：关闭声报警使能，报警音量设置为 0 */
                    sys_cfg.alarm_sound = 0U;
                    if(sys_cfg.alarm_volume > 0U)
                    {
                        /* 保存当前音量，用于之后恢复 */
                        sys_cfg.alarm_volume_saved = sys_cfg.alarm_volume;
                    }
                    sys_cfg.alarm_volume = 0U;
                    DEV_PRINTF("[声报警关闭] 音量已设置为 0，保存值=%u%%\r\n", (unsigned)sys_cfg.alarm_volume_saved);
                }
            }

            sys_cfg.alarm_light = (uint8_t)((ctrl >> NET_REG_CTRL2_LIGHT_BIT) & 1U);
            sys_cfg.display_enable = (uint8_t)((ctrl >> NET_REG_CTRL2_DISPLAY_BIT) & 1U);
            DEV_PRINTF("[NET] 声/光/屏控制 声=%u 光=%u 屏=%u 音量=%u%%\r\n",
                        (unsigned)sys_cfg.alarm_sound, (unsigned)sys_cfg.alarm_light,
                        (unsigned)sys_cfg.display_enable, (unsigned)sys_cfg.alarm_volume);
            cfg_updated = true;
            need_dev_sync = true;
        }
    }

    if(!cfg_updated)
        return;

    (void)net_cfg_try_save_flash();
    if(addr_changed)
    {
        Net_Device_Update_Addr();
        {
            uint32_t lora_addr = (uint32_t)sys_cfg.dev_addr;
            if(!LORA_Param(LORA_PARAM_ADDR, &lora_addr, true))
                DEV_PRINTF("[NET] LoRa 模块地址同步失败\r\n");
        }
    }
    if(need_dev_sync)
    {
        update_sys_cfg = true;
        Net_Config_Sync_To_Registers(CFG_IDX_DEVICE_ADDR);
    }
}

/********************************************************************************************
* 函数名：Net_Sync_SensorData_To_Registers
* 描  述：将实时传感器数据（剂量率、温度、湿度、气压、CO2、PM2.5）写入保持寄存器
*         供上位机/主站读取
* 输  入：无
* 输  出：无
* 调  用：外部调用（传感器数据更新后）
********************************************************************************************/
void Net_Sync_SensorData_To_Registers(void)
{
    uint32_t dose_u32;
    int32_t temp_i32;
    uint32_t press_u32;
    uint32_t hum_u32;
    uint32_t co2_u32;
    uint32_t pm25_u32;
    
    if(!net_w5500_dh)
    {
        DEV_PRINTF("[传感器同步] net_w5500_dh 未注册，跳过同步\r\n");
        return;
    }
    
    /* 1. 辐射剂量率（uSv/h -> uSv/h*100，2 寄存器 = 4 字节） */
    dose_u32 = (uint32_t)(data_var.real_rate * 100.0f);
    Net_Reg_Holding_Write_U32(net_w5500_dh, NET_REG_DOSE_RATE, dose_u32);
    
    /* 2. 温度（℃ -> ℃*10，2 寄存器 = 4 字节，int32 支持负数） */
    temp_i32 = (int32_t)(env_data.temperature * 10.0f);
    Net_Reg_Holding_Write_U32(net_w5500_dh, NET_REG_TEMP, (uint32_t)temp_i32);
    
    /* 3. 气压（Pa，2 寄存器 = 4 字节，直接上传） */
    press_u32 = (uint32_t)env_data.baro;  // env_data.baro 单位是 Pa，直接上传
    Net_Reg_Holding_Write_U32(net_w5500_dh, NET_REG_PRESS, press_u32);
    
    /* 4. 湿度（%，2 寄存器 = 4 字节，不需要缩放） */
    hum_u32 = (uint32_t)(env_data.humidity);  // env_data.humidity 单位是 %，直接上传
    Net_Reg_Holding_Write_U32(net_w5500_dh, NET_REG_HUM, hum_u32);
    
    /* 5. CO2 含量（ppm，2 寄存器 = 4 字节，不需要缩放） */
    co2_u32 = (uint32_t)(env_data.CO2);  // env_data.CO2 单位是 ppm，直接上传
    Net_Reg_Holding_Write_U32(net_w5500_dh, NET_REG_CO2, co2_u32);
    
    /* 6. PM2.5（ug/m³*10，2 寄存器 = 4 字节） */
    pm25_u32 = (uint32_t)(env_data.PM2_5 * 10);
    Net_Reg_Holding_Write_U32(net_w5500_dh, NET_REG_PM2D5, pm25_u32);
    
    // printf("[传感器同步] 剂量率=%.2f uSv/h, 温度=%.1f ℃，气压=%.1f hPa, 湿度=%.1f %%, CO2=%u ppm, PM2.5=%u ug/m³\r\n",
    //         (double)data_var.real_rate, (double)env_data.temperature, (double)env_data.baro,
    //         (double)env_data.humidity, (unsigned)env_data.CO2, (unsigned)env_data.PM2_5);
}

static uint32_t net_pack_dose_value_reg(float dose_uSv)
{
    uint16_t word;
    float scaled;

    if(dose_uSv < 0.0f)
        dose_uSv = 0.0f;

    if(dose_uSv >= 1000000.0f)
    {
        scaled = (dose_uSv / 1000000.0f) * 100.0f;
        word = ((uint16_t)(scaled + 0.5f) & 0x3FFFu) | NET_DOSE_UNIT_SV;
    }
    else if(dose_uSv >= 10.0f)
    {
        scaled = (dose_uSv / 1000.0f) * 100.0f;
        word = ((uint16_t)(scaled + 0.5f) & 0x3FFFu) | NET_DOSE_UNIT_MSV;
    }
    else
    {
        scaled = dose_uSv * 100.0f;
        word = ((uint16_t)(scaled + 0.5f) & 0x3FFFu) | NET_DOSE_UNIT_USV;
    }

    return (uint32_t)word;
}

static void net_reg_write_bytes(Net_Device_t *dev, uint16_t reg_addr, const uint8_t *data, uint16_t nbytes)
{
    uint16_t i;
    uint16_t nregs = (uint16_t)(nbytes / 2U);

    if(!dev || !data || (nbytes & 1U))
        return;

    for(i = 0; i < nregs; i++)
    {
        uint16_t w = (uint16_t)data[i * 2U] | ((uint16_t)data[i * 2U + 1U] << 8);
        Net_Reg_Holding_Write_U16(dev, (uint16_t)(reg_addr + i), w);
    }
}

static bool net_active_upload_reg_block(Net_Device_t *dev, uint16_t start_reg, uint16_t reg_qty)
{
    uint8_t frame_buf[48];
    uint16_t frame_idx = 0;
    uint8_t byte_count;
    uint16_t i;

    if(!dev || !dev->reg_tb || reg_qty == 0U || (reg_qty & 1U))
        return false;

    if((dev == net_w5500_dh) && !Net_Tcp_DataSendReady())
        return false;

    byte_count = (uint8_t)(reg_qty * 2U);

    frame_buf[frame_idx++] = dev->addr;
    frame_buf[frame_idx++] = NET_FC_ACTIVE_UPLOAD;
    frame_buf[frame_idx++] = byte_count;
    frame_buf[frame_idx++] = (uint8_t)(start_reg & 0xFF);
    frame_buf[frame_idx++] = (uint8_t)((start_reg >> 8) & 0xFF);

    for(i = 0; i < reg_qty; i += 2U)
    {
        uint32_t value = Net_Reg_Holding_Read_U32(dev, (uint16_t)(start_reg + i));
        frame_buf[frame_idx++] = (uint8_t)(value & 0xFF);
        frame_buf[frame_idx++] = (uint8_t)((value >> 8) & 0xFF);
        frame_buf[frame_idx++] = (uint8_t)((value >> 16) & 0xFF);
        frame_buf[frame_idx++] = (uint8_t)((value >> 24) & 0xFF);
    }

    /* 不含 CRC 入队；Net_Txbuf_Write 会追加 CRC，避免重复 CRC 导致多发 2 字节 */
    return Net_TxQueue_Push(dev, frame_buf, frame_idx);
}

/* ===================== 5 分钟历史数据按时间段查询上传（寄存器 108/112） ===================== */

static unsigned net_is_leap(unsigned y)
{
    return ((y % 4U == 0U && y % 100U != 0U) || (y % 400U == 0U)) ? 1U : 0U;
}

static uint32_t net_unix_from_ymdhms(unsigned y, unsigned mo, unsigned d, unsigned h, unsigned mi, unsigned s)
{
    static const uint16_t mdays[12] = {31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};
    uint32_t days = 0U;
    unsigned yy;
    unsigned m;

    for(yy = 1970U; yy < y; yy++)
        days += net_is_leap(yy) ? 366U : 365U;
    for(m = 1U; m < mo; m++)
    {
        unsigned dim = mdays[m - 1U];
        if(m == 2U && net_is_leap(y))
            dim = 29U;
        days += dim;
    }
    days += (uint32_t)(d - 1U);
    return days * 86400UL + (uint32_t)h * 3600U + (uint32_t)mi * 60U + (uint32_t)s;
}

static void net_unix_to_reg_time6(uint32_t u, uint8_t out[8])
{
    uint32_t days = u / 86400UL;
    uint32_t rem = u % 86400UL;
    unsigned y = 1970U;
    unsigned mo = 1U;
    unsigned d = 1U;
    unsigned h;
    unsigned mi;
    unsigned se;
    static const uint16_t mdays[12] = {31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};

    h = (unsigned)(rem / 3600U);
    rem %= 3600U;
    mi = (unsigned)(rem / 60U);
    se = (unsigned)(rem % 60U);

    while(days >= (net_is_leap(y) ? 366U : 365U))
    {
        days -= (net_is_leap(y) ? 366U : 365U);
        y++;
    }
    while(1)
    {
        unsigned dim = mdays[mo - 1U];
        if(mo == 2U && net_is_leap(y))
            dim = 29U;
        if(days < dim)
            break;
        days -= dim;
        mo++;
    }
    d = (unsigned)(days + 1U);

    memset(out, 0, 8U);
    out[0] = (uint8_t)(y % 100U);
    out[1] = (uint8_t)mo;
    out[2] = (uint8_t)d;
    out[3] = (uint8_t)h;
    out[4] = (uint8_t)mi;
    out[5] = (uint8_t)se;
}

static void net_unix_to_datetime_str(uint32_t u, char *out, size_t outlen)
{
    uint8_t t[8];
    unsigned y;

    if(!out || outlen < 20U)
        return;

    net_unix_to_reg_time6(u, t);
    y = t[0];
    if(y <= 99U)
        y += 2000U;

    (void)snprintf(out, outlen, "%04u-%02u-%02u %02u:%02u:%02u",
                   y, (unsigned)t[1], (unsigned)t[2],
                   (unsigned)t[3], (unsigned)t[4], (unsigned)t[5]);
}

static void net_read_reg_bytes(Net_Device_t *dev, uint16_t reg_addr, uint8_t *out, uint16_t nbytes)
{
    uint16_t i;
    uint16_t nregs = (uint16_t)(nbytes / 2U);

    if(!dev || !out || (nbytes & 1U))
        return;

    for(i = 0U; i < nregs; i++)
    {
        uint16_t w = Net_Reg_Holding_Read_U16(dev, (uint16_t)(reg_addr + i));
        out[i * 2U] = (uint8_t)(w & 0xFFU);
        out[i * 2U + 1U] = (uint8_t)(w >> 8);
    }
}

static bool net_reg_time6_range_ok(const uint8_t t[8])
{
    if(!t)
        return false;
    if(t[1] < 1U || t[1] > 12U || t[2] < 1U || t[2] > 31U)
        return false;
    if(t[3] > 23U || t[4] > 59U || t[5] > 59U)
        return false;
    return true;
}

static void net_try_apply_time_cfg(Net_Device_t *dev)
{
    uint8_t t[8];
    DateTime_t dt;

    if(!dev)
        return;

    net_read_reg_bytes(dev, NET_REG_DATA_TIME_CFG, t, sizeof(t));
    if(!net_reg_time6_range_ok(t))
        return;

    memset(&dt, 0, sizeof(dt));
    dt.year = t[0];
    dt.month = t[1];
    dt.day = t[2];
    dt.hour = t[3];
    dt.minute = t[4];
    dt.second = t[5];
    dt.week = 0U;

    pcf8563_set_cur_time(&dt);
    pcf8563_get_cur_time(&date_time);

    DEV_PRINTF("[NET] 系统时间同步: %02u-%02u-%02u %02u:%02u:%02u\r\n",
               (unsigned)dt.year, (unsigned)dt.month, (unsigned)dt.day,
               (unsigned)dt.hour, (unsigned)dt.minute, (unsigned)dt.second);
}

static void net_time_cfg_on_reg_written(Net_Device_t *dev, uint16_t reg, uint16_t qreg)
{
    uint32_t end_reg;
    uint16_t i;

    if(!dev || qreg == 0U)
        return;

    end_reg = (uint32_t)reg + (uint32_t)qreg;
    if(end_reg <= (uint32_t)NET_REG_DATA_TIME_CFG ||
       reg > (NET_REG_DATA_TIME_CFG + NET_REG_TIME_CFG_QREG - 1U))
        return;

    for(i = reg; (uint32_t)i < end_reg; i++)
    {
        if(i >= NET_REG_DATA_TIME_CFG &&
           i < (NET_REG_DATA_TIME_CFG + NET_REG_TIME_CFG_QREG))
        {
            g_reg_write_mask.time_cfg |= (uint8_t)(1U << (i - NET_REG_DATA_TIME_CFG));
        }
    }

    if(g_reg_write_mask.time_cfg != NET_REG_TIME_CFG_WRITTEN_ALL)
        return;

    g_reg_write_mask.time_cfg = 0U;
    net_try_apply_time_cfg(dev);
}

static uint32_t net_reg_time_to_unix(Net_Device_t *dev, uint16_t reg_addr)
{
    uint8_t t[8];
    unsigned y;

    net_read_reg_bytes(dev, reg_addr, t, sizeof(t));
    if(!net_reg_time6_range_ok(t))
        return 0U;

    y = t[0];
    if(y <= 99U)
        y += 2000U;
    return net_unix_from_ymdhms(y, t[1], t[2], t[3], t[4], t[5]);
}

static bool net_hist_both_times_ready(Net_Device_t *dev)
{
    uint8_t start[8];
    uint8_t end[8];

    net_read_reg_bytes(dev, NET_REG_DATA_TIME_START, start, sizeof(start));
    net_read_reg_bytes(dev, NET_REG_DATA_TIME_END, end, sizeof(end));
    return net_reg_time6_range_ok(start) && net_reg_time6_range_ok(end);
}

static void net_hist_mark_written(uint16_t reg, uint16_t qreg)
{
    uint32_t end_reg;
    uint16_t i;

    if(qreg == 0U)
        return;

    end_reg = (uint32_t)reg + (uint32_t)qreg;
    for(i = reg; (uint32_t)i < end_reg; i++)
    {
        if(i >= NET_REG_DATA_TIME_START &&
           i < (NET_REG_DATA_TIME_START + NET_HIST_TIME_QREG))
        {
            g_reg_write_mask.hist_start |= (uint8_t)(1U << (i - NET_REG_DATA_TIME_START));
        }
        else if(i >= NET_REG_DATA_TIME_END &&
                i < (NET_REG_DATA_TIME_END + NET_HIST_TIME_QREG))
        {
            g_reg_write_mask.hist_end |= (uint8_t)(1U << (i - NET_REG_DATA_TIME_END));
        }
    }
}

static bool net_hist_both_blocks_written(void)
{
    return (g_reg_write_mask.hist_start == NET_HIST_TIME_WRITTEN_ALL) &&
           (g_reg_write_mask.hist_end == NET_HIST_TIME_WRITTEN_ALL);
}

static bool net_hist_write_touched(uint16_t reg, uint16_t qreg)
{
    uint32_t end_reg;

    if(qreg == 0U)
        qreg = 1U;
    end_reg = (uint32_t)reg + (uint32_t)qreg;
    return (reg <= (NET_REG_DATA_TIME_END + 3U)) && (end_reg > NET_REG_DATA_TIME_START);
}

static void net_clear_hist_query_regs(Net_Device_t *dev)
{
    uint16_t reg;

    if(!dev)
        return;
    for(reg = NET_REG_DATA_TIME_START; reg <= NET_REG_HIST_QUERY_LAST; reg++)
        Net_Reg_Holding_Write_U16(dev, reg, 0U);
}

static uint16_t net_tx_queue_idle_slots(Net_Device_t *dev)
{
    uint16_t free_cnt;

    if(!dev || !dev->txq)
        return 0U;
    free_cnt = Dev_Get_Queue_FreeCount((Dev_Queue_t *)dev->txq);
    if(free_cnt <= NET_5MIN_HIST_TXQ_RESERVE)
        return 0U;
    return (uint16_t)(free_cnt - NET_5MIN_HIST_TXQ_RESERVE);
}

static bool net_tx_queue_is_empty(Net_Device_t *dev)
{
    if(!dev || !dev->txq)
        return true;
    return (Dev_Get_Queue_Occupied((Dev_Queue_t *)dev->txq) == QUEUE_INVALID_IDX);
}

static bool net_queue_5min_record_from_raw(Net_Device_t *dev, uint32_t unix_ts, float dose_uSv)
{
    uint8_t dt_bytes[8];

    if(!dev)
        return false;

    net_unix_to_reg_time6(unix_ts, dt_bytes);
    net_reg_write_bytes(dev, NET_REG_DATA_TIME, dt_bytes, sizeof(dt_bytes));
    if(dose_uSv < 0.0f)
        dose_uSv = 0.0f;
    Net_Reg_Holding_Write_U32(dev, NET_REG_DOSE_RATE_5MIN, net_pack_dose_value_reg(dose_uSv));
    return net_active_upload_reg_block(dev, NET_REG_DATA_TIME, 6U);
}

static void net_hist_finish_and_clear(Net_Device_t *dev)
{
    DEV_PRINTF("[历史记录] 上传完成，共 %u 条\r\n", (unsigned)g_5min_hist.queued);
    net_clear_hist_query_regs(dev);
    net_hist_ctx_reset();
}

static void net_hist_try_start(Net_Device_t *dev)
{
    uint32_t ts_start;
    uint32_t ts_end;
    uint16_t count;

    if(!dev || g_5min_hist.state != NET_5MIN_HIST_IDLE)
        return;
    if(!net_hist_both_times_ready(dev))
        return;

    ts_start = net_reg_time_to_unix(dev, NET_REG_DATA_TIME_START);
    ts_end = net_reg_time_to_unix(dev, NET_REG_DATA_TIME_END);
    if(ts_start == 0U || ts_end == 0U || ts_end < ts_start)
        return;

    count = HistRecord_GetValidCount();
    g_5min_hist.dev = dev;
    g_5min_hist.ts_start = ts_start;
    g_5min_hist.ts_end = ts_end;
    g_5min_hist.queued = 0U;
    g_5min_hist.scan_logical = (int16_t)count - 1;

    if(g_5min_hist.scan_logical < 0)
    {
        net_hist_finish_and_clear(dev);
        return;
    }

    g_5min_hist.state = NET_5MIN_HIST_UPLOADING;
    {
        char ts_start_str[24];
        char ts_end_str[24];

        net_unix_to_datetime_str(ts_start, ts_start_str, sizeof(ts_start_str));
        net_unix_to_datetime_str(ts_end, ts_end_str, sizeof(ts_end_str));
        DEV_PRINTF("[历史记录] 查询记录 %s ~ %s\r\n", ts_start_str, ts_end_str);
    }
}

static void net_hist_upload_pump(Net_Device_t *dev)
{
    uint16_t idle_slots;

    if(!dev || g_5min_hist.state == NET_5MIN_HIST_IDLE || g_5min_hist.dev != dev)
        return;

    if(g_5min_hist.state == NET_5MIN_HIST_UPLOADING)
    {
        idle_slots = net_tx_queue_idle_slots(dev);
        while(g_5min_hist.scan_logical >= 0 && idle_slots > 0U)
        {
            uint32_t uts = 0U;
            float dose = 0.0f;
            int rd;

            rd = HistRecord_ReadRecordRaw((uint16_t)g_5min_hist.scan_logical, &uts, &dose);
            if(rd == 0 && uts >= g_5min_hist.ts_start && uts <= g_5min_hist.ts_end)
            {
                if(!net_queue_5min_record_from_raw(dev, uts, dose))
                    return;
                g_5min_hist.queued++;
                idle_slots--;
            }
            g_5min_hist.scan_logical--;
        }

        if(g_5min_hist.scan_logical < 0)
        {
            g_5min_hist.state = NET_5MIN_HIST_DRAINING;
            g_5min_hist.drain_since = HAL_GetTick();
        }
        return;
    }

    if(g_5min_hist.state == NET_5MIN_HIST_DRAINING)
    {
        if(!net_tx_queue_is_empty(dev))
            return;
        if((HAL_GetTick() - g_5min_hist.drain_since) < NET_5MIN_HIST_DRAIN_MS)
            return;
        net_hist_finish_and_clear(dev);
    }
}

static void net_hist_on_reg_written(Net_Device_t *dev, uint16_t reg, uint16_t qreg)
{
    if(!dev || !net_hist_write_touched(reg, qreg))
        return;

    net_hist_mark_written(reg, qreg);
    if(!net_hist_both_blocks_written())
        return;

    net_reg_write_mask_reset_hist();
    net_hist_try_start(dev);
    net_hist_upload_pump(dev);
}

void Net_5MinHistory_Upload_Task(void)
{
    if(Ota_IsHeartbeatPaused())
        return;
    if(g_5min_hist.state != NET_5MIN_HIST_IDLE && g_5min_hist.dev)
        net_hist_upload_pump(g_5min_hist.dev);
}

void Net_Sync_5MinRecord_To_Registers(float dose_uSv, const DateTime_t *dt)
{
    uint8_t dt_bytes[8];
    uint32_t dose_reg;

    if(!net_w5500_dh || !dt)
        return;

    memset(dt_bytes, 0, sizeof(dt_bytes));
    dt_bytes[0] = (uint8_t)(dt->year % 100U);
    dt_bytes[1] = dt->month;
    dt_bytes[2] = dt->day;
    dt_bytes[3] = dt->hour;
    dt_bytes[4] = dt->minute;
    dt_bytes[5] = dt->second;

    net_reg_write_bytes(net_w5500_dh, NET_REG_DATA_TIME, dt_bytes, sizeof(dt_bytes));

    if(dose_uSv < 0.0f)
        dose_uSv = 0.0f;
    dose_reg = net_pack_dose_value_reg(dose_uSv);
    Net_Reg_Holding_Write_U32(net_w5500_dh, NET_REG_DOSE_RATE_5MIN, dose_reg);
}

void Net_Active_Upload_5MinRecord_Dispatch(void)
{
    if(Ota_IsHeartbeatPaused())
        return;
    if(tx_inft.crt == INFT_TCP)
    {
        if(net_w5500_dh)
            (void)net_active_upload_reg_block(net_w5500_dh, NET_REG_DATA_TIME, 6U);
    }
    else if(tx_inft.crt == INFT_CAN)
    {
        if(net_can_dh)
            (void)net_active_upload_reg_block(net_can_dh, NET_REG_DATA_TIME, 6U);
    }
    else if(tx_inft.crt == INFT_LORA)
    {
        if(net_lora_dh)
            (void)net_active_upload_reg_block(net_lora_dh, NET_REG_DATA_TIME, 6U);
    }
}

/* OTA Flash 写入/擦除按 32 字节对齐（末包 Reg_Flash_Write 可能超出 file_size） */
static uint32_t Ota_FlashAlignUp32(uint32_t byte_len)
{
    if((byte_len % 32U) == 0U)
        return byte_len;
    return byte_len + (32U - (byte_len % 32U));
}

/* STM32H7：校验前 invalidate Download 区 D-Cache（小文件 32 对齐 OTA 已通过，暂关闭）
static void Ota_InvalidateDownloadCache(uint32_t byte_len)
{
    uint32_t addr = DOWNLOAD_FLASH_ADDR;
    uint32_t end_addr = addr + byte_len;
    uint32_t line_addr = addr & ~31U;
    int32_t inv_len = (int32_t)(((end_addr - line_addr) + 31U) & ~31U);

    SCB_InvalidateDCache_by_Addr((void *)line_addr, inv_len);
    __DSB();
    __ISB();
}
*/

/* 单包写后读回校验：invalidate 本包 Flash 区再 memcmp（仅 len 有效字节） */
//static bool Ota_VerifyFlashWrite(uint32_t write_addr, const uint8_t *expected, uint32_t len)
//{
//    uint32_t line_addr = write_addr & ~31U;
//    uint32_t end_addr = write_addr + len;
//    int32_t inv_len = (int32_t)(((end_addr - line_addr) + 31U) & ~31U);
//    const uint8_t *flash_ptr = (const uint8_t *)write_addr;
//    uint32_t i;

//#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
//    SCB_InvalidateDCache_by_Addr((void *)line_addr, inv_len);
//    __DSB();
//    __ISB();
//#endif

//    for(i = 0U; i < len; i++)
//    {
//        if(flash_ptr[i] != expected[i])
//        {
//            DEV_PRINTF("[OTA] Flash 读回校验失败：addr=0x%08lX, idx=%lu, exp=0x%02X, got=0x%02X\r\n",
//                        (unsigned long)(write_addr + i), (unsigned long)i,
//                        (unsigned)expected[i], (unsigned)flash_ptr[i]);
//            return false;
//        }
//    }
//    return true;
//}

/********************************************************************************************
* 函数名：Ota_PrepareDownload
* 描  述：OTA 下载准备函数（擦除 Flash 扇区）
* 输  入：@param: file_size -> 固件文件大小（字节）
* 输  出：@retval: true -> 准备成功；false -> 准备失败
* 调  用：外部调用（OTA 开始时）
********************************************************************************************/
bool Ota_PrepareDownload(uint32_t file_size)
{
    uint32_t erase_addr = 0;
    uint32_t erase_end = 0;
    uint32_t erase_bytes = 0;
    uint8_t status = 0;
    
    /* 检查文件大小是否超过 Download 区容量（896KB） */
    if(file_size > DOWNLOAD_FLASH_SIZE)
    {
        DEV_PRINTF("[OTA] 文件大小超限：%lu > %lu\r\n", (unsigned long)file_size, (unsigned long)DOWNLOAD_FLASH_SIZE);
        return false;
    }
    
    /* 擦除范围覆盖末包 32 字节对齐后的 Flash 写入长度（补 0 区也需可编程） */
    erase_bytes = Ota_FlashAlignUp32(file_size);
    if(erase_bytes > DOWNLOAD_FLASH_SIZE)
    {
        DEV_PRINTF("[OTA] 对齐后擦除长度超限：%lu > %lu\r\n",
                    (unsigned long)erase_bytes, (unsigned long)DOWNLOAD_FLASH_SIZE);
        return false;
    }
    
    erase_addr = DOWNLOAD_FLASH_ADDR;
    erase_end = DOWNLOAD_FLASH_ADDR + erase_bytes;
    
    DEV_PRINTF("[OTA] 开始擦除 Flash: 0x%08lX - 0x%08lX (file=%lu, erase=%lu bytes)\r\n",
                (unsigned long)erase_addr, (unsigned long)erase_end,
                (unsigned long)file_size, (unsigned long)erase_bytes);
    
    /* 循环擦除扇区（每个扇区 128KB） */
    INTX_DISABLE();
    Reg_Flash_Unlock(1);
    
    while(erase_addr < erase_end)
    {
        status = Reg_Flash_EraseSector(erase_addr);
        if(status)
        {
            Reg_Flash_Lock(1);
            INTX_ENABLE();
            DEV_PRINTF("[OTA] 擦除扇区失败：0x%08lX, status=%d\r\n", (unsigned long)erase_addr, (int)status);
            return false;
        }
        
        /* 跳转到下一个扇区（128KB 对齐） */
        erase_addr += FLASH_SECTOR_SIZE;
    }
    
    status = Reg_Flash_EraseSector(OTA_FLAG_FLASH_ADDR);
    if(status)
    {
        Reg_Flash_Lock(1);
        INTX_ENABLE();
        DEV_PRINTF("[OTA] 擦除扇区失败：0x%08lX, status=%d\r\n", (unsigned long)erase_addr, (int)status);
        return false;
    }
    Reg_Flash_Lock(1);
    INTX_ENABLE();
    
    DEV_PRINTF("[OTA] Flash 擦除完成！\r\n");
    return true;
}

/********************************************************************************************
* 函数名：Ota_WriteData
* 描  述：OTA 数据写入函数（写入固件数据到 Flash）
*         支持任意长度写入，内部自动处理 32 字节对齐
* 输  入：@param: offset -> 写入偏移量（从 Download 区起始地址开始）
*         @param: *data -> 数据缓冲区
*         @param: len -> 数据长度（字节）
* 输  出：@retval: true -> 写入成功；false -> 写入失败
* 调  用：外部调用（接收固件数据时）
********************************************************************************************/
bool Ota_WriteData(uint32_t offset, uint8_t *data, uint32_t len)
{
    uint32_t write_addr = DOWNLOAD_FLASH_ADDR + offset;
    uint32_t write_len = len;
    uint32_t pad_len = 0;
    uint32_t flash_buf[64];  /* 256 字节，4 字节对齐；单包最大 224 + 32 填充 */
    uint32_t *p_write = NULL;
    bool need_copy = false;
    
    /* 检查写入地址是否越界 */
    if(write_addr >= (DOWNLOAD_FLASH_ADDR + DOWNLOAD_FLASH_SIZE))
    {
        DEV_PRINTF("[OTA] 写入地址越界：0x%08lX\r\n", (unsigned long)write_addr);
        return false;
    }
    
    if(len > sizeof(flash_buf))
    {
        DEV_PRINTF("[OTA] 单包长度超限：%lu > %lu\r\n",
                    (unsigned long)len, (unsigned long)sizeof(flash_buf));
        return false;
    }
    
    /* 非 32 字节对齐时补 0xFF 到下一 32 字节边界（不参与 CRC，仅满足 Flash 编程粒度） */
    if((len % 32U) != 0U)
    {
        pad_len = 32U - (len % 32U);
        write_len = len + pad_len;
        DEV_PRINTF("[OTA] 数据长度非 32 字节对齐，自动填充：%lu + %lu = %lu\r\n",
                    (unsigned long)len, (unsigned long)pad_len, (unsigned long)write_len);
    }
    
    if((write_addr + write_len) > (DOWNLOAD_FLASH_ADDR + DOWNLOAD_FLASH_SIZE))
    {
        DEV_PRINTF("[OTA] 写入范围越界：addr=0x%08lX, len=%lu\r\n",
                    (unsigned long)write_addr, (unsigned long)write_len);
        return false;
    }
    
    /* 末包补 0 或未 4 字节对齐的 reg_tb：拷贝到对齐缓冲后再写 Flash */
    need_copy = ((len % 32U) != 0U)
             || (((uint32_t)(size_t)(void *)data & 3U) != 0U);
    if(need_copy)
    {
        memset(flash_buf, 0xFF, sizeof(flash_buf));
        memcpy(flash_buf, data, len);
        p_write = flash_buf;
    }
    else
    {
        p_write = (uint32_t *)data;
    }
    
    if(!Reg_Flash_Write(write_addr, p_write, write_len / 4U))
    {
        DEV_PRINTF("[OTA] Flash 写入失败：addr=0x%08lX, len=%lu\r\n",
                    (unsigned long)write_addr, (unsigned long)write_len);
        return false;
    }
//    if(!Ota_VerifyFlashWrite(write_addr, p_expected, len))
//    {
//        DEV_PRINTF("[OTA] Flash 读回校验失败：offset=%lu, len=%lu\r\n",
//                    (unsigned long)offset, (unsigned long)len);
//        return false;
//    }
    
    return true;
}

/********************************************************************************************
* 函数名：Ota_ProcessPacket
* 描  述：OTA 数据包处理函数（接收、校验、写入 Flash）
* 输  入：@param: *dev -> 设备句柄
*         @param: len -> 寄存器数量（每个寄存器 2 字节）
* 输  出：无
* 调  用：外部调用（Net_App_HandleWriteMulti 中调用）
********************************************************************************************/
void Ota_ProcessPacket(Net_Device_t *dev, uint16_t len)
{
    uint32_t data_len = 0;

    ota_touch_activity();
    // DEV_PRINTF("[OTA] Ota_ProcessPacket 调用：len=%d, g_ota_state=%d\r\n", len, (int)g_ota_state);
    
    if(g_ota_state != OTA_STATE_STARTED)
    {
        DEV_PRINTF("[OTA] 状态错误，无法接收数据：%d\r\n", (int)g_ota_state);
        
        /* 如果是 ERROR 状态，重新上传错误状态告知上位机 */
        if(g_ota_state == OTA_STATE_ERROR)
        {
            /* 更新寄存器 204-207 并上传错误状态 */
            if(dev->reg_tb && dev->reg_sz >= (NET_REG_OTA_STATE + 4))
            {
                dev->reg_tb[NET_REG_OTA_STATE * 2] = (uint8_t)(g_ota_state & 0xFF);
                dev->reg_tb[NET_REG_OTA_STATE * 2 + 1] = (uint8_t)((g_ota_state >> 8) & 0xFF);
                dev->reg_tb[NET_REG_OTA_STATE * 2 + 2] = (uint8_t)((g_ota_state >> 16) & 0xFF);
                dev->reg_tb[NET_REG_OTA_STATE * 2 + 3] = (uint8_t)((g_ota_state >> 24) & 0xFF);
                
                dev->reg_tb[NET_REG_OTA_STATE * 2 + 4] = (uint8_t)(g_ota_written_bytes & 0xFF);
                dev->reg_tb[NET_REG_OTA_STATE * 2 + 5] = (uint8_t)((g_ota_written_bytes >> 8) & 0xFF);
                dev->reg_tb[NET_REG_OTA_STATE * 2 + 6] = (uint8_t)((g_ota_written_bytes >> 16) & 0xFF);
                dev->reg_tb[NET_REG_OTA_STATE * 2 + 7] = (uint8_t)((g_ota_written_bytes >> 24) & 0xFF);
            }
            Net_Active_Upload_OtaStatus(dev, g_ota_state, g_ota_written_bytes);
            DEV_PRINTF("[OTA] ERROR 状态已上传：state=3, written=%lu\r\n", (unsigned long)g_ota_written_bytes);
        }
        return;
    }
    
    /* 计算数据长度（len 是寄存器数量，每个寄存器 2 字节） */
    data_len = len * 2;
    
    /* 确保数据长度是 4 字节的倍数 */
    if(data_len % 4 != 0)
    {
        DEV_PRINTF("[OTA] 数据长度不是 4 字节倍数：%lu\r\n", (unsigned long)data_len);
        g_ota_state = OTA_STATE_ERROR;
        
        /* 更新寄存器并上传错误状态 */
        if(dev->reg_tb && dev->reg_sz >= (NET_REG_OTA_STATE + 4))
        {
            dev->reg_tb[NET_REG_OTA_STATE * 2] = (uint8_t)(g_ota_state & 0xFF);
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 1] = (uint8_t)((g_ota_state >> 8) & 0xFF);
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 2] = (uint8_t)((g_ota_state >> 16) & 0xFF);
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 3] = (uint8_t)((g_ota_state >> 24) & 0xFF);
            
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 4] = (uint8_t)(g_ota_written_bytes & 0xFF);
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 5] = (uint8_t)((g_ota_written_bytes >> 8) & 0xFF);
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 6] = (uint8_t)((g_ota_written_bytes >> 16) & 0xFF);
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 7] = (uint8_t)((g_ota_written_bytes >> 24) & 0xFF);
        }
        Net_Active_Upload_OtaStatus(dev, g_ota_state, g_ota_written_bytes);
        return;
    }
    
    /* 检查写入偏移量是否越界 */
    if((g_ota_written_bytes + data_len) > g_ota_file_size)
    {
        DEV_PRINTF("[OTA] 数据越界：offset=%lu, len=%lu, total=%lu\r\n",
                    (unsigned long)g_ota_written_bytes, (unsigned long)data_len, (unsigned long)g_ota_file_size);
        g_ota_state = OTA_STATE_ERROR;
        
        /* 更新寄存器并上传错误状态 */
        if(dev->reg_tb && dev->reg_sz >= (NET_REG_OTA_STATE + 4))
        {
            dev->reg_tb[NET_REG_OTA_STATE * 2] = (uint8_t)(g_ota_state & 0xFF);
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 1] = (uint8_t)((g_ota_state >> 8) & 0xFF);
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 2] = (uint8_t)((g_ota_state >> 16) & 0xFF);
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 3] = (uint8_t)((g_ota_state >> 24) & 0xFF);
            
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 4] = (uint8_t)(g_ota_written_bytes & 0xFF);
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 5] = (uint8_t)((g_ota_written_bytes >> 8) & 0xFF);
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 6] = (uint8_t)((g_ota_written_bytes >> 16) & 0xFF);
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 7] = (uint8_t)((g_ota_written_bytes >> 24) & 0xFF);
        }
        Net_Active_Upload_OtaStatus(dev, g_ota_state, g_ota_written_bytes);
        return;
    }
    
    /* 更新状态为校验中 */
    g_ota_state = OTA_STATE_VERIFY;
    
    /* 写入寄存器 204-207（8 字节） */
    if(dev->reg_tb && dev->reg_sz >= (NET_REG_OTA_STATE + 4))
    {
        /* 寄存器 204-205：state（32 位） */
        dev->reg_tb[NET_REG_OTA_STATE * 2] = (uint8_t)(g_ota_state & 0xFF);
        dev->reg_tb[NET_REG_OTA_STATE * 2 + 1] = (uint8_t)((g_ota_state >> 8) & 0xFF);
        dev->reg_tb[NET_REG_OTA_STATE * 2 + 2] = (uint8_t)((g_ota_state >> 16) & 0xFF);
        dev->reg_tb[NET_REG_OTA_STATE * 2 + 3] = (uint8_t)((g_ota_state >> 24) & 0xFF);
        
        /* 寄存器 206-207：written_bytes（32 位） */
        dev->reg_tb[NET_REG_OTA_STATE * 2 + 4] = (uint8_t)(g_ota_written_bytes & 0xFF);
        dev->reg_tb[NET_REG_OTA_STATE * 2 + 5] = (uint8_t)((g_ota_written_bytes >> 8) & 0xFF);
        dev->reg_tb[NET_REG_OTA_STATE * 2 + 6] = (uint8_t)((g_ota_written_bytes >> 16) & 0xFF);
        dev->reg_tb[NET_REG_OTA_STATE * 2 + 7] = (uint8_t)((g_ota_written_bytes >> 24) & 0xFF);
    }
    
    /* 主动发送一次当前 OTA 状态 */
    // DEV_PRINTF("[OTA] 准备上传 VERIFY 状态：state=%d, written=%lu\r\n", (int)g_ota_state, (unsigned long)g_ota_written_bytes);
    if(!Net_Active_Upload_OtaStatus(dev, g_ota_state, g_ota_written_bytes))
    {
        DEV_PRINTF("[OTA] VERIFY 状态上传失败！\r\n");
    }
    // else
    // {
    //     DEV_PRINTF("[OTA] VERIFY 状态上传成功\r\n");
    // }
    
    /* 从寄存器表读取 OTA 数据（寄存器 208+，最多 128 字节） */
    /* 注意：reg_tb 是字节数组，寄存器地址需要乘以 2 才能得到正确的字节偏移量 */
    uint8_t *ota_data = &dev->reg_tb[NET_REG_OTA_DATA * 2];
    
    /* 校验数据包（此处简化处理，实际可添加 CRC32 校验） */
    /* 假设校验通过，调用 Ota_WriteData 写入 Flash */
    if(Ota_WriteData(g_ota_written_bytes, ota_data, data_len))
    {
        /* 写入成功，更新已写入字节数 */
        g_ota_written_bytes += data_len;
        
        /* 更新状态为接收中 */
        g_ota_state = OTA_STATE_STARTED;
        
        /* 写入寄存器 204-207（8 字节） */
        if(dev->reg_tb && dev->reg_sz >= (NET_REG_OTA_STATE + 4))
        {
            /* 寄存器 204-205：state（32 位） */
            dev->reg_tb[NET_REG_OTA_STATE * 2] = (uint8_t)(g_ota_state & 0xFF);
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 1] = (uint8_t)((g_ota_state >> 8) & 0xFF);
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 2] = (uint8_t)((g_ota_state >> 16) & 0xFF);
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 3] = (uint8_t)((g_ota_state >> 24) & 0xFF);
            
            /* 寄存器 206-207：written_bytes（32 位） */
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 4] = (uint8_t)(g_ota_written_bytes & 0xFF);
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 5] = (uint8_t)((g_ota_written_bytes >> 8) & 0xFF);
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 6] = (uint8_t)((g_ota_written_bytes >> 16) & 0xFF);
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 7] = (uint8_t)((g_ota_written_bytes >> 24) & 0xFF);
        }
        
        /* 主动发送一次当前 OTA 状态 */
        // DEV_PRINTF("[OTA] 准备上传 STARTED 状态：state=%d, written=%lu\r\n", (int)g_ota_state, (unsigned long)g_ota_written_bytes);
        if(!Net_Active_Upload_OtaStatus(dev, g_ota_state, g_ota_written_bytes))
        {
            DEV_PRINTF("[OTA] STARTED 状态上传失败！\r\n");
        }
        // else
        // {
        //     DEV_PRINTF("[OTA] STARTED 状态上传成功！\r\n");
        // }
        
        // DEV_PRINTF("[OTA] 数据包写入成功：offset=%lu, len=%lu, total=%lu/%lu\r\n",
        //             (unsigned long)(g_ota_written_bytes - data_len), (unsigned long)data_len,
        //             (unsigned long)g_ota_written_bytes, (unsigned long)g_ota_file_size);
    }
    else
    {
        /* 写入失败，设置错误状态 */
        DEV_PRINTF("[OTA] 数据包写入失败！\r\n");
        g_ota_state = OTA_STATE_ERROR;
        
        /* 更新寄存器并上传错误状态 */
        if(dev->reg_tb && dev->reg_sz >= (NET_REG_OTA_STATE + 4))
        {
            dev->reg_tb[NET_REG_OTA_STATE * 2] = (uint8_t)(g_ota_state & 0xFF);
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 1] = (uint8_t)((g_ota_state >> 8) & 0xFF);
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 2] = (uint8_t)((g_ota_state >> 16) & 0xFF);
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 3] = (uint8_t)((g_ota_state >> 24) & 0xFF);
            
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 4] = (uint8_t)(g_ota_written_bytes & 0xFF);
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 5] = (uint8_t)((g_ota_written_bytes >> 8) & 0xFF);
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 6] = (uint8_t)((g_ota_written_bytes >> 16) & 0xFF);
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 7] = (uint8_t)((g_ota_written_bytes >> 24) & 0xFF);
        }
        
        Net_Active_Upload_OtaStatus(dev, g_ota_state, g_ota_written_bytes);
    }
}

/********************************************************************************************
* 函数名：BL_Crc32
* 描  述：CRC32 计算函数（与 IAP 代码一致）
*         多项式：0x04C11DB7
*         初始值：0xFFFFFFFF
*         输入：高位对齐，左移
*         输出：不异或
* 输  入：@param: *data -> 数据缓冲区
*         @param: len -> 数据长度
* 输  出：@retval: 计算得到的 CRC32 值
* 调  用：内部调用
********************************************************************************************/
static uint32_t BL_Crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFU;
    uint32_t i;
    uint8_t  j;
    
    for(i = 0U; i < len; i++)
    {
        crc ^= ((uint32_t)data[i] << 24);
        for(j = 0U; j < 8U; j++)
        {
            if((crc & 0x80000000U) != 0U)
                crc = (crc << 1) ^ 0x04C11DB7U;
            else
                crc <<= 1;
        }
    }
    return crc;
}

/********************************************************************************************
* 函数名：Ota_VerifyFirmware
* 描  述：OTA 固件校验函数（校验 Flash 中的固件）
*         计算 Download 区固件的 CRC32，与上位机发送的 CRC 比较
* 输  入：无
* 输  出：@retval: true -> 校验成功；false -> 校验失败
* 调  用：外部调用（固件接收完成后）
********************************************************************************************/
bool Ota_VerifyFirmware(void)
{
    uint32_t calc_crc = 0;
    
    /* 检查文件大小是否有效 */
    if(g_ota_file_size == 0 || g_ota_file_size > DOWNLOAD_FLASH_SIZE)
    {
        DEV_PRINTF("[OTA] 文件大小无效：%lu\r\n", (unsigned long)g_ota_file_size);
        return false;
    }
    
    /* 计算 Download 区固件的 CRC32 */
    /* Ota_InvalidateDownloadCache(g_ota_file_size); */
    calc_crc = BL_Crc32((const uint8_t *)DOWNLOAD_FLASH_ADDR, g_ota_file_size);
    
    DEV_PRINTF("[OTA] 固件 CRC 校验：计算值=0x%08lX, 期望值=0x%08lX\r\n",
                (unsigned long)calc_crc, (unsigned long)g_ota_file_crc);
    
    /* 与上位机发送的 CRC 比较 */
    if(calc_crc != g_ota_file_crc)
    {
        DEV_PRINTF("[OTA] CRC 校验失败！\r\n");
        return false;
    }
    
    DEV_PRINTF("[OTA] CRC 校验成功！\r\n");
    return true;
}

/********************************************************************************************
* 函数名：Ota_HandleFinishCommand
* 描  述：处理 OTA 结束指令（寄存器 202-203）
*         1. 如果处于 DONE 状态且收到重启指令，立即重启
*         2. 否则校验固件并更新 OTA 标志
* 输  入：@param: *dev -> 设备句柄
* 输  出：无
* 调  用：外部调用（Net_App_HandleWriteMulti 中调用）
********************************************************************************************/
void Ota_HandleFinishCommand(Net_Device_t *dev)
{
    ota_touch_activity();
    /* 1. 检查是否是重启指令（处于 DONE 状态，且 state=0, written_bytes=0） */
    if(g_ota_state == OTA_STATE_DONE)
    {
        uint32_t received_value = Net_Reg_Holding_Read_U32(dev, NET_REG_OTA_CRC32);
        
        if(received_value == 0)
        {
            DEV_PRINTF("[OTA] 收到重启指令，立即重启...\r\n");
            g_ota_reboot_pending = 0;
            vTaskDelay(100);
            NVIC_SystemReset();
        }
    }
    
    /* 2. 从寄存器表读取文件 CRC32（寄存器 202-203，4 字节） */
    g_ota_file_crc = Net_Reg_Holding_Read_U32(dev, NET_REG_OTA_CRC32);
    DEV_PRINTF("[OTA] 收到结束指令，文件 CRC32=0x%08lX\r\n", (unsigned long)g_ota_file_crc);
    
    /* 3. 更新状态为校验中，写入寄存器 204-207（8 字节） */
    g_ota_state = OTA_STATE_VERIFY;
    
    if(dev->reg_tb && dev->reg_sz >= (NET_REG_OTA_STATE + 4))
    {
        /* 寄存器 204-205：state（32 位） */
        dev->reg_tb[NET_REG_OTA_STATE * 2] = (uint8_t)(g_ota_state & 0xFF);
        dev->reg_tb[NET_REG_OTA_STATE * 2 + 1] = (uint8_t)((g_ota_state >> 8) & 0xFF);
        dev->reg_tb[NET_REG_OTA_STATE * 2 + 2] = (uint8_t)((g_ota_state >> 16) & 0xFF);
        dev->reg_tb[NET_REG_OTA_STATE * 2 + 3] = (uint8_t)((g_ota_state >> 24) & 0xFF);
        
        /* 寄存器 206-207：written_bytes（32 位） */
        dev->reg_tb[NET_REG_OTA_STATE * 2 + 4] = (uint8_t)(g_ota_written_bytes & 0xFF);
        dev->reg_tb[NET_REG_OTA_STATE * 2 + 5] = (uint8_t)((g_ota_written_bytes >> 8) & 0xFF);
        dev->reg_tb[NET_REG_OTA_STATE * 2 + 6] = (uint8_t)((g_ota_written_bytes >> 16) & 0xFF);
        dev->reg_tb[NET_REG_OTA_STATE * 2 + 7] = (uint8_t)((g_ota_written_bytes >> 24) & 0xFF);
    }
    
    /* 主动发送一次当前 OTA 状态 */
    Net_Active_Upload_OtaStatus(dev, g_ota_state, g_ota_written_bytes);
    
    /* 4. 校验 Flash 中的固件数据 */
    if(Ota_VerifyFirmware())
    {
        /* 校验成功，更新 OTA 标志 */
        if(Ota_FinishDownload())
        {
            /* 更新状态为完成 */
            g_ota_state = OTA_STATE_DONE;
            g_ota_done_time = HAL_GetTick();
//            g_ota_reboot_pending = 1;
            
            /* 写入寄存器 204-207（8 字节） */
            if(dev->reg_tb && dev->reg_sz >= (NET_REG_OTA_STATE + 4))
            {
                /* 寄存器 204-205：state（32 位） */
                dev->reg_tb[NET_REG_OTA_STATE * 2] = (uint8_t)(g_ota_state & 0xFF);
                dev->reg_tb[NET_REG_OTA_STATE * 2 + 1] = (uint8_t)((g_ota_state >> 8) & 0xFF);
                dev->reg_tb[NET_REG_OTA_STATE * 2 + 2] = (uint8_t)((g_ota_state >> 16) & 0xFF);
                dev->reg_tb[NET_REG_OTA_STATE * 2 + 3] = (uint8_t)((g_ota_state >> 24) & 0xFF);
                
                /* 寄存器 206-207：written_bytes（32 位） */
                dev->reg_tb[NET_REG_OTA_STATE * 2 + 4] = (uint8_t)(g_ota_written_bytes & 0xFF);
                dev->reg_tb[NET_REG_OTA_STATE * 2 + 5] = (uint8_t)((g_ota_written_bytes >> 8) & 0xFF);
                dev->reg_tb[NET_REG_OTA_STATE * 2 + 6] = (uint8_t)((g_ota_written_bytes >> 16) & 0xFF);
                dev->reg_tb[NET_REG_OTA_STATE * 2 + 7] = (uint8_t)((g_ota_written_bytes >> 24) & 0xFF);
            }
            
            /* 主动发送一次当前 OTA 状态 */
            Net_Active_Upload_OtaStatus(dev, g_ota_state, g_ota_written_bytes);
            DEV_PRINTF("[OTA] 固件校验成功，立即重启\r\n");
            NVIC_SystemReset();  // 系统重启
        }
        else
        {
            /* 更新 OTA 标志失败 */
            g_ota_state = OTA_STATE_ERROR;
            DEV_PRINTF("[OTA] 更新 OTA 标志失败！\r\n");
        }
//        taskEXIT_CRITICAL();
    }
    else
    {
        /* CRC 校验失败 */
        g_ota_state = OTA_STATE_ERROR;
        DEV_PRINTF("[OTA] 固件 CRC 校验失败！\r\n");
    }
    
    /* 5. 如果状态为错误，主动发送一次错误状态 */
    if(g_ota_state == OTA_STATE_ERROR)
    {
        if(dev->reg_tb && dev->reg_sz >= (NET_REG_OTA_STATE + 4))
        {
            dev->reg_tb[NET_REG_OTA_STATE * 2] = (uint8_t)(g_ota_state & 0xFF);
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 1] = (uint8_t)((g_ota_state >> 8) & 0xFF);
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 2] = (uint8_t)((g_ota_state >> 16) & 0xFF);
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 3] = (uint8_t)((g_ota_state >> 24) & 0xFF);
            
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 4] = (uint8_t)(g_ota_written_bytes & 0xFF);
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 5] = (uint8_t)((g_ota_written_bytes >> 8) & 0xFF);
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 6] = (uint8_t)((g_ota_written_bytes >> 16) & 0xFF);
            dev->reg_tb[NET_REG_OTA_STATE * 2 + 7] = (uint8_t)((g_ota_written_bytes >> 24) & 0xFF);
        }
        
        Net_Active_Upload_OtaStatus(dev, g_ota_state, g_ota_written_bytes);
    }
}

/********************************************************************************************
* 函数名：Ota_Thread_Task
* 描  述：OTA 后台任务（检查重启定时器）
*         当进入 DONE 状态后，如果 1 秒内未收到重启指令，则自动重启
* 输  入：无
* 输  出：无
* 调  用：外部调用（Net_Thread_Task 或主循环）
********************************************************************************************/
void Ota_Thread_Task(void)
{
    uint32_t now = HAL_GetTick();

    /* 上位机中途停止：STARTED/VERIFY/ERROR 超过 OTA_SESSION_IDLE_MS 无活动则恢复 IDLE */
    if(g_ota_last_activity_ms != 0U)
    {
        if(g_ota_state == OTA_STATE_STARTED ||
           g_ota_state == OTA_STATE_VERIFY ||
           g_ota_state == OTA_STATE_ERROR)
        {
            if((now - g_ota_last_activity_ms) > OTA_SESSION_IDLE_MS)
                ota_reset_idle_session();
        }
    }

    /* 检查是否需要重启 */
    if(g_ota_reboot_pending && g_ota_state == OTA_STATE_DONE)
    {
        uint32_t elapsed = HAL_GetTick() - g_ota_done_time;
        
        /* 如果超过 1 秒未收到重启指令，则自动重启 */
        if(elapsed >= 1000)
        {
            DEV_PRINTF("[OTA] 1 秒超时，自动重启...\r\n");
            g_ota_reboot_pending = 0;
            HAL_Delay(100);  // 短暂延时确保数据发送完成
            NVIC_SystemReset();  // 系统重启
        }
    }
}

/********************************************************************************************
* 函数名：Ota_FinishDownload
* 描  述：OTA 下载完成处理函数（设置 OTA 标志到 Flash）
*         写入 OTA_Flag 结构体到 OTA_FLAG_FLASH_ADDR，标记固件可更新
* 输  入：无
* 输  出：@retval: true -> 完成成功；false -> 完成失败
* 调  用：外部调用（固件校验通过后）
********************************************************************************************/
bool Ota_FinishDownload(void)
{
    OtaFlag_t ota_flag = {0};
    uint32_t flag_crc = 0;
    
    /* 初始化 OTA 标志结构 */
    ota_flag.magic = OTA_FLAG_MAGIC;
    ota_flag.app_size = g_ota_file_size;
    ota_flag.app_crc32 = g_ota_file_crc;
    ota_flag.status = OTA_STATUS_PENDING;  /* 标记为待更新状态 */
    
    /* 清零保留字段 */
    for(uint8_t i = 0; i < 4; i++)
    {
        ota_flag.reserved[i] = 0;
    }
    
    /* 计算标志 CRC（不包含 flag_crc 本身） */
    flag_crc = BL_Crc32((const uint8_t *)&ota_flag, 32);
    ota_flag.flag_crc = flag_crc;
    
    DEV_PRINTF("[OTA] 写入 OTA 标志：magic=0x%08lX, size=%lu, crc=0x%08lX, status=%lu, flag_crc=0x%08lX\r\n",
                (unsigned long)ota_flag.magic, (unsigned long)ota_flag.app_size,
                (unsigned long)ota_flag.app_crc32, (unsigned long)ota_flag.status,
                (unsigned long)ota_flag.flag_crc);
    
    /* 写入 OTA 标志到 Flash */
    /* 注意：Reg_Flash_Write 会自动处理擦除和验证 */
    if(!Reg_Flash_Write(OTA_FLAG_FLASH_ADDR, (uint32_t*)&ota_flag, sizeof(OtaFlag_t) / sizeof(uint32_t)))
    {
        DEV_PRINTF("[OTA] OTA 标志写入失败\r\n");
        return false;
    }
    
//    taskENTER_CRITICAL();
    /* 读取验证（Reg_Flash_Write 已经验证过，这里再次确认） */
    OtaFlag_t verify_flag;
    memcpy(&verify_flag, (void *)OTA_FLAG_FLASH_ADDR, sizeof(OtaFlag_t));
    
    if(verify_flag.magic != OTA_FLAG_MAGIC)
    {
        DEV_PRINTF("[OTA] OTA 标志写入验证失败：magic=0x%08lX\r\n", (unsigned long)verify_flag.magic);
        return false;
    }
    
    if(verify_flag.status != OTA_STATUS_PENDING)
    {
        DEV_PRINTF("[OTA] OTA 标志状态验证失败：status=%lu\r\n", (unsigned long)verify_flag.status);
        return false;
    }
    DEV_PRINTF("[OTA] OTA 标志写入成功！\r\n");
    
//    while(1){
//        vTaskDelay(1000);
//    }
//    taskEXIT_CRITICAL();
    return true;
}




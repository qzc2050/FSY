/**********************************************************************************************************
 * 文件名: net_raw_app.h
 * 概  述: 网络/串口裸协议应用（解析入口与发送错误回调；寄存器/从机 PDU 见 net_raw_protocol.h）
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

#include "./net_raw/net_raw_protocol.h"


//! ---------------------- ↓ 辐射报警仪寄存器地址宏定义（协议文档 07.xlsx） ↓ ---------------------- !//
/* 只读寄存器（传感器数据） */
#define NET_REG_DOSE_RATE           (1U)    // 辐射量（uSv/h，*100，2 寄存器）
#define NET_REG_TEMP                (3U)    // 温度（℃，*10，2 寄存器）
#define NET_REG_PRESS               (5U)    // 气压（Pa，*1，2 寄存器）
#define NET_REG_HUM                 (7U)    // 湿度（%，*1，2 寄存器）
#define NET_REG_CO2                 (9U)    // CO2 含量（ppm，*10，2 寄存器）
#define NET_REG_PM2D5               (11U)   // PM2.5（ug/m³，*10，2 寄存器）
#define NET_REG_ALARM_BIT1          (13U)   // 报警状态（32 位标志，2 寄存器）
#define NET_REG_STATUS_BIT          (15U)   // 设备状态（32 位标志，2 寄存器）
#define NET_REG_RESERVED_17         (17U)   // 预留（2 寄存器）
#define NET_REG_RESERVED_19         (19U)   // 预留（2 寄存器）
#define NET_REG_RESERVED_21         (21U)   // 预留（2 寄存器）
#define NET_REG_DATA_TIME           (30U)   // 时间戳（年月日时分秒，4 寄存器）
#define NET_REG_DOSE_RATE_5MIN      (34U)   // 5 分钟累计剂量（2 寄存器，单位编码见协议）

/* 读写寄存器（配置参数） */
#define NET_REG_ALERT_THRESHOLD1    (50U)   // 辐射量上阈值（uSv/h*100，2 寄存器）
#define NET_REG_ALERT_THRESHOLD2    (52U)   // 辐射量下阈值（uSv/h*100，2 寄存器）
#define NET_REG_ALERT_THRESHOLD3    (54U)   // 温度上阈值（℃*10，2 寄存器）
#define NET_REG_ALERT_THRESHOLD4    (56U)   // 温度下阈值（℃*10，2 寄存器）
#define NET_REG_ALERT_THRESHOLD5    (58U)   // 气压上阈值（hPa*10，2 寄存器）
#define NET_REG_ALERT_THRESHOLD6    (60U)   // 气压下阈值（hPa*10，2 寄存器）
#define NET_REG_ALERT_THRESHOLD7    (62U)   // 湿度上阈值（%*10，2 寄存器）
#define NET_REG_ALERT_THRESHOLD8    (64U)   // 湿度下阈值（%*10，2 寄存器）
#define NET_REG_ALERT_THRESHOLD9    (66U)   // CO2 上阈值（ppm*100，2 寄存器）
#define NET_REG_ALERT_THRESHOLD10   (68U)   // CO2 下阈值（ppm*100，2 寄存器）
#define NET_REG_ALERT_THRESHOLD11   (70U)   // PM2.5 上阈值（ug/m³*100，2 寄存器）
#define NET_REG_ALERT_THRESHOLD12   (72U)   // PM2.5 下阈值（ug/m³*100，2 寄存器）
#define NET_REG_ALARM_BITEN         (82U)   // 报警禁止掩码（32 位，2 寄存器；bit=1 禁止）
#define NET_REG_ALARM_BITEN_DOSE_HI_BIT   (0U)   // bit0：辐射上阈值报警禁止
#define NET_REG_ALARM_BITEN_DOSE_LO_BIT   (1U)   // bit1：辐射下阈值报警禁止
#define NET_REG_ALARM_BITEN_SOUND_BIT     (2U)   // bit2：声报警禁止
#define NET_REG_ALARM_BITEN_DOSE_OFF_BIT  (3U)   // bit3：辐射离线禁止（暂未实现）

/* reg82 禁止剂量报警时，暂存阈值有效标志（落 Flash，见 sys_cfg.dose_th_shadow_flags） */
#define NET_DOSE_SHADOW_HI_VALID          (1U << 0)
#define NET_DOSE_SHADOW_LO_VALID          (1U << 1)
#define NET_VOLUME_SHADOW_VALID           (1U << 2)   /* 声报警关闭时暂存音量 */
#define NET_REG_SERIALNUM           (86U)   // 序列号（16 字节 ASCII，8 寄存器）
#define NET_REG_DATA_TIME_CFG       (94U)   // 系统时间（年月日时分秒，4 寄存器）
#define NET_REG_SW_VERSION          (98U)   // 软件版本号（20 字节 ASCII，10 寄存器）
#define NET_REG_DATA_TIME_START     (108U)  // 历史数据开始时间（4 寄存器）
#define NET_REG_DATA_TIME_END       (112U)  // 历史数据结束时间（4 寄存器）
#define NET_REG_HIST_QUERY_LAST     (116U)  // 历史查询区末寄存器（上传完成后 108～116 清零）
#define NET_REG_REBOOT              (120U)  // 设备重启控制（0x0001=重启，1 寄存器）
#define NET_REG_ADDRESS             (121U)  // 通信地址（0x01-0x7F，1 寄存器）
#define NET_REG_ALARM_VOLUME        (122U)  // 报警音量（0-100 百分比，1 寄存器）
#define NET_REG_CONTROL_BIT2        (123U)  // 控制位 2（对应地址 15 的 status_bit，1 寄存器）

#define NET_REG_OTA_FILE_SIZE       (200U)  // OTA 文件大小（4 字节，2 寄存器）
#define NET_REG_OTA_CRC32           (202U)  // OTA 文件 CRC32（4 字节，2 寄存器）- OTA 结束指令
#define NET_REG_OTA_STATE           (204U)  // OTA 状态 + 已写入字节数（8 字节，4 寄存器）
#define NET_REG_OTA_DATA            (208U)  // OTA 数据寄存器（固件数据，N 寄存器）

// 报警状态标志位
// 辐射报警位（0-3）
#define RATE_HIGH_ALARM_BIT       0    // 辐射上阈值报警
#define RATE_LOW_ALARM_BIT        1    // 辐射下阈值报警
#define RATE_OFFLINE_BIT          2    // 辐射检测离线
#define RATE_RESERVED_BIT         3    // 保留

// 温度报警位（4-7）
#define TEMP_HIGH_ALARM_BIT       4    // 温度上阈值报警
#define TEMP_LOW_ALARM_BIT        5    // 温度下阈值报警
#define TEMP_OFFLINE_BIT          6    // 温度检测离线
#define TEMP_RESERVED_BIT         7    // 保留

// 气压报警位（8-11）
#define PRESS_HIGH_ALARM_BIT      8    // 气压上阈值报警
#define PRESS_LOW_ALARM_BIT       9    // 气压下阈值报警
#define PRESS_OFFLINE_BIT         10   // 气压检测离线
#define PRESS_RESERVED_BIT        11   // 保留

// 湿度报警位（12-15）
#define HUM_HIGH_ALARM_BIT        12   // 湿度上阈值报警
#define HUM_LOW_ALARM_BIT         13   // 湿度下阈值报警
#define HUM_OFFLINE_BIT           14   // 湿度检测离线
#define HUM_RESERVED_BIT          15   // 保留

// CO2 报警位（16-19）
#define CO2_HIGH_ALARM_BIT        16   // CO2 上阈值报警
#define CO2_LOW_ALARM_BIT         17   // CO2 下阈值报警
#define CO2_OFFLINE_BIT           18   // CO2 检测离线
#define CO2_RESERVED_BIT          19   // 保留

// PM2.5 报警位（20-23）
#define PM25_HIGH_ALARM_BIT       20   // PM2.5 上阈值报警
#define PM25_LOW_ALARM_BIT        21   // PM2.5 下阈值报警
#define PM25_OFFLINE_BIT          22   // PM2.5 检测离线
#define PM25_RESERVED_BIT         23   // 保留

// 声光报警位（24-31）
#define BEEP_DAMAGE_BIT           24   // 声报警损坏
#define BEEP_RESERVED_BIT         25   // 保留
#define BEEP_OFFLINE_BIT          26   // 声报警离线
#define BEEP_RESERVED2_BIT        27   // 保留
#define LIGHT_DAMAGE_BIT          28   // 光报警损坏
#define LIGHT_RESERVED_BIT        29   // 保留
#define LIGHT_OFFLINE_BIT         30   // 光报警离线
#define LIGHT_RESERVED2_BIT       31   // 保留

/* OTA Flash 分区定义（基于 IAP 代码） */
#define DOWNLOAD_FLASH_ADDR         (0x08100000UL)  /* Bank2 Download 区起始地址 */
#define DOWNLOAD_FLASH_SIZE         (0x000E0000UL)  /* Download 区大小：896KB */
#define OTA_FLAG_FLASH_ADDR         (0x081E0000UL)  /* Bank2 扇区 15 - OTA Flag 区 */

/* OTA 标志定义（与 IAP 代码一致） */
#define OTA_FLAG_MAGIC              (0x4F544155U)   /* 'OTAU' */
#define OTA_STATUS_IDLE             (0x00000000U)
#define OTA_STATUS_PENDING          (0x00000001U)   /* 待更新状态 */
#define OTA_STATUS_DONE             (0x00000002U)

/** 上位机停止 OTA 后，超过此时间无新数据则 MCU 自动回到 IDLE 并恢复心跳 */
#define OTA_SESSION_IDLE_MS         (15000U)

/* 5 分钟历史查询上传（寄存器 108/112，本文件私有） */
#define NET_5MIN_HIST_TXQ_RESERVE   2U   /* 发送队列预留空位，避免占满其它主动上传 */
#define NET_5MIN_HIST_DRAIN_MS      200U /* 队列排空后再等 ACK 在途，然后清 108～116 */

/* 写寄存器接收侧：分段长度与写满判定（应用层解析用） */
#define NET_REG_TIME_CFG_QREG         (4U)
#define NET_REG_TIME_CFG_WRITTEN_ALL  ((uint8_t)((1U << NET_REG_TIME_CFG_QREG) - 1U))
#define NET_HIST_TIME_QREG            (4U)
#define NET_HIST_TIME_WRITTEN_ALL     ((uint8_t)((1U << NET_HIST_TIME_QREG) - 1U))
#define NET_REG_THR_PAIR_CNT          (12U)
#define NET_REG_THR_REG_CNT           (NET_REG_THR_PAIR_CNT * 2U)
#define NET_REG_ALARM_BITEN_QREG        (2U)
#define NET_REG_ALARM_BITEN_WRITTEN_ALL ((uint8_t)((1U << NET_REG_ALARM_BITEN_QREG) - 1U))

#define NET_REG_REBOOT_CMD          (0x0001U)
#define NET_REG_CTRL2_SOUND_BIT     (0U)
#define NET_REG_CTRL2_LIGHT_BIT     (1U)
#define NET_REG_CTRL2_DISPLAY_BIT   (2U)

/* 协议寄存器剂量：uint16 低 14 位 = 数值*100，bit15:14 = 单位(00 uSv, 01 mSv, 10 Sv) */
#define NET_DOSE_UNIT_USV    (0x0000u)
#define NET_DOSE_UNIT_MSV    (0x4000u)
#define NET_DOSE_UNIT_SV     (0x8000u)


/* OTA 状态定义 */
typedef enum {
    OTA_STATE_IDLE    = 0,  // 空闲，未开始
    OTA_STATE_STARTED = 1,  // 已开始，接收中
    OTA_STATE_VERIFY  = 2,  // 接收完毕，校验中
    OTA_STATE_ERROR   = 3,  // 出错（CRC 不对/越界等）
    OTA_STATE_DONE    = 4   // 标记已写入，等待重启
} Ota_State_t;

/* 配置参数索引枚举（用于部分更新） */
typedef enum {
    CFG_IDX_RATE = 0,         // 辐射剂量率（剂量率阈值）
    CFG_IDX_ENV,              // 环境参数（温度、气压、湿度、CO2、PM2.5 阈值）
    CFG_IDX_ALARM_STATE,      // 报警状态（32 位标志）
    CFG_IDX_ALARM_EN,         // 报警使能
    CFG_IDX_DEVICE_ADDR,      // 设备参数（设备地址、报警音量、控制位 2）
    CFG_IDX_DEVICE_INFO,      // 设备信息（序列号、软件版本）
    CFG_IDX_MAX               // 配置索引总数（用于遍历所有配置）
} Config_Index_t;

/* 设备接口定义 */
typedef enum {
    INFT_NULL,
    INFT_TCP,
    INFT_CAN,
    INFT_LORA,
    INFT_NUM,
}Dev_Inft_t;

typedef enum
{
    NET_5MIN_HIST_IDLE = 0,
    NET_5MIN_HIST_UPLOADING,
    NET_5MIN_HIST_DRAINING,
} Net_5MinHistState_t;

typedef struct
{
    Net_5MinHistState_t state;
    Net_Device_t *dev;
    uint32_t ts_start;
    uint32_t ts_end;
    int16_t scan_logical;
    uint16_t queued;
    uint32_t drain_since;
} Net_5MinHistCtx_t;

typedef struct {
    uint8_t  time_cfg;    /* reg94～97，每位 1 寄存器 */
    uint8_t  hist_start;  /* reg108～111 */
    uint8_t  hist_end;    /* reg112～115 */
    uint32_t thr_regs;    /* reg50～73，每位 1 寄存器 */
    uint8_t  alarm_biten; /* reg82～83，每位 1 寄存器 */
} Net_RegWriteMask_t;

typedef struct {
    uint8_t crt;       // 当前传输接口
    bool tcp;          // TCP 接口连接
    bool can;          // CAN 接口连接
    bool lora;         // LORA 接口连接
}Dev_Inft_Struct;

/* OTA 标志结构体 */
typedef struct
{
    uint32_t magic;          /* 魔数：0x4F544155 */
    uint32_t app_size;       /* App 固件大小 */
    uint32_t app_crc32;      /* App 固件 CRC32 */
    uint32_t status;         /* 更新状态 */
    uint32_t reserved[4];    /* 保留字段 */
    uint32_t flag_crc;       /* 标志 CRC32 */
    uint32_t reserved2[7];
} OtaFlag_t;
//! ---------------------- ↑ 寄存器地址宏定义 ↑ ---------------------- !//


extern Dev_Inft_Struct tx_inft;


/* 与 net_raw_app.c 实现顺序一致 */
extern void Net_Resolve_Handle(Net_Device_t *dev, uint8_t fc, uint16_t reg, uint16_t len);

/* 应用层处理函数接口（用户自定义逻辑） */
extern void Net_App_HandleReadMulti(Net_Device_t *dev, uint16_t reg, uint16_t len);      // 读多寄存器 (0x03)
extern void Net_App_HandleReadSingle(Net_Device_t *dev, uint16_t reg, uint16_t len);     // 读单寄存器 (0x05)
extern void Net_App_HandleWriteSingle(Net_Device_t *dev, uint16_t reg, uint16_t len);    // 写单寄存器 (0x06)
extern void Net_App_HandleWriteMulti(Net_Device_t *dev, uint16_t reg, uint16_t len);     // 写多寄存器 (0x10)
extern void Net_App_HandleReadMultiResp(Net_Device_t *dev, uint16_t reg, uint16_t len);  // 读多应答 (0x13)
extern void Net_App_HandleReadSingleResp(Net_Device_t *dev, uint16_t reg, uint16_t len); // 读单应答 (0x15)
extern void Net_App_HandleActiveUploadMulti(Net_Device_t *dev, uint16_t reg, uint16_t len);  // 主动上传多 (0x23)
extern void Net_App_HandleActiveUploadSingle(Net_Device_t *dev, uint16_t reg, uint16_t len); // 主动上传单 (0x25)

extern void Net_Send_Err_Handle(uint16_t id, uint8_t *sdata, uint16_t size);

/* 用户自定义接口 */
extern void Net_User_Registers_Init(void);


/* OTA 相关函数声明 */
extern bool Ota_PrepareDownload(uint32_t file_size);  // OTA 下载准备（擦除 Flash）
extern void Ota_ProcessPacket(Net_Device_t *dev, uint16_t len);  // OTA 数据包处理
extern bool Ota_WriteData(uint32_t offset, uint8_t *data, uint32_t len);  // OTA 数据写入
extern bool Ota_VerifyFirmware(void);  // OTA 固件校验
extern bool Ota_FinishDownload(void);  // OTA 下载完成处理
extern void Ota_Thread_Task(void);  // OTA 后台任务（检查重启定时器）
extern void Ota_HandleFinishCommand(Net_Device_t *dev);  // 处理 OTA 结束指令
/** OTA 传输进行中（STARTED/VERIFY）时为 true，用于暂停传感器心跳等非 OTA 主动上传 */
extern bool Ota_IsHeartbeatPaused(void);


/* 从机主动上传接口（使用 Net_TxQueue_Push 实现） */
extern void Net_Active_Upload_Periodic(Net_Device_t *dev);  // 定期上传传感器数据 reg1-16
extern void Net_Active_Upload_Thresholds(Net_Device_t *dev);  // 定期上传阈值 reg50-73
extern void Net_Active_Upload_DeviceParams(Net_Device_t *dev);  // 定期上传报警使能/地址/音量/控制
extern void Net_Active_Upload_Scheduled(void);  // 周期任务统一入口（按当前链路定时发送）
extern void Net_Active_Upload_SerialNum(Net_Device_t *dev);  // 上传寄存器表序列号 reg86-93
extern bool Net_Active_Upload_SensorData(Net_Device_t *dev, uint8_t upload_type);
extern bool Net_Active_Upload_AlarmStatus(Net_Device_t *dev, uint32_t alarm_status);
extern bool Net_Active_Upload_DeviceStatus(Net_Device_t *dev, uint32_t device_status);
extern bool Net_Active_Upload_OtaStatus(Net_Device_t *dev, uint32_t state, uint32_t written_bytes);  // OTA 状态主动上传


/* 配置同步接口：将设备配置（阈值、开关等）写入保持寄存器 */
extern void Net_Config_Sync_To_Registers(Config_Index_t idx);  // 可指定更新单个配置
extern void Net_Config_Sync_All(void);  // 遍历所有配置索引，更新所有参数


/* 传感器数据同步接口：将实时传感器数据写入保持寄存器 */
extern void Net_Sync_SensorData_To_Registers(void);

/* 5 分钟记录：写入寄存器 30(时间戳)+34(5 分钟累计剂量) 并按当前链路主动上传 */
struct time_type__;
extern void Net_Sync_5MinRecord_To_Registers(float dose_uSv, const struct time_type__ *dt);
extern void Net_Active_Upload_5MinRecord_Dispatch(void);
/** 5 分钟历史按时间段上传：写 108/112 后在 Net_Thread_Task 之后周期调用 */
extern void Net_5MinHistory_Upload_Task(void);


extern void Net_TxInft_UpdateCrt(void);


#ifdef __cplusplus
}
#endif

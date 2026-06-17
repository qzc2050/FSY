#ifndef __DEVICE_CONFIG_H
#define __DEVICE_CONFIG_H

#include <stdint.h>
#include "../dev_protocol/net_raw/net_raw_config.h"

/* ==================== 产品配置宏定义 ==================== */
#define DEVICE_SOFTWARE_VERSION      "V1.0.0.20260601R"

/* 产品型号（根据实际硬件填写） */
#ifndef DEVICE_PRODUCT_MODEL
#define DEVICE_PRODUCT_MODEL        "FSY-I"    // 默认产品型号
#endif

/* 协议类型（Modbus RTU over TCP/UDP） */
#ifndef DEVICE_PROTOCOL_TYPE
#define DEVICE_PROTOCOL_TYPE        "Modbus-RTU"     // 默认协议类型
#endif

/* 协议地址（使用 net_raw 默认从机地址） */
#ifndef DEVICE_PROTOCOL_ADDR
#define DEVICE_PROTOCOL_ADDR        NET_RAW_SLAVE_ADDR_DEFAULT
#endif

/* 发现帧内控制/数据端口与 network_cmd.h 中 SETTING_SOCKET_PORT、DATA_UPLOAD_SOCKET_PORT 一致 */

/* 协议类型字段：文档示例为数字（如 2）；与 DEVICE_PROTOCOL_TYPE 字符串可并存 */
#ifndef DEVICE_PROTOCOL_TYPE_CODE
#define DEVICE_PROTOCOL_TYPE_CODE   (0U)
#endif
#ifndef DEVICE_UDP_DISCOVER_RESERVED
#define DEVICE_UDP_DISCOVER_RESERVED (0U)
#endif

#ifndef DEVICE_CFG_SN_LEN
#define DEVICE_CFG_SN_LEN 12
#endif
#ifndef DEVICE_CFG_HW_VER_LEN
#define DEVICE_CFG_HW_VER_LEN 16
#endif
#ifndef DEVICE_CFG_SW_VER_LEN
#define DEVICE_CFG_SW_VER_LEN 24
#endif

/* 串口说明用：配置存于外部 Flash 末尾配置区 */
#define DEVICE_CFG_STORAGE_LABEL "EXT_FLASH"

#ifndef DEVICE_CFG_DEFAULT_SN
#define DEVICE_CFG_DEFAULT_SN "SN9876543210"
#endif
#ifndef DEVICE_CFG_DEFAULT_HW
#define DEVICE_CFG_DEFAULT_HW "HW1314520168"
#endif


/* 辐射数据定时上传配置 */
// 定时上传周期（单位：毫秒），建议 5 秒 ~ 10 秒
#define NET_ACTIVE_UPLOAD_PERIOD_MS   (950)
#define GEIGER_SAVE_INTERVAL_5MIN_MS  (300000)  /* 5 分钟 */



/* ==================== 恢复出厂设置默认值 ==================== */
/* 剂量率阈值（单位：uSv/h） */
#ifndef DEVICE_CFG_DEFAULT_RATE_TH_RH
#define DEVICE_CFG_DEFAULT_RATE_TH_RH       (2.5f)    // 剂量率上限 1000.0 uSv/h = 1.00 mSv/h
#endif
#ifndef DEVICE_CFG_DEFAULT_RATE_TH_RL
#define DEVICE_CFG_DEFAULT_RATE_TH_RL       (0.0f)     // 剂量率下限 0 = 不启用
#endif

/* 温度阈值（单位：℃） */
#ifndef DEVICE_CFG_DEFAULT_TEMP_TH_HI
#define DEVICE_CFG_DEFAULT_TEMP_TH_HI       (40.0f)     // 温度上限 40.00 ℃
#endif
#ifndef DEVICE_CFG_DEFAULT_TEMP_TH_LO
#define DEVICE_CFG_DEFAULT_TEMP_TH_LO       (0.0f)      // 温度下限 0 = 不启用
#endif

/* 气压阈值（单位：hPa） */
#ifndef DEVICE_CFG_DEFAULT_PRESS_TH_HI
#define DEVICE_CFG_DEFAULT_PRESS_TH_HI      (1080.0f)    // 气压上限 1080.0 hPa
#endif
#ifndef DEVICE_CFG_DEFAULT_PRESS_TH_LO
#define DEVICE_CFG_DEFAULT_PRESS_TH_LO      (0.0f)       // 气压下限 0 = 不启用
#endif

/* 湿度阈值（单位：%RH） */
#ifndef DEVICE_CFG_DEFAULT_HUM_TH_HI
#define DEVICE_CFG_DEFAULT_HUM_TH_HI        (99.0f)      // 湿度上限 99.0 %RH
#endif
#ifndef DEVICE_CFG_DEFAULT_HUM_TH_LO
#define DEVICE_CFG_DEFAULT_HUM_TH_LO        (0.0f)       // 湿度下限 0 = 不启用
#endif

/* CO2 阈值（单位：ppm） */
#ifndef DEVICE_CFG_DEFAULT_CO2_TH_HI
#define DEVICE_CFG_DEFAULT_CO2_TH_HI        (1200U)      // CO2 上限 1200 ppm
#endif
#ifndef DEVICE_CFG_DEFAULT_CO2_TH_LO
#define DEVICE_CFG_DEFAULT_CO2_TH_LO        (0U)        // CO2 下限 0 = 不启用
#endif

/* PM2.5 阈值（单位：ug/m³） */
#ifndef DEVICE_CFG_DEFAULT_PM25_TH_HI
#define DEVICE_CFG_DEFAULT_PM25_TH_HI       (80U)        // PM2.5 上限 80 ug/m³
#endif
#ifndef DEVICE_CFG_DEFAULT_PM25_TH_LO
#define DEVICE_CFG_DEFAULT_PM25_TH_LO       (0U)        // PM2.5 下限 0 = 不启用
#endif

/* 报警和显示使能 */
#ifndef DEVICE_CFG_DEFAULT_ALARM_SOUND
#define DEVICE_CFG_DEFAULT_ALARM_SOUND      (1U)       // 声报警：开
#endif
#ifndef DEVICE_CFG_DEFAULT_ALARM_LIGHT
#define DEVICE_CFG_DEFAULT_ALARM_LIGHT      (1U)       // 光报警：开
#endif
#ifndef DEVICE_CFG_DEFAULT_ALARM_VOLUME
#define DEVICE_CFG_DEFAULT_ALARM_VOLUME     (80U)      // 报警音量：80%
#endif
#ifndef DEVICE_CFG_DEFAULT_DISPLAY
#define DEVICE_CFG_DEFAULT_DISPLAY          (1U)       // 屏幕：开
#endif
#ifndef DEVICE_CFG_DEFAULT_BRIGHT
#define DEVICE_CFG_DEFAULT_BRIGHT           (100U)     // 屏幕亮度：100%
#endif
#ifndef DEVICE_CFG_DEFAULT_DEV_ADDR
#define DEVICE_CFG_DEFAULT_DEV_ADDR         (1U)       // 设备地址：1
#endif
#ifndef DEVICE_CFG_DEFAULT_LANGUAGE
#define DEVICE_CFG_DEFAULT_LANGUAGE         (0U)       // 语言：0=中文, 1=English
#endif
#ifndef DEVICE_CFG_DEFAULT_SENS
#define DEVICE_CFG_DEFAULT_SENS (120.0f)
#endif

int DeviceConfig_Init(void);

uint8_t DeviceConfig_IsReady(void);

int DeviceConfig_PrintFromFile(void);

int DeviceConfig_SetSn(const char *sn);
int DeviceConfig_SetHwVer(const char *hw);
int DeviceConfig_SetSwVer(const char *sw);
int DeviceConfig_SetSensitivity(float sens);

/** 将当前 sys_cfg 全量写回 Flash（含阈值、开关、版本号等） */
int DeviceConfig_WriteFromSysCfg(void);

#endif

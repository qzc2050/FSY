#ifndef FSY_REGMAP_H

#define FSY_REGMAP_H



#include "aht20.h"

#include "bmp280.h"

#include "ens160.h"

#include "pm25.h"

#include <stdint.h>
#include <stdbool.h>



/* 实时区（0x23 / 部分 0x03） */

#define FSY_RT_REG_START          0x0001U

#define FSY_RT_REG_COUNT          11U

#define FSY_RT_REG_DATA_BYTES     (FSY_RT_REG_COUNT * 4U)

/** 0x23 第 8 项 / 0x03 reg15(0x000F)：设备状态，镜像 reg123 的 bit13/14 */
#define FSY_RT_REG_STATUS_BIT     15U

/** s_rt_regs[] 下标：与 0x23 从 reg1 起第 8 个 u32 对齐（协议亦称 reg15） */
#define FSY_RT_IDX_STATUS_BIT     7U



/* A 类配置（协议标准） */

/* 实时 5min 快照（只读）：reg30~33 时间 8B，reg34~35 D5 μSv×100 */
#define FSY_REG_DATA_TIME_5MIN   30U
#define FSY_REG_DATA_TIME_5MIN_REGS 4U
#define FSY_REG_DOSE_5MIN        34U
#define FSY_REG_DOSE_5MIN_REGS   2U
#define FSY_REG_5MIN_SNAPSHOT_REGS \
    (FSY_REG_DATA_TIME_5MIN_REGS + FSY_REG_DOSE_5MIN_REGS)

#define FSY_REG_DOSE_HI_TH       50U
#define FSY_REG_DOSE_LO_TH       52U
#define FSY_REG_DWORD_REGS       2U

#define FSY_REG_ALARM_ENABLE     82U
#define FSY_REG_ALARM_ENABLE_REGS 2U

#define FSY_ALARM_BIT_DOSE_HI    0U
#define FSY_ALARM_BIT_DOSE_LO    1U

#define FSY_REG_SERIALNUM         86U

#define FSY_REG_SERIALNUM_REGS    8U

#define FSY_REG_TIME              94U
#define FSY_REG_TIME_REGS         4U

#define FSY_REG_SOFTWARE_VERSION  98U
#define FSY_REG_SOFTWARE_VERSION_REGS 10U

#define FSY_REG_ADDRESS           121U
#define FSY_REG_ALARM_VOLUME      122U
#define FSY_REG_CONTROL_BIT2      123U
#define FSY_REG_CONTROL_BIT2_REGS 2U

/** reg123 u32：bit9 LoRa 协议输出，bit13 光报警，bit14 背光，bit15 外置报警在线（只读） */
#define FSY_CTRL2_BIT_LORA_POWER  9U
#define FSY_CTRL2_BIT_ALARM_LIGHT 13U
#define FSY_CTRL2_BIT_SCREEN      14U
#define FSY_CTRL2_BIT_EXT_ALARM   15U

/* C 类扩展 */

#define FSY_REG_PRODUCT_MODEL     130U

#define FSY_REG_PRODUCT_MODEL_REGS 8U

#define FSY_REG_PRODUCT_NAME      146U

#define FSY_REG_PRODUCT_NAME_REGS 8U

#define FSY_REG_STATIC_IP         138U
#define FSY_REG_STATIC_IP_REGS    2U

/* 盖革 / EWMA 算法（C 类，每项 uint32 占 2 reg，小端） */
#define FSY_REG_GEIGER_SENS           154U
#define FSY_REG_EWMA_THRESHOLD_CPS    156U
#define FSY_REG_EWMA_THRESHOLD_DELTA  158U
#define FSY_REG_EWMA_ALPHA_LOW        160U
#define FSY_REG_EWMA_ALPHA_HIGH       162U
#define FSY_REG_EWMA_BOOST_DURATION   164U
#define FSY_REG_RATE_LIMIT            166U
#define FSY_REG_GEIGER_BACKGROUND_CPM 168U
#define FSY_REG_GEIGER_DEAD_TIME_US   171U
#define FSY_REG_GEIGER_PARAM_REGS     2U

#define FSY_REG_DHCP_ENABLE       170U

#define FSY_REG_LANGUAGE          174U

#define FSY_REG_HW_VERSION        180U
#define FSY_REG_HW_VERSION_REGS   8U

/* 调试：上一秒盖革原始计数 CPS（只读，uint32 占 2 reg，不落 Flash） */
#define FSY_REG_GEIGER_SEC_CPS    190U
#define FSY_REG_GEIGER_SEC_CPS_REGS 2U

/* 当前在线 IP（只读，W5500 SIPR，2 reg IPv4，不落 Flash） */
#define FSY_REG_CURRENT_IP        192U
#define FSY_REG_CURRENT_IP_REGS   2U

void Fsy_Regmap_Init(void);



int Fsy_Regmap_ReadU32(uint16_t reg_addr, uint32_t *value);



int Fsy_Regmap_ReadBlock(uint16_t start_reg, uint16_t reg_count,

                         uint8_t *out, uint16_t out_cap);



int Fsy_Regmap_WriteBlock(uint16_t start_reg, const uint8_t *data,

                          uint16_t byte_count);



int Fsy_Regmap_BuildRtPayload(uint8_t *out, uint16_t out_cap);



void Fsy_Regmap_UpdateEnv(const AHT20_Data_t *aht, const BMP280_Data_t *bmp,

                          const ENS160_Data_t *ens, const PM25_Data_t *pm25);



void Fsy_Regmap_UpdateDoseRate(float rate_usv_h);

/** 更新最近一次 5min 快照（reg30~35）；dt 为协议 8 字节时间，dose_x100=D5×100 */
void Fsy_Regmap_Sync5MinSnapshot(const uint8_t dt8[8], uint32_t dose_x100);

void Fsy_Regmap_SetGeigerSecCps(uint32_t cps);

void Fsy_Regmap_ApplyAlarmEnable(uint32_t enable_mask);

/** 仅更新剂量报警 bit0/1，保留环境传感器离线等其它位 */
void Fsy_Regmap_PatchDoseAlarmBit(uint8_t bit_pos, bool is_alarm);

void Fsy_Regmap_SyncAlarmStatus(uint32_t alarm_status);

/** 将 reg123 bit13/14 同步到 0x23 reg15（s_rt_regs[7]） */
void Fsy_Regmap_SyncStatusBitFromCtrl(void);

uint32_t Fsy_Regmap_GetAlarmStatus(void);

#endif


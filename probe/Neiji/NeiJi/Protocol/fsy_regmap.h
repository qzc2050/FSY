#ifndef FSY_REGMAP_H

#define FSY_REGMAP_H



#include "aht20.h"

#include "bmp280.h"

#include "ens160.h"

#include "pm25.h"

#include <stdint.h>



/* 实时区（0x23 / 部分 0x03） */

#define FSY_RT_REG_START          0x0001U

#define FSY_RT_REG_COUNT          11U

#define FSY_RT_REG_DATA_BYTES     (FSY_RT_REG_COUNT * 4U)



/* A 类配置（协议标准） */

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
#define FSY_REG_GEIGER_PARAM_REGS     2U

#define FSY_REG_DHCP_ENABLE       170U

#define FSY_REG_LANGUAGE          174U

#define FSY_REG_HW_VERSION        180U
#define FSY_REG_HW_VERSION_REGS   8U

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

void Fsy_Regmap_ApplyAlarmEnable(uint32_t enable_mask);

void Fsy_Regmap_SyncAlarmStatus(uint32_t alarm_status);

uint32_t Fsy_Regmap_GetAlarmStatus(void);

#endif


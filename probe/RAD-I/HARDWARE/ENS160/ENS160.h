#ifndef __ENS160_H_
#define __ENS160_H_

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

// 用户需要实现的I2C函数
typedef struct {
    int (*i2c_write)(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len);
    int (*i2c_read)(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len);
    void (*delay_ms)(uint32_t ms);
    void (*debug_print)(const char *msg);
} ENS160_I2C_Funcs;

// Chip constants
#define ENS160_PARTID             0x0160
#define ENS161_PARTID             0x0161
#define ENS160_BOOTING            10

// 7-bit I2C slave address of the ENS160
#define ENS160_I2CADDR_0          0x52        //ADDR low
#define ENS160_I2CADDR_1          0x53        //ADDR high

// ENS160 registers for version V0
#define ENS160_REG_PART_ID        0x00        // 2 byte register
#define ENS160_REG_OPMODE         0x10
#define ENS160_REG_CONFIG         0x11
#define ENS160_REG_COMMAND        0x12
#define ENS160_REG_TEMP_IN        0x13
#define ENS160_REG_RH_IN          0x15
#define ENS160_REG_DATA_STATUS    0x20
#define ENS160_REG_DATA_AQI       0x21
#define ENS160_REG_DATA_TVOC      0x22
#define ENS160_REG_DATA_ECO2      0x24            
#define ENS160_REG_DATA_BL        0x28
#define ENS160_REG_DATA_T         0x30
#define ENS160_REG_DATA_RH        0x32
#define ENS160_REG_DATA_MISR      0x38
#define ENS160_REG_GPR_WRITE_0    0x40
#define ENS160_REG_GPR_WRITE_1    (ENS160_REG_GPR_WRITE_0 + 1)
#define ENS160_REG_GPR_WRITE_2    (ENS160_REG_GPR_WRITE_0 + 2)
#define ENS160_REG_GPR_WRITE_3    (ENS160_REG_GPR_WRITE_0 + 3)
#define ENS160_REG_GPR_WRITE_4    (ENS160_REG_GPR_WRITE_0 + 4)
#define ENS160_REG_GPR_WRITE_5    (ENS160_REG_GPR_WRITE_0 + 5)
#define ENS160_REG_GPR_WRITE_6    (ENS160_REG_GPR_WRITE_0 + 6)
#define ENS160_REG_GPR_WRITE_7    (ENS160_REG_GPR_WRITE_0 + 7)
#define ENS160_REG_GPR_READ_0     0x48
#define ENS160_REG_GPR_READ_4     (ENS160_REG_GPR_READ_0 + 4)
#define ENS160_REG_GPR_READ_6     (ENS160_REG_GPR_READ_0 + 6)
#define ENS160_REG_GPR_READ_7     (ENS160_REG_GPR_READ_0 + 7)

// ENS160 data register fields
#define ENS160_COMMAND_NOP        0x00
#define ENS160_COMMAND_CLRGPR     0xCC
#define ENS160_COMMAND_GET_APPVER 0x0E 
#define ENS160_COMMAND_SETTH      0x02
#define ENS160_COMMAND_SETSEQ     0xC2

#define ENS160_OPMODE_RESET       0xF0
#define ENS160_OPMODE_DEP_SLEEP   0x00
#define ENS160_OPMODE_IDLE        0x01
#define ENS160_OPMODE_STD         0x02
#define ENS160_OPMODE_LP          0x03    
#define ENS160_OPMODE_CUSTOM      0xC0

#define ENS160_SEQ_ACK_NOTCOMPLETE 0x80
#define ENS160_SEQ_ACK_COMPLETE    0xC0

#define IS_ENS160_SEQ_ACK_NOT_COMPLETE(x) (ENS160_SEQ_ACK_NOTCOMPLETE == (ENS160_SEQ_ACK_NOTCOMPLETE & (x)))
#define IS_ENS160_SEQ_ACK_COMPLETE(x)     (ENS160_SEQ_ACK_COMPLETE == (ENS160_SEQ_ACK_COMPLETE & (x)))

#define ENS160_DATA_STATUS_NEWDAT 0x02
#define ENS160_DATA_STATUS_NEWGPR 0x01

#define ENS160_DATA_WAIT_MS       200U

#define IS_NEWDAT(x)            (ENS160_DATA_STATUS_NEWDAT == (ENS160_DATA_STATUS_NEWDAT & (x)))
#define IS_NEWGPR(x)            (ENS160_DATA_STATUS_NEWGPR == (ENS160_DATA_STATUS_NEWGPR & (x)))
#define IS_NEW_DATA_AVAILABLE(x) (0 != ((ENS160_DATA_STATUS_NEWDAT | ENS160_DATA_STATUS_NEWGPR ) & (x)))

#define CONVERT_RS_RAW2OHMS_I(x) (1 << ((x) >> 11))
#define CONVERT_RS_RAW2OHMS_F(x) (powf(2.0f, (float)(x) / 2048.0f))

typedef struct {
    ENS160_I2C_Funcs i2c_funcs;
    uint8_t slave_addr;
    bool available;
    uint8_t rev_ens16x;
    
    uint8_t fw_ver_major;
    uint8_t fw_ver_minor;
    uint8_t fw_ver_build;
    
    uint16_t step_count;
    
    // Measurement data
    uint8_t data_aqi;
    uint16_t data_tvoc;
    uint16_t data_eco2;
    uint16_t data_aqi500;
    uint32_t hp0_rs;
    uint32_t hp0_bl;
    uint32_t hp1_rs;
    uint32_t hp1_bl;
    uint32_t hp2_rs;
    uint32_t hp2_bl;
    uint32_t hp3_rs;
    uint32_t hp3_bl;
    uint8_t misr;
    
    bool debug_enabled;
} ENS160_HandleTypeDef;


extern ENS160_HandleTypeDef ens160;
extern ENS160_I2C_Funcs i2c_funcs;


// 初始化函数
int ens160_i2c_write(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len);
int ens160_i2c_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len);
void ens160_delay_ms(uint32_t ms);
void ens160_debug_print(const char *msg);
bool ENS160_Init(ENS160_HandleTypeDef *hens160, uint8_t slave_addr, ENS160_I2C_Funcs *i2c_funcs, bool debug);
void ENS160_Setting(void);

// 基本操作函数
bool ENS160_Reset(ENS160_HandleTypeDef *hens160);
bool ENS160_SetMode(ENS160_HandleTypeDef *hens160, uint8_t mode);
bool ENS160_Measure(ENS160_HandleTypeDef *hens160, bool wait_for_new);
bool ENS160_MeasureRaw(ENS160_HandleTypeDef *hens160, bool wait_for_new);
bool ENS160_SetEnvData(ENS160_HandleTypeDef *hens160, float temperature, float humidity);
bool ENS160_SetEnvData210(ENS160_HandleTypeDef *hens160, uint16_t temp_raw, uint16_t rh_raw);

// 自定义模式函数
bool ENS160_InitCustomMode(ENS160_HandleTypeDef *hens160, uint16_t step_num);
bool ENS160_AddCustomStep(ENS160_HandleTypeDef *hens160, uint16_t time, 
                          bool measure_hp0, bool measure_hp1, bool measure_hp2, bool measure_hp3,
                          uint16_t temp_hp0, uint16_t temp_hp1, uint16_t temp_hp2, uint16_t temp_hp3);

// 数据获取函数
uint8_t ENS160_GetAQI(ENS160_HandleTypeDef *hens160);
uint16_t ENS160_GetTVOC(ENS160_HandleTypeDef *hens160);
uint16_t ENS160_GetECO2(ENS160_HandleTypeDef *hens160);
uint16_t ENS160_GetAQI500(ENS160_HandleTypeDef *hens160);
uint32_t ENS160_GetHP0Resistance(ENS160_HandleTypeDef *hens160);
uint32_t ENS160_GetHP1Resistance(ENS160_HandleTypeDef *hens160);
uint32_t ENS160_GetHP2Resistance(ENS160_HandleTypeDef *hens160);
uint32_t ENS160_GetHP3Resistance(ENS160_HandleTypeDef *hens160);
uint32_t ENS160_GetHP0Baseline(ENS160_HandleTypeDef *hens160);
uint32_t ENS160_GetHP1Baseline(ENS160_HandleTypeDef *hens160);
uint32_t ENS160_GetHP2Baseline(ENS160_HandleTypeDef *hens160);
uint32_t ENS160_GetHP3Baseline(ENS160_HandleTypeDef *hens160);
uint8_t ENS160_GetMISR(ENS160_HandleTypeDef *hens160);
uint8_t ENS160_GetRevision(ENS160_HandleTypeDef *hens160);
bool ENS160_IsAvailable(ENS160_HandleTypeDef *hens160);

void print_ens160_data(ENS160_HandleTypeDef *ens160);
void print_raw_data(ENS160_HandleTypeDef *ens160);
void evaluate_air_quality(ENS160_HandleTypeDef *ens160);
void check_sensor_status(ENS160_HandleTypeDef *ens160);
void ENS160_Measure_Task(void);

#endif /* __ENS160_H_ */
                          
          




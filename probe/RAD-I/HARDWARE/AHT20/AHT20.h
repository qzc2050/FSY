#ifndef __AHT20_H
#define __AHT20_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

// AHT20 设备地址
#define AHT20_I2C_ADDR              (0x38 << 1)  // 7位地址左移1位

// AHT20 命令定义
#define AHT20_CMD_INIT              0xE1
#define AHT20_CMD_MEASURE           0xAC
#define AHT20_CMD_RESET             0xBA
#define AHT20_CMD_SOFTRESET         0xBE

// AHT20 状态位
#define AHT20_STATUS_BUSY           0x80
#define AHT20_STATUS_CALIBRATED     0x08

// 函数声明
bool AHT20_Reset(void);
void AHT20_Init(void);
bool AHT20_StartMeasurement(void);
bool AHT20_ReadData(uint32_t *humidity, uint32_t *temperature);
bool AHT20_ReadRawData(uint8_t *data);
uint8_t AHT20_CheckCalibration(void);
void AHT20_ReadOnce(uint32_t *humidity, uint32_t *temperature);









#endif






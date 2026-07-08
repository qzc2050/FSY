#include "bmp280.h"
#include "i2c.h"
#include "main.h"
#include "sensor_common.h"
#include "cmsis_os.h"

#define BMP280_I2C_ADDR  (0x76U << 1)

#define BMP280_REG_ID          0xD0U
#define BMP280_REG_RESET       0xE0U
#define BMP280_REG_CTRL_MEAS   0xF4U
#define BMP280_REG_CONFIG      0xF5U
#define BMP280_REG_PRESS_MSB   0xF7U

static BMP280_Data_t s_bmp280;
static int32_t t_fine;
static uint16_t dig_T1;
static int16_t dig_T2;
static int16_t dig_T3;
static uint16_t dig_P1;
static int16_t dig_P2;
static int16_t dig_P3;
static int16_t dig_P4;
static int16_t dig_P5;
static int16_t dig_P6;
static int16_t dig_P7;
static int16_t dig_P8;
static int16_t dig_P9;

static HAL_StatusTypeDef BMP280_ReadRegs(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return I2C4_Mem_Read(BMP280_I2C_ADDR, reg, buf, len, 50U);
}

static HAL_StatusTypeDef BMP280_WriteReg(uint8_t reg, uint8_t value)
{
    return I2C4_Mem_Write(BMP280_I2C_ADDR, reg, &value, 1U, 50U);
}

static void BMP280_ReadCalibration(void)
{
    uint8_t buf[24];

    if (BMP280_ReadRegs(0x88U, buf, sizeof(buf)) != HAL_OK) {
        return;
    }

    dig_T1 = (uint16_t)((uint16_t)buf[1] << 8 | (uint16_t)buf[0]);
    dig_T2 = (int16_t)((int16_t)buf[3] << 8 | (int16_t)buf[2]);
    dig_T3 = (int16_t)((int16_t)buf[5] << 8 | (int16_t)buf[4]);
    dig_P1 = (uint16_t)((uint16_t)buf[7] << 8 | (uint16_t)buf[6]);
    dig_P2 = (int16_t)((int16_t)buf[9] << 8 | (int16_t)buf[8]);
    dig_P3 = (int16_t)((int16_t)buf[11] << 8 | (int16_t)buf[10]);
    dig_P4 = (int16_t)((int16_t)buf[13] << 8 | (int16_t)buf[12]);
    dig_P5 = (int16_t)((int16_t)buf[15] << 8 | (int16_t)buf[14]);
    dig_P6 = (int16_t)((int16_t)buf[17] << 8 | (int16_t)buf[16]);
    dig_P7 = (int16_t)((int16_t)buf[19] << 8 | (int16_t)buf[18]);
    dig_P8 = (int16_t)((int16_t)buf[21] << 8 | (int16_t)buf[20]);
    dig_P9 = (int16_t)((int16_t)buf[23] << 8 | (int16_t)buf[22]);
}

void BMP280_Init(void)
{
    uint8_t id;

    s_bmp280.temperature_c = 0.0f;
    s_bmp280.pressure_pa = 0.0f;
    s_bmp280.online = 0U;
    s_bmp280.last_update_tick = 0U;

    if (BMP280_ReadRegs(BMP280_REG_ID, &id, 1U) != HAL_OK) {
        return;
    }

    (void)BMP280_WriteReg(BMP280_REG_RESET, 0xB6U);
    osDelay(10U);
    BMP280_ReadCalibration();
    (void)BMP280_WriteReg(BMP280_REG_CTRL_MEAS, 0x27U);
    (void)BMP280_WriteReg(BMP280_REG_CONFIG, 0xA0U);
}

void BMP280_Update(void)
{
    uint8_t buf[6];
    int32_t adc_T;
    int32_t adc_P;
    int32_t var1;
    int32_t var2;
    int32_t t;
    uint32_t p;

    if (BMP280_ReadRegs(BMP280_REG_PRESS_MSB, buf, sizeof(buf)) != HAL_OK) {
        return;
    }

    adc_P = (int32_t)(((uint32_t)buf[0] << 12) | ((uint32_t)buf[1] << 4) | ((uint32_t)buf[2] >> 4));
    adc_T = (int32_t)(((uint32_t)buf[3] << 12) | ((uint32_t)buf[4] << 4) | ((uint32_t)buf[5] >> 4));

    var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) * ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12) *
            ((int32_t)dig_T3)) >> 14;
    t_fine = var1 + var2;
    t = (t_fine * 5 + 128) >> 8;
    s_bmp280.temperature_c = (float)t / 100.0f;

    var1 = (((int32_t)t_fine) >> 1) - (int32_t)64000;
    var2 = (((var1 >> 2) * (var1 >> 2)) >> 11) * ((int32_t)dig_P6);
    var2 = var2 + ((var1 * (int32_t)dig_P5) << 1);
    var2 = (var2 >> 2) + ((int32_t)dig_P4 << 16);
    var1 = (((dig_P3 * (((var1 >> 2) * (var1 >> 2)) >> 13)) >> 3) +
            ((((int32_t)dig_P2) * var1) >> 1)) >> 18;
    var1 = ((((32768 + var1)) * ((int32_t)dig_P1)) >> 15);
    if (var1 == 0) {
        return;
    }

    p = (uint32_t)(((uint32_t)1048576 - (uint32_t)adc_P) - (uint32_t)(var2 >> 12));
    p = (uint32_t)(((p * 3125U) / (uint32_t)var1) << 1);
    var1 = (((int32_t)dig_P9) * ((int32_t)(((p >> 3) * (p >> 3)) >> 13))) >> 12;
    var2 = (((int32_t)(p >> 2)) * (int32_t)dig_P8) >> 13;
    p = (uint32_t)((int32_t)p + ((var1 + var2 + dig_P7) >> 4));

    s_bmp280.pressure_pa = (float)p;
    s_bmp280.last_update_tick = HAL_GetTick();
    s_bmp280.online = 1U;
}

void BMP280_GetData(BMP280_Data_t *out)
{
    if (out == NULL) {
        return;
    }

    *out = s_bmp280;
    if (sensor_tick_is_stale(s_bmp280.last_update_tick, SENSOR_OFFLINE_MS)) {
        out->online = 0U;
    }
}

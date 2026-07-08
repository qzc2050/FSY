#include "ens160.h"
#include "i2c.h"
#include "main.h"
#include "sensor_common.h"
#include "cmsis_os.h"

#define ENS160_I2C_ADDR            (0x52U << 1)
#define ENS160_REG_PART_ID         0x00U
#define ENS160_REG_OPMODE          0x10U
#define ENS160_REG_TEMP_IN         0x13U
#define ENS160_REG_RH_IN           0x15U
#define ENS160_REG_DEVICE_STATUS   0x20U
#define ENS160_REG_DATA_TVOC       0x22U
#define ENS160_REG_DATA_ECO2       0x24U
#define ENS160_OPMODE_DEEP_SLEEP   0x00U
#define ENS160_OPMODE_IDLE         0x01U
#define ENS160_OPMODE_STANDARD     0x02U
#define ENS160_STATUS_STATER       0x40U
#define ENS160_STATUS_VALID_MASK   0x0CU
#define ENS160_STATUS_VALID_BAD    0x0CU

static ENS160_Data_t s_ens160;
static uint8_t s_init_ok;

static HAL_StatusTypeDef ENS160_WriteReg(uint8_t reg, uint8_t value)
{
    return I2C4_Mem_Write(ENS160_I2C_ADDR, reg, &value, 1U, 50U);
}

static HAL_StatusTypeDef ENS160_WriteReg16Le(uint8_t reg, uint16_t value)
{
    uint8_t buf[2];

    buf[0] = (uint8_t)(value & 0xFFU);
    buf[1] = (uint8_t)(value >> 8);
    return I2C4_Mem_Write(ENS160_I2C_ADDR, reg, buf, 2U, 50U);
}

static HAL_StatusTypeDef ENS160_ReadRegs(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return I2C4_Mem_Read(ENS160_I2C_ADDR, reg, buf, len, 50U);
}

static uint16_t ENS160_ReadU16Le(uint8_t reg)
{
    uint8_t buf[2];

    if (ENS160_ReadRegs(reg, buf, sizeof(buf)) != HAL_OK) {
        return 0U;
    }
    return (uint16_t)(((uint16_t)buf[1] << 8) | (uint16_t)buf[0]);
}

void ENS160_Init(void)
{
    uint8_t id[2];
    uint8_t opmode;

    s_init_ok = 0U;
    s_ens160.eco2 = 0U;
    s_ens160.tvoc = 0U;
    s_ens160.online = 0U;
    s_ens160.device_status = 0U;
    s_ens160.warmup_phase = 0U;
    s_ens160.last_update_tick = 0U;

    if (ENS160_ReadRegs(ENS160_REG_PART_ID, id, sizeof(id)) != HAL_OK) {
        return;
    }
    if ((id[0] != 0x60U) || (id[1] != 0x01U)) {
        return;
    }
    if (ENS160_ReadRegs(ENS160_REG_OPMODE, &opmode, 1U) != HAL_OK) {
        return;
    }
    if (opmode == ENS160_OPMODE_STANDARD) {
        s_init_ok = 1U;
        return;
    }
    if (opmode == ENS160_OPMODE_DEEP_SLEEP) {
        (void)ENS160_WriteReg(ENS160_REG_OPMODE, ENS160_OPMODE_IDLE);
        osDelay(20U);
    }
    if (opmode != ENS160_OPMODE_IDLE) {
        (void)ENS160_WriteReg(ENS160_REG_OPMODE, ENS160_OPMODE_IDLE);
        osDelay(20U);
    }
    (void)ENS160_WriteReg(ENS160_REG_OPMODE, ENS160_OPMODE_STANDARD);
    osDelay(100U);
    s_init_ok = 1U;
}

void ENS160_SetCompensation(float temp_c, float rh_percent)
{
    uint16_t temp_in;
    uint16_t rh_in;
    float kelvin;
    float rh_clamped;

    if (s_init_ok == 0U) {
        return;
    }

    kelvin = temp_c + 273.15f;
    if (kelvin < 0.0f) {
        kelvin = 0.0f;
    }
    rh_clamped = rh_percent;
    if (rh_clamped < 0.0f) {
        rh_clamped = 0.0f;
    }
    if (rh_clamped > 100.0f) {
        rh_clamped = 100.0f;
    }

    temp_in = (uint16_t)(kelvin * 64.0f);
    rh_in = (uint16_t)(rh_clamped * 512.0f);
    (void)ENS160_WriteReg16Le(ENS160_REG_TEMP_IN, temp_in);
    (void)ENS160_WriteReg16Le(ENS160_REG_RH_IN, rh_in);
}

void ENS160_Update(void)
{
    uint8_t status;
    uint16_t tvoc;
    uint16_t eco2;

    if (s_init_ok == 0U) {
        return;
    }
    if (ENS160_ReadRegs(ENS160_REG_DEVICE_STATUS, &status, 1U) != HAL_OK) {
        return;
    }

    s_ens160.device_status = status;
    s_ens160.warmup_phase = (uint8_t)((status & ENS160_STATUS_VALID_MASK) >> 2);
    if ((status & ENS160_STATUS_STATER) != 0U) {
        s_ens160.last_update_tick = HAL_GetTick();
        s_ens160.online = 1U;
        return;
    }
    if ((status & ENS160_STATUS_VALID_MASK) == ENS160_STATUS_VALID_BAD) {
        s_ens160.last_update_tick = HAL_GetTick();
        s_ens160.online = 1U;
        return;
    }

    tvoc = ENS160_ReadU16Le(ENS160_REG_DATA_TVOC);
    eco2 = ENS160_ReadU16Le(ENS160_REG_DATA_ECO2);
    s_ens160.tvoc = tvoc;
    s_ens160.eco2 = eco2;
    s_ens160.last_update_tick = HAL_GetTick();
    s_ens160.online = 1U;
}

void ENS160_GetData(ENS160_Data_t *out)
{
    if (out == NULL) {
        return;
    }

    *out = s_ens160;
    if (s_init_ok == 0U) {
        out->online = 0U;
        return;
    }
    if (sensor_tick_is_stale(s_ens160.last_update_tick, SENSOR_OFFLINE_MS)) {
        out->online = 0U;
    }
}

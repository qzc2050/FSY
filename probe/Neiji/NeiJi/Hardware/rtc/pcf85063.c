#include "pcf85063.h"
#include "i2c.h"
#include "main.h"

#define PCF85063_I2C_ADDR  (0x51U << 1)

static int Pcf85063_WriteReg(uint8_t reg, uint8_t value)
{
    if (HAL_I2C_Mem_Write(&hi2c1, PCF85063_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                          &value, 1U, 50U) != HAL_OK) {
        return -1;
    }
    return 0;
}

static uint8_t Pcf85063_ReadReg(uint8_t reg)
{
    uint8_t value = 0U;

    if (HAL_I2C_Mem_Read(&hi2c1, PCF85063_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                         &value, 1U, 50U) != HAL_OK) {
        return 0xFFU;
    }
    return value;
}

static uint8_t bcd_to_dec(uint8_t bcd)
{
    return (uint8_t)(((bcd >> 4) * 10U) + (bcd & 0x0FU));
}

static uint8_t dec_to_bcd(uint8_t dec)
{
    return (uint8_t)(((dec / 10U) << 4) | (dec % 10U));
}

static int dt_valid(const Pcf85063_DateTime_t *dt)
{
    if (dt == NULL) {
        return 0;
    }
    if ((dt->year > 99U) || (dt->month == 0U) || (dt->month > 12U) ||
        (dt->day == 0U) || (dt->day > 31U) ||
        (dt->hour > 23U) || (dt->minute > 59U) || (dt->second > 59U)) {
        return 0;
    }
    return 1;
}

void Pcf85063_Init(void)
{
    uint8_t ctrl1 = Pcf85063_ReadReg(0x00U);

    if (ctrl1 != 0xFFU) {
        (void)Pcf85063_WriteReg(0x00U, (uint8_t)(ctrl1 & (uint8_t)~0x20U));
    }
}

int Pcf85063_GetTime(Pcf85063_DateTime_t *out)
{
    if (out == NULL) {
        return -1;
    }

    out->second = bcd_to_dec((uint8_t)(Pcf85063_ReadReg(0x04U) & 0x7FU));
    out->minute = bcd_to_dec((uint8_t)(Pcf85063_ReadReg(0x05U) & 0x7FU));
    out->hour = bcd_to_dec((uint8_t)(Pcf85063_ReadReg(0x06U) & 0x3FU));
    out->day = bcd_to_dec((uint8_t)(Pcf85063_ReadReg(0x07U) & 0x3FU));
    out->week = (uint8_t)(Pcf85063_ReadReg(0x08U) & 0x07U);
    out->month = bcd_to_dec((uint8_t)(Pcf85063_ReadReg(0x09U) & 0x1FU));
    out->year = bcd_to_dec(Pcf85063_ReadReg(0x0AU));
    out->online = (out->second <= 59U) ? 1U : 0U;
    return 0;
}

int Pcf85063_SetTime(const Pcf85063_DateTime_t *dt)
{
    uint8_t regs[7];
    uint8_t ctrl1;

    if (!dt_valid(dt)) {
        return -1;
    }

    regs[0] = dec_to_bcd(dt->second);
    regs[1] = dec_to_bcd(dt->minute);
    regs[2] = dec_to_bcd(dt->hour);
    regs[3] = dec_to_bcd(dt->day);
    regs[4] = (uint8_t)(dt->week & 0x07U);
    regs[5] = dec_to_bcd(dt->month);
    regs[6] = dec_to_bcd((uint8_t)(dt->year % 100U));

    ctrl1 = Pcf85063_ReadReg(0x00U);
    if (ctrl1 != 0xFFU) {
        (void)Pcf85063_WriteReg(0x00U, (uint8_t)(ctrl1 | 0x20U));
    }

    if (HAL_I2C_Mem_Write(&hi2c1, PCF85063_I2C_ADDR, 0x04U, I2C_MEMADD_SIZE_8BIT,
                          regs, (uint16_t)sizeof(regs), 100U) != HAL_OK) {
        if (ctrl1 != 0xFFU) {
            (void)Pcf85063_WriteReg(0x00U, (uint8_t)(ctrl1 & (uint8_t)~0x20U));
        }
        return -1;
    }

    if (ctrl1 != 0xFFU) {
        (void)Pcf85063_WriteReg(0x00U, (uint8_t)(ctrl1 & (uint8_t)~0x20U));
    }
    return 0;
}

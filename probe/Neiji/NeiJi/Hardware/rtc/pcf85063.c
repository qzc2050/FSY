#include "pcf85063.h"
#include "i2c.h"
#include "main.h"
#include "cmsis_os.h"

#define PCF85063_I2C_ADDR  (0x51U << 1)

static osMutexId_t s_rtc_mutex;
static const osMutexAttr_t s_rtc_mutex_attr = {
    .name = "rtcMutex",
};

static void rtc_lock(void)
{
    if (s_rtc_mutex != NULL) {
        (void)osMutexAcquire(s_rtc_mutex, osWaitForever);
    }
}

static void rtc_unlock(void)
{
    if (s_rtc_mutex != NULL) {
        (void)osMutexRelease(s_rtc_mutex);
    }
}

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

static int bcd_is_valid(uint8_t bcd)
{
    return (((bcd & 0x0FU) <= 9U) && (((bcd >> 4) & 0x0FU) <= 9U)) ? 1 : 0;
}

static uint8_t bcd_to_dec(uint8_t bcd)
{
    return (uint8_t)(((bcd >> 4) * 10U) + (bcd & 0x0FU));
}

static uint8_t dec_to_bcd(uint8_t dec)
{
    return (uint8_t)(((dec / 10U) << 4) | (dec % 10U));
}

static void pcf85063_clear_stop(void)
{
    uint8_t ctrl1 = Pcf85063_ReadReg(0x00U);

    if ((ctrl1 != 0xFFU) && ((ctrl1 & 0x20U) != 0U)) {
        (void)Pcf85063_WriteReg(0x00U, (uint8_t)(ctrl1 & (uint8_t)~0x20U));
    }
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

static int regs_to_dt(const uint8_t regs[7], Pcf85063_DateTime_t *out)
{
    uint8_t raw;

    if ((out == NULL) || (regs == NULL)) {
        return -1;
    }

    raw = (uint8_t)(regs[0] & 0x7FU);
    if (!bcd_is_valid(raw)) {
        return -1;
    }
    out->second = bcd_to_dec(raw);

    raw = (uint8_t)(regs[1] & 0x7FU);
    if (!bcd_is_valid(raw)) {
        return -1;
    }
    out->minute = bcd_to_dec(raw);

    raw = (uint8_t)(regs[2] & 0x3FU);
    if (!bcd_is_valid(raw)) {
        return -1;
    }
    out->hour = bcd_to_dec(raw);

    raw = (uint8_t)(regs[3] & 0x3FU);
    if (!bcd_is_valid(raw)) {
        return -1;
    }
    out->day = bcd_to_dec(raw);

    out->week = (uint8_t)(regs[4] & 0x07U);

    raw = (uint8_t)(regs[5] & 0x1FU);
    if (!bcd_is_valid(raw)) {
        return -1;
    }
    out->month = bcd_to_dec(raw);

    if (!bcd_is_valid(regs[6])) {
        return -1;
    }
    out->year = bcd_to_dec(regs[6]);
    out->online = 1U;

    return dt_valid(out) ? 0 : -1;
}

void Pcf85063_Init(void)
{
    if (s_rtc_mutex == NULL) {
        s_rtc_mutex = osMutexNew(&s_rtc_mutex_attr);
    }

    rtc_lock();
    pcf85063_clear_stop();
    rtc_unlock();
}

int Pcf85063_GetTime(Pcf85063_DateTime_t *out)
{
    uint8_t regs[7];

    if (out == NULL) {
        return -1;
    }

    rtc_lock();
    if (HAL_I2C_Mem_Read(&hi2c1, PCF85063_I2C_ADDR, 0x04U, I2C_MEMADD_SIZE_8BIT,
                         regs, (uint16_t)sizeof(regs), 100U) != HAL_OK) {
        rtc_unlock();
        return -1;
    }
    if (regs_to_dt(regs, out) != 0) {
        rtc_unlock();
        return -1;
    }
    rtc_unlock();
    return 0;
}

int Pcf85063_SetTime(const Pcf85063_DateTime_t *dt)
{
    uint8_t regs[7];
    uint8_t ctrl1;
    int ret = -1;

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

    rtc_lock();
    ctrl1 = Pcf85063_ReadReg(0x00U);
    if (ctrl1 != 0xFFU) {
        (void)Pcf85063_WriteReg(0x00U, (uint8_t)(ctrl1 | 0x20U));
    }

    if (HAL_I2C_Mem_Write(&hi2c1, PCF85063_I2C_ADDR, 0x04U, I2C_MEMADD_SIZE_8BIT,
                          regs, (uint16_t)sizeof(regs), 100U) == HAL_OK) {
        ret = 0;
    }

    pcf85063_clear_stop();
    rtc_unlock();
    return ret;
}

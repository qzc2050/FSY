#include "fsy_regmap.h"
#include "device_config.h"
#include "net_config.h"
#include "w5500.h"
#include "pcf85063.h"
#include "aht20.h"
#include "bmp280.h"
#include "ens160.h"
#include "pm25.h"
#include <stddef.h>
#include <string.h>

static uint32_t s_rt_regs[FSY_RT_REG_COUNT];
static uint32_t s_alarm_status;
static uint32_t s_geiger_sec_cps;
static uint8_t s_time_write_buf[8];

static const char *cfg_software_version_text(void)
{
    return DEVICE_SOFTWARE_VERSION;
}

#define FSY_ALARM_ENV_MASK  ((1UL << 6) | (1UL << 10) | (1UL << 14) | \
                             (1UL << 18) | (1UL << 22))

static void store_u16_le(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)(v >> 8);
}

static void store_u32_le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
    p[2] = (uint8_t)((v >> 16) & 0xFFU);
    p[3] = (uint8_t)(v >> 24);
}

static int reg_in_range(uint16_t reg, uint16_t base, uint16_t count)
{
    return (reg >= base) && (reg < (uint16_t)(base + count));
}

static void store_u32_reg_at(uint8_t *out, uint32_t value, uint16_t base_reg, uint16_t reg)
{
    uint16_t word;

    if (reg == base_reg) {
        word = (uint16_t)(value & 0xFFFFU);
    } else {
        word = (uint16_t)((value >> 16) & 0xFFFFU);
    }
    store_u16_le(out, word);
}

static void update_alarm_status(uint32_t set_mask, uint32_t clear_mask)
{
    s_alarm_status |= set_mask;
    s_alarm_status &= ~clear_mask;
    s_rt_regs[6] = s_alarm_status;
}

void Fsy_Regmap_Init(void)
{
    static const uint32_t template[FSY_RT_REG_COUNT] = {
        0x00000000U, 0x00000130U, 0x00018D7FU, 0x00000020U,
        0x00000604U, 0x00000172U, 0x00000000U, 0x00006000U,
        0x00000000U, 0x00000000U, 0x00000000U,
    };

    memcpy(s_rt_regs, template, sizeof(template));
}

int Fsy_Regmap_ReadU32(uint16_t reg_addr, uint32_t *value)
{
    uint32_t index;

    if (value == NULL) {
        return -1;
    }
    if (reg_addr == FSY_RT_REG_STATUS_BIT) {
        Fsy_Regmap_SyncStatusBitFromCtrl();
        *value = s_rt_regs[FSY_RT_IDX_STATUS_BIT];
        return 0;
    }
    if ((reg_addr < FSY_RT_REG_START) ||
        (reg_addr >= (FSY_RT_REG_START + FSY_RT_REG_COUNT))) {
        return -1;
    }

    index = (uint32_t)(reg_addr - FSY_RT_REG_START);
    *value = s_rt_regs[index];
    return 0;
}

/*
 * Neiji reg15（0x23 第 8 项 / 0x03 读 0x000F）：reg123 的只读镜像。
 * - 仅 bit13=光报警使能、bit14=背光使能（与 reg123 一致）
 * - bit15 外置报警在线：未接 IO，恒 0
 * - 其余 bit 恒 0，非 GPIO 采样
 */
void Fsy_Regmap_SyncStatusBitFromCtrl(void)
{
    uint32_t mirror;

    if (DeviceConfig_IsReady() != 0U) {
        mirror = DeviceConfig_GetControlBit2Mirror();
    } else {
        /* 配置未就绪：默认 bit13+14=1 */
        mirror = (1UL << FSY_CTRL2_BIT_ALARM_LIGHT) | (1UL << FSY_CTRL2_BIT_SCREEN);
    }
    s_rt_regs[FSY_RT_IDX_STATUS_BIT] = mirror;
}

static int read_time_reg(uint16_t reg, uint8_t *out, uint16_t out_cap)
{
    Pcf85063_DateTime_t dt;
    uint8_t bytes[8] = {0};
    uint16_t off;

    if (out_cap < 2U) {
        return -1;
    }
    if ((reg < FSY_REG_TIME) ||
        (reg >= (uint16_t)(FSY_REG_TIME + FSY_REG_TIME_REGS))) {
        return -1;
    }
    if ((Pcf85063_GetTime(&dt) != 0) || (dt.online == 0U)) {
        return -1;
    }

    bytes[0] = (uint8_t)(dt.year % 100U);
    bytes[1] = dt.month;
    bytes[2] = dt.day;
    bytes[3] = dt.hour;
    bytes[4] = dt.minute;
    bytes[5] = dt.second;

    off = (uint16_t)((reg - FSY_REG_TIME) * 2U);
    store_u16_le(out, (uint16_t)bytes[off] | ((uint16_t)bytes[off + 1U] << 8));
    return 2;
}

static int read_current_ip_reg(uint16_t reg, uint8_t *out, uint16_t out_cap)
{
    uint8_t ip[4];

    if (out_cap < 2U) {
        return -1;
    }
    if (!reg_in_range(reg, FSY_REG_CURRENT_IP, FSY_REG_CURRENT_IP_REGS)) {
        return -1;
    }

    getSIPR(ip);
    if (reg == FSY_REG_CURRENT_IP) {
        store_u16_le(out, (uint16_t)ip[0] | ((uint16_t)ip[1] << 8));
    } else {
        store_u16_le(out, (uint16_t)ip[2] | ((uint16_t)ip[3] << 8));
    }
    return 2;
}

static int read_software_version_reg(uint16_t reg, uint8_t *out, uint16_t out_cap)
{
    uint16_t off;
    uint8_t lo = 0U;
    uint8_t hi = 0U;

    if (out_cap < 2U) {
        return -1;
    }
    if ((reg < FSY_REG_SOFTWARE_VERSION) ||
        (reg >= (uint16_t)(FSY_REG_SOFTWARE_VERSION + FSY_REG_SOFTWARE_VERSION_REGS))) {
        return -1;
    }

    off = (uint16_t)((reg - FSY_REG_SOFTWARE_VERSION) * 2U);
    if (off < (uint16_t)(strlen(cfg_software_version_text()) + 1U)) {
        lo = (uint8_t)cfg_software_version_text()[off];
    }
    if ((off + 1U) < (uint16_t)(strlen(cfg_software_version_text()) + 1U)) {
        hi = (uint8_t)cfg_software_version_text()[off + 1U];
    }
    store_u16_le(out, (uint16_t)lo | ((uint16_t)hi << 8));
    return 2;
}

static int time_bytes_valid(const uint8_t bytes[8])
{
    if ((bytes[0] > 99U) || (bytes[1] == 0U) || (bytes[1] > 12U) ||
        (bytes[2] == 0U) || (bytes[2] > 31U) ||
        (bytes[3] > 23U) || (bytes[4] > 59U) || (bytes[5] > 59U)) {
        return 0;
    }
    return 1;
}

static uint8_t time_calc_weekday(uint8_t year2, uint8_t month, uint8_t day)
{
    uint32_t y = 2000U + (uint32_t)(year2 % 100U);
    uint32_t m = month;
    uint32_t d = day;
    uint32_t k;
    uint32_t j;
    int w;

    if (m < 3U) {
        m += 12U;
        y -= 1U;
    }
    k = y % 100U;
    j = y / 100U;
    w = (int)((d + (13U * (m + 1U)) / 5U + k + k / 4U + j / 4U + 5U * j) % 7);
    return (uint8_t)((w + 6) % 7);
}

static int write_time_regs(uint16_t start_reg, const uint8_t *data, uint16_t byte_count)
{
    uint16_t reg_count;
    Pcf85063_DateTime_t dt;

    if ((data == NULL) || ((byte_count % 2U) != 0U) || (byte_count == 0U)) {
        return -1;
    }

    reg_count = (uint16_t)(byte_count / 2U);
    if ((start_reg != FSY_REG_TIME) || (reg_count != FSY_REG_TIME_REGS)) {
        return -1;
    }

    s_time_write_buf[0] = data[0];
    s_time_write_buf[1] = data[1];
    s_time_write_buf[2] = data[2];
    s_time_write_buf[3] = data[3];
    s_time_write_buf[4] = data[4];
    s_time_write_buf[5] = data[5];
    s_time_write_buf[6] = data[6];
    s_time_write_buf[7] = data[7];

    if (!time_bytes_valid(s_time_write_buf)) {
        return -1;
    }

    dt.year = s_time_write_buf[0];
    dt.month = s_time_write_buf[1];
    dt.day = s_time_write_buf[2];
    dt.hour = s_time_write_buf[3];
    dt.minute = s_time_write_buf[4];
    dt.second = s_time_write_buf[5];
    dt.week = time_calc_weekday(dt.year, dt.month, dt.day);
    dt.online = 1U;

    return Pcf85063_SetTime(&dt);
}

int Fsy_Regmap_ReadBlock(uint16_t start_reg, uint16_t reg_count,
                         uint8_t *out, uint16_t out_cap)
{
    uint16_t i;
    uint16_t byte_count;

    if (out == NULL) {
        return -1;
    }
    if (reg_count == 0U) {
        return 0;
    }

    byte_count = (uint16_t)(reg_count * 2U);
    if (out_cap < byte_count) {
        return -1;
    }

    memset(out, 0, byte_count);
    for (i = 0U; i < reg_count; i++) {
        uint16_t reg = (uint16_t)(start_reg + i);
        uint32_t val = 0U;
        uint8_t cfg_pair[2];

        if ((reg >= FSY_REG_TIME) &&
            (reg < (uint16_t)(FSY_REG_TIME + FSY_REG_TIME_REGS))) {
            if (read_time_reg(reg, &out[(uint16_t)(i * 2U)],
                              (uint16_t)(byte_count - (i * 2U))) == 2) {
                continue;
            }
        }

        if ((reg >= FSY_REG_SOFTWARE_VERSION) &&
            (reg < (uint16_t)(FSY_REG_SOFTWARE_VERSION + FSY_REG_SOFTWARE_VERSION_REGS))) {
            if (read_software_version_reg(reg, &out[(uint16_t)(i * 2U)],
                                          (uint16_t)(byte_count - (i * 2U))) == 2) {
                continue;
            }
        }

        if (Fsy_Regmap_ReadU32(reg, &val) == 0) {
            store_u16_le(&out[(uint16_t)(i * 2U)], (uint16_t)(val & 0xFFFFU));
            continue;
        }

        if (reg_in_range(reg, FSY_REG_GEIGER_SEC_CPS, FSY_REG_GEIGER_SEC_CPS_REGS)) {
            store_u32_reg_at(&out[(uint16_t)(i * 2U)], s_geiger_sec_cps,
                             FSY_REG_GEIGER_SEC_CPS, reg);
            continue;
        }

        if (reg_in_range(reg, FSY_REG_CURRENT_IP, FSY_REG_CURRENT_IP_REGS)) {
            if (read_current_ip_reg(reg, &out[(uint16_t)(i * 2U)],
                                    (uint16_t)(byte_count - (i * 2U))) == 2) {
                continue;
            }
        }

        if ((DeviceConfig_IsReady() != 0U) &&
            (DeviceConfig_ReadRegBlock(reg, 1U, cfg_pair, sizeof(cfg_pair)) == 2)) {
            out[(uint16_t)(i * 2U)] = cfg_pair[0];
            out[(uint16_t)(i * 2U + 1U)] = cfg_pair[1];
        }
    }
    return (int)byte_count;
}

static int reg_is_configurable(uint16_t reg)
{
    if ((reg >= FSY_REG_SERIALNUM) &&
        (reg < (uint16_t)(FSY_REG_SERIALNUM + FSY_REG_SERIALNUM_REGS))) {
        return 1;
    }
    if (reg == FSY_REG_ADDRESS) {
        return 1;
    }
    if (reg == FSY_REG_ALARM_VOLUME) {
        return 1;
    }
    if ((reg >= FSY_REG_CONTROL_BIT2) &&
        (reg < (uint16_t)(FSY_REG_CONTROL_BIT2 + FSY_REG_CONTROL_BIT2_REGS))) {
        return 1;
    }
    if ((reg >= FSY_REG_PRODUCT_MODEL) &&
        (reg < (uint16_t)(FSY_REG_PRODUCT_MODEL + FSY_REG_PRODUCT_MODEL_REGS))) {
        return 1;
    }
    if ((reg >= FSY_REG_PRODUCT_NAME) &&
        (reg < (uint16_t)(FSY_REG_PRODUCT_NAME + FSY_REG_PRODUCT_NAME_REGS))) {
        return 1;
    }
    if ((reg >= FSY_REG_STATIC_IP) &&
        (reg < (uint16_t)(FSY_REG_STATIC_IP + FSY_REG_STATIC_IP_REGS))) {
        return 1;
    }
    if (reg == FSY_REG_DHCP_ENABLE) {
        return 1;
    }
    if (reg == FSY_REG_LANGUAGE) {
        return 1;
    }
    if ((reg >= FSY_REG_HW_VERSION) &&
        (reg < (uint16_t)(FSY_REG_HW_VERSION + FSY_REG_HW_VERSION_REGS))) {
        return 1;
    }
    if ((reg >= FSY_REG_GEIGER_SENS) &&
        (reg < (uint16_t)(FSY_REG_RATE_LIMIT + FSY_REG_GEIGER_PARAM_REGS))) {
        return 1;
    }
    if ((reg >= FSY_REG_DOSE_HI_TH) &&
        (reg < (uint16_t)(FSY_REG_DOSE_HI_TH + FSY_REG_DWORD_REGS))) {
        return 1;
    }
    if ((reg >= FSY_REG_DOSE_LO_TH) &&
        (reg < (uint16_t)(FSY_REG_DOSE_LO_TH + FSY_REG_DWORD_REGS))) {
        return 1;
    }
    if ((reg >= FSY_REG_ALARM_ENABLE) &&
        (reg < (uint16_t)(FSY_REG_ALARM_ENABLE + FSY_REG_ALARM_ENABLE_REGS))) {
        return 1;
    }
    if ((reg >= FSY_REG_TIME) &&
        (reg < (uint16_t)(FSY_REG_TIME + FSY_REG_TIME_REGS))) {
        return 1;
    }
    return 0;
}

int Fsy_Regmap_WriteBlock(uint16_t start_reg, const uint8_t *data,
                          uint16_t byte_count)
{
    uint16_t i;
    uint16_t reg_count;

    if ((data == NULL) || ((byte_count % 2U) != 0U) || (byte_count == 0U)) {
        return -1;
    }

    reg_count = (uint16_t)(byte_count / 2U);
    if ((start_reg == FSY_REG_TIME) && (reg_count == FSY_REG_TIME_REGS)) {
        for (i = 0U; i < reg_count; i++) {
            uint16_t reg = (uint16_t)(start_reg + i);
            if (!reg_is_configurable(reg)) {
                return -1;
            }
        }
        return write_time_regs(start_reg, data, byte_count);
    }

    if (DeviceConfig_IsReady() == 0U) {
        return -1;
    }

    for (i = 0U; i < reg_count; i++) {
        uint16_t reg = (uint16_t)(start_reg + i);
        if ((reg >= FSY_REG_SOFTWARE_VERSION) &&
            (reg < (uint16_t)(FSY_REG_SOFTWARE_VERSION + FSY_REG_SOFTWARE_VERSION_REGS))) {
            return -1;
        }
        if (!reg_is_configurable(reg)) {
            return -1;
        }
    }

    return DeviceConfig_WriteRegBlock(start_reg, data, byte_count);
}

int Fsy_Regmap_BuildRtPayload(uint8_t *out, uint16_t out_cap)
{
    uint16_t i;

    if (out == NULL) {
        return -1;
    }
    if (out_cap < FSY_RT_REG_DATA_BYTES) {
        return -1;
    }

    Fsy_Regmap_SyncStatusBitFromCtrl();

    for (i = 0U; i < FSY_RT_REG_COUNT; i++) {
        store_u32_le(&out[(uint16_t)(i * 4U)], s_rt_regs[i]);
    }
    return (int)FSY_RT_REG_DATA_BYTES;
}

void Fsy_Regmap_UpdateEnv(const AHT20_Data_t *aht, const BMP280_Data_t *bmp,
                          const ENS160_Data_t *ens, const PM25_Data_t *pm25)
{
    uint32_t alarm = 0U;

    if ((aht == NULL) || (bmp == NULL) || (ens == NULL) || (pm25 == NULL)) {
        return;
    }

    s_rt_regs[1] = (uint32_t)((int32_t)(aht->temperature_c * 10.0f));
    s_rt_regs[2] = (uint32_t)bmp->pressure_pa;
    s_rt_regs[3] = (uint32_t)aht->humidity_rh;
    s_rt_regs[4] = (uint32_t)ens->eco2;
    s_rt_regs[5] = (uint32_t)pm25->pm2_5 * 10U;

    if (aht->online == 0U) {
        alarm |= (1UL << 6);
        alarm |= (1UL << 14);
    }
    if (bmp->online == 0U) {
        alarm |= (1UL << 10);
    }
    if (ens->online == 0U) {
        alarm |= (1UL << 18);
    }
    if (pm25->online == 0U) {
        alarm |= (1UL << 22);
    }

    s_alarm_status = (s_alarm_status & ~FSY_ALARM_ENV_MASK) | alarm;
    s_rt_regs[6] = s_alarm_status;
    Fsy_Regmap_SyncStatusBitFromCtrl();
    s_rt_regs[8] = 0U;
    s_rt_regs[9] = 0U;
    s_rt_regs[10] = 0U;
}

void Fsy_Regmap_SetGeigerSecCps(uint32_t cps)
{
    s_geiger_sec_cps = cps;
}

void Fsy_Regmap_UpdateDoseRate(float rate_usv_h)
{
    uint32_t hi_x100 = 0U;
    uint32_t lo_x100 = 0U;
    uint32_t alarm_enable = 0U;
    uint32_t rate_x100;
    uint32_t set_mask = 0U;
    uint32_t clear_mask = (1UL << FSY_ALARM_BIT_DOSE_HI) |
                          (1UL << FSY_ALARM_BIT_DOSE_LO);

    if (rate_usv_h < 0.0f) {
        rate_usv_h = 0.0f;
    }

    rate_x100 = (uint32_t)(rate_usv_h * 100.0f);
    s_rt_regs[0] = rate_x100;

    DeviceConfig_GetDoseAlarmConfig(&hi_x100, &lo_x100, &alarm_enable, NULL);

    if (((alarm_enable & (1UL << FSY_ALARM_BIT_DOSE_HI)) != 0U) &&
        (hi_x100 > 0U) && (rate_x100 > hi_x100)) {
        set_mask |= (1UL << FSY_ALARM_BIT_DOSE_HI);
    }

    if (((alarm_enable & (1UL << FSY_ALARM_BIT_DOSE_LO)) != 0U) &&
        (lo_x100 > 0U) && (rate_x100 < lo_x100)) {
        set_mask |= (1UL << FSY_ALARM_BIT_DOSE_LO);
    }

    clear_mask &= ~set_mask;
    update_alarm_status(set_mask, clear_mask);
}

void Fsy_Regmap_SyncAlarmStatus(uint32_t alarm_status)
{
    s_alarm_status = alarm_status;
    s_rt_regs[6] = s_alarm_status;
}

uint32_t Fsy_Regmap_GetAlarmStatus(void)
{
    return s_alarm_status;
}

void Fsy_Regmap_ApplyAlarmEnable(uint32_t enable_mask)
{
    uint32_t clear_mask = 0U;

    if ((enable_mask & (1UL << FSY_ALARM_BIT_DOSE_HI)) == 0U) {
        clear_mask |= (1UL << FSY_ALARM_BIT_DOSE_HI);
    }
    if ((enable_mask & (1UL << FSY_ALARM_BIT_DOSE_LO)) == 0U) {
        clear_mask |= (1UL << FSY_ALARM_BIT_DOSE_LO);
    }

    update_alarm_status(0U, clear_mask);
}

void Fsy_Regmap_PatchDoseAlarmBit(uint8_t bit_pos, bool is_alarm)
{
    uint32_t set_mask = 0U;
    uint32_t clear_mask = 0U;

    if (bit_pos > FSY_ALARM_BIT_DOSE_LO) {
        return;
    }

    if (is_alarm) {
        set_mask = (1UL << bit_pos);
    } else {
        clear_mask = (1UL << bit_pos);
    }
    update_alarm_status(set_mask, clear_mask);
}

#include "device_config.h"
#include "set_flash.h"
#include "flash_layout.h"
#include "ext_flash_layout.h"
#include "flash_fs_mutex.h"
#include "w25qxx.h"
#include "w25q_port.h"
#include "net_config.h"
#include "network_cmd.h"
#include "fsy_dispatch.h"
#include "fsy_regmap.h"
#include "sys_cfg_defaults.h"
#include "geiger.h"
#include "dose_rate.h"

#include "cmsis_os.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define CFG_MAGIC                 0x44455643U /* 'DEVC' */
#define CFG_VERSION               10U
#define CFG_VERSION_V9            9U
#define CFG_VERSION_V8            8U
#define CFG_VERSION_V7            7U
#define CFG_VERSION_V6            6U
#define CFG_VERSION_V5            5U
#define CFG_VERSION_V4            4U
#define CFG_VERSION_V3            3U
#define CFG_VERSION_V2            2U
#define CFG_VERSION_V1            1U

#define CFG_SN_FIELD_LEN          16U
#define CFG_MODEL_FIELD_LEN       16U

#define CFG_DHCP_ENABLE           1U
#define CFG_DHCP_DISABLE          0U

#define CFG_DOSE_HI_DEFAULT_X100  (10000UL * 100UL)
#define CFG_DOSE_LO_DEFAULT_X100  0UL
#define CFG_ALARM_DOSE_DEFAULT    ((1UL << FSY_ALARM_BIT_DOSE_HI) | \
                                   (1UL << FSY_ALARM_BIT_DOSE_LO))

#define CFG_GEIGER_SENS_X100_DEFAULT       12000UL
#define CFG_EWMA_THRESHOLD_CPS_DEFAULT     100UL
#define CFG_EWMA_THRESHOLD_DELTA_DEFAULT   10UL
#define CFG_EWMA_ALPHA_LOW_X100_DEFAULT    3UL
#define CFG_EWMA_ALPHA_HIGH_X100_DEFAULT   35UL
#define CFG_EWMA_BOOST_DURATION_DEFAULT    20UL
#define CFG_RATE_LIMIT_X100_DEFAULT        (10000UL * 100UL)

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    uint32_t crc32;
    char sn[CFG_SN_FIELD_LEN];
    char product_model[CFG_MODEL_FIELD_LEN];
    uint8_t dev_addr;
    uint8_t dhcp_enable;
    uint8_t static_ip[4];
    uint32_t dose_hi_x100;
    uint32_t dose_lo_x100;
    uint32_t alarm_enable_mask;
    uint8_t alarm_volume;
    char product_name[CFG_MODEL_FIELD_LEN];
    uint8_t language;
} DeviceCfgBlobV6;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    uint32_t crc32;
    char sn[CFG_SN_FIELD_LEN];
    char product_model[CFG_MODEL_FIELD_LEN];
    uint8_t dev_addr;
    uint8_t dhcp_enable;
    uint8_t static_ip[4];
    uint32_t dose_hi_x100;
    uint32_t dose_lo_x100;
    uint32_t alarm_enable_mask;
    uint8_t alarm_volume;
    char product_name[CFG_MODEL_FIELD_LEN];
    uint8_t language;
    uint8_t alarm_sound;
    uint8_t alarm_light;
} DeviceCfgBlobV7;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    uint32_t crc32;
    char sn[CFG_SN_FIELD_LEN];
    char product_model[CFG_MODEL_FIELD_LEN];
    uint8_t dev_addr;
    uint8_t dhcp_enable;
    uint8_t static_ip[4];
    uint32_t dose_hi_x100;
    uint32_t dose_lo_x100;
    uint32_t alarm_enable_mask;
    uint8_t alarm_volume;
    char product_name[CFG_MODEL_FIELD_LEN];
    uint8_t language;
    uint8_t alarm_sound;
    uint8_t alarm_light;
    uint32_t geiger_sens_x100;
    uint32_t ewma_threshold_cps;
    uint32_t ewma_threshold_delta;
    uint32_t ewma_alpha_low_x100;
    uint32_t ewma_alpha_high_x100;
    uint32_t ewma_boost_duration;
    uint32_t rate_limit_x100;
} DeviceCfgBlobV9;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    uint32_t crc32;
    char sn[CFG_SN_FIELD_LEN];
    char product_model[CFG_MODEL_FIELD_LEN];
    uint8_t dev_addr;
    uint8_t dhcp_enable;
    uint8_t static_ip[4];
    uint32_t dose_hi_x100;
    uint32_t dose_lo_x100;
    uint32_t alarm_enable_mask;
    uint8_t alarm_volume;
    char product_name[CFG_MODEL_FIELD_LEN];
    uint8_t language;
    uint8_t alarm_sound;
    uint8_t alarm_light;
    uint32_t geiger_sens_x100;
    uint32_t ewma_threshold_cps;
    uint32_t ewma_threshold_delta;
    uint32_t ewma_alpha_low_x100;
    uint32_t ewma_alpha_high_x100;
    uint32_t ewma_boost_duration;
    uint32_t rate_limit_x100;
    char hw_version[CFG_MODEL_FIELD_LEN];
} DeviceCfgBlob;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    uint32_t crc32;
    char sn[CFG_SN_FIELD_LEN];
    char product_model[CFG_MODEL_FIELD_LEN];
    uint8_t dev_addr;
    uint8_t dhcp_enable;
    uint8_t static_ip[4];
    uint32_t dose_hi_x100;
    uint32_t dose_lo_x100;
    uint32_t alarm_enable_mask;
    uint8_t alarm_volume;
    char product_name[CFG_MODEL_FIELD_LEN];
} DeviceCfgBlobV5;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    uint32_t crc32;
    char sn[CFG_SN_FIELD_LEN];
    char product_model[CFG_MODEL_FIELD_LEN];
    uint8_t dev_addr;
    uint8_t dhcp_enable;
    uint8_t static_ip[4];
    uint32_t dose_hi_x100;
    uint32_t dose_lo_x100;
    uint32_t alarm_enable_mask;
    uint8_t alarm_volume;
    uint8_t reserved[3];
} DeviceCfgBlobV4;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    uint32_t crc32;
    char sn[CFG_SN_FIELD_LEN];
    char product_model[CFG_MODEL_FIELD_LEN];
    uint8_t dev_addr;
    uint8_t dhcp_enable;
    uint8_t static_ip[4];
    uint32_t dose_hi_x100;
    uint32_t dose_lo_x100;
    uint32_t alarm_enable_mask;
} DeviceCfgBlobV3;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    uint32_t crc32;
    char sn[CFG_SN_FIELD_LEN];
    char product_model[CFG_MODEL_FIELD_LEN];
    uint8_t dev_addr;
    uint8_t dhcp_enable;
    uint8_t static_ip[4];
} DeviceCfgBlobV2;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    uint32_t crc32;
    char sn[CFG_SN_FIELD_LEN];
    char product_model[CFG_MODEL_FIELD_LEN];
    uint8_t dev_addr;
    uint8_t reserved[3];
} DeviceCfgBlobV1;

static const uint8_t s_default_static_ip[4] = {192, 168, 2, 100};

static uint8_t s_ready;
static volatile uint8_t s_save_pending;
static DeviceCfgBlob s_cfg;

static uint32_t cfg_crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFU;
    size_t i;

    for (i = 0U; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            if ((crc & 1U) != 0U) {
                crc = (crc >> 1) ^ 0xEDB88320U;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

static void cfg_fixed_copy(char *dst, size_t width, const char *src)
{
    size_t n;

    if ((dst == NULL) || (width == 0U)) {
        return;
    }
    memset(dst, 0, width);
    if (src == NULL) {
        return;
    }
    n = strlen(src);
    if (n > width) {
        n = width;
    }
    memcpy(dst, src, n);
}

static void cfg_apply_network_defaults(DeviceCfgBlob *blob)
{
    blob->dhcp_enable = CFG_DHCP_ENABLE;
    memcpy(blob->static_ip, s_default_static_ip, sizeof(blob->static_ip));
}

static void cfg_apply_geiger_defaults(DeviceCfgBlob *blob)
{
    blob->geiger_sens_x100 = CFG_GEIGER_SENS_X100_DEFAULT;
    blob->ewma_threshold_cps = CFG_EWMA_THRESHOLD_CPS_DEFAULT;
    blob->ewma_threshold_delta = CFG_EWMA_THRESHOLD_DELTA_DEFAULT;
    blob->ewma_alpha_low_x100 = CFG_EWMA_ALPHA_LOW_X100_DEFAULT;
    blob->ewma_alpha_high_x100 = CFG_EWMA_ALPHA_HIGH_X100_DEFAULT;
    blob->ewma_boost_duration = CFG_EWMA_BOOST_DURATION_DEFAULT;
    blob->rate_limit_x100 = CFG_RATE_LIMIT_X100_DEFAULT;
}

static void cfg_apply_hw_defaults(DeviceCfgBlob *blob)
{
    cfg_fixed_copy(blob->hw_version, CFG_MODEL_FIELD_LEN, DEVICE_CFG_DEFAULT_HW);
}

static void cfg_apply_alarm_defaults(DeviceCfgBlob *blob)
{
    blob->dose_hi_x100 = CFG_DOSE_HI_DEFAULT_X100;
    blob->dose_lo_x100 = CFG_DOSE_LO_DEFAULT_X100;
    blob->alarm_enable_mask = CFG_ALARM_DOSE_DEFAULT;
    blob->alarm_volume = (uint8_t)DEVICE_CFG_DEFAULT_ALARM_VOLUME;
    blob->alarm_sound = (uint8_t)DEVICE_CFG_DEFAULT_ALARM_SOUND;
    blob->alarm_light = (uint8_t)DEVICE_CFG_DEFAULT_ALARM_LIGHT;
    cfg_fixed_copy(blob->product_name, CFG_MODEL_FIELD_LEN, NEIJI_PRODUCT_NAME);
}

static void cfg_apply_defaults(void)
{
    memset(&s_cfg, 0, sizeof(s_cfg));
    s_cfg.magic = CFG_MAGIC;
    s_cfg.version = CFG_VERSION;
    cfg_fixed_copy(s_cfg.sn, CFG_SN_FIELD_LEN, NEIJI_DEVICE_SN);
    cfg_fixed_copy(s_cfg.product_model, CFG_MODEL_FIELD_LEN, DEVICE_PRODUCT_MODEL);
    cfg_fixed_copy(s_cfg.product_name, CFG_MODEL_FIELD_LEN, NEIJI_PRODUCT_NAME);
    s_cfg.language = (uint8_t)DEVICE_CFG_DEFAULT_LANGUAGE;
    s_cfg.alarm_sound = (uint8_t)DEVICE_CFG_DEFAULT_ALARM_SOUND;
    s_cfg.alarm_light = (uint8_t)DEVICE_CFG_DEFAULT_ALARM_LIGHT;
    s_cfg.dev_addr = DEVICE_CFG_DEFAULT_DEV_ADDR;
    cfg_apply_network_defaults(&s_cfg);
    cfg_apply_alarm_defaults(&s_cfg);
    cfg_apply_geiger_defaults(&s_cfg);
    cfg_apply_hw_defaults(&s_cfg);
}

static uint32_t cfg_calc_crc_v1(const DeviceCfgBlobV1 *blob)
{
    DeviceCfgBlobV1 tmp;

    memcpy(&tmp, blob, sizeof(tmp));
    tmp.crc32 = 0U;
    return cfg_crc32((const uint8_t *)&tmp.version,
                     sizeof(DeviceCfgBlobV1) - offsetof(DeviceCfgBlobV1, version));
}

static int cfg_blob_v1_valid(const DeviceCfgBlobV1 *blob)
{
    if (blob->magic != CFG_MAGIC) {
        return 0;
    }
    if (blob->version != CFG_VERSION_V1) {
        return 0;
    }
    if (blob->crc32 != cfg_calc_crc_v1(blob)) {
        return 0;
    }
    if ((blob->dev_addr == 0U) || (blob->dev_addr > 247U)) {
        return 0;
    }
    return 1;
}

static uint32_t cfg_calc_crc_v2(const DeviceCfgBlobV2 *blob)
{
    DeviceCfgBlobV2 tmp;

    memcpy(&tmp, blob, sizeof(tmp));
    tmp.crc32 = 0U;
    return cfg_crc32((const uint8_t *)&tmp.version,
                     sizeof(DeviceCfgBlobV2) - offsetof(DeviceCfgBlobV2, version));
}

static int cfg_blob_v2_valid(const DeviceCfgBlobV2 *blob)
{
    if (blob->magic != CFG_MAGIC) {
        return 0;
    }
    if (blob->version != CFG_VERSION_V2) {
        return 0;
    }
    if (blob->crc32 != cfg_calc_crc_v2(blob)) {
        return 0;
    }
    if ((blob->dev_addr == 0U) || (blob->dev_addr > 247U)) {
        return 0;
    }
    if (blob->dhcp_enable > 1U) {
        return 0;
    }
    return 1;
}

static uint32_t cfg_calc_crc_v9(const DeviceCfgBlobV9 *blob)
{
    DeviceCfgBlobV9 tmp;

    memcpy(&tmp, blob, sizeof(tmp));
    tmp.crc32 = 0U;
    return cfg_crc32((const uint8_t *)&tmp.version,
                     sizeof(DeviceCfgBlobV9) - offsetof(DeviceCfgBlobV9, version));
}

static uint32_t cfg_calc_crc(const DeviceCfgBlob *blob)
{
    DeviceCfgBlob tmp;

    memcpy(&tmp, blob, sizeof(tmp));
    tmp.crc32 = 0U;
    return cfg_crc32((const uint8_t *)&tmp.version,
                     sizeof(DeviceCfgBlob) - offsetof(DeviceCfgBlob, version));
}

static void cfg_refresh_crc_v9(DeviceCfgBlobV9 *blob)
{
    blob->crc32 = cfg_calc_crc_v9(blob);
}

static void cfg_refresh_crc(DeviceCfgBlob *blob)
{
    blob->crc32 = cfg_calc_crc(blob);
}

static void cfg_upgrade_v1_to_current(const DeviceCfgBlobV1 *v1, DeviceCfgBlob *cur)
{
    memset(cur, 0, sizeof(*cur));
    cur->magic = CFG_MAGIC;
    cur->version = CFG_VERSION;
    memcpy(cur->sn, v1->sn, sizeof(cur->sn));
    memcpy(cur->product_model, v1->product_model, sizeof(cur->product_model));
    cur->dev_addr = v1->dev_addr;
    cfg_apply_network_defaults(cur);
    cfg_apply_alarm_defaults(cur);
    cfg_apply_geiger_defaults(cur);
    cfg_apply_hw_defaults(cur);
    cfg_refresh_crc(cur);
}

static void cfg_upgrade_v2_to_v3(const DeviceCfgBlobV2 *old, DeviceCfgBlob *cur)
{
    memset(cur, 0, sizeof(*cur));
    cur->magic = CFG_MAGIC;
    cur->version = CFG_VERSION;
    memcpy(cur->sn, old->sn, sizeof(cur->sn));
    memcpy(cur->product_model, old->product_model, sizeof(cur->product_model));
    cur->dev_addr = old->dev_addr;
    cur->dhcp_enable = old->dhcp_enable;
    memcpy(cur->static_ip, old->static_ip, sizeof(cur->static_ip));
    cfg_apply_alarm_defaults(cur);
    cfg_apply_geiger_defaults(cur);
    cfg_apply_hw_defaults(cur);
    cfg_refresh_crc(cur);
}

static uint32_t cfg_calc_crc_v3(const DeviceCfgBlobV3 *blob)
{
    DeviceCfgBlobV3 tmp;

    memcpy(&tmp, blob, sizeof(tmp));
    tmp.crc32 = 0U;
    return cfg_crc32((const uint8_t *)&tmp.version,
                     sizeof(DeviceCfgBlobV3) - offsetof(DeviceCfgBlobV3, version));
}

static int cfg_blob_v3_valid(const DeviceCfgBlobV3 *blob)
{
    if (blob->magic != CFG_MAGIC) {
        return 0;
    }
    if (blob->version != CFG_VERSION_V3) {
        return 0;
    }
    if (blob->crc32 != cfg_calc_crc_v3(blob)) {
        return 0;
    }
    if ((blob->dev_addr == 0U) || (blob->dev_addr > 247U)) {
        return 0;
    }
    if (blob->dhcp_enable > 1U) {
        return 0;
    }
    if ((blob->alarm_enable_mask & ~CFG_ALARM_DOSE_DEFAULT) != 0U) {
        return 0;
    }
    return 1;
}

static void cfg_upgrade_v3_to_v4(const DeviceCfgBlobV3 *old, DeviceCfgBlob *cur)
{
    memset(cur, 0, sizeof(*cur));
    cur->magic = CFG_MAGIC;
    cur->version = CFG_VERSION;
    memcpy(cur->sn, old->sn, sizeof(cur->sn));
    memcpy(cur->product_model, old->product_model, sizeof(cur->product_model));
    cur->dev_addr = old->dev_addr;
    cur->dhcp_enable = old->dhcp_enable;
    memcpy(cur->static_ip, old->static_ip, sizeof(cur->static_ip));
    cur->dose_hi_x100 = old->dose_hi_x100;
    cur->dose_lo_x100 = old->dose_lo_x100;
    cur->alarm_enable_mask = old->alarm_enable_mask;
    cur->alarm_volume = (uint8_t)DEVICE_CFG_DEFAULT_ALARM_VOLUME;
    cfg_fixed_copy(cur->product_name, CFG_MODEL_FIELD_LEN, NEIJI_PRODUCT_NAME);
    cfg_apply_geiger_defaults(cur);
    cfg_apply_hw_defaults(cur);
    cfg_refresh_crc(cur);
}

static uint32_t cfg_calc_crc_v4(const DeviceCfgBlobV4 *blob)
{
    DeviceCfgBlobV4 tmp;

    memcpy(&tmp, blob, sizeof(tmp));
    tmp.crc32 = 0U;
    return cfg_crc32((const uint8_t *)&tmp.version,
                     sizeof(DeviceCfgBlobV4) - offsetof(DeviceCfgBlobV4, version));
}

static int cfg_blob_v4_valid(const DeviceCfgBlobV4 *blob)
{
    if (blob->magic != CFG_MAGIC) {
        return 0;
    }
    if (blob->version != CFG_VERSION_V4) {
        return 0;
    }
    if (blob->crc32 != cfg_calc_crc_v4(blob)) {
        return 0;
    }
    if ((blob->dev_addr == 0U) || (blob->dev_addr > 247U)) {
        return 0;
    }
    if (blob->dhcp_enable > 1U) {
        return 0;
    }
    if ((blob->alarm_enable_mask & ~CFG_ALARM_DOSE_DEFAULT) != 0U) {
        return 0;
    }
    if (blob->alarm_volume > 100U) {
        return 0;
    }
    return 1;
}

static void cfg_upgrade_v4_to_v5(const DeviceCfgBlobV4 *old, DeviceCfgBlob *cur)
{
    memset(cur, 0, sizeof(*cur));
    cur->magic = CFG_MAGIC;
    cur->version = CFG_VERSION;
    memcpy(cur->sn, old->sn, sizeof(cur->sn));
    memcpy(cur->product_model, old->product_model, sizeof(cur->product_model));
    cur->dev_addr = old->dev_addr;
    cur->dhcp_enable = old->dhcp_enable;
    memcpy(cur->static_ip, old->static_ip, sizeof(cur->static_ip));
    cur->dose_hi_x100 = old->dose_hi_x100;
    cur->dose_lo_x100 = old->dose_lo_x100;
    cur->alarm_enable_mask = old->alarm_enable_mask;
    cur->alarm_volume = old->alarm_volume;
    cfg_fixed_copy(cur->product_name, CFG_MODEL_FIELD_LEN, NEIJI_PRODUCT_NAME);
    cur->language = (uint8_t)DEVICE_CFG_DEFAULT_LANGUAGE;
    cfg_apply_geiger_defaults(cur);
    cfg_apply_hw_defaults(cur);
    cfg_refresh_crc(cur);
}

static uint32_t cfg_calc_crc_v5(const DeviceCfgBlobV5 *blob)
{
    DeviceCfgBlobV5 tmp;

    memcpy(&tmp, blob, sizeof(tmp));
    tmp.crc32 = 0U;
    return cfg_crc32((const uint8_t *)&tmp.version,
                     sizeof(DeviceCfgBlobV5) - offsetof(DeviceCfgBlobV5, version));
}

static int cfg_blob_v5_valid(const DeviceCfgBlobV5 *blob)
{
    if (blob->magic != CFG_MAGIC) {
        return 0;
    }
    if (blob->version != CFG_VERSION_V5) {
        return 0;
    }
    if (blob->crc32 != cfg_calc_crc_v5(blob)) {
        return 0;
    }
    if ((blob->dev_addr == 0U) || (blob->dev_addr > 247U)) {
        return 0;
    }
    if (blob->dhcp_enable > 1U) {
        return 0;
    }
    if ((blob->alarm_enable_mask & ~CFG_ALARM_DOSE_DEFAULT) != 0U) {
        return 0;
    }
    if (blob->alarm_volume > 100U) {
        return 0;
    }
    return 1;
}

static void cfg_upgrade_v5_to_v6(const DeviceCfgBlobV5 *old, DeviceCfgBlob *cur)
{
    memset(cur, 0, sizeof(*cur));
    cur->magic = CFG_MAGIC;
    cur->version = CFG_VERSION;
    memcpy(cur->sn, old->sn, sizeof(cur->sn));
    memcpy(cur->product_model, old->product_model, sizeof(cur->product_model));
    cur->dev_addr = old->dev_addr;
    cur->dhcp_enable = old->dhcp_enable;
    memcpy(cur->static_ip, old->static_ip, sizeof(cur->static_ip));
    cur->dose_hi_x100 = old->dose_hi_x100;
    cur->dose_lo_x100 = old->dose_lo_x100;
    cur->alarm_enable_mask = old->alarm_enable_mask;
    cur->alarm_volume = old->alarm_volume;
    memcpy(cur->product_name, old->product_name, sizeof(cur->product_name));
    cur->language = (uint8_t)DEVICE_CFG_DEFAULT_LANGUAGE;
    cur->alarm_sound = (uint8_t)DEVICE_CFG_DEFAULT_ALARM_SOUND;
    cur->alarm_light = (uint8_t)DEVICE_CFG_DEFAULT_ALARM_LIGHT;
    cfg_apply_geiger_defaults(cur);
    cfg_apply_hw_defaults(cur);
    cfg_refresh_crc(cur);
}

static uint32_t cfg_calc_crc_v6(const DeviceCfgBlobV6 *blob)
{
    DeviceCfgBlobV6 tmp;

    memcpy(&tmp, blob, sizeof(tmp));
    tmp.crc32 = 0U;
    return cfg_crc32((const uint8_t *)&tmp.version,
                     sizeof(DeviceCfgBlobV6) - offsetof(DeviceCfgBlobV6, version));
}

static int cfg_blob_v6_valid(const DeviceCfgBlobV6 *blob)
{
    if (blob->magic != CFG_MAGIC) {
        return 0;
    }
    if (blob->version != CFG_VERSION_V6) {
        return 0;
    }
    if (blob->crc32 != cfg_calc_crc_v6(blob)) {
        return 0;
    }
    if ((blob->dev_addr == 0U) || (blob->dev_addr > 247U)) {
        return 0;
    }
    if (blob->dhcp_enable > 1U) {
        return 0;
    }
    if ((blob->alarm_enable_mask & ~CFG_ALARM_DOSE_DEFAULT) != 0U) {
        return 0;
    }
    if (blob->alarm_volume > 100U) {
        return 0;
    }
    if (blob->language > 1U) {
        return 0;
    }
    return 1;
}

static uint32_t cfg_calc_crc_v7(const DeviceCfgBlobV7 *blob)
{
    DeviceCfgBlobV7 tmp;

    memcpy(&tmp, blob, sizeof(tmp));
    tmp.crc32 = 0U;
    return cfg_crc32((const uint8_t *)&tmp.version,
                     sizeof(DeviceCfgBlobV7) - offsetof(DeviceCfgBlobV7, version));
}

static void cfg_refresh_crc_v7(DeviceCfgBlobV7 *blob)
{
    blob->crc32 = cfg_calc_crc_v7(blob);
}

static void cfg_upgrade_v6_to_v7(const DeviceCfgBlobV6 *old, DeviceCfgBlob *cur)
{
    DeviceCfgBlobV7 *out = (DeviceCfgBlobV7 *)cur;

    memset(cur, 0, sizeof(*cur));
    out->magic = CFG_MAGIC;
    out->version = CFG_VERSION_V7;
    memcpy(out->sn, old->sn, sizeof(out->sn));
    memcpy(out->product_model, old->product_model, sizeof(out->product_model));
    out->dev_addr = old->dev_addr;
    out->dhcp_enable = old->dhcp_enable;
    memcpy(out->static_ip, old->static_ip, sizeof(out->static_ip));
    out->dose_hi_x100 = old->dose_hi_x100;
    out->dose_lo_x100 = old->dose_lo_x100;
    out->alarm_enable_mask = old->alarm_enable_mask;
    out->alarm_volume = old->alarm_volume;
    memcpy(out->product_name, old->product_name, sizeof(out->product_name));
    out->language = old->language;
    out->alarm_sound = (uint8_t)DEVICE_CFG_DEFAULT_ALARM_SOUND;
    out->alarm_light = (uint8_t)DEVICE_CFG_DEFAULT_ALARM_LIGHT;
    cfg_refresh_crc_v7(out);
}

static int cfg_blob_v7_valid(const DeviceCfgBlobV7 *blob)
{
    if (blob->magic != CFG_MAGIC) {
        return 0;
    }
    if (blob->version != CFG_VERSION_V7) {
        return 0;
    }
    if (blob->crc32 != cfg_calc_crc_v7(blob)) {
        return 0;
    }
    if ((blob->dev_addr == 0U) || (blob->dev_addr > 247U)) {
        return 0;
    }
    if (blob->dhcp_enable > 1U) {
        return 0;
    }
    if ((blob->alarm_enable_mask & ~CFG_ALARM_DOSE_DEFAULT) != 0U) {
        return 0;
    }
    if (blob->alarm_volume > 100U) {
        return 0;
    }
    if (blob->language > 1U) {
        return 0;
    }
    if (blob->alarm_sound > 1U) {
        return 0;
    }
    if (blob->alarm_light > 1U) {
        return 0;
    }
    return 1;
}

static void cfg_upgrade_v7_to_v8(const DeviceCfgBlobV7 *old, DeviceCfgBlob *cur)
{
    memset(cur, 0, sizeof(*cur));
    cur->magic = CFG_MAGIC;
    memcpy(cur->sn, old->sn, sizeof(cur->sn));
    memcpy(cur->product_model, old->product_model, sizeof(cur->product_model));
    cur->dev_addr = old->dev_addr;
    cur->dhcp_enable = old->dhcp_enable;
    memcpy(cur->static_ip, old->static_ip, sizeof(cur->static_ip));
    cur->dose_hi_x100 = old->dose_hi_x100;
    cur->dose_lo_x100 = old->dose_lo_x100;
    cur->alarm_enable_mask = old->alarm_enable_mask;
    cur->alarm_volume = old->alarm_volume;
    memcpy(cur->product_name, old->product_name, sizeof(cur->product_name));
    cur->language = old->language;
    cur->alarm_sound = old->alarm_sound;
    cur->alarm_light = old->alarm_light;
    cfg_apply_geiger_defaults(cur);
    ((DeviceCfgBlobV9 *)cur)->version = CFG_VERSION_V9;
    cfg_refresh_crc_v9((DeviceCfgBlobV9 *)cur);
}

static int cfg_geiger_fields_valid_v8(const DeviceCfgBlobV9 *blob)
{
    if (blob->geiger_sens_x100 == 0U) {
        return 0;
    }
    if (blob->ewma_alpha_low_x100 > 10000U) {
        return 0;
    }
    if (blob->ewma_alpha_high_x100 > 10000U) {
        return 0;
    }
    if (blob->rate_limit_x100 == 0U) {
        return 0;
    }
    return 1;
}

static int cfg_blob_v8_valid(const DeviceCfgBlobV9 *blob)
{
    if (blob->magic != CFG_MAGIC) {
        return 0;
    }
    if (blob->version != CFG_VERSION_V8) {
        return 0;
    }
    if (blob->crc32 != cfg_calc_crc_v9(blob)) {
        return 0;
    }
    if ((blob->dev_addr == 0U) || (blob->dev_addr > 247U)) {
        return 0;
    }
    if (blob->dhcp_enable > 1U) {
        return 0;
    }
    if ((blob->alarm_enable_mask & ~CFG_ALARM_DOSE_DEFAULT) != 0U) {
        return 0;
    }
    if (blob->alarm_volume > 100U) {
        return 0;
    }
    if (blob->language > 1U) {
        return 0;
    }
    if (blob->alarm_sound > 1U) {
        return 0;
    }
    if (blob->alarm_light > 1U) {
        return 0;
    }
    if (!cfg_geiger_fields_valid_v8(blob)) {
        return 0;
    }
    return 1;
}

static int cfg_blob_v9_valid(const DeviceCfgBlobV9 *blob)
{
    if (blob->magic != CFG_MAGIC) {
        return 0;
    }
    if (blob->version != CFG_VERSION_V9) {
        return 0;
    }
    if (blob->crc32 != cfg_calc_crc_v9(blob)) {
        return 0;
    }
    if ((blob->dev_addr == 0U) || (blob->dev_addr > 247U)) {
        return 0;
    }
    if (blob->dhcp_enable > 1U) {
        return 0;
    }
    if ((blob->alarm_enable_mask & ~CFG_ALARM_DOSE_DEFAULT) != 0U) {
        return 0;
    }
    if (blob->alarm_volume > 100U) {
        return 0;
    }
    if (blob->language > 1U) {
        return 0;
    }
    if (blob->alarm_sound > 1U) {
        return 0;
    }
    if (blob->alarm_light > 1U) {
        return 0;
    }
    if (!cfg_geiger_fields_valid((const DeviceCfgBlob *)blob)) {
        return 0;
    }
    return 1;
}

static void cfg_upgrade_v9_to_v10(const DeviceCfgBlobV9 *old, DeviceCfgBlob *cur)
{
    memcpy(cur, old, sizeof(*old));
    cur->magic = CFG_MAGIC;
    cur->version = CFG_VERSION;
    cfg_apply_hw_defaults(cur);
    cfg_refresh_crc(cur);
}

static void cfg_migrate_v8_alpha_to_x100(DeviceCfgBlobV9 *blob)
{
    if (blob->ewma_alpha_low_x100 > 100U) {
        blob->ewma_alpha_low_x100 /= 100U;
    }
    if (blob->ewma_alpha_high_x100 > 100U) {
        blob->ewma_alpha_high_x100 /= 100U;
    }
}

static int cfg_geiger_fields_valid(const DeviceCfgBlob *blob)
{
    if (blob->geiger_sens_x100 == 0U) {
        return 0;
    }
    if (blob->ewma_alpha_low_x100 > 100U) {
        return 0;
    }
    if (blob->ewma_alpha_high_x100 > 100U) {
        return 0;
    }
    if (blob->rate_limit_x100 == 0U) {
        return 0;
    }
    return 1;
}

static int cfg_blob_valid(const DeviceCfgBlob *blob)
{
    DeviceCfgBlobV1 v1;
    DeviceCfgBlobV2 v2;

    if (blob->magic != CFG_MAGIC) {
        return 0;
    }
    if (blob->version == CFG_VERSION_V1) {
        memcpy(&v1, blob, sizeof(v1));
        return cfg_blob_v1_valid(&v1);
    }
    if (blob->version == CFG_VERSION_V2) {
        memcpy(&v2, blob, sizeof(v2));
        return cfg_blob_v2_valid(&v2);
    }
    if (blob->version != CFG_VERSION) {
        return 0;
    }
    if (blob->crc32 != cfg_calc_crc(blob)) {
        return 0;
    }
    if ((blob->dev_addr == 0U) || (blob->dev_addr > 247U)) {
        return 0;
    }
    if (blob->dhcp_enable > 1U) {
        return 0;
    }
    if ((blob->alarm_enable_mask & ~CFG_ALARM_DOSE_DEFAULT) != 0U) {
        return 0;
    }
    if (blob->alarm_volume > 100U) {
        return 0;
    }
    if (blob->language > 1U) {
        return 0;
    }
    if (blob->alarm_sound > 1U) {
        return 0;
    }
    if (blob->alarm_light > 1U) {
        return 0;
    }
    if (!cfg_geiger_fields_valid(blob)) {
        return 0;
    }
    return 1;
}

static int cfg_blob_normalize(DeviceCfgBlob *blob)
{
    DeviceCfgBlobV1 v1;
    DeviceCfgBlobV2 v2;
    DeviceCfgBlobV3 v3;
    DeviceCfgBlobV4 v4;

    if (blob->magic != CFG_MAGIC) {
        return 0;
    }
    if (blob->version == CFG_VERSION) {
        if (!cfg_blob_valid(blob)) {
            return 0;
        }
        return 1;
    }
    if (blob->version == CFG_VERSION_V1) {
        memcpy(&v1, blob, sizeof(v1));
        if (!cfg_blob_v1_valid(&v1)) {
            return 0;
        }
        cfg_upgrade_v1_to_current(&v1, blob);
        return 1;
    }
    if (blob->version == CFG_VERSION_V2) {
        memcpy(&v2, blob, sizeof(v2));
        if (!cfg_blob_v2_valid(&v2)) {
            return 0;
        }
        cfg_upgrade_v2_to_v3(&v2, blob);
        return 1;
    }
    if (blob->version == CFG_VERSION_V3) {
        memcpy(&v3, blob, sizeof(v3));
        if (!cfg_blob_v3_valid(&v3)) {
            return 0;
        }
        cfg_upgrade_v3_to_v4(&v3, blob);
        return 1;
    }
    if (blob->version == CFG_VERSION_V4) {
        memcpy(&v4, blob, sizeof(v4));
        if (!cfg_blob_v4_valid(&v4)) {
            return 0;
        }
        cfg_upgrade_v4_to_v5(&v4, blob);
        return 1;
    }
    if (blob->version == CFG_VERSION_V5) {
        DeviceCfgBlobV5 v5;

        memcpy(&v5, blob, sizeof(v5));
        if (!cfg_blob_v5_valid(&v5)) {
            return 0;
        }
        cfg_upgrade_v5_to_v6(&v5, blob);
        return 1;
    }
    if (blob->version == CFG_VERSION_V6) {
        DeviceCfgBlobV6 v6;

        memcpy(&v6, blob, sizeof(v6));
        if (!cfg_blob_v6_valid(&v6)) {
            return 0;
        }
        cfg_upgrade_v6_to_v7(&v6, blob);
    }
    if (blob->version == CFG_VERSION_V7) {
        DeviceCfgBlobV7 v7;

        memcpy(&v7, blob, sizeof(v7));
        if (!cfg_blob_v7_valid(&v7)) {
            return 0;
        }
        cfg_upgrade_v7_to_v8(&v7, blob);
    }
    if (blob->version == CFG_VERSION_V8) {
        DeviceCfgBlobV9 *v9blob = (DeviceCfgBlobV9 *)blob;

        if (!cfg_blob_v8_valid(v9blob)) {
            return 0;
        }
        cfg_migrate_v8_alpha_to_x100(v9blob);
        v9blob->version = CFG_VERSION_V9;
        cfg_refresh_crc_v9(v9blob);
    }
    if (blob->version == CFG_VERSION_V9) {
        DeviceCfgBlobV9 v9;

        memcpy(&v9, blob, sizeof(v9));
        if (!cfg_blob_v9_valid(&v9)) {
            return 0;
        }
        cfg_upgrade_v9_to_v10(&v9, blob);
        return 1;
    }
    return 0;
}

static void cfg_log_invalid(const DeviceCfgBlob *blob, const char *label, uint32_t addr)
{
    if (blob->magic == 0xFFFFFFFFU) {
        printf("[CFG] W25Q empty %s @0x%06lX\r\n", label, (unsigned long)addr);
        return;
    }

    printf("[CFG] invalid %s @0x%06lX magic=0x%08lX crc=0x%08lX calc=0x%08lX addr=%u\r\n",
           label,
           (unsigned long)addr,
           (unsigned long)blob->magic,
           (unsigned long)blob->crc32,
           (unsigned long)cfg_calc_crc(blob),
           (unsigned)blob->dev_addr);
}

static int cfg_read_blob_w25q(uint32_t addr, DeviceCfgBlob *blob)
{
    if (W25Qx_QSPI_FastRead((uint8_t *)blob, addr, (uint32_t)sizeof(*blob)) != QSPI_OK) {
        return -1;
    }
    return 0;
}

static int cfg_read_blob_internal(uint32_t offset, DeviceCfgBlob *blob)
{
    return SetFlash_Read(offset, blob, (uint32_t)sizeof(*blob));
}

static int cfg_write_sector(uint32_t addr, const DeviceCfgBlob *blob)
{
    static uint8_t sector[EXT_FLASH_SECTOR_SIZE] __attribute__((aligned(4)));

    memcpy(sector, blob, sizeof(*blob));
    memset(sector + sizeof(*blob), 0xFF, EXT_FLASH_SECTOR_SIZE - sizeof(*blob));

    if (W25Qx_QSPI_Erase_Block(addr) != QSPI_OK) {
        return -1;
    }
    if (W25Qx_QSPI_Write(sector, addr, EXT_FLASH_SECTOR_SIZE) != QSPI_OK) {
        return -1;
    }
    return 0;
}

static int cfg_flush_to_flash(void)
{
    DeviceCfgBlob blob = s_cfg;
    int ret;

    cfg_refresh_crc(&blob);
    s_cfg = blob;

    flash_fs_lock();
    ret = cfg_write_sector(EXT_FLASH_CFG_PRIMARY_ADDR, &blob);
    if (ret == 0) {
        ret = cfg_write_sector(EXT_FLASH_CFG_BACKUP_ADDR, &blob);
    }
    flash_fs_unlock();

    if (ret != 0) {
        printf("[CFG] W25Q write fail\r\n");
    } else {
        printf("[CFG] W25Q saved @0x%06lX\r\n", (unsigned long)EXT_FLASH_CFG_PRIMARY_ADDR);
    }
    return ret;
}

static int cfg_try_load_internal(DeviceCfgBlob *out)
{
    DeviceCfgBlob blob;
    DeviceCfgBlob backup;
    int primary_ok = 0;
    int backup_ok = 0;

    if (cfg_read_blob_internal(SET_DEVICE_CFG_OFFSET, &blob) != 0) {
        return 0;
    }

    primary_ok = cfg_blob_valid(&blob);
    if (!primary_ok) {
        cfg_log_invalid(&blob, "internal", SET_DEVICE_CFG_ADDR);
    }

    if (cfg_read_blob_internal(SET_BACKUP_CFG_OFFSET, &backup) == 0) {
        backup_ok = cfg_blob_valid(&backup);
        if (!backup_ok && (backup.magic != 0xFFFFFFFFU)) {
            cfg_log_invalid(&backup, "internal-bak", SET_BACKUP_CFG_ADDR);
        }
    }

    if (primary_ok) {
        (void)cfg_blob_normalize(&blob);
        *out = blob;
        return 1;
    }
    if (backup_ok) {
        printf("[CFG] use internal backup copy\r\n");
        (void)cfg_blob_normalize(&backup);
        *out = backup;
        return 1;
    }
    return 0;
}

static void cfg_schedule_save(void)
{
    s_save_pending = 1U;
}

static int cfg_commit(void)
{
    cfg_schedule_save();
    return 0;
}

static void CfgPersistTask(void *argument)
{
    (void)argument;

    for (;;) {
        if (s_save_pending != 0U) {
            s_save_pending = 0U;
            if (cfg_flush_to_flash() != 0) {
                s_save_pending = 1U;
                (void)osDelay(500);
                continue;
            }
        }
        (void)osDelay(50);
    }
}

void DeviceConfig_TaskInit(void)
{
    static const osThreadAttr_t attr = {
        .name = "cfgPersist",
        .stack_size = 1024U * 4U,
        .priority = (osPriority_t)osPriorityBelowNormal,
    };

    (void)osThreadNew(CfgPersistTask, NULL, &attr);
}

static void cfg_apply_geiger_runtime(void)
{
    EwmaGlobalConfig ec;
    float sens = (float)s_cfg.geiger_sens_x100 / 100.0f;

    if (sens > 0.0f) {
        (void)DoseRate_SetSensitivity(sens);
        sys_cfg.sensitivity = sens;
    }

    ec.threshold_cps = (int)s_cfg.ewma_threshold_cps;
    ec.threshold_delta = (int)s_cfg.ewma_threshold_delta;
    ec.alpha_low = (float)s_cfg.ewma_alpha_low_x100 / 100.0f;
    ec.alpha_high = (float)s_cfg.ewma_alpha_high_x100 / 100.0f;
    ec.boost_duration = (int)s_cfg.ewma_boost_duration;
    (void)DoseRate_SetEwmaConfig(&ec);
    (void)DoseRate_SetRateLimitUsvh((float)s_cfg.rate_limit_x100 / 100.0f);
}

static void cfg_apply_runtime(void)
{
    if ((s_cfg.dev_addr == 0U) || (s_cfg.dev_addr > 247U)) {
        s_cfg.dev_addr = DEVICE_CFG_DEFAULT_DEV_ADDR;
    }
    Fsy_Dispatch_SetDeviceAddr(s_cfg.dev_addr);
    Fsy_Regmap_ApplyAlarmEnable(s_cfg.alarm_enable_mask);
    sys_cfg.alarm_volume = s_cfg.alarm_volume;
    sys_cfg.alarm_sound = s_cfg.alarm_sound;
    sys_cfg.alarm_light = s_cfg.alarm_light;
    sys_cfg.th_rh_rate = (float)s_cfg.dose_hi_x100 / 100.0f;
    sys_cfg.th_rl_rate = (float)s_cfg.dose_lo_x100 / 100.0f;
    sys_cfg.language = s_cfg.language;
    memset(sys_cfg.hw_version, 0, sizeof(sys_cfg.hw_version));
    memcpy(sys_cfg.hw_version, s_cfg.hw_version, CFG_MODEL_FIELD_LEN);
    cfg_apply_geiger_runtime();
}

void DeviceConfig_ApplyGeigerAlgorithm(void)
{
    if (s_ready != 0U) {
        cfg_apply_geiger_runtime();
    }
}

static void cfg_print(void)
{
    printf("[CFG] SN=%.*s model=%.*s addr=%u dhcp=%u ip=%u.%u.%u.%u dose_hi=%lu dose_lo=%lu alarm_en=0x%08lX vol=%u\r\n",
           (int)CFG_SN_FIELD_LEN, s_cfg.sn,
           (int)CFG_MODEL_FIELD_LEN, s_cfg.product_model,
           (unsigned)s_cfg.dev_addr,
           (unsigned)s_cfg.dhcp_enable,
           (unsigned)s_cfg.static_ip[0], (unsigned)s_cfg.static_ip[1],
           (unsigned)s_cfg.static_ip[2], (unsigned)s_cfg.static_ip[3],
           (unsigned long)s_cfg.dose_hi_x100,
           (unsigned long)s_cfg.dose_lo_x100,
           (unsigned long)s_cfg.alarm_enable_mask,
           (unsigned)s_cfg.alarm_volume);
}

static void store_u16_le(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)(v >> 8);
}

static uint16_t load_u16_le(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t load_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
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

static int reg_in_range(uint16_t reg, uint16_t start, uint16_t count)
{
    return (reg >= start) && (reg < (uint16_t)(start + count));
}

static int reg_range_inside(uint16_t start_reg, uint16_t reg_count,
                            uint16_t base_reg, uint16_t count)
{
    uint16_t end_reg;

    if (reg_count == 0U) {
        return 0;
    }
    end_reg = (uint16_t)(start_reg + reg_count - 1U);
    return reg_in_range(start_reg, base_reg, count) &&
           reg_in_range(end_reg, base_reg, count);
}

static int write_sn_regs(uint16_t start_reg, const uint8_t *data, uint16_t byte_count)
{
    uint16_t i;
    char sn[CFG_SN_FIELD_LEN + 1U];

    if (!reg_in_range(start_reg, 86U, 8U)) {
        return -1;
    }
    if ((uint16_t)(start_reg - 86U) * 2U + byte_count > 16U) {
        return -1;
    }

    memcpy(sn, s_cfg.sn, CFG_SN_FIELD_LEN);
    sn[CFG_SN_FIELD_LEN] = '\0';

    for (i = 0U; i < (uint16_t)(byte_count / 2U); i++) {
        uint16_t reg = (uint16_t)(start_reg + i);
        uint16_t off = (uint16_t)((reg - 86U) * 2U);
        uint16_t word = load_u16_le(&data[i * 2U]);

        if (off < CFG_SN_FIELD_LEN) {
            sn[off] = (char)(word & 0xFFU);
        }
        if ((off + 1U) < CFG_SN_FIELD_LEN) {
            sn[off + 1U] = (char)(word >> 8);
        }
    }

    cfg_fixed_copy(s_cfg.sn, CFG_SN_FIELD_LEN, sn);
    return cfg_commit();
}

static int write_model_regs(uint16_t start_reg, const uint8_t *data, uint16_t byte_count)
{
    uint16_t i;
    char model[CFG_MODEL_FIELD_LEN + 1U];

    if (!reg_in_range(start_reg, 130U, 8U)) {
        return -1;
    }
    if ((uint16_t)(start_reg - 130U) * 2U + byte_count > 16U) {
        return -1;
    }

    memcpy(model, s_cfg.product_model, CFG_MODEL_FIELD_LEN);
    model[CFG_MODEL_FIELD_LEN] = '\0';

    for (i = 0U; i < (uint16_t)(byte_count / 2U); i++) {
        uint16_t reg = (uint16_t)(start_reg + i);
        uint16_t off = (uint16_t)((reg - 130U) * 2U);
        uint16_t word = load_u16_le(&data[i * 2U]);

        if (off < CFG_MODEL_FIELD_LEN) {
            model[off] = (char)(word & 0xFFU);
        }
        if ((off + 1U) < CFG_MODEL_FIELD_LEN) {
            model[off + 1U] = (char)(word >> 8);
        }
    }

    cfg_fixed_copy(s_cfg.product_model, CFG_MODEL_FIELD_LEN, model);
    return cfg_commit();
}

static int write_hw_version_regs(uint16_t start_reg, const uint8_t *data, uint16_t byte_count)
{
    uint16_t i;
    char hw[CFG_MODEL_FIELD_LEN + 1U];

    if (!reg_in_range(start_reg, FSY_REG_HW_VERSION, FSY_REG_HW_VERSION_REGS)) {
        return -1;
    }
    if ((uint16_t)(start_reg - FSY_REG_HW_VERSION) * 2U + byte_count > 16U) {
        return -1;
    }

    memcpy(hw, s_cfg.hw_version, CFG_MODEL_FIELD_LEN);
    hw[CFG_MODEL_FIELD_LEN] = '\0';

    for (i = 0U; i < (uint16_t)(byte_count / 2U); i++) {
        uint16_t reg = (uint16_t)(start_reg + i);
        uint16_t off = (uint16_t)((reg - FSY_REG_HW_VERSION) * 2U);
        uint16_t word = load_u16_le(&data[i * 2U]);

        if (off < CFG_MODEL_FIELD_LEN) {
            hw[off] = (char)(word & 0xFFU);
        }
        if ((off + 1U) < CFG_MODEL_FIELD_LEN) {
            hw[off + 1U] = (char)(word >> 8);
        }
    }

    cfg_fixed_copy(s_cfg.hw_version, CFG_MODEL_FIELD_LEN, hw);
    memset(sys_cfg.hw_version, 0, sizeof(sys_cfg.hw_version));
    memcpy(sys_cfg.hw_version, s_cfg.hw_version, CFG_MODEL_FIELD_LEN);
    return cfg_commit();
}

static int write_name_regs(uint16_t start_reg, const uint8_t *data, uint16_t byte_count)
{
    uint16_t i;
    char name[CFG_MODEL_FIELD_LEN + 1U];

    if (!reg_in_range(start_reg, FSY_REG_PRODUCT_NAME, FSY_REG_PRODUCT_NAME_REGS)) {
        return -1;
    }
    if ((uint16_t)(start_reg - FSY_REG_PRODUCT_NAME) * 2U + byte_count > 16U) {
        return -1;
    }

    memcpy(name, s_cfg.product_name, CFG_MODEL_FIELD_LEN);
    name[CFG_MODEL_FIELD_LEN] = '\0';

    for (i = 0U; i < (uint16_t)(byte_count / 2U); i++) {
        uint16_t reg = (uint16_t)(start_reg + i);
        uint16_t off = (uint16_t)((reg - FSY_REG_PRODUCT_NAME) * 2U);
        uint16_t word = load_u16_le(&data[i * 2U]);

        if (off < CFG_MODEL_FIELD_LEN) {
            name[off] = (char)(word & 0xFFU);
        }
        if ((off + 1U) < CFG_MODEL_FIELD_LEN) {
            name[off + 1U] = (char)(word >> 8);
        }
    }

    cfg_fixed_copy(s_cfg.product_name, CFG_MODEL_FIELD_LEN, name);
    return cfg_commit();
}

static int write_addr_reg(uint16_t value)
{
    if (value == 0U) {
        return -1;
    }
    if (value > 247U) {
        value = 247U;
    }

    s_cfg.dev_addr = (uint8_t)value;
    cfg_apply_runtime();
    return cfg_commit();
}

static int read_ip_reg_pair(const uint8_t ip[4], uint16_t base_reg, uint16_t reg,
                            uint8_t *out, uint16_t out_cap)
{
    if (out_cap < 2U) {
        return -1;
    }
    if (reg == base_reg) {
        store_u16_le(out, (uint16_t)ip[0] | ((uint16_t)ip[1] << 8));
    } else if (reg == (uint16_t)(base_reg + 1U)) {
        store_u16_le(out, (uint16_t)ip[2] | ((uint16_t)ip[3] << 8));
    } else {
        return -1;
    }
    return 2;
}

static int write_static_ip_regs(uint16_t start_reg, const uint8_t *data, uint16_t byte_count)
{
    uint16_t i;
    uint8_t ip[4];

    if (!reg_in_range(start_reg, FSY_REG_STATIC_IP, FSY_REG_STATIC_IP_REGS)) {
        return -1;
    }
    if ((uint16_t)(start_reg - FSY_REG_STATIC_IP) * 2U + byte_count > 4U) {
        return -1;
    }

    memcpy(ip, s_cfg.static_ip, sizeof(ip));
    for (i = 0U; i < (uint16_t)(byte_count / 2U); i++) {
        uint16_t reg = (uint16_t)(start_reg + i);
        uint16_t off = (uint16_t)((reg - FSY_REG_STATIC_IP) * 2U);
        uint16_t word = load_u16_le(&data[i * 2U]);

        if (off < 4U) {
            ip[off] = (uint8_t)(word & 0xFFU);
        }
        if ((off + 1U) < 4U) {
            ip[off + 1U] = (uint8_t)(word >> 8);
        }
    }

    memcpy(s_cfg.static_ip, ip, sizeof(ip));
    return cfg_commit();
}

static int write_dhcp_reg(uint16_t value)
{
    s_cfg.dhcp_enable = (value != 0U) ? CFG_DHCP_ENABLE : CFG_DHCP_DISABLE;
    return cfg_commit();
}

static int write_u32_pair(uint16_t start_reg, const uint8_t *data, uint16_t byte_count,
                          uint16_t base_reg, uint32_t *value)
{
    uint16_t i;
    uint32_t next;

    if ((value == NULL) || !reg_range_inside(start_reg, (uint16_t)(byte_count / 2U),
                                             base_reg, FSY_REG_DWORD_REGS)) {
        return -1;
    }
    if ((uint16_t)(start_reg - base_reg) * 2U + byte_count > 4U) {
        return -1;
    }

    next = *value;
    for (i = 0U; i < (uint16_t)(byte_count / 2U); i++) {
        uint16_t reg = (uint16_t)(start_reg + i);
        uint32_t word = (uint32_t)load_u16_le(&data[i * 2U]);

        if (reg == base_reg) {
            next = (next & 0xFFFF0000UL) | word;
        } else {
            next = (next & 0x0000FFFFUL) | (word << 16);
        }
    }

    *value = next;
    return cfg_commit();
}

static int write_alarm_enable_regs(uint16_t start_reg, const uint8_t *data, uint16_t byte_count)
{
    int ret;

    ret = write_u32_pair(start_reg, data, byte_count,
                         FSY_REG_ALARM_ENABLE, &s_cfg.alarm_enable_mask);
    if (ret == 0) {
        s_cfg.alarm_enable_mask &= CFG_ALARM_DOSE_DEFAULT;
        Fsy_Regmap_ApplyAlarmEnable(s_cfg.alarm_enable_mask);
    }
    return ret;
}

static int write_alarm_volume_reg(uint16_t value)
{
    if (value > 100U) {
        return -1;
    }
    s_cfg.alarm_volume = (uint8_t)value;
    sys_cfg.alarm_volume = (uint8_t)value;
    if (value == 0U) {
        s_cfg.alarm_sound = 0U;
        sys_cfg.alarm_sound = 0U;
    } else if (sys_cfg.alarm_sound == 0U) {
        s_cfg.alarm_sound = 1U;
        sys_cfg.alarm_sound = 1U;
    }
    return cfg_commit();
}

static int write_language_reg(uint16_t value)
{
    if (value > 1U) {
        return -1;
    }
    s_cfg.language = (uint8_t)value;
    sys_cfg.language = (uint8_t)value;
    return cfg_commit();
}

static int read_ascii_reg_at(const char *text, size_t text_len,
                             uint16_t base_reg, uint16_t reg,
                             uint8_t *out, uint16_t out_cap)
{
    size_t off;
    uint8_t lo = 0U;
    uint8_t hi = 0U;

    if (out_cap < 2U) {
        return -1;
    }
    if (reg < base_reg) {
        return -1;
    }

    off = (size_t)(reg - base_reg) * 2U;
    if (off < text_len) {
        lo = (uint8_t)text[off];
    }
    if ((off + 1U) < text_len) {
        hi = (uint8_t)text[off + 1U];
    }
    store_u16_le(out, (uint16_t)lo | ((uint16_t)hi << 8));
    return 2;
}

uint8_t DeviceConfig_IsReady(void)
{
    return s_ready;
}

uint8_t DeviceConfig_GetDevAddr(void)
{
    return s_cfg.dev_addr;
}

const char *DeviceConfig_GetSn(void)
{
    return s_cfg.sn;
}

const char *DeviceConfig_GetProductModel(void)
{
    return s_cfg.product_model;
}

const char *DeviceConfig_GetProductName(void)
{
    return s_cfg.product_name;
}

const char *DeviceConfig_GetHwVersion(void)
{
    return s_cfg.hw_version;
}

void DeviceConfig_GetNetwork(uint8_t *dhcp_enable, uint8_t static_ip[4])
{
    if (dhcp_enable != NULL) {
        *dhcp_enable = s_cfg.dhcp_enable;
    }
    if (static_ip != NULL) {
        memcpy(static_ip, s_cfg.static_ip, 4U);
    }
}

void DeviceConfig_GetDoseAlarmConfig(uint32_t *hi_x100, uint32_t *lo_x100,
                                     uint32_t *alarm_enable_mask,
                                     uint8_t *alarm_volume)
{
    if (hi_x100 != NULL) {
        *hi_x100 = s_cfg.dose_hi_x100;
    }
    if (lo_x100 != NULL) {
        *lo_x100 = s_cfg.dose_lo_x100;
    }
    if (alarm_enable_mask != NULL) {
        *alarm_enable_mask = s_cfg.alarm_enable_mask;
    }
    if (alarm_volume != NULL) {
        *alarm_volume = s_cfg.alarm_volume;
    }
}

int DeviceConfig_Init(void)
{
    DeviceCfgBlob blob;
    DeviceCfgBlob backup;
    int primary_ok = 0;
    int backup_ok = 0;
    int migrated = 0;
    int need_resave = 0;

    if (W25Q_Port_Init() != 0) {
        printf("[CFG] W25Q unavailable\r\n");
        if (cfg_try_load_internal(&blob) != 0) {
            s_cfg = blob;
            cfg_apply_runtime();
            s_ready = 1U;
            cfg_print();
            return 0;
        }
        printf("[CFG] RAM defaults (no storage)\r\n");
        cfg_apply_defaults();
        cfg_apply_runtime();
        s_ready = 1U;
        cfg_print();
        return -1;
    }

    flash_fs_lock();
    if (cfg_read_blob_w25q(EXT_FLASH_CFG_PRIMARY_ADDR, &blob) == 0) {
        primary_ok = cfg_blob_valid(&blob);
        if (!primary_ok) {
            cfg_log_invalid(&blob, "primary", EXT_FLASH_CFG_PRIMARY_ADDR);
        }
    } else {
        printf("[CFG] W25Q primary read fail\r\n");
    }

    if (cfg_read_blob_w25q(EXT_FLASH_CFG_BACKUP_ADDR, &backup) == 0) {
        backup_ok = cfg_blob_valid(&backup);
        if (!backup_ok && (backup.magic != 0xFFFFFFFFU)) {
            cfg_log_invalid(&backup, "backup", EXT_FLASH_CFG_BACKUP_ADDR);
        }
    }
    flash_fs_unlock();

    if (primary_ok) {
        if (blob.version == CFG_VERSION_V1 || blob.version == CFG_VERSION_V5) {
            need_resave = 1;
        }
        (void)cfg_blob_normalize(&blob);
        s_cfg = blob;
    } else if (backup_ok) {
        printf("[CFG] use W25Q backup copy\r\n");
        if (backup.version == CFG_VERSION_V1 || backup.version == CFG_VERSION_V5) {
            need_resave = 1;
        }
        (void)cfg_blob_normalize(&backup);
        s_cfg = backup;
    } else if (cfg_try_load_internal(&blob) != 0) {
        printf("[CFG] migrate internal -> W25Q\r\n");
        s_cfg = blob;
        migrated = 1;
    } else {
        printf("[CFG] no valid config, RAM defaults (save via GUI)\r\n");
        cfg_apply_defaults();
        cfg_refresh_crc(&s_cfg);
    }

    cfg_apply_runtime();
    s_ready = 1U;
    cfg_print();

    if (migrated != 0 || need_resave != 0) {
        (void)cfg_flush_to_flash();
    }

    return 0;
}

int DeviceConfig_ReadRegBlock(uint16_t start_reg, uint16_t reg_count,
                              uint8_t *out, uint16_t out_cap)
{
    uint16_t i;

    if ((out == NULL) || (reg_count == 0U)) {
        return -1;
    }
    if (out_cap < (uint16_t)(reg_count * 2U)) {
        return -1;
    }

    memset(out, 0, (size_t)reg_count * 2U);

    for (i = 0U; i < reg_count; i++) {
        uint16_t reg = (uint16_t)(start_reg + i);
        uint8_t pair[2];

        if (reg_in_range(reg, 86U, 8U)) {
            if (read_ascii_reg_at(s_cfg.sn, CFG_SN_FIELD_LEN, 86U, reg, pair, sizeof(pair)) < 0) {
                return -1;
            }
            memcpy(&out[i * 2U], pair, 2U);
        } else if (reg == 121U) {
            store_u16_le(&out[i * 2U], s_cfg.dev_addr);
        } else if (reg == FSY_REG_ALARM_VOLUME) {
            store_u16_le(&out[i * 2U], s_cfg.alarm_volume);
        } else if (reg_in_range(reg, FSY_REG_DOSE_HI_TH, FSY_REG_DWORD_REGS)) {
            store_u32_reg_at(&out[i * 2U], s_cfg.dose_hi_x100, FSY_REG_DOSE_HI_TH, reg);
        } else if (reg_in_range(reg, FSY_REG_DOSE_LO_TH, FSY_REG_DWORD_REGS)) {
            store_u32_reg_at(&out[i * 2U], s_cfg.dose_lo_x100, FSY_REG_DOSE_LO_TH, reg);
        } else if (reg_in_range(reg, FSY_REG_ALARM_ENABLE, FSY_REG_ALARM_ENABLE_REGS)) {
            store_u32_reg_at(&out[i * 2U], s_cfg.alarm_enable_mask, FSY_REG_ALARM_ENABLE, reg);
        } else if (reg_in_range(reg, 130U, 8U)) {
            if (read_ascii_reg_at(s_cfg.product_model, CFG_MODEL_FIELD_LEN, 130U, reg, pair, sizeof(pair)) < 0) {
                return -1;
            }
            memcpy(&out[i * 2U], pair, 2U);
        } else if (reg_in_range(reg, FSY_REG_PRODUCT_NAME, FSY_REG_PRODUCT_NAME_REGS)) {
            if (read_ascii_reg_at(s_cfg.product_name, CFG_MODEL_FIELD_LEN,
                                   FSY_REG_PRODUCT_NAME, reg, pair, sizeof(pair)) < 0) {
                return -1;
            }
            memcpy(&out[i * 2U], pair, 2U);
        } else if (reg_in_range(reg, FSY_REG_STATIC_IP, FSY_REG_STATIC_IP_REGS)) {
            if (read_ip_reg_pair(s_cfg.static_ip, FSY_REG_STATIC_IP, reg, pair, sizeof(pair)) < 0) {
                return -1;
            }
            memcpy(&out[i * 2U], pair, 2U);
        } else if (reg_in_range(reg, FSY_REG_GEIGER_SENS, FSY_REG_GEIGER_PARAM_REGS)) {
            store_u32_reg_at(&out[i * 2U], s_cfg.geiger_sens_x100, FSY_REG_GEIGER_SENS, reg);
        } else if (reg_in_range(reg, FSY_REG_EWMA_THRESHOLD_CPS, FSY_REG_GEIGER_PARAM_REGS)) {
            store_u32_reg_at(&out[i * 2U], s_cfg.ewma_threshold_cps,
                             FSY_REG_EWMA_THRESHOLD_CPS, reg);
        } else if (reg_in_range(reg, FSY_REG_EWMA_THRESHOLD_DELTA, FSY_REG_GEIGER_PARAM_REGS)) {
            store_u32_reg_at(&out[i * 2U], s_cfg.ewma_threshold_delta,
                             FSY_REG_EWMA_THRESHOLD_DELTA, reg);
        } else if (reg_in_range(reg, FSY_REG_EWMA_ALPHA_LOW, FSY_REG_GEIGER_PARAM_REGS)) {
            store_u32_reg_at(&out[i * 2U], s_cfg.ewma_alpha_low_x100,
                             FSY_REG_EWMA_ALPHA_LOW, reg);
        } else if (reg_in_range(reg, FSY_REG_EWMA_ALPHA_HIGH, FSY_REG_GEIGER_PARAM_REGS)) {
            store_u32_reg_at(&out[i * 2U], s_cfg.ewma_alpha_high_x100,
                             FSY_REG_EWMA_ALPHA_HIGH, reg);
        } else if (reg_in_range(reg, FSY_REG_EWMA_BOOST_DURATION, FSY_REG_GEIGER_PARAM_REGS)) {
            store_u32_reg_at(&out[i * 2U], s_cfg.ewma_boost_duration,
                             FSY_REG_EWMA_BOOST_DURATION, reg);
        } else if (reg_in_range(reg, FSY_REG_RATE_LIMIT, FSY_REG_GEIGER_PARAM_REGS)) {
            store_u32_reg_at(&out[i * 2U], s_cfg.rate_limit_x100, FSY_REG_RATE_LIMIT, reg);
        } else if (reg == FSY_REG_DHCP_ENABLE) {
            store_u16_le(&out[i * 2U], s_cfg.dhcp_enable);
        } else if (reg == FSY_REG_LANGUAGE) {
            store_u16_le(&out[i * 2U], s_cfg.language);
        } else if (reg_in_range(reg, FSY_REG_HW_VERSION, FSY_REG_HW_VERSION_REGS)) {
            if (read_ascii_reg_at(s_cfg.hw_version, CFG_MODEL_FIELD_LEN,
                                   FSY_REG_HW_VERSION, reg, pair, sizeof(pair)) < 0) {
                return -1;
            }
            memcpy(&out[i * 2U], pair, 2U);
        } else {
            store_u16_le(&out[i * 2U], 0U);
        }
    }

    return (int)(reg_count * 2U);
}

int DeviceConfig_WriteRegBlock(uint16_t start_reg, const uint8_t *data,
                               uint16_t byte_count)
{
    uint16_t i;
    int ret = 0;

    if ((data == NULL) || ((byte_count % 2U) != 0U) || (byte_count == 0U)) {
        return -1;
    }

    if (reg_in_range(start_reg, 86U, 8U) &&
        reg_in_range((uint16_t)(start_reg + (byte_count / 2U) - 1U), 86U, 8U)) {
        return write_sn_regs(start_reg, data, byte_count);
    }

    if (reg_in_range(start_reg, 130U, 8U) &&
        reg_in_range((uint16_t)(start_reg + (byte_count / 2U) - 1U), 130U, 8U)) {
        return write_model_regs(start_reg, data, byte_count);
    }

    if (reg_in_range(start_reg, FSY_REG_PRODUCT_NAME, FSY_REG_PRODUCT_NAME_REGS) &&
        reg_in_range((uint16_t)(start_reg + (byte_count / 2U) - 1U),
                     FSY_REG_PRODUCT_NAME, FSY_REG_PRODUCT_NAME_REGS)) {
        return write_name_regs(start_reg, data, byte_count);
    }

    if (reg_in_range(start_reg, FSY_REG_STATIC_IP, FSY_REG_STATIC_IP_REGS) &&
        reg_in_range((uint16_t)(start_reg + (byte_count / 2U) - 1U),
                     FSY_REG_STATIC_IP, FSY_REG_STATIC_IP_REGS)) {
        return write_static_ip_regs(start_reg, data, byte_count);
    }

    if (reg_in_range(start_reg, FSY_REG_HW_VERSION, FSY_REG_HW_VERSION_REGS) &&
        reg_in_range((uint16_t)(start_reg + (byte_count / 2U) - 1U),
                     FSY_REG_HW_VERSION, FSY_REG_HW_VERSION_REGS)) {
        return write_hw_version_regs(start_reg, data, byte_count);
    }

    if (reg_range_inside(start_reg, (uint16_t)(byte_count / 2U),
                         FSY_REG_DOSE_HI_TH, FSY_REG_DWORD_REGS)) {
        return write_u32_pair(start_reg, data, byte_count,
                              FSY_REG_DOSE_HI_TH, &s_cfg.dose_hi_x100);
    }

    if (reg_range_inside(start_reg, (uint16_t)(byte_count / 2U),
                         FSY_REG_DOSE_LO_TH, FSY_REG_DWORD_REGS)) {
        return write_u32_pair(start_reg, data, byte_count,
                              FSY_REG_DOSE_LO_TH, &s_cfg.dose_lo_x100);
    }

    if (reg_range_inside(start_reg, (uint16_t)(byte_count / 2U),
                         FSY_REG_ALARM_ENABLE, FSY_REG_ALARM_ENABLE_REGS)) {
        return write_alarm_enable_regs(start_reg, data, byte_count);
    }

    if (reg_range_inside(start_reg, (uint16_t)(byte_count / 2U),
                         FSY_REG_GEIGER_SENS, FSY_REG_GEIGER_PARAM_REGS)) {
        int ret = write_u32_pair(start_reg, data, byte_count,
                                 FSY_REG_GEIGER_SENS, &s_cfg.geiger_sens_x100);
        if (ret == 0) {
            cfg_apply_geiger_runtime();
        }
        return ret;
    }
    if (reg_range_inside(start_reg, (uint16_t)(byte_count / 2U),
                         FSY_REG_EWMA_THRESHOLD_CPS, FSY_REG_GEIGER_PARAM_REGS)) {
        int ret = write_u32_pair(start_reg, data, byte_count,
                                 FSY_REG_EWMA_THRESHOLD_CPS, &s_cfg.ewma_threshold_cps);
        if (ret == 0) {
            cfg_apply_geiger_runtime();
        }
        return ret;
    }
    if (reg_range_inside(start_reg, (uint16_t)(byte_count / 2U),
                         FSY_REG_EWMA_THRESHOLD_DELTA, FSY_REG_GEIGER_PARAM_REGS)) {
        int ret = write_u32_pair(start_reg, data, byte_count,
                                 FSY_REG_EWMA_THRESHOLD_DELTA, &s_cfg.ewma_threshold_delta);
        if (ret == 0) {
            cfg_apply_geiger_runtime();
        }
        return ret;
    }
    if (reg_range_inside(start_reg, (uint16_t)(byte_count / 2U),
                         FSY_REG_EWMA_ALPHA_LOW, FSY_REG_GEIGER_PARAM_REGS)) {
        int ret = write_u32_pair(start_reg, data, byte_count,
                                 FSY_REG_EWMA_ALPHA_LOW, &s_cfg.ewma_alpha_low_x100);
        if (ret == 0) {
            cfg_apply_geiger_runtime();
        }
        return ret;
    }
    if (reg_range_inside(start_reg, (uint16_t)(byte_count / 2U),
                         FSY_REG_EWMA_ALPHA_HIGH, FSY_REG_GEIGER_PARAM_REGS)) {
        int ret = write_u32_pair(start_reg, data, byte_count,
                                 FSY_REG_EWMA_ALPHA_HIGH, &s_cfg.ewma_alpha_high_x100);
        if (ret == 0) {
            cfg_apply_geiger_runtime();
        }
        return ret;
    }
    if (reg_range_inside(start_reg, (uint16_t)(byte_count / 2U),
                         FSY_REG_EWMA_BOOST_DURATION, FSY_REG_GEIGER_PARAM_REGS)) {
        int ret = write_u32_pair(start_reg, data, byte_count,
                                 FSY_REG_EWMA_BOOST_DURATION, &s_cfg.ewma_boost_duration);
        if (ret == 0) {
            cfg_apply_geiger_runtime();
        }
        return ret;
    }
    if (reg_range_inside(start_reg, (uint16_t)(byte_count / 2U),
                         FSY_REG_RATE_LIMIT, FSY_REG_GEIGER_PARAM_REGS)) {
        int ret = write_u32_pair(start_reg, data, byte_count,
                                 FSY_REG_RATE_LIMIT, &s_cfg.rate_limit_x100);
        if (ret == 0) {
            cfg_apply_geiger_runtime();
        }
        return ret;
    }

    for (i = 0U; i < (byte_count / 2U); i++) {
        uint16_t reg = (uint16_t)(start_reg + i);
        uint16_t value = load_u16_le(&data[i * 2U]);

        if (reg == 121U) {
            ret = write_addr_reg(value);
        } else if (reg == FSY_REG_ALARM_VOLUME) {
            ret = write_alarm_volume_reg(value);
        } else if (reg_in_range(reg, FSY_REG_DOSE_HI_TH, FSY_REG_DWORD_REGS)) {
            ret = write_u32_pair(reg, &data[i * 2U], 2U,
                                 FSY_REG_DOSE_HI_TH, &s_cfg.dose_hi_x100);
        } else if (reg_in_range(reg, FSY_REG_DOSE_LO_TH, FSY_REG_DWORD_REGS)) {
            ret = write_u32_pair(reg, &data[i * 2U], 2U,
                                 FSY_REG_DOSE_LO_TH, &s_cfg.dose_lo_x100);
        } else if (reg_in_range(reg, FSY_REG_ALARM_ENABLE, FSY_REG_ALARM_ENABLE_REGS)) {
            ret = write_alarm_enable_regs(reg, &data[i * 2U], 2U);
        } else if (reg == FSY_REG_DHCP_ENABLE) {
            ret = write_dhcp_reg(value);
        } else if (reg == FSY_REG_LANGUAGE) {
            ret = write_language_reg(value);
        } else if (reg_in_range(reg, 86U, 8U)) {
            ret = write_sn_regs(reg, &data[i * 2U], 2U);
        } else if (reg_in_range(reg, 130U, 8U)) {
            ret = write_model_regs(reg, &data[i * 2U], 2U);
        } else if (reg_in_range(reg, FSY_REG_PRODUCT_NAME, FSY_REG_PRODUCT_NAME_REGS)) {
            ret = write_name_regs(reg, &data[i * 2U], 2U);
        } else if (reg_in_range(reg, FSY_REG_HW_VERSION, FSY_REG_HW_VERSION_REGS)) {
            ret = write_hw_version_regs(reg, &data[i * 2U], 2U);
        } else if (reg_in_range(reg, FSY_REG_STATIC_IP, FSY_REG_STATIC_IP_REGS)) {
            ret = write_static_ip_regs(reg, &data[i * 2U], 2U);
        } else {
            return -1;
        }

        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

int DeviceConfig_SetAlarmSound(uint8_t on)
{
    s_cfg.alarm_sound = (on != 0U) ? 1U : 0U;
    sys_cfg.alarm_sound = s_cfg.alarm_sound;
    return cfg_commit();
}

int DeviceConfig_SetAlarmLight(uint8_t on)
{
    s_cfg.alarm_light = (on != 0U) ? 1U : 0U;
    sys_cfg.alarm_light = s_cfg.alarm_light;
    return cfg_commit();
}

void DeviceConfig_GetAlarmOutput(uint8_t *sound, uint8_t *light, uint8_t *volume)
{
    if (volume != NULL) {
        *volume = s_cfg.alarm_volume;
    }
    if (sound != NULL) {
        *sound = s_cfg.alarm_sound;
    }
    if (light != NULL) {
        *light = s_cfg.alarm_light;
    }
}

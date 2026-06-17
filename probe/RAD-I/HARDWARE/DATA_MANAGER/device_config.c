#include "device_config.h"
#include "geiger.h"
#include "ext_flash_layout.h"
#include "flash_fs_mutex.h"
#include "w25qxx.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "../../dev_protocol/net_raw/net_raw_bsp.h"  // 用于 Net_Device_Update_Addr


// 配置版本（v2：增加 reg82 剂量阈值 Flash 暂存字段）
#define CFG_VERSION 2U
#define CFG_VERSION_V1 1U


#define CFG_MAGIC 0x44455643u

typedef struct __attribute__((packed))
{
    uint32_t magic;
    uint32_t version;
    char sn[DEVICE_CFG_SN_LEN + 1];
    float sensitivity;

    float th_rl_rate;
    float th_rh_rate;
    float temp_th_hi;
    float temp_th_lo;
    float press_th_hi;
    float press_th_lo;
    float hum_th_hi;
    float hum_th_lo;
    uint32_t co2_th_hi;
    uint32_t co2_th_lo;
    uint16_t pm25_th_hi;
    uint16_t pm25_th_lo;

    uint8_t alarm_sound;
    uint8_t alarm_light;
    uint8_t alarm_volume;
    uint8_t display_enable;
    uint8_t bright_sz;
    uint8_t dev_addr;
    uint8_t language;

    float th_rh_rate_saved;
    float th_rl_rate_saved;
    uint8_t dose_th_shadow_flags;
    uint8_t _shadow_rsvd[3];

    char hw[DEVICE_CFG_HW_VER_LEN];
} DeviceCfgBlob;

/* v1 布局（无 reg82 暂存字段），用于从旧 Flash 迁移 */
typedef struct __attribute__((packed))
{
    uint32_t magic;
    uint32_t version;
    char sn[DEVICE_CFG_SN_LEN + 1];
    float sensitivity;

    float th_rl_rate;
    float th_rh_rate;
    float temp_th_hi;
    float temp_th_lo;
    float press_th_hi;
    float press_th_lo;
    float hum_th_hi;
    float hum_th_lo;
    uint32_t co2_th_hi;
    uint32_t co2_th_lo;
    uint16_t pm25_th_hi;
    uint16_t pm25_th_lo;

    uint8_t alarm_sound;
    uint8_t alarm_light;
    uint8_t alarm_volume;
    uint8_t display_enable;
    uint8_t bright_sz;
    uint8_t dev_addr;
    uint8_t language;

    char hw[DEVICE_CFG_HW_VER_LEN];
} DeviceCfgBlobV1;

static uint8_t s_ready;
static uint8_t s_cfg_sector[EXT_FLASH_META_SECTOR_SIZE];

extern bool update_sys_cfg;


static void blob_copy_str_fixed(char *dst, size_t width, const char *src)
{
    size_t n;

    if (dst == NULL || width == 0U)
        return;

    if (src == NULL) {
        memset(dst, 0, width);
        return;
    }
    n = strlen(src);
    if (n > width)
        n = width;

    memcpy(dst, src, n);
    if (n < width)
        memset(dst + n, 0, width - n);
}

static void blob_fixed_to_cstr(char *dst, size_t dst_cap, const char *src, size_t src_len)
{
    size_t n;

    if (dst == NULL || dst_cap == 0U)
        return;
    n = src_len;
    if (n >= dst_cap)
        n = dst_cap - 1U;
    
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static void copy_fixed_width_pad(char *dst, size_t width, const char *src)
{
    size_t n;

    if (dst == NULL || width == 0U) {
        return;
    }
    if (src == NULL) {
        memset(dst, 0, width + 1U);
        return;
    }
    n = strlen(src);
    if (n > width) {
        n = width;
    }
    memcpy(dst, src, n);
    memset(dst + n, 0, width + 1U - n);
}

static void apply_defaults_to_sys(void)
{
    strncpy((void *)sys_cfg.SN, DEVICE_CFG_DEFAULT_SN, (size_t)SN_LEN);
    sys_cfg.SN[SN_LEN] = '\0';
    strncpy((void *)sys_cfg.hw_version, DEVICE_CFG_DEFAULT_HW, (size_t)HW_VER_LEN);
    sys_cfg.hw_version[HW_VER_LEN] = '\0';
    sys_cfg.sensitivity = DEVICE_CFG_DEFAULT_SENS;

    /* 使用宏定义的默认值 */
    sys_cfg.th_rl_rate = DEVICE_CFG_DEFAULT_RATE_TH_RL;
    sys_cfg.th_rh_rate = DEVICE_CFG_DEFAULT_RATE_TH_RH;
    sys_cfg.th_rh_rate_saved = 0.0f;
    sys_cfg.th_rl_rate_saved = 0.0f;
    sys_cfg.dose_th_shadow_flags = 0U;
    sys_cfg.temp_th_hi = DEVICE_CFG_DEFAULT_TEMP_TH_HI;
    sys_cfg.temp_th_lo = DEVICE_CFG_DEFAULT_TEMP_TH_LO;
    sys_cfg.press_th_hi = DEVICE_CFG_DEFAULT_PRESS_TH_HI;
    sys_cfg.press_th_lo = DEVICE_CFG_DEFAULT_PRESS_TH_LO;
    sys_cfg.hum_th_hi = DEVICE_CFG_DEFAULT_HUM_TH_HI;
    sys_cfg.hum_th_lo = DEVICE_CFG_DEFAULT_HUM_TH_LO;
    sys_cfg.co2_th_hi = DEVICE_CFG_DEFAULT_CO2_TH_HI;
    sys_cfg.co2_th_lo = DEVICE_CFG_DEFAULT_CO2_TH_LO;
    sys_cfg.pm25_th_hi = DEVICE_CFG_DEFAULT_PM25_TH_HI;
    sys_cfg.pm25_th_lo = DEVICE_CFG_DEFAULT_PM25_TH_LO;
    sys_cfg.alarm_sound = DEVICE_CFG_DEFAULT_ALARM_SOUND;
    sys_cfg.alarm_light = DEVICE_CFG_DEFAULT_ALARM_LIGHT;
    sys_cfg.alarm_volume = DEVICE_CFG_DEFAULT_ALARM_VOLUME;
    sys_cfg.alarm_volume_saved = 0U;
    sys_cfg.display_enable = DEVICE_CFG_DEFAULT_DISPLAY;
    sys_cfg.bright_sz = (float)DEVICE_CFG_DEFAULT_BRIGHT;
    sys_cfg.dev_addr = DEVICE_CFG_DEFAULT_DEV_ADDR;
    sys_cfg.language = DEVICE_CFG_DEFAULT_LANGUAGE;
}

static void apply_blob_v1_to_sys(const DeviceCfgBlobV1 *b)
{
    strncpy((void *)sys_cfg.SN, b->sn, (size_t)SN_LEN);
    sys_cfg.SN[SN_LEN] = '\0';
    blob_fixed_to_cstr((void *)sys_cfg.hw_version, sizeof(sys_cfg.hw_version), b->hw, sizeof(b->hw));
    sys_cfg.sensitivity = b->sensitivity;

    sys_cfg.th_rl_rate = b->th_rl_rate;
    sys_cfg.th_rh_rate = b->th_rh_rate;
    sys_cfg.th_rh_rate_saved = 0.0f;
    sys_cfg.th_rl_rate_saved = 0.0f;
    sys_cfg.dose_th_shadow_flags = 0U;
    sys_cfg.temp_th_hi = b->temp_th_hi;
    sys_cfg.temp_th_lo = b->temp_th_lo;
    sys_cfg.press_th_hi = b->press_th_hi;
    sys_cfg.press_th_lo = b->press_th_lo;
    sys_cfg.hum_th_hi = b->hum_th_hi;
    sys_cfg.hum_th_lo = b->hum_th_lo;
    sys_cfg.co2_th_hi = b->co2_th_hi;
    sys_cfg.co2_th_lo = b->co2_th_lo;
    sys_cfg.pm25_th_hi = b->pm25_th_hi;
    sys_cfg.pm25_th_lo = b->pm25_th_lo;
    sys_cfg.alarm_sound = b->alarm_sound;
    sys_cfg.alarm_light = b->alarm_light;
    sys_cfg.alarm_volume = b->alarm_volume;
    sys_cfg.alarm_volume_saved = 0U;
    sys_cfg.display_enable = DEVICE_CFG_DEFAULT_DISPLAY;
    sys_cfg.bright_sz = (float)DEVICE_CFG_DEFAULT_BRIGHT;
    sys_cfg.dev_addr = DEVICE_CFG_DEFAULT_DEV_ADDR;
    sys_cfg.language = DEVICE_CFG_DEFAULT_LANGUAGE;
}

static void apply_blob_to_sys(const DeviceCfgBlob *b)
{
    strncpy((void *)sys_cfg.SN, b->sn, (size_t)SN_LEN);
    sys_cfg.SN[SN_LEN] = '\0';
    blob_fixed_to_cstr((void *)sys_cfg.hw_version, sizeof(sys_cfg.hw_version), b->hw, sizeof(b->hw));
    sys_cfg.sensitivity = b->sensitivity;

    sys_cfg.th_rl_rate = b->th_rl_rate;
    sys_cfg.th_rh_rate = b->th_rh_rate;
    sys_cfg.th_rh_rate_saved = b->th_rh_rate_saved;
    sys_cfg.th_rl_rate_saved = b->th_rl_rate_saved;
    sys_cfg.dose_th_shadow_flags = b->dose_th_shadow_flags;
    sys_cfg.temp_th_hi = b->temp_th_hi;
    sys_cfg.temp_th_lo = b->temp_th_lo;
    sys_cfg.press_th_hi = b->press_th_hi;
    sys_cfg.press_th_lo = b->press_th_lo;
    sys_cfg.hum_th_hi = b->hum_th_hi;
    sys_cfg.hum_th_lo = b->hum_th_lo;
    sys_cfg.co2_th_hi = b->co2_th_hi;
    sys_cfg.co2_th_lo = b->co2_th_lo;
    sys_cfg.pm25_th_hi = b->pm25_th_hi;
    sys_cfg.pm25_th_lo = b->pm25_th_lo;
    sys_cfg.alarm_sound = b->alarm_sound;
    sys_cfg.alarm_light = b->alarm_light;
    sys_cfg.alarm_volume = b->alarm_volume;
    sys_cfg.alarm_volume_saved = b->_shadow_rsvd[0];
    sys_cfg.display_enable = b->display_enable;
    sys_cfg.bright_sz = b->bright_sz;
    sys_cfg.dev_addr = b->dev_addr;
    sys_cfg.language = b->language;
}

static void fill_blob_from_sys(DeviceCfgBlob *b)
{
    memset(b, 0, sizeof(*b));
    b->magic = CFG_MAGIC;
    b->version = CFG_VERSION;
    copy_fixed_width_pad(b->sn, (size_t)DEVICE_CFG_SN_LEN, (void *)sys_cfg.SN);
    b->sensitivity = sys_cfg.sensitivity;

    b->th_rl_rate = sys_cfg.th_rl_rate;
    b->th_rh_rate = sys_cfg.th_rh_rate;
    b->th_rh_rate_saved = sys_cfg.th_rh_rate_saved;
    b->th_rl_rate_saved = sys_cfg.th_rl_rate_saved;
    b->dose_th_shadow_flags = sys_cfg.dose_th_shadow_flags;
    b->temp_th_hi = sys_cfg.temp_th_hi;
    b->temp_th_lo = sys_cfg.temp_th_lo;
    b->press_th_hi = sys_cfg.press_th_hi;
    b->press_th_lo = sys_cfg.press_th_lo;
    b->hum_th_hi = sys_cfg.hum_th_hi;
    b->hum_th_lo = sys_cfg.hum_th_lo;
    b->co2_th_hi = sys_cfg.co2_th_hi;
    b->co2_th_lo = sys_cfg.co2_th_lo;
    b->pm25_th_hi = sys_cfg.pm25_th_hi;
    b->pm25_th_lo = sys_cfg.pm25_th_lo;
    b->alarm_sound = sys_cfg.alarm_sound;
    b->alarm_light = sys_cfg.alarm_light;
    b->alarm_volume = sys_cfg.alarm_volume;
    b->_shadow_rsvd[0] = sys_cfg.alarm_volume_saved;
    b->display_enable = sys_cfg.display_enable;
    b->bright_sz = sys_cfg.bright_sz;
    b->dev_addr = sys_cfg.dev_addr;
    b->language = sys_cfg.language;

    blob_copy_str_fixed(b->hw, sizeof(b->hw), (void *)sys_cfg.hw_version);
}

static int cfg_blob_write(const DeviceCfgBlob *blob)
{
    uint32_t base = EXT_FLASH_CONFIG_REGION_BASE;

    memcpy(s_cfg_sector, blob, sizeof(DeviceCfgBlob));
    memset(s_cfg_sector + sizeof(DeviceCfgBlob), 0xFF,
            EXT_FLASH_META_SECTOR_SIZE - sizeof(DeviceCfgBlob));
    if (W25Qx_QSPI_Erase_Block(base) != QSPI_OK) {
        return -4;
    }
    if (W25Qx_QSPI_Write(s_cfg_sector, base, EXT_FLASH_META_SECTOR_SIZE) != QSPI_OK) {
        return -4;
    }
    return 0;
}

static int cfg_blob_read_v2(DeviceCfgBlob *out)
{
    if (W25Qx_QSPI_FastRead((uint8_t *)out, EXT_FLASH_CONFIG_REGION_BASE, sizeof(DeviceCfgBlob)) != QSPI_OK) {
        return -4;
    }
    return 0;
}

static int write_runtime_to_flash(void)
{
    DeviceCfgBlob b;

    fill_blob_from_sys(&b);
    return cfg_blob_write(&b);
}

/** 与 Flash v2 块字段一致，供 Init 与 cfg,read 共用 */
static void print_cfg_blob_human(const DeviceCfgBlob *b)
{
    char hwpr[DEVICE_CFG_HW_VER_LEN + 1];

    if (b == NULL)
        return;

    blob_fixed_to_cstr(hwpr, sizeof(hwpr), b->hw, sizeof(b->hw));
    printf("序列号：%s\r\n", b->sn);
    printf("硬件版本：%s\r\n", hwpr);
    printf("灵敏度：%.6f\r\n", (double)b->sensitivity);
    printf("剂量率上限：%.2f uSv/h\r\n", (double)b->th_rh_rate);
    printf("剂量率下限：%.2f uSv/h\r\n", (double)b->th_rl_rate);
    printf("温度上限：%.2f C\r\n", (double)b->temp_th_hi);
    printf("温度下限：%.2f C\r\n", (double)b->temp_th_lo);
    printf("气压上限：%.1f hPa\r\n", (double)b->press_th_hi);
    printf("气压下限：%.1f hPa\r\n", (double)b->press_th_lo);
    printf("湿度上限：%.1f %%RH\r\n", (double)b->hum_th_hi);
    printf("湿度下限：%.1f %%RH\r\n", (double)b->hum_th_lo);
    printf("CO2 上限：%lu ppm\r\n", (unsigned long)b->co2_th_hi);
    printf("CO2 下限：%lu ppm\r\n", (unsigned long)b->co2_th_lo);
    printf("PM2.5 上限：%u\r\n", (unsigned)b->pm25_th_hi);
    printf("PM2.5 下限：%u\r\n", (unsigned)b->pm25_th_lo);
    printf("声报警：%s\r\n", b->alarm_sound ? "开" : "关");
    printf("光报警：%s\r\n", b->alarm_light ? "开" : "关");
    printf("报警音量：%u%%\r\n", (unsigned)b->alarm_volume);
    printf("显示屏：%s\r\n", b->display_enable ? "开" : "关");
    printf("屏幕亮度：%u%%\r\n", (unsigned)b->bright_sz);
    printf("设备地址：%u\r\n", (unsigned)b->dev_addr);
    printf("语言：%s\r\n", b->language == 0 ? "中文" : "English");
}

uint8_t DeviceConfig_IsReady(void)
{
    return s_ready;
}

int DeviceConfig_Init(void)
{
    uint32_t header[2];
    DeviceCfgBlob b;
    int ret = 0;

    flash_fs_lock();
    if (W25Qx_QSPI_FastRead((uint8_t *)header, EXT_FLASH_CONFIG_REGION_BASE, 8U) != QSPI_OK) {
        printf("[配置] QSPI 读取失败\r\n");
        ret = -4;
        goto out;
    }
    if (header[0] != CFG_MAGIC) {
        printf("[配置] %s 无有效数据，写入默认值...\r\n", DEVICE_CFG_STORAGE_LABEL);
        apply_defaults_to_sys();
        if (write_runtime_to_flash() != 0) {
            printf("[配置] 写入默认值失败\r\n");
            ret = -1;
            goto out;
        }
    } else if (header[1] == CFG_VERSION_V1) {
        DeviceCfgBlobV1 old;

        if (W25Qx_QSPI_FastRead((uint8_t *)&old, EXT_FLASH_CONFIG_REGION_BASE, sizeof(old)) != QSPI_OK) {
            ret = -4;
            goto out;
        }
        apply_blob_v1_to_sys(&old);
        if (write_runtime_to_flash() != 0) {
            printf("[配置] v1->v2 迁移写入失败\r\n");
            ret = -1;
            goto out;
        }
    } else if (header[1] == CFG_VERSION) {
        if (cfg_blob_read_v2(&b) != 0) {
            ret = -4;
            goto out;
        }
        apply_blob_to_sys(&b);
        if (write_runtime_to_flash() != 0) {
            printf("[配置] 重写 Flash 失败\r\n");
            ret = -1;
            goto out;
        }
    } else {
        printf("[配置] 未知版本 %lu，写入默认值...\r\n", (unsigned long)header[1]);
        apply_defaults_to_sys();
        if (write_runtime_to_flash() != 0) {
            printf("[配置] 写入默认值失败\r\n");
            ret = -1;
            goto out;
        }
    }
    s_ready = 1U;
    printf("[配置] 从 Flash 加载完毕！\r\n");
    fill_blob_from_sys(&b);
    print_cfg_blob_human(&b);
    
    /* 更新协议设备地址 */
    Net_Device_Update_Addr();
out:
    flash_fs_unlock();
    return ret;
}

int DeviceConfig_PrintFromFile(void)
{
    DeviceCfgBlob b;

    flash_fs_lock();
    if (!s_ready) {
        printf("[配置] 未初始化\r\n");
        flash_fs_unlock();
        return -1;
    }
    if (cfg_blob_read_v2(&b) != 0) {
        printf("[配置] 读取失败\r\n");
        flash_fs_unlock();
        return -4;
    }
    printf("[配置] %s（地址 0x%08lX）\r\n",
           DEVICE_CFG_STORAGE_LABEL, (unsigned long)EXT_FLASH_CONFIG_REGION_BASE);
    if (b.magic == CFG_MAGIC) {
        print_cfg_blob_human(&b);
    } else {
        printf("数据无效（魔数错误）\r\n");
    }
    printf("[配置] 结束\r\n");
    flash_fs_unlock();
    return 0;
}

int DeviceConfig_WriteFromSysCfg(void)
{
    int r;

    if (!s_ready) {
        return -1;
    }
    flash_fs_lock();
    r = write_runtime_to_flash();
    flash_fs_unlock();
    update_sys_cfg = true;
    return r;
}

int DeviceConfig_SetSn(const char *sn)
{
    if (sn == NULL) {
        return -1;
    }
    copy_fixed_width_pad((void *)sys_cfg.SN, (size_t)SN_LEN, sn);
    return DeviceConfig_WriteFromSysCfg();
}

int DeviceConfig_SetHwVer(const char *hw)
{
    if (hw == NULL) {
        return -1;
    }
    copy_fixed_width_pad((void *)sys_cfg.hw_version, (size_t)HW_VER_LEN, hw);
    return DeviceConfig_WriteFromSysCfg();
}



int DeviceConfig_SetSensitivity(float sens)
{
    sys_cfg.sensitivity = sens;
    return DeviceConfig_WriteFromSysCfg();
}

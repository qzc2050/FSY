#include "geiger.h"
#include "dose_rate.h"
#include "tim.h"
#include "main.h"
#include "fsy_regmap.h"
#include "fsy_upload.h"
#include "fsy_link.h"
#include "alarm_output.h"
#include "device_config.h"
#include "pcf85063.h"
#include "hist_5min.h"

#define Dev_Tk_Wait(ms, tk_var)  ((uint32_t)((HAL_GetTick() - (tk_var)) >= (ms)))
#define Dev_Tk_Init(tk_ptr)      do { *(tk_ptr) = HAL_GetTick(); } while (0)

/* RTC 墙钟对齐：minute%5==0 && second==0 为 5min 边界；上电起即累计，首条不完整窗也上报 */
static uint16_t s_dose_last_boundary_hm;  /* 上次触发的 hour*60+minute，防同分钟重复 */
static uint8_t s_dose_boundary_inited;

/* 上电 blank：从 Geiger_Init/HV 使能起计时，前 GEIGER_BOOT_BLANK_MS 不上报真实剂量率 */
static uint32_t s_geiger_boot_tick;
static uint8_t s_geiger_boot_blank_done;

static uint8_t geiger_boot_blank_active(void)
{
    if (s_geiger_boot_blank_done) {
        return 0U;
    }
    if ((HAL_GetTick() - s_geiger_boot_tick) < GEIGER_BOOT_BLANK_MS) {
        return 1U;
    }
    return 0U;
}

/* 模拟计数数据（用于模拟模式） */
static const uint32_t simulated_counts[] = {
    0,0,0,1,1,0,0,0,1,0,0,0,0,1,0,0,0,1,0,0,0,1,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,1,1,0,0,0,1,0,0,0,0,1,0,0,0,1,0,0,0,1,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,
    2,1,0,2,1,0,2,4,4,0,5,5,6,4,1,1,4,1,4,1,0,4,2,3,7,1,2,4,1,2,2,8,6,1,4,1,2,0,1,2,3,2,4,0,0,
    3,1,6,1,2,5,0,4,5,3,5,4,3,3,3,4,3,3,3,3,3,4,7,3,3,3,5,1,5,3,2,3,4,4,0,6,3,1,5,5,1,3,0,3,7,2,
    26,26,24,26,34,34,24,18,26,24,26,20,23,24,33,26,19,20,17,28,23,27,23,33,23,21,20,28,21,24,31,
    18,18,28,19,26,20,24,29,19,24,27,25,23,24,22,22,21,27,28,24,21,32,28,27,21,20,29,25,19,33,28,24,30
};
static const uint32_t simulated_counts_size = sizeof(simulated_counts) / sizeof(simulated_counts[0]);
static uint32_t sim_index = 0;

float alpha_arge[7] = {0.15f,0.20f,0.1f,0.7f,0.75f,0.8f,1.0f};
bool one_second_cnt_func = false;

HD_Mode_Param hd_param = {
    .over_cnt = HD_MD_EN_OVER_CNT,
    .muti_cnt = HD_MD_EN_MULTI_CNT,
    .once_cnt = HD_MD_EN_ONCE_CNT,
    .keep_cnt = HD_MD_EN_KEEP_CNT
};

Data_Var_Struct data_var = {0};
volatile Sys_Cfg_Struct sys_cfg = {
    .th_rl_rate = DEVICE_CFG_DEFAULT_RATE_TH_RL,
    .th_rh_rate = DEVICE_CFG_DEFAULT_RATE_TH_RH,
    .th_rh_rate_saved = 0.0f,
    .th_rl_rate_saved = 0.0f,
    .dose_th_shadow_flags = 0U,
    .sensitivity = DEVICE_CFG_DEFAULT_SENS,
    .temp_th_hi = DEVICE_CFG_DEFAULT_TEMP_TH_HI,
    .temp_th_lo = DEVICE_CFG_DEFAULT_TEMP_TH_LO,
    .press_th_hi = DEVICE_CFG_DEFAULT_PRESS_TH_HI,
    .press_th_lo = DEVICE_CFG_DEFAULT_PRESS_TH_LO,
    .hum_th_hi = DEVICE_CFG_DEFAULT_HUM_TH_HI,
    .hum_th_lo = DEVICE_CFG_DEFAULT_HUM_TH_LO,
    .co2_th_hi = DEVICE_CFG_DEFAULT_CO2_TH_HI,
    .co2_th_lo = DEVICE_CFG_DEFAULT_CO2_TH_LO,
    .pm25_th_hi = DEVICE_CFG_DEFAULT_PM25_TH_HI,
    .pm25_th_lo = DEVICE_CFG_DEFAULT_PM25_TH_LO,
    .alarm_sound = DEVICE_CFG_DEFAULT_ALARM_SOUND,
    .alarm_light = DEVICE_CFG_DEFAULT_ALARM_LIGHT,
    .alarm_volume = DEVICE_CFG_DEFAULT_ALARM_VOLUME,
    .alarm_volume_saved = 0U,
    .display_enable = DEVICE_CFG_DEFAULT_DISPLAY,
    .dev_addr = DEVICE_CFG_DEFAULT_DEV_ADDR,
    .language = DEVICE_CFG_DEFAULT_LANGUAGE,
    .SN = DEVICE_CFG_DEFAULT_SN,
    .hw_version = DEVICE_CFG_DEFAULT_HW,
    .bright_sz = DEVICE_CFG_DEFAULT_BRIGHT,
};

char ccnt[48] = {0};

static int geiger_local_flash_busy(void)
{
    return 0;
}

/********************************************************************************************
* 函数名：Geiger_Dose_On30SecondTick
* 描  述：30s 边界：D30 并入 5min 窗，清零 30s 累计
********************************************************************************************/
static void Geiger_Dose_On30SecondTick(void)
{
    float d30 = data_var.dose_30s_acc;

    data_var.dose_5min_acc += d30;
    data_var.dose_30s_acc = 0.0f;
}

/********************************************************************************************
* 函数名：Geiger_Dose_On5MinuteTick
* 描  述：5min 窗结束：写 Flash(整 μSv) + reg30~35 + 0x23；RTC 无效则跳过
*         dt 为窗结束时刻（墙钟边界）
********************************************************************************************/
static void Geiger_Dose_On5MinuteTick(const Pcf85063_DateTime_t *dt)
{
    float d5;
    uint8_t dt8[8];
    uint32_t dose_x100;
    uint32_t unix_ts;

    /* 末块 D30 先并入，再取 D5（与旧 tick 路径「先 30s 后 5min」一致） */
    data_var.dose_5min_acc += data_var.dose_30s_acc;
    data_var.dose_30s_acc = 0.0f;
    d5 = data_var.dose_5min_acc;
    data_var.dose_5min_acc = 0.0f;

    if (dt == NULL || dt->online == 0U) {
        return;
    }

    dt8[0] = (uint8_t)(dt->year % 100U);
    dt8[1] = dt->month;
    dt8[2] = dt->day;
    dt8[3] = dt->hour;
    dt8[4] = dt->minute;
    dt8[5] = dt->second;
    dt8[6] = 0U;
    dt8[7] = 0U;

    if (d5 < 0.0f) {
        d5 = 0.0f;
    }
    dose_x100 = (uint32_t)(d5 * DOSE_5MIN_PROTOCOL_SCALE + 0.5f);
#if DOSE_5MIN_TEST_FAST
    if ((dose_x100 == 0U) && (d5 > 0.0f)) {
        dose_x100 = 1U;
    }
#endif

    unix_ts = Hist5Min_DateTimeToUnix(dt);
    if (unix_ts != 0U) {
        (void)Hist5Min_Write(unix_ts, dose_x100);
    }

    Fsy_Regmap_Sync5MinSnapshot(dt8, dose_x100);
    (void)Fsy_Upload_Send5Min(dt8, dose_x100, Fsy_Link_WriteUpload);
}

/********************************************************************************************
* 函数名：Geiger_Dose_Periodic_Save
* 描  述：RTC 墙钟对齐：minute%5==0 && second==0 为 5min 边界；
*         上电起即累计，首条不完整窗也上报（有总比没有好）。
*         窗内 30s 块：second==0 或 30（与墙钟对齐）。
*         测试快模式仍用相对 tick。
********************************************************************************************/
static void Geiger_Dose_Periodic_Save(void)
{
#if DOSE_5MIN_TEST_FAST
    static uint32_t block_30s_tk = 0;
    static uint32_t block_5min_tk = 0;

    if (geiger_local_flash_busy()) {
        return;
    }

    if (Dev_Tk_Wait(DOSE_BLOCK_30S_MS, block_30s_tk)) {
        Dev_Tk_Init(&block_30s_tk);
        Geiger_Dose_On30SecondTick();
    }

    if (Dev_Tk_Wait(DOSE_REPORT_5MIN_MS, block_5min_tk)) {
        Dev_Tk_Init(&block_5min_tk);
        {
            Pcf85063_DateTime_t dt;
            if ((Pcf85063_GetTime(&dt) == 0) && (dt.online != 0U)) {
                Geiger_Dose_On5MinuteTick(&dt);
            } else {
                data_var.dose_5min_acc = 0.0f;
                data_var.dose_30s_acc = 0.0f;
            }
        }
    }
#else
    Pcf85063_DateTime_t dt;
    uint8_t at_5min;
    uint8_t at_30s;
    static uint8_t last_30s_half = 0xFFU; /* 0=秒0侧, 1=秒30侧, 防重复 */

    if (geiger_local_flash_busy()) {
        return;
    }

    if ((Pcf85063_GetTime(&dt) != 0) || (dt.online == 0U)) {
        return;
    }

    at_5min = ((dt.minute % 5U) == 0U) && (dt.second == 0U);
    at_30s = (dt.second == 0U) || (dt.second == 30U);

    if (at_5min) {
        uint16_t hm = (uint16_t)((uint16_t)dt.hour * 60U + (uint16_t)dt.minute);

        if (!s_dose_boundary_inited || s_dose_last_boundary_hm != hm) {
            s_dose_boundary_inited = 1U;
            s_dose_last_boundary_hm = hm;
            Geiger_Dose_On5MinuteTick(&dt);
            last_30s_half = 0U; /* 与 second==0 对齐，避免同秒再跑 30s */
        }
        return;
    }

    if (at_30s) {
        uint8_t half = (dt.second >= 30U) ? 1U : 0U;
        if (last_30s_half != half) {
            last_30s_half = half;
            Geiger_Dose_On30SecondTick();
        }
    }
#endif
}

/********************************************************************************************
* 函数名：Geiger_Init
* 描  述：盖革管计数器初始化
********************************************************************************************/
void Geiger_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_TIM2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin = GEIGER_PIN_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
    HAL_GPIO_Init(GEIGER_PIN_GPIO_Port, &GPIO_InitStruct);
    
    // 定时器外部时钟源配置
    TIM_SlaveConfigTypeDef sSlaveConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 0;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 65535;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
    {
        Error_Handler();
    }
    sSlaveConfig.SlaveMode = TIM_SLAVEMODE_EXTERNAL1;
    sSlaveConfig.InputTrigger = TIM_TS_ETRF;
    sSlaveConfig.TriggerPolarity = TIM_TRIGGERPOLARITY_NONINVERTED;
    sSlaveConfig.TriggerPrescaler = TIM_TRIGGERPRESCALER_DIV1;
    sSlaveConfig.TriggerFilter = 0;
    if (HAL_TIM_SlaveConfigSynchro(&htim2, &sSlaveConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
    {
        Error_Handler();
    }

    HAL_NVIC_SetPriority(TIM2_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
    TIM_ETR_SetConfig(TIM2, TIM_ETRPRESCALER_DIV1, TIM_ETRPOLARITY_NONINVERTED, 1);
    __HAL_TIM_CLEAR_IT(&htim2, TIM_IT_UPDATE);
    __HAL_TIM_ENABLE_IT(&htim2,TIM_IT_UPDATE);
    HAL_TIM_Base_Start_IT(&htim2);
    
    // 初始化剂量率 EWMA 滤波器
    DoseRate_Init();
    DeviceConfig_ApplyGeigerAlgorithm();

#if GEIGER_HV_ENABLE
    HAL_GPIO_WritePin(HV_EN_GPIO_Port, HV_EN_Pin, GPIO_PIN_SET);
#endif

    s_geiger_boot_tick = HAL_GetTick();
    s_geiger_boot_blank_done = 0U;
    data_var.real_rate = 0.0f;
}

/********************************************************************************************
* 函数名：HAL_TIM_PeriodElapsedCallback
* 描  述：盖革管计数器溢出（TIM2 从模式计数）
********************************************************************************************/
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) {
        data_var.over_num++;
    }
}

/********************************************************************************************
* 函数名：Geiger_Dose_Calculate
* 描述  ：按本秒原始 CPS 积分到当前 30s 窗（μSv）；不用 EWMA real_rate
* 输入  ：cps_1s — 本秒盖革计数（与 DoseRate_UpdateFromCps 同源）
********************************************************************************************/
static void Geiger_Dose_Calculate(uint32_t cps_1s)
{
    float single_dose_val;
    float sens = sys_cfg.sensitivity;

    if (sens <= 0.0f) {
        return;
    }

    /* μSv/s = (cps×60/sens)/3600 = cps/(sens×60) */
    single_dose_val = ((float)cps_1s / (sens * 60.0f));
    data_var.dose_30s_acc += single_dose_val;
}

/********************************************************************************************
* 函数名：Geiger_Doserate_Calculate
* 描  述：盖革管剂量率计算
********************************************************************************************/
void Geiger_Doserate_Calculate(void)
{
    static uint32_t ago_cnt = 0;
    static uint32_t sample_tk = 0;
    static uint32_t cal_tk = 0;
    static bool first_clr = true;
    static uint32_t once_cnt = 0;      // 单次计算累计的盖革管计数（用于模拟/手动模式）

    // 获取盖革管计数（每 100ms 累加一次）
    if(first_clr)
    {
        first_clr = false;
        ago_cnt = TIM2->CNT;
    }

    if(Dev_Tk_Wait(100, sample_tk))
    {
        uint32_t delta;

        Dev_Tk_Init(&sample_tk);

        delta = (65536U * data_var.over_num + TIM2->CNT - ago_cnt);
        ago_cnt = TIM2->CNT;
        data_var.over_num = 0;

        data_var.geiger_crt_cnt += delta;
        once_cnt += delta;  /* 只加本 100ms 增量；勿 += geiger_crt_cnt（会三角放大） */
    }

    /* 上电 blank：排空 HV 爬升脉冲，剂量率保持 0，不进 EWMA/5min 积分 */
    if (geiger_boot_blank_active()) {
        if (Dev_Tk_Wait(1000, cal_tk)) {
            Dev_Tk_Init(&cal_tk);
            once_cnt = 0;
            data_var.geiger_crt_cnt = 0;
        }
        data_var.real_rate = 0.0f;
        Geiger_Dose_Periodic_Save();
        return;
    }

    if (!s_geiger_boot_blank_done) {
        s_geiger_boot_blank_done = 1U;
        DoseRate_ResetFilter();
        once_cnt = 0;
        data_var.geiger_crt_cnt = 0;
        ago_cnt = TIM2->CNT;
        data_var.over_num = 0;
        data_var.real_rate = 0.0f;
        Dev_Tk_Init(&sample_tk);
        Dev_Tk_Init(&cal_tk);
    }

    // 使用新的 EWMA 算法计算剂量率（每秒更新一次）
    if(Dev_Tk_Wait(1000, cal_tk))
    {
        Dev_Tk_Init(&cal_tk);
        
        if (DoseRate_GetInputMode() == DR_INPUT_MODE_SIM)
        {
            // 模拟模式：使用预设的模拟计数（覆盖 once_cnt）
            once_cnt = simulated_counts[sim_index++];
            if(sim_index >= simulated_counts_size)
                sim_index = 0;
        }
        else if (DoseRate_GetInputMode() == DR_INPUT_MODE_MANUAL)
        {
            // 手动模式：使用手动设置的 CPS（覆盖 once_cnt）
            once_cnt = DoseRate_GetManualCps();
        }
        // 真实模式：不覆盖，直接使用 once_cnt（已累加的值）
        
        // 使用 EWMA 算法更新剂量率，输入为 CPS（每秒计数）
        data_var.real_rate = DoseRate_UpdateFromCps(once_cnt);
        Alarm_Output_NotifyCps(once_cnt);

        /* 5min 账本：用本秒原始 CPS 积分（与 EWMA 显示分离） */
        Geiger_Dose_Calculate(once_cnt);
        
        // 超过量程限制
        if(data_var.real_rate > DoseRate_GetRateLimitUsvh())
            data_var.real_rate = DoseRate_GetRateLimitUsvh();
        
        // 重置累计计数（为下一秒做准备）
        once_cnt = 0;
        data_var.geiger_crt_cnt = 0;
    }

    Geiger_Dose_Periodic_Save();
}

/***************************************************************************************************
* 函数名：abtain_deal_cnt
* 描述：计算指定组数内的盖革管计数
* 输入：crt_pos -> 当前缓存位置, group -> 指定组数, *cnt_buf -> 数组指针
* 输出：盖革管计数总数
***************************************************************************************************/
uint32_t abtain_deal_cnt(uint8_t crt_pos,uint8_t group,uint32_t *cnt_buf)
{
    uint8_t i;
    uint32_t sum_cnt = 0;
    
    if(crt_pos > group)
    {
        for(i = 1;i <= group;i++)
            sum_cnt += cnt_buf[crt_pos - i];
    }
    else
    {
        for(i = 0;i < crt_pos;i++)
			sum_cnt += cnt_buf[i];
		
		for(i = (group - crt_pos);i > 0;i--)
			sum_cnt += cnt_buf[LONG_DEAL_TIME - i];
    }
//    printf("sum_cnt: %d\r\n",sum_cnt);
    return sum_cnt;
}

/***************************************************************************************************
* 函数名：Muti_Cnt_Detect
* 描述：判断是否连续多组数据超过指定计数
* 输入：cmp_val -> 比较数值，crt_pos -> 当前缓存位置, group -> 指定组数, *cnt_buf -> 数组指针
* 输出：true -> 超过，false -> 未超过
***************************************************************************************************/
bool Muti_Cnt_Detect(uint8_t crt_pos,uint8_t group,uint32_t *cnt_buf)
{
    bool hd_mode = true;
    uint32_t read_pos,total_cnt = 0;
    
    if(cnt_buf[crt_pos] >= hd_param.once_cnt)
        return true;
    
    for(uint8_t i = 0;i < 3;i++)
    {
        read_pos = crt_pos - i;
        if(crt_pos < i)
            read_pos += LONG_DEAL_TIME;
        
        total_cnt += cnt_buf[read_pos];
        
        if(cnt_buf[read_pos] < hd_param.muti_cnt)
            hd_mode = false;
    }
    
    if(hd_param.over_cnt)
    {
        if(total_cnt >= hd_param.over_cnt)
            return true;
    }
    
    return hd_mode;
}

/********************************************************************************************
* 函数名：dose_rate_adaptive_ema
* 描  述：EMA平滑
* 输  入: cnt 此次计数值
********************************************************************************************/
float dose_rate_adaptive_ema(float current_count) {
    float diff = 0;
    static float avg = 0.0f;
    static float last_count = 0;
    static bool initialized = false;
    float alpha = 1.0f;

    if (!initialized) {
        avg = current_count;
        last_count = current_count;
        initialized = true;
    } else {
        // 根据计数变化调整α
        diff = (current_count > last_count) ? (current_count - last_count):(last_count - current_count);
        last_count = current_count;

        // {0.5f,0.8f,0.1f,0.2f,0.3f,0.5f,1.0f}
        if(diff == 0)
            alpha = alpha_arge[0];
        else if(diff < 0.66f)   // 实时剂量率的变化幅度 = 1.0 uSv/h
            alpha = alpha_arge[1];
        else if(diff < 3.0f)    // 4.5 uSv/h
            alpha = alpha_arge[2];
        else if(diff < 5.3333f) // 8.0 uSv/h
            alpha = alpha_arge[3];
        else if(diff < 7.0f)    // 10.5 uSv/h
            alpha = alpha_arge[4];
        else if(diff < 10.0f)   // 15 uSv/h
            alpha = alpha_arge[5];
        else
            alpha = alpha_arge[6];

        avg = alpha * current_count + (1 - alpha) * avg;
    }
    return avg;
}

/********************************************************************************************
* 函数名：Real_Data_deal
* 描述  ：计算实时剂量率
********************************************************************************************/
/***************************************************************************************************
* 函数名：Dose_Rate_TH_Alarm
* 描述：剂量值/剂量率超阈值处理
* 输入：无
* 输出：无
***************************************************************************************************/
void Dose_Rate_TH_Alarm(void)
{
    uint32_t hi_x100 = 0U;
    uint32_t lo_x100 = 0U;
    uint32_t alarm_enable = 0U;
    uint8_t hi_alarm = 0U;
    uint8_t lo_alarm = 0U;

    DeviceConfig_GetDoseAlarmConfig(&hi_x100, &lo_x100, &alarm_enable, NULL);

    if (((alarm_enable & (1UL << RATE_HIGH_ALARM_BIT)) != 0U) &&
        (data_var.real_rate >= (float)RATE_LIMIT)) {
        hi_alarm = 1U;
    } else if (((alarm_enable & (1UL << RATE_HIGH_ALARM_BIT)) != 0U) &&
               (hi_x100 > 0U) && (sys_cfg.th_rh_rate > 0.0f) &&
               (data_var.real_rate >= sys_cfg.th_rh_rate)) {
        hi_alarm = 1U;
    }

    if (((alarm_enable & (1UL << RATE_LOW_ALARM_BIT)) != 0U) &&
        (lo_x100 > 0U) && (sys_cfg.th_rl_rate > 0.0f) &&
        (data_var.real_rate < sys_cfg.th_rl_rate)) {
        lo_alarm = 1U;
    }

    Alarm_Status_Update(RATE_HIGH_ALARM_BIT, hi_alarm != 0U);
    Alarm_Status_Update(RATE_LOW_ALARM_BIT, lo_alarm != 0U);
}

/********************************************************************************************
* 函数名：Alarm_Status_Update
* 描述  ：更新报警状态标志位的指定位，并在状态变化时同步到寄存器表
* 输入  ：@param: bit_pos -> 位位置（0-31）
*         @param: is_alarm -> true=报警，false=正常
* 输出  ：无
********************************************************************************************/
void Alarm_Status_Update(uint8_t bit_pos, bool is_alarm)
{
    uint32_t old_status;
    
    if(bit_pos >= 32)
        return;
    
    old_status = sys_cfg.alarm_status;
    
    if(is_alarm)
        sys_cfg.alarm_status |= (1U << bit_pos);   // 设置报警位
    else
        sys_cfg.alarm_status &= ~(1U << bit_pos);  // 清除报警位
    
    /* 剂量 bit 局部更新，避免覆盖 0x23 中环境传感器离线位 */
    if (sys_cfg.alarm_status != old_status) {
        if (bit_pos <= RATE_LOW_ALARM_BIT) {
            Fsy_Regmap_PatchDoseAlarmBit(bit_pos, is_alarm);
        } else {
            Fsy_Regmap_SyncAlarmStatus(sys_cfg.alarm_status);
        }
    }
}

/********************************************************************************************
* 函数名：Alarm_Status_Clear
* 描述  ：清除所有报警状态
* 输入  ：无
* 输出  ：无
********************************************************************************************/
void Alarm_Status_Clear(void)
{
    sys_cfg.alarm_status = 0;
}

/********************************************************************************************
* 函数名：Alarm_Status_Get
* 描述  ：获取当前报警状态
* 输入  ：无
* 输出  ：32 位报警状态标志
********************************************************************************************/
uint32_t Alarm_Status_Get(void)
{
    return sys_cfg.alarm_status;
}








#include "geiger.h"
#include "tim.h"
#include "main.h"
#include "fsy_regmap.h"
#include "dose_rate.h"
#include "alarm_output.h"

#define Dev_Tk_Wait(ms, tk_var)  ((uint32_t)((HAL_GetTick() - (tk_var)) >= (ms)))
#define Dev_Tk_Init(tk_ptr)      do { *(tk_ptr) = HAL_GetTick(); } while (0)

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

static void Geiger_Dose_Periodic_Save(void)
{
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

#if GEIGER_HV_ENABLE
    HAL_GPIO_WritePin(HV_EN_GPIO_Port, HV_EN_Pin, GPIO_PIN_SET);
#endif
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
* 描述  ：累计剂量值
* 输入  : 无
********************************************************************************************/
void Geiger_Dose_Calculate(void)
{
    float single_dose_val;
    
    //此次剂量累计值
    single_dose_val = ((float)data_var.geiger_crt_cnt / (sys_cfg.sensitivity * 60));
    data_var.dose_five_min += single_dose_val;    // 5分钟累计剂量值累计
    data_var.dose_thirty_sec += single_dose_val;  // 30秒累计剂量值累计
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
        Dev_Tk_Init(&sample_tk);
        
        data_var.geiger_crt_cnt += (65536 * data_var.over_num + TIM2->CNT - ago_cnt);
        ago_cnt = TIM2->CNT;        // 记录这次读取完的计数器计数
        data_var.over_num = 0;      // 清空计时器溢出次数
        
        // 累加到 once_cnt（真实模式下使用）
        once_cnt += data_var.geiger_crt_cnt;
    }

    // data_var.geiger_crt_cnt = 2;

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
        
        // 超过量程限制
        if(data_var.real_rate > RATE_LIMIT)
            data_var.real_rate = RATE_LIMIT;
        
        // 重置累计计数（为下一秒做准备）
        once_cnt = 0;
        data_var.geiger_crt_cnt = 0;
    }
    
    Geiger_Dose_Calculate();

    // printf("Geiger cnt: %u", crt_cnt);
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
    if(data_var.real_rate >= (float)RATE_LIMIT)
    {
        Alarm_Status_Update(RATE_HIGH_ALARM_BIT, true);
    }
    else if(sys_cfg.th_rh_rate > 0.0f &&
             (data_var.real_rate >= sys_cfg.th_rh_rate))
    {
        Alarm_Status_Update(RATE_HIGH_ALARM_BIT, true);
    }
    else if(sys_cfg.th_rl_rate > 0.0f &&
             (data_var.real_rate < sys_cfg.th_rl_rate))
    {
        Alarm_Status_Update(RATE_LOW_ALARM_BIT, true);
    }
    else
    {
        Alarm_Status_Update(RATE_HIGH_ALARM_BIT, false);
        Alarm_Status_Update(RATE_LOW_ALARM_BIT, false);
    }
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
    
    /* 只有报警状态真正变化时才同步到寄存器表 */
    if(sys_cfg.alarm_status != old_status)
        Fsy_Regmap_SyncAlarmStatus(sys_cfg.alarm_status);
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








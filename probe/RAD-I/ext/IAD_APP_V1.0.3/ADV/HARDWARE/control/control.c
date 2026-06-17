#include "control.h"
#include "beep.h"
#include "ui_menu.h"

char str_temp[18];    // 字符串数组缓（任何字符串显示/打印皆可调用）
bool ref_sta = true;

struct time_type__ data_time;



int simulated_counts[] = { 0,0,0,1,1,0,0,0,1,0,0,0,0,1,0,0,0,1,0,0,0,1,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,1,1,0,0,0,1,0,0,0,0,1,0,0,0,1,0,0,0,1,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,
                           2,1,0,2,1,0,2,4,4,0,5,5,6,4,1,1,4,1,4,1,0,4,2,3,7,1,2,4,1,2,2,8,6,1,4,1,2,0,1,2,3,2,4,0,0,
                           3,1,6,1,2,5,0,4,5,3,5,4,3,3,3,4,3,3,3,3,3,4,7,3,3,3,5,1,5,3,2,3,4,4,0,6,3,1,5,5,1,3,0,3,7,2,
                           26,26,24,26,34,34,24,18,26,24,26,20,23,24,33,26,19,20,17,28,23,27,23,33,23,21,20,28,21,24,31,
                          18,18,28,19,26,20,24,29,19,24,27,25,23,24,22,22,21,27,28,24,21,32,28,27,21,20,29,25,19,33,28,24,30, };
int count_size = sizeof(simulated_counts) / sizeof(simulated_counts[0]);


/********************************************************************************************
* 函数名：Real_Data_deal
* 描述  ：计算实时剂量率
********************************************************************************************/
void Real_Data_deal(void)
{   
    static uint16_t i = 0;
    static uint32_t rate_tk = 0;        //剂量率计算计时
    static uint32_t once_cnt = 0;       //单次计算累计的盖革管计数                       


    once_cnt += data_var.geiger_crt_cnt;
    
    if(!System_Time_Wait(1000,rate_tk))
        return;
    System_Time_Init(&rate_tk);
    
    
    if ((DoseRate_GetInputMode() == DR_INPUT_MODE_SIM) || test_cmd)
    {
        once_cnt = simulated_counts[i++];
        if(i >= count_size)
            i = 0;
    }
    else if (DoseRate_GetInputMode() == DR_INPUT_MODE_MANUAL)
    {
        once_cnt = DoseRate_GetManualCps();
    }
    
    /* 盖革管测量模拟数据 */ 
    if(one_second_cnt_func)
        OLED_ShowNum(0,32,once_cnt,4,32);  //显示实时剂量率
    
    /* 盖革管测量模拟数据 */ 
    data_var.real_rate = DoseRate_UpdateFromCps(once_cnt);   // 计算实时剂量率

    if(one_second_cnt_func && dose_rate_print_func)
        printf("计数: %d | 剂量率: %.2f\r\n", once_cnt, data_var.real_rate);
    else if(one_second_cnt_func)
        printf("计数: %d\r\n", once_cnt);
    else if(dose_rate_print_func)
        printf("剂量率: %.2f\r\n", data_var.real_rate);
    
    
    once_cnt = 0;
    
//    printf("%.2f,",data_var.real_rate);
    if(data_var.real_rate > 1E4)  //测量值超过上限，限值
        data_var.real_rate = 1E4;
    
    if(data_var.real_rate > data_var.day_top_rate)    //保存每日最高实时剂量率
    {
        data_var.day_top_rate = (uint32_t)(data_var.real_rate * 100) / 100.0f;      //避免被四舍五入，如99.99四舍五入为100.0
        STMDATAEEPROM_Write(DAY_TOP_RATE_ADDR,(uint32_t *)(&data_var.day_top_rate),1);
    }
}

/********************************************************************************************
* 函数名：Sum_Dose
* 描述  ：累计剂量值
* 输入  : cnt 此次计数值
********************************************************************************************/
void Sum_Dose(void)
{
    float single_dose_val;
    
    //此次剂量累计值
    single_dose_val = ((float)data_var.geiger_crt_cnt / (DataUnit_To_Float(sys_cfg.sensitivity) * 60));
    data_var.main_dose += single_dose_val;  //主界面DOSE值累计
    data_var.day_dose += single_dose_val;   //当日累计剂量值累计
    data_var.crt_dose += single_dose_val;   //总累计剂量值累计
    
    if(timing_ctr.mode == TIMING_MODE_RUN)
        timing_ctr.dose += single_dose_val;  //计时模式剂量值累计
}

//extern uint16_t set_cnt;
/********************************************************************************************
* 函数名：Get_tim_cnt
* 描述  ：获取单次采样时间内计数器的计数（盖革管触发次数）
********************************************************************************************/
void Get_tim_cnt(void)
{
    static bool first_clr = true;
    static uint32_t ago_cnt = 0;
    
    if(first_clr)
    {
        first_clr = false;
        ago_cnt = LPTIM1->CNT;
    }
        
    data_var.geiger_crt_cnt += 65536 * data_var.over_num + LPTIM1->CNT - ago_cnt;
    ago_cnt = LPTIM1->CNT;    // 记录这次读取完的计数器计数
    data_var.over_num = 0;    // 清空计时器溢出次数
    
//    if(set_cnt)
//    {
//        data_var.geiger_crt_cnt = set_cnt;
//        set_cnt = 0;
//    }
//	printf("t: %d\r\n", data_var.geiger_crt_cnt);
}

/***************************************************************************************************
* 函数名：Dose_Rate_TH_Alarm
* 描述：剂量值/剂量率超阈值处理
* 输入：无
* 输出：无
***************************************************************************************************/
void Dose_Rate_TH_Alarm(void)
{
    if((data_var.real_rate >= RATE_LIMIT) || (data_var.main_dose >= DOSE_LIMIT)\
        || (data_var.day_dose >= DOSE_LIMIT) || (data_var.crt_dose >= DOSE_LIMIT))  // 剂量率/剂量超上限
        Beep_Ctr(BEEP_EVENT_LIMIT);
	else if(data_var.crt_dose >= DataUnit_To_Float(sys_cfg.th_crt_dose)\
        || (data_var.day_dose >= DataUnit_To_Float(sys_cfg.th_day_dose)))   // 当前剂量/每日剂量超阈值
		Beep_Ctr(BEEP_EVENT_DTH);
    else if((beep_event == BEEP_EVENT_LIMIT) || (beep_event == BEEP_EVENT_DTH))
        Beep_Ctr(BEEP_EVENT_CLR);
	else if(data_var.real_rate >= DataUnit_To_Float(sys_cfg.th_real_rate))  // 剂量率超阈值
	{
		Beep_Ctr(BEEP_EVENT_RTH);
        Data_Refresh(true);  // 非剂量率报警时立即刷新屏幕剂量率
	}
}

/********************************************************************************************
* 函数名：Data_Cal
* 描述  ：数据处理、采集
********************************************************************************************/
void Data_Cal(void)
{
    static uint32_t acq_tk = 0; // 采集的起始时间
    
    if(!System_Time_Wait(SAMPLE_PERIOD,acq_tk))
        return;
    System_Time_Init(&acq_tk);  // 重新计时
    
    Get_tim_cnt();      // 数据采集 - 盖革管计数
    Sum_Dose();         // 数据处理 - 剂量累计
    Real_Data_deal();   // 数据处理 - 剂量率
    Dose_Rate_TH_Alarm();   // 数据处理 - 报警
    data_var.geiger_crt_cnt = 0;    // 数据采集 - 清空盖革管计数
}

/********************************************************************************************
* 函数名：Exit_LPR
* 描  述：退出低功耗，正常运行
********************************************************************************************/
void Exit_LPR(bool event)
{
    sys_bits.run_md = RUN_MODE;     //标志进入正常运行模式
    EXTI->IMR &= 0xFFFF1000;        //关闭部分外部中断
    EXIT_LOW_POWER_RUN_MODE();      //退出低功耗模式
    
    if(event == KEY_BRIGHT)
        Set_Sc_Extinct_Time(0);     //恢复设置的熄屏时间
    else    // event == ALARM_BRIGHT
        LPR_Time_Cnt = 5000;       //5*1000    5秒进入低功耗模式（不可放置在退出中断前）
}

/********************************************************************************************
* 函数名：Data_Save_Cnt_Add
* 描述  ：有效数据保存数量加1
********************************************************************************************/
void Data_Save_Cnt_Add(void)
{
    if(data_var.history_data_num < ALL_DATA_NUM)  //记录保存的有效数据的个数
    {
        data_var.history_data_num++;    //有效历史记录个数+1
        STMDATAEEPROM_Write(HISTORY_NUM_ADDR,(uint32_t *)(&data_var.history_data_num),1);

        if((data_var.history_data_num == ALL_DATA_NUM) && !sys_cfg.rec_cir)
        {
            sys_cfg.rec_cir = 1;        //完成一轮历史记录保存
            Save_Sys_Config();
        }
    }
    
    if(data_var.data_ofs_num < ALL_DATA_NUM)    //记录保存数据的偏移地址
    {
        data_var.data_ofs_num++;        //偏移地址+1
        
        if(data_var.data_ofs_num == ALL_DATA_NUM)    //偏移地址到了最后一个地址，从头开始覆盖
            data_var.data_ofs_num = 0;

        STMDATAEEPROM_Write(HISTORY_NUM_ADDR,(uint32_t *)(&data_var.history_data_num),1);
    }
}

/********************************************************************************************
* 函数名：Flash_Save_Day_Data
* 描述  ：保存当日累计剂量值和当日平均剂量率
********************************************************************************************/
void Flash_Save_Day_Data(void)
{
    udata.day.rec_type = DAY_TYPE;
    udata.day.rec_date = Date_Conv(data_var.day_date);
    Float_To_DataUnit(data_var.day_dose,DAY_DOSE_SW);

    // +10s时间计算，避免CU指令后平均剂量率大于当日最高
    //	day_data.aver_dose_rate = (float)data_var.day_dose * 3600 / data_var.day_acc_tk * 1000;    //计算当日的平均剂量率
//    Float_To_DataUnit((float)data_var.day_dose * 3600000 / (data_var.day_acc_tk + 10000),RATE_SW);
    Float_To_DataUnit((float)data_var.day_dose * 3600000 / data_var.day_acc_tk,RATE_SW);
    STMDATAEEPROM_Write(DATA_BASE_ADDR + data_var.data_ofs_num * HIS_DATA_SIZE,(uint32_t *)(&udata),2);
    Data_Save_Cnt_Add();
}

/********************************************************************************************
* 函数名：Update_DayData_To_EEPROM
* 描述  ：更新每日数据到EEPROM
* 输入  ：true -> 清空每日数据并更新   false -> 仅更新
********************************************************************************************/
void Update_DayData_To_EEPROM(bool sta)
{
    if(sta)
    {
        data_var.day_dose = 0;
//        data_var.day_acc_tk = 0;
        data_var.day_acc_tk = 20000;    // 初始20s计时，避免当天刚开机时的平均剂量率大于当日最高剂量
        data_var.day_top_rate = 0;
    }
    STMDATAEEPROM_Write(DAY_DOSE_ADDR,(uint32_t *)(&data_var.day_dose),3);
}

/********************************************************************************************
* 函数名：Update_CrtData_To_EEPROM
* 描述  ：更新总累计值到EEPROM
* 输入  ：true -> 清空每日数据并更新   false -> 仅更新
********************************************************************************************/
void Update_CrtData_To_EEPROM(bool sta)
{
    if(sta)
        data_var.crt_dose = 0;
    STMDATAEEPROM_Write(CRT_DOSE_ADDR,(uint32_t *)(&data_var.crt_dose),1);
}

/********************************************************************************************
* 函数名：Gap_Save_Data
* 描述  ：定时保存累计剂量值和累计时间
********************************************************************************************/
void Gap_Save_Data(void)
{
    static uint32_t save_tk = 0;
    
    if(!System_Time_Wait(DAY_PER_SAVE_TIME,save_tk))    //定时保存每日累计数据，方便下次开机继承累计剂量和当日累计开机时间
        return;
    System_Time_Init(&save_tk);
    
    Update_DayData_To_EEPROM(false);
    Update_CrtData_To_EEPROM(false);
}

/********************************************************************************************
* 函数名：Data_Refresh
* 描  述：剂量率、剂量值数值刷新
* 输  入：ref -> true: 立即刷新  false -> 等待刷新
********************************************************************************************/
void Data_Refresh(bool ref)
{
    static struct data_struct{
        float day_dose;
        float aver_rate;
        float top_rate;
        float main_dose;
        float crt_dose;
    }dat_buf;
    
    if(sys_bits.run_md != RUN_MODE)
        return;
        
    if(ref)
        memset(&dat_buf,0xffffffff,sizeof(dat_buf));
    
    // 主界面下刷新数据
    if(crt_depth == DEPTH_HOME_1)
    {
        if((beep_event != BEEP_EVENT_RTH) || ref)
            Unit_Show(64,16,32,144,28,16,data_var.real_rate,0);      // 刷新屏幕实时剂量率

        if(timing_ctr.mode == TIMING_MODE_OFF)   // 正常模式（非计时模式）
        {
            if(dat_buf.main_dose != data_var.main_dose)
            {
                dat_buf.main_dose = data_var.main_dose;
                OLED_Dose_Show(149,48,16,dat_buf.main_dose);
            }
        }
        else if(timing_ctr.mode)   //当前为计时模式
            OLED_Dose_Show(149,48,16,timing_ctr.dose);
    }
    else if(crt_depth == DEPTH_HOME_2)  //剂量累计界面刷新数据
    {
        // +10s时间计算，避免CU指令后平均剂量率大于当日最高
        float dose_rate = (float)data_var.day_dose * 3600000 / data_var.day_acc_tk;
//        float dose_rate = (float)data_var.day_dose * 3600000 / (data_var.day_acc_tk + 1E4);
        dose_rate = (uint32_t)(dose_rate * 100) / 100.0f;

        if(dat_buf.day_dose != data_var.day_dose)    //当日总累计剂量值变化时刷新
        {
            dat_buf.day_dose = data_var.day_dose;
            OLED_Dose_Show(66,7,12,dat_buf.day_dose);
        }
        if(dat_buf.aver_rate != dose_rate)        //当日平均剂量率变化时刷新
        {
            dat_buf.aver_rate = dose_rate;
            Unit_Show(66,26,12,96,26,12,dat_buf.aver_rate,1);
        }
        if(dat_buf.top_rate != data_var.day_top_rate)    //当日最高剂量率变化时刷新
        {
            dat_buf.top_rate = data_var.day_top_rate;
            Unit_Show(66,45,12,96,45,12,dat_buf.top_rate,1);
        }
        if(dat_buf.crt_dose != data_var.crt_dose)    //总累计剂量值变化时刷新
        {
            dat_buf.crt_dose = data_var.crt_dose;
            OLED_Dose_Show(171,36,12,dat_buf.crt_dose);
        }
    }
}

/********************************************************************************************
* 函数名：Gap_Execute
* 描述  ：定时刷新OLED屏幕显示，更新电池电量、时间、数据
********************************************************************************************/
void Gap_Execute(void)
{
    static uint32_t ref_tk = 0;     // 屏幕刷新起始时间
    
    if(key_ctr.sd_cd_tk >= 250)     //若长按后松开250ms，则认为放弃关机(一直长按数到51则会置1重数，并刷新关机界面直到关机)
    {
        key_ctr.sd_cd_tk = 0;
        sys_bits.sd_req = KEEPING_POWER_ON;
        key_ctr.sd_tk = 0;
        ref_sta = true;
        menu_func(NULL,MENU_HOME_1);
    }
    
    if(!(System_Time_Wait(1000,ref_tk) || ref_sta))
        return;
    System_Time_Init(&ref_tk);  // 重新计时
    
    DateTime_Refresh(ref_sta);  // 刷新日期时间
    Battery_Detect(ref_sta);    // 电池电量检测
    Data_Refresh(ref_sta);      // 刷新数据
    Icon_Refresh();             // 图标刷新（主界面）
    Gap_Save_Data();            // 定时保存每日数据
    Request_Twinkle(NULL,NULL,NULL,NULL,false);  // 闪烁指定字符串
//    Clr_Program_Update();
}

/********************************************************************************************
* 函数名：Icon_Refresh
* 描述  ：定时刷新OLED显示，更新电池电量，更新时间
********************************************************************************************/
void Icon_Refresh(void)
{
    uint8_t *icon_p = NULL;
    static bool crt_icon;

    if(sys_bits.run_md != RUN_MODE)
        return;

    if(crt_depth == DEPTH_HOME_1)
    {
        if(crt_icon)
        {
            if((beep_event == BEEP_EVENT_NULL) || (beep_event == BEEP_EVENT_TIMING) || (beep_event == BEEP_EVENT_LB))
                icon_p = (uint8_t *)&radiation_icon2;   //刷新辐射标志，表示正在运行
            else if(beep_event != BEEP_EVENT_RTH)
                OLED_Draw_Fill(218,19,18,36,0x00);      //清除报警标志，实现闪烁效果
        }
        else
        {
            if((beep_event == BEEP_EVENT_NULL) || (beep_event == BEEP_EVENT_TIMING) || (beep_event == BEEP_EVENT_LB))
                icon_p = (uint8_t *)&radiation_icon;    //刷新辐射标志，表示正在运行
            else if(beep_event == BEEP_EVENT_DTH)
                icon_p = (uint8_t *)&warning_icon;      //超阈值报警标志
            else if(beep_event == BEEP_EVENT_LIMIT)
                icon_p = (uint8_t *)&warning_icon2;     //超上限报警
        }
        if(icon_p)
            OLED_DrawSingleBMP(218,19,36,36,icon_p);
        crt_icon = !crt_icon;
    }
}

/********************************************************************************************
* 函数名：Request_Twinkle
* 描  述：定时闪烁指定位置的字符串
* 输  入：x,y -> 位置，*str -> 字符串指针，len -> 字符串有效个数，update -> 更新字符串
* 说  明：(fsize = 0,update = true) -> 关闭闪烁
********************************************************************************************/
void Request_Twinkle(uint8_t x, uint8_t y, char *str, uint8_t len,bool update)
{
    static bool crt_twk;    // 当前显示状态
    static uint8_t xpos,ypos,lenth,strbuf[6] = {0};
    
    if(update)
    {
        if(len)
        {
            if(lenth)
                OLED_ShowString(xpos,ypos,strbuf,32);
            
            xpos = x;
            ypos = y;
            memset(strbuf,0,6);    // 清空数组
            lenth = len;
            memcpy(strbuf,str,len);
            OLED_ShowString(xpos,ypos,strbuf,32);
            crt_twk = true;
        }
        else
           lenth = 0;
    }
    else if(lenth)
    {
        if(crt_twk)
            OLED_Draw_Fill(xpos,ypos,8 * lenth,32,0x00);     //清除报警标志，实现闪烁效果
        else
            OLED_ShowString(xpos,ypos,strbuf,32);
        
        crt_twk = !crt_twk;
    }
}

/********************************************************************************************
* 函数名：Sys_Reset
* 描述  ：恢复默认设置
********************************************************************************************/
void Sys_Reset(void)
{
    OLED_Clear();
    OLED_ShowChinese(72,24,"正在恢复默认设置",16);
    
    STMDATAEEPROM_Read(TH_REAL_RATE_ADDR,(uint32_t *)&sys_cfg.th_real_rate,6);
    
    Float_To_DataUnit(2.5f,RATE_SW);
    sys_cfg.th_real_rate.data = udata.day.rate_data;
    sys_cfg.th_real_rate.unit = udata.day.rate_unit;
    Float_To_DataUnit(136.0f,DAY_DOSE_SW);
    sys_cfg.th_day_dose.data = udata.day.sum_data;
    sys_cfg.th_day_dose.unit = udata.day.sum_unit;
    Float_To_DataUnit(50000.0f,CRT_DOSE_SW);
    sys_cfg.th_crt_dose.data = udata.dose.sum_data;
    sys_cfg.th_crt_dose.unit = udata.dose.sum_unit;
    STMDATAEEPROM_Write(TH_REAL_RATE_ADDR,(uint32_t *)(&sys_cfg.th_real_rate),2);

//        sys_cfg.power_tk = 0;     //不管
    sys_cfg.bright_sz = 5;      //默认亮度等级5
    sys_cfg.scr_off_idx = 0;    //默认熄屏时间15s
//        sys_cfg.sd_func = 0;      //不管
    Set_Bright_Grade(0);
    Set_Sc_Extinct_Time(1);  //此处上传系统配置到EEPROM
    
    Update_DayData_To_EEPROM(true);
    Update_CrtData_To_EEPROM(true);
    DoseRate_Init();
    DoseRate_SetInputMode(DR_INPUT_MODE_REAL);
    DoseRate_SetManualCps(0);
    
    OLED_Clear();
    OLED_ShowChinese(88,24,"设置成功!",16);
    HAL_Delay(500);

    menu_func(NULL,MENU_HOME_1);
}

/********************************************************************************************
* 函数名：Detect_first_use
* 描述  ：检测首次使用，进行基础配置
********************************************************************************************/
uint8_t Detect_first_use(void)
{
    uint32_t get_use_sta = 0;    //获取是否为初次使用
    
    STMDATAEEPROM_Read(FIRST_USE_STA_ADDR,(uint32_t *)(&get_use_sta),1);
    if(get_use_sta != 0x1415)
    {
        OLED_Clear();
        Beep_Off();
        OLED_ShowChinese(78,13,"检测为初次使用",12);
        OLED_ShowChinese(72,39,"准备进行基础设置",12);

        STMDATAEEPROM_Clear_Part(DATA_BASE_ADDR,494);

        // 清除计时模式数据保存偏移(△)、历史记录一轮标志(△)
        // 清除每日剂量(√)、最高剂量率(√)、当日设备累计启动时间(√)、
        // 清除当前剂量(√)、已保存历史记录个数(√)
        // 清除当前剂量的起始时间(√)、覆盖保存数据个数(√)、2组计时模式的记录(√)
//        STMDATAEEPROM_Clear_Part(TIMING_DATA_ADDR,11); // --> (√)
//        STMDATAEEPROM_Clear_Part(PW_TK_ADDR,1);     // --> (△)
        STMDATAEEPROM_Clear_Part(PW_TK_ADDR,12);     // --> (△)(√)
//        STMDATAEEPROM_Clear_Part(DATA_BASE_ADDR,512);    // 清除整个EEPROM
        
        pcf8563_get_cur_time(&data_time);
        data_var.day_date = Get_Date_uint();
        data_var.clr_date = Date_Conv(data_var.day_date);
        STMDATAEEPROM_Write(CLR_DATE_ADDR,(uint32_t *)(&data_var.clr_date),2);  // CLR_DATE_ADDR和DAY_DATE_ADDR均更新

        Sys_Reset();      //恢复默认设置
        get_use_sta = 0x1415;
        STMDATAEEPROM_Write(FIRST_USE_STA_ADDR,(uint32_t *)(&get_use_sta),1);   //标志为非首次使用
        return 0;
    }
    return 1;
}

/********************************************************************************************
* 函数名：Decrease_Offset_Position
* 描述  ：覆盖以往的历史数据
* 输出  ：无
********************************************************************************************/
void Decrease_Offset_Position(void)
{
    if(!sys_cfg.rec_cir)  //未保存一轮数据
    {
        if(!data_var.data_ofs_num)
            return;
        else
            data_var.data_ofs_num--;
    }
    else  //已保存一轮数据
    {
        if(!data_var.data_ofs_num)
            data_var.data_ofs_num = ALL_DATA_NUM - 1;
        else
            data_var.data_ofs_num--;
    }
}

/********************************************************************************************
* 函数名：Increase_Offset_Position
* 描述  ：覆盖往后的历史数据
* 输出  ：无
********************************************************************************************/
void Increase_Offset_Position(void)
{
    if(data_var.data_ofs_num == (ALL_DATA_NUM - 1))
        data_var.data_ofs_num = 0;
    else
        data_var.data_ofs_num++;
}

/********************************************************************************************
* 函数名：Adjust_Offset_Position
* 描述  ：将当前设置日期，跟历史记录的保存日期进行对比，调整下个记录保存的偏移地址，对历史记录进行覆盖
* 输入  ：date --> 当前设置日期
* 输出  ：有效数据增加/减少的个数
* 说明  ：0：首次比较为默认偏移地址处的日期，判断向前还是向后比较；
          1.1：若向前比较，则当当前日期大于被比较的日期时，改为向后比较；
          1.2：若向后比较，则当当前日期符合历史记录保存的时间顺序时，退出比较，并返回有效数据增加/减少的个数。
********************************************************************************************/
int16_t Adjust_Offset_Position(uint32_t date)
{
    bool first_cmp_sta = true;      		//首次比较标志
    bool inc_dec_flag = false;       		// fasle：比较以往的数据  true：比较往后的数据  
    uint8_t greater_cnt = 0;        		//记录当前日期比被比较日期大的次数
    uint8_t i = ALL_DATA_NUM;               //最多比较的数据组数
    uint8_t max_data_offset_addr = 0;       //历史记录（记录数据）的最大日期的偏移地址
    int16_t delta_offset = 0;               //记录有效数据增加/减少的个数
    uint16_t data_offset_num = 0;   		//备份旧的偏移位置
    uint32_t cmp_date = 0;                  //cmp_date为被比较日期（数据保存日期），从EEPROM中读取获得数据保存日期（保存为历史记录时的日期）
    uint32_t max_date = 0;                  //记录最大保存日期，需要时用于比较
    
    data_offset_num = data_var.data_ofs_num;    //记录初始时，下个数据保存的偏移地址
    while(i--)
    {
        STMDATAEEPROM_Read(DATA_BASE_ADDR + data_var.data_ofs_num * HIS_DATA_SIZE,(uint32_t *)(&udata),2);     //读取每条历史记录的保存日期
        
        if(udata.day.rec_type)     //每日累计的历史记录
            cmp_date = udata.day.rec_date;
        else   //总累计的历史记录
            cmp_date = udata.dose.clr_date;

        cmp_date = Date_Inconvert(cmp_date);

        if(!data_var.data_ofs_num && !cmp_date)   //未保存历史记录
            return delta_offset;

        if(!inc_dec_flag)   //比较以往的数据
        {
            if(date >= cmp_date)   //设置的当前日期大于被比较日期------------------------------> 从大于修改为大于等于
            {
                if(cmp_date)       //被比较日期不为0
                {
                    inc_dec_flag = true;    //比较往后的数据
                    greater_cnt++;
                    if(max_date < cmp_date)
                    {
                        max_date = cmp_date;
                        max_data_offset_addr = data_var.data_ofs_num;
                    }
                }
                else    //被比较日期为0
                {
                    if(first_cmp_sta)    //首次比较
                    {
                        first_cmp_sta = false;   //确保往后的偏移地址没有历史记录
                        goto next;
                    }
                    return delta_offset;
                }
            }
        }
        else    //比较往后的数据（inc_dec_flag = 1）
        {
            if(date == cmp_date)
            {
                if(udata.day.rec_type)  //每日累计的历史记录，退出比较
                    return delta_offset;
            }
            else if(date > cmp_date)    //当前日期大于被比较日期，且被比较日期为0
            {
                if(cmp_date)
                {
                    greater_cnt++;
                    if(max_date < cmp_date) //记录比较过程中的最大日期的偏移位置
                    {
                        max_date = cmp_date;
                        max_data_offset_addr = data_var.data_ofs_num;
                    }
                }
                else
                    return delta_offset;
            }
//			else
//				;   // printf("未知原因！\r\n")
        }
        
        next:
        if(inc_dec_flag)        //往后比较
        {
            delta_offset++;
            Increase_Offset_Position();
        }
        else    //往前比较
        {
            delta_offset--;
            Decrease_Offset_Position();
        }
        first_cmp_sta = false;  //清除首次比较标志位
    }
    
    if(!inc_dec_flag && !sys_cfg.rec_cir)   //所有历史记录的日期都大于当前日期
    {
        delta_offset = -ALL_DATA_NUM;
        data_var.data_ofs_num = 0;
        return delta_offset;
    }
    else if(greater_cnt == ALL_DATA_NUM)    //当前设置日期比历史记录的保存日期都大
    {
        delta_offset = ALL_DATA_NUM + 1;
        data_var.data_ofs_num = max_data_offset_addr;
        Increase_Offset_Position();
        return delta_offset;
    }
    
    delta_offset = 0;
    data_var.data_ofs_num = data_offset_num;
    return delta_offset;
}

/********************************************************************************************
* 函数名：Flash_Save_Test
* 描述  ：保存当日累计剂量值和当日平均剂量率,150组
********************************************************************************************/
void Flash_Save_Test(void)
{
    udata.day.rec_type = DAY_TYPE;
    udata.day.rate_unit = UNIT_USV_H;
    udata.day.sum_unit = UNIT_USV_H;
    
    for(uint8_t i = 1;i <= 9;i++)
    {
        for(uint8_t j = 1;j <= 30;j++)
        {
            udata.day.rec_date = i * 10000 + j * 100 + 24;  //模拟数据
            udata.day.sum_data = 120 * i + j * 33;          //模拟数据
            udata.day.rate_data = 150 * (i - 1) + j * 23;   //模拟数据
            
            STMDATAEEPROM_Write(DATA_BASE_ADDR + data_var.data_ofs_num * HIS_DATA_SIZE,(uint32_t *)(&udata),2);  
            Data_Save_Cnt_Add();
        }
    }
}

/********************************************************************************************
* 函数名：Aging_Test
* 描述  ：老化测试
********************************************************************************************/
void Aging_Test(void)
{
    static uint8_t old_aging_sta = AGING_OFF;
    static uint32_t aging_tk = 0;
    
    if(sys_bits.aging_md != AGING_OFF)
    {
        if(old_aging_sta == AGING_OFF)
        {
            old_aging_sta = sys_bits.aging_md;
            System_Time_Init(&aging_tk);
        }
        
//        if(aging_test_t >= 10800000)   //3小时 = 10800000 ms = 3 * 60 * 60 * 1000；
        if(System_Time_Wait(10800000,aging_tk))
        {
            if(sys_bits.aging_md == KEEPING_BRIGHT)
            {
                sys_bits.aging_md = KEEPING_LPR;
                key_ctr.up_tk = LPR_Time_Cnt;
            }
            else   // sys_bits.aging_md == KEEPING_LPR
            {
                sys_bits.aging_md = KEEPING_BRIGHT;
                sys_bits.run_md = EXIT_LPR_MODE;
            }
            old_aging_sta = sys_bits.aging_md;
            System_Time_Init(&aging_tk);
        }
    }
}

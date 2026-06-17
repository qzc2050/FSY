#include "key.h"
#include "lptim.h"
#include "beep.h"
#include "ui_menu.h"
#include "control.h"

static uint8_t key_longup_times = 0;	//长按后松开按键的时间=key_longup_times*20ms
__IO BtnControl_st key_s[KEY_NUM] = {0};

/********************************************************************************************
 * 函数名：Btn_GPIO_Init
 * 描述  ：按键IO口初始化
 ********************************************************************************************/
void Btn_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = KEY_INT_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(KEY_INT_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = KEY1_Pin;
    HAL_GPIO_Init(KEY1_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = KEY2_Pin;
    HAL_GPIO_Init(KEY2_GPIO_Port, &GPIO_InitStruct);	
}

/********************************************************************************************
 * 函数名：KeyScan
 * 描述  ：按键扫描
 * 输入  ：按键接口
 * 输出  ：按键状态
 * 调用  ：内部调用
 ********************************************************************************************/
void KeyScan(BtnControl_st *pBtn, uint8_t bPin)
{
    switch (pBtn->KeyStep)
    {
        case 0:
        {
            if (!bPin)//有键按下
            {
//				printf("KEY按下\r\n");
                pBtn->KeyState = KeyDown;
                if(pBtn->DoubleTimes > 0){
                    pBtn->KeyStep = 4;//双击
//					printf("双击处理\r\n");
                } 
                else
                    pBtn->KeyStep = 2;//不是双击
            }
            else
            {
                if(pBtn->DoubleTimes > 0)
                {
                    pBtn->DoubleTimes--;
                    if ((pBtn->DoubleTimes == 0)&&(pBtn->KeyState == KeyShort))
                    {
//						if(pBtn->Key_LPR_Sta)
//						{
//							pBtn->KeyStatus = NULL;	
//							pBtn->KeyState = KeyUp;
//							pBtn->Key_LPR_Sta = 0;
////							printf("强制无操作处理\r\n");
//						}else 
                        if(pBtn->Key_Long_Sta)
                        {
                            pBtn->KeyStatus = KeyLongUp;
                            pBtn->KeyState = KeyUp;
//							printf("强制无操作处理AAA\r\n");
                        }
                        else
                        {
                            pBtn->KeyStatus = KeyShort;	
                            pBtn->KeyState = KeyUp;
//							printf("短按处理\r\n");
                        }
                    }
                }
                else
                    pBtn->DownTimes = 0;
            }
            break;
        }
        case 1://按下
        {
            if(!bPin)
            {
                pBtn->DownTimes++;
                if(pBtn->DownTimes < LONG_KET_TIME_BASE)
                    pBtn->KeyState = KeyShort;//按下时间小于160ms   8*20ms 
                else
                {
                    if(pBtn->Key_Long_Sta)
                    {
                        pBtn->KeyStatus	= KeyLongUp;
                        pBtn->KeyState	= KeyUp;
                        pBtn->KeyStep = 1;	//弹起
                        pBtn->DownTimes = 0;
                        /*****未修改前*****/
//							key_longup_times = 0;
                        /*****未修改前*****/
                        
                        /*****修改（索引值对应界面支持持续长按）*****/
                        if(key_ctr.muti_long)   //即数值设置界面
                        {
                            pBtn->DownTimes = LONG_KET_TIME_BASE; 
                            if(key_longup_times >= 10)
                                goto key_long_deal;
                        }
                        else
                            key_longup_times = 0;
                        /*****修改*****/
                        bPin = 1;
//						  printf("\r\n长按无效处理\r\n");  //关闭界面持续长按
                        return;
                    }
                    key_long_deal:
                    pBtn->KeyStatus	= KeyLong;
                    pBtn->KeyState	= KeyLong;
                    pBtn->Key_Long_Sta = 1;
                    key_longup_times = 0;
//					  printf("长按处理\r\n");
                }
                if(pBtn->DownTimes >= LONG_KET_TIME_BASE)//防止加满溢出
                    pBtn->DownTimes = LONG_KET_TIME_BASE; 
            }
            else
                pBtn->KeyStep = 2; 
            break;
        }
        case 2:
        {
            if(bPin)
                pBtn->KeyStep = 3;  //弹起
            else 
                pBtn->KeyStep = 1;  //按下
            break;
        }
        case 3://弹起
        {
            if(bPin)//弹起
            {
                if(pBtn->DownTimes <LONG_KET_TIME_BASE)
                    pBtn->DoubleTimes = 7;
                pBtn->DownTimes = 0;
                pBtn->KeyStep = 0;
            }
            else
                pBtn->KeyStep = 2;
            break;
        }
        case 4:
        {
            if(bPin)//如果弹起的话就执行
            {
                pBtn->KeyState = KeyUp;
                pBtn->KeyStatus	= KeyDouble;
                pBtn->KeyStep = 0;	
            }
            break;
        }
        default: pBtn->KeyStep = 0;
            break;
    }
}

/********************************************************************************************
 * 函数名：Key_Func
 * 描述  ：按键扫描
 * 输入  : 无
 * 输出  ：无
 * 调用  ：外部调用
 ********************************************************************************************/
void Key_Func(void)
{
    KeyScan((BtnControl_st *)&key_s[KEY_PC],BTN0);
    KeyScan((BtnControl_st *)&key_s[KEY_L],BTN1);
    KeyScan((BtnControl_st *)&key_s[KEY_R],BTN2);
}

/**************************************************************************
 * 函数名	：BtnOpenPower
 * 描述  	：一键开关
 * 输入  	：无
 * 输出  	：无
 * 调用  	：内部调用
**************************************************************************/
void BtnOpenPower(void)
{
    static uint32_t press_tk = 0;

    if(System_Time_Wait(20,press_tk))
    {
        System_Time_Init(&press_tk);
        KeyScan((BtnControl_st *)&key_s[KEY_PC],BTN0);     //按键扫描
    }
/*****************按键KEY0操作***************/
    switch (key_s[KEY_PC].KeyStatus)
    {
        case KeyLong:       // ======长按======
            BAT_LINK();
            BLUE_LED_ON();
            OLED_Init();
            /************************开机界面************************/
//            OLED_DrawBMP(41,17,172,28,(uint8_t *)power_on);
        
//            OLED_DrawBMP(41,17,170,25,(uint8_t *)power_on);

            OLED_DrawSingleBMP(41,17,170,25,(uint8_t *)WHITE_LOGO);

//            OLED_DrawSingleBMP(40,17,74,25,(uint8_t *)LOGO_1);
//            OLED_DrawBMP(114,17,24,25,(uint8_t *)LOGO_2);
//            OLED_DrawSingleBMP(138,17,26,25,(uint8_t *)LOGO_3);
//            OLED_DrawBMP(164,17,42,25,(uint8_t *)LOGO_4);
            
//            while(1);
            HAL_Delay(1000);
//            OLED_Clear();
            /************************开机界面************************/
            Beep_On();
            if(Detect_first_use())     //检测首次使用，初始化基础配置
                Base_Oper();
            ref_sta = true;  // 刷新日期时间、电池电量、数据
            BLUE_LED_OFF();
            sys_bits.power_sta = POWER_ON;
            break;
        case KeyUp:       // ======松开======
//            BLUE_LED_OFF();    // 暂时去掉，查看效果
            break;
    }
    key_s[KEY_PC].KeyStatus = KeyNull;
/*****************按键KEY0操作***************/
}

/**************************************************************************
 * 函数名	：Power_Off_Operation
 * 描述  	：断电操作
**************************************************************************/
void Power_Off_Operation(void)
{
    if(sys_bits.power_sta == POWER_ON)
    {
        key_s[0].Key_Long_Sta = 0;
        
        if(sys_bits.sd_req == KEEPING_POWER_ON)   //当前状态为开机
        {
            OLED_Clear();
            crt_depth = DEPTH_POWEROFF;
            sys_bits.sd_req = REQUEST_POWER_OFF;   //请求关机
            key_ctr.sd_cd_tk = 1;

            OLED_DrawSingleBMP(118,8,18,32,(uint8_t *)&power_off_icon_2);   //开始关机倒计时
            OLED_ShowNum(124,48,3,1,16);
        }
        if(key_ctr.sd_cd_tk >= 51)      //隔一小段时间进行关机倒数
        {
            key_ctr.sd_cd_tk = 1;
            key_ctr.sd_tk++;
            if((key_ctr.sd_tk % 6) == 0)
            {
                OLED_DrawSingleBMP(118,8,18,32,(uint8_t *)&power_off_icon_2 + (key_ctr.sd_tk / 6) * 96);
                OLED_ShowNum(124,48,3-(key_ctr.sd_tk/6),1,16);
            }
            if(key_ctr.sd_tk >= 16)   //确认为关机
                Sys_Shutdown();    //关机判断，关机处理
//                sys_bits.power_sta = POWER_OFF;
        }
    }
}

/**************************************************************************
 * 函数名	：Sys_Shutdown
 * 描述  	：关机操作
**************************************************************************/
void Sys_Shutdown(void)
{
//    cheak_date(1);      //日期检测
    Update_DayData_To_EEPROM(false);
    Update_CrtData_To_EEPROM(false);
    
    if(timing_ctr.mode != TIMING_MODE_OFF)
        Timing_Mode_Data_Save();
    
    OLED_WR_REG(0xAE);   // OLED进入低功耗模式（关闭屏幕）

    Beep_On();
    BAT_UNLINK();
    HAL_Delay(50);
    BLUE_LED_OFF();
    Beep_Off();
    printf("Shutdown!\r\n");
    while(1);
}

/**************************************************************************
 * 函数名：Key_PC_Double
 * 描  述：开关机按键双击处理
**************************************************************************/
void Key_PC_Double(void)
{
    uint8_t target_inft;
    
    switch(crt_inft)
    {
        case SET_RTH_INT:     target_inft = SET_RTH_UNIT;break;
        case SET_RTH_POINT:   target_inft = SET_RTH_INT;break;
        case SET_RTH_UNIT:    target_inft = SET_RTH_POINT;break;
        case SET_DAYTH_INT:   target_inft = SET_DAYTH_UNIT;break;
        case SET_DAYTH_POINT: target_inft = SET_DAYTH_INT;break;
        case SET_DAYTH_UNIT:  target_inft = SET_DAYTH_POINT;break;
        case SET_CRTTH_INT:   target_inft = SET_CRTTH_UNIT;break;
        case SET_CRTTH_POINT: target_inft = SET_CRTTH_INT;break;
        case SET_CRTTH_UNIT:  target_inft = SET_CRTTH_POINT;break;
        case SET_TIME_YEAR:   target_inft = SET_TIME_MIN;break;
        case SET_TIME_MONTH:  target_inft = SET_TIME_YEAR;break;
        case SET_TIME_DAY:    target_inft = SET_TIME_MONTH;break;
        case SET_TIME_HOUR:   target_inft = SET_TIME_DAY;break;
        case SET_TIME_MIN:    target_inft = SET_TIME_HOUR;break;
        default: 
            if(crt_depth == DEPTH_HOME_1) 
                return;
            target_inft = MENU_HOME_1;
            break;
    }
    menu_func(NULL,target_inft);
}

/**************************************************************************
 * 函数名：Key_PC_Long
 * 描  述：开关机按键长按处理
**************************************************************************/
void Key_PC_Long(void)
{
    key_ctr.muti_long = false;  //关闭多次触发长按

    if(crt_depth == DEPTH_SET_TIME)
        SAVE_RTC_TIME();
    else if((crt_depth == DEPTH_SET_RTH) || (crt_depth == DEPTH_SET_DAYTH) || (crt_depth == DEPTH_SET_CRTTH))
        Save_Val();
    else if((crt_depth != DEPTH_HOME_1) && crt_depth != (DEPTH_HOME_2) && (crt_depth != DEPTH_POWEROFF))
        menu_func(NULL,MENU_HOME_1);
    else
        Power_Off_Operation();
    Request_Twinkle(NULL,NULL,NULL,NULL,true);  // 关闭闪烁
}

/**************************************************************************
 * 函数名：Key_L_Long
 * 描  述：<< 键长按处理
**************************************************************************/
void Key_L_Long(void)
{
    if((crt_inft == MENU_HOME_1) && (!timing_ctr.mode))          //进入计时模式
        Enter_Timing_Mode();
    else if((crt_depth == DEPTH_HOME_1) && (timing_ctr.mode))
        Exit_Timing_Mode();
    else if(crt_inft == HIS_PAGE_HOME)
    {
        sys_bits.key_ls = 1;
        menu_func(NULL,HIS_PAGE_UP);
    }
    else if((crt_depth == DEPTH_SET_RTH) || (crt_depth == DEPTH_SET_DAYTH)\
        || (crt_depth == DEPTH_SET_CRTTH) || (crt_depth == DEPTH_SET_TIME) || (crt_depth == DEPTH_SET_BAR))
        menu_func(NULL,SET_VAL_SUB);
}

/**************************************************************************
 * 函数名：Key_R_Long
 * 描  述：>> 键长按处理
**************************************************************************/
void Key_R_Long(void)
{
    if(((crt_depth == DEPTH_HOME_1) || (crt_depth == DEPTH_HOME_2)) && (!timing_ctr.mode))
        menu_func(NULL,TIMING_HIS);
    else if(crt_depth == DEPTH_TIMING_HIS)
        Exit_Timing_Mode_History();
    else if((crt_depth == DEPTH_HOME_1) && timing_ctr.mode)
        Timing_Mode_Restart();
    else if(crt_inft == HIS_PAGE_HOME)
    {
        sys_bits.key_ls = 1;
        menu_func(NULL,HIS_PAGE_DOWN);
    }
    else if((crt_depth == DEPTH_SET_RTH) || (crt_depth == DEPTH_SET_DAYTH)\
        || (crt_depth == DEPTH_SET_CRTTH) || (crt_depth == DEPTH_SET_TIME) || (crt_depth == DEPTH_SET_BAR))
        menu_func(NULL,SET_VAL_INC);
}

/**************************************************************************
 * 函数名：KeyOperate
 * 描  述：按键操作处理
**************************************************************************/
void KeyOperate(void)
{
    uint8_t time_flag = 0;
    static uint32_t press_tk = 0;
    
    if(sys_bits.his_tip)
    {
        History_tip_keep();
        return;
    }
    
    if(System_Time_Wait(20,press_tk))
    {
        System_Time_Init(&press_tk);
        
        key_longup_times++;
        Key_Func();     //按键扫描
        time_flag = 1;  //定时进行判断按键目前无操作 
    }
    
    for(uint8_t idx = 0;idx < KEY_NUM;idx++)
    {
        switch (key_s[idx].KeyStatus)
        {
            case KeyShort:  // ======按下======
                menu_func(idx,MENU_NULL);
                KEY_DEBUG_PRINTF("KEY%d 短按！\r\n",idx);
                break;
            case KeyDouble: // ======双击======	
                if(idx != KEY_PC)
                    menu_func(idx,MENU_NULL);
                else
                    Key_PC_Double();
                KEY_DEBUG_PRINTF("KEY%d 双击！\r\n",idx);
                break;
            case KeyLong:       // ======长按======
                if(idx == KEY_PC)
                    Key_PC_Long();
                else if(idx == KEY_L)
                    Key_L_Long();
                else
                    Key_R_Long();
                KEY_DEBUG_PRINTF("KEY%d 长按！\r\n",idx);
                break;
            default: 
                break;
        }
        if(time_flag)
        {
            if(key_s[idx].KeyStatus != KeyNull)
                key_ctr.up_tk = 0;
        }
        key_s[idx].KeyStatus = KeyNull;
    }
    
    if(key_longup_times >= 30)   //按键松开600ms后视为长按结束
    {
        key_longup_times = 0;
        key_s[0].Key_Long_Sta = 0;
        key_s[1].Key_Long_Sta = 0;
        key_s[2].Key_Long_Sta = 0;
    }
    
    if(key_ctr.up_tk >= LPR_Time_Cnt)
    {
        key_ctr.up_tk = 0;

        if(sys_bits.aging_md == KEEPING_BRIGHT)   //当前为报警状态，不进入低功耗
            sys_bits.run_md = RUN_MODE;
        else             //无按键触摸一段时间，且无报警
        {
            if(LPR_Time_Cnt != 0xFFFF)
            {
                sys_bits.run_md = LPR_MODE;
                ENTER_LOW_POWER_RUN_MODE();     //进入低功耗运行模式
            }
        }
    }
}

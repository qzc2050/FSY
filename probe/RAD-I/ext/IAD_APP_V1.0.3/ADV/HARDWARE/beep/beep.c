#include "beep.h"
#include "oled.h"
#include "ui_menu.h"
#include "sys_ctr.h"
#include "oledfont.h"
#include "control.h"
#include "low_power_run.h"

uint8_t beep_event = BEEP_EVENT_NULL;

/********************************************************************************************
* 函数名：Beep_On
* 描  述：打开蜂鸣器
* 输  入：无
* 输  出：无
********************************************************************************************/
void Beep_On(void)
{
    // __HAL_TIM_SET_COMPARE(&htim2,TIM_CHANNEL_3,BEEP_DUTY);
}

/********************************************************************************************
* 函数名：Beep_Off
* 描  述：关闭蜂鸣器
* 输  入：无
* 输  出：无
********************************************************************************************/
void Beep_Off(void)
{
    __HAL_TIM_SET_COMPARE(&htim2,TIM_CHANNEL_3,0);
}

/********************************************************************************************
* 函数名：Beep_Alternate
* 描  述：蜂鸣器交替响n次
* 输  入：time(单位:ms)，即每time毫秒转换一次蜂鸣器状态   
          cnt -> 蜂鸣器交替响次数
          ref -> 刷新报警次数和时间
* 输  出：蜂鸣器当前状态
********************************************************************************************/
static bool Beep_Alternate(uint16_t time,uint8_t cnt,bool ref)
{
    static uint32_t beep_tk = 0;            // 计时的起始时间
	static bool beep_sta = BEEP_STA_ON;     // 当前蜂鸣器状态
	static uint8_t beep_times = 0;          // 当前次数
	
    if(ref)
    {
        beep_times = cnt;
        beep_tk -= time;
        beep_sta = BEEP_STA_ON;     // 当前蜂鸣器状态
    }
    
    if(System_Time_Wait(time,beep_tk))
	{
        System_Time_Init(&beep_tk);
        
        if(!beep_times)
        {
            beep_event = BEEP_EVENT_NULL;  // 报警完成
            return beep_sta;
        }
        
		if(beep_sta)
        {
			Beep_On();
            ALARM_LED_ON();
        }
		else
		{
			Beep_Off();
            ALARM_LED_OFF();
            beep_times--;
		}
		beep_sta = !beep_sta;  // 切换报警状态
	}
    return beep_sta;
}

/********************************************************************************************
* 函数名：Beep_Ctr
* 描  述：蜂鸣器应用接口
* 输  入：req_event -> 蜂鸣器指令
* 输  出：无
********************************************************************************************/
void Beep_Ctr(uint8_t req_event)
{
    bool ref = false;
    bool beeping = false;
    
    if(req_event > beep_event)
    {
        if(sys_bits.run_md != RUN_MODE)  // 低功耗模式下请求报警
            Exit_LPR(ALARM_BRIGHT);
        ref = true;
        beep_event = req_event;
    }
    
	switch(beep_event)
    {
        case BEEP_EVENT_NULL:
            Beep_Off();
            return;
        case BEEP_EVENT_RTH:
            beeping = Beep_Alternate(500,1,ref);

            if(crt_depth == DEPTH_HOME_1)
            {
                if(!beeping)
                    OLED_DrawSingleBMP(218,19,36,36,(uint8_t *)&warning_icon);    //超阈值报警标志
                else
                    OLED_Draw_Fill(218,19,18,36,0x00);     //清除报警标志，实现闪烁效果
            }
            break;
        case BEEP_EVENT_DTH:
            Beep_On();
            ALARM_LED_ON();
            break;
        case BEEP_EVENT_LIMIT:
            Beep_On();
            ALARM_LED_ON();
            break;
        case BEEP_EVENT_CLR:
            ALARM_LED_OFF();
            beep_event = BEEP_EVENT_NULL;
            return;
        case BEEP_EVENT_TIMING:
            Beep_Alternate(500,1,ref);
            ALARM_LED_OFF();
            break;
        case BEEP_EVENT_LB:
            Beep_Alternate(500,2,ref);
            break;
        case BEEP_EVENT_TEST:
            Beep_On();
            break;
        case BEEP_EVENT_STOP_TEST:
            beep_event = BEEP_EVENT_NULL;
            break;
        default:return;
    }
    
    if((LPR_Time_Cnt - key_ctr.up_tk) < 5000)
        key_ctr.up_tk = LPR_Time_Cnt - 5000;
}














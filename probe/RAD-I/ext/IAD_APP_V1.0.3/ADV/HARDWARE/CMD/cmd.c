#include "control.h"
#include "beep.h"
#include "stm_flash.h"

extern __IO uint16_t  USART1_RX_STA; //接收状态标记
extern uint8_t USART1_RX_BUF[];      //接收缓冲,最大USART_REC_LEN个字节.
extern __IO uint8_t rec_offset_addr[ALL_DATA_NUM];


//extern HD_Mode_Param hd_param;


bool one_second_cnt_func = false;
bool dose_rate_print_func = false;
bool test_cmd = false;
/********************************************************************************************
* 函数名：Usart_Cmd_Tip
* 描述  ：从串口接收指令
* 输入  : 无
* 输出  ：无
* 调用  ：外部调用
********************************************************************************************/
void Usart_Cmd_Tip(void)
{
	printf("\r\nMAX17048 ID: %#X\r\n\r\n",max17048_get_verison());
//	printf("--> Ver. %02x%08x\r\n",DEVICE_TYPE,SOFTWARE_VERSION);
	printf("\"F1\": 获取记录\r\n");
	printf("\"CC\": 更新程序\r\n");
	printf("\"CU\": 重置出厂\r\n");
	printf("\"CT\": 打印盖革管每秒采集到的脉冲计数\r\n");
	printf("\"CM\": 切换计数、剂量率打印开关\r\n");
	printf("\"CP\": 清除功耗时间(T: %d min)\r\n",sys_cfg.power_tk*5);
	printf("\"RE\": 恢复默认\r\n");
	printf("\"BT\": beep test\r\n");
	printf("\"ST\": 模拟记录\r\n");
	printf("\"SN\": 序列号\r\n");
	printf("\"MT\": 数据模拟\r\n");
	printf("\"AT\": 老化测试\r\n");
	printf("\"setsen,xxx,end\": 设置灵敏度\r\n");
	printf("\"setoff,xxx,end\": 自动关机功能(0/1-开/关)\r\n--当前值: %d--\r\n",sys_cfg.sd_func);
	printf("\"setptime,xxx,yyy,end\": 设置时间(xxx年月日->230831;yyy时分秒->182024)\r\n");
	printf("\"setSN,xxx,end\": 设置序列号\r\n");
	printf("\"drcfg,end\": 打印当前剂量率算法配置\r\n");
	printf("\"setdrmode,xxx,end\": 输入模式(0真实,1模拟,2手动)\r\n");
	printf("\"setdrcps,xxx,end\": 设置手动模式CPS\r\n");
	printf("\"setdrsens,xxx,end\": 设置灵敏度(cpm/uSv/h)\r\n");
	printf("\"setdrth,xxx,end\": 设置阈值CPS\r\n");
	printf("\"setdrdelta,xxx,end\": 设置突变阈值CPS\r\n");
	printf("\"setdralow,xxx,end\": 设置慢速alpha(0~1)\r\n");
	printf("\"setdrahigh,xxx,end\": 设置快速alpha(0~1)\r\n");
	printf("\"setdrboost,xxx,end\": 设置Boost持续秒数\r\n");
	printf("\"setdrall,t,d,al,ah,b,end\": 一次性设置全部EWMA参数\r\n");
	printf("\"setdrreset,end\": 重置EWMA状态\r\n"); 
}

/********************************************************************************************
* 函数名：Uasrt_Cmd_Rx
* 描述  ：从串口接收指令
* 输入  : 无
* 输出  ：无
* 调用  ：外部调用
********************************************************************************************/
void Uasrt_Cmd_Rx(void)
{
	char *p;
//	char gstr[6][16];
	char gstr[9][16];

	uint8_t len,i;
	uint32_t date_buf,get_use_sta;
	Dev_SN_Union SN_temp = {0};
    static bool up_sta = false;
    static bool beep_test = false;
    static bool aging_test = false;

	if (USART1_RX_STA & 0x8000) {     //接受完成
		len = USART1_RX_STA & 0x3fff;   //得到此次接收到的数据长度
		USART1_RX_STA = 0;
        key_ctr.up_tk = 0;

		if (len < 3) {
			if (USART1_RX_BUF[0] == 'F') {
				if (USART1_RX_BUF[1] == '1') {
					UART_PRINTF_HISTORY();
				}
				return;
			}
			else if (USART1_RX_BUF[0] == 'A') {
				if (USART1_RX_BUF[1] == 'T') {
                    if(aging_test)
                    {
                        aging_test = false;
                        sys_bits.aging_md = AGING_OFF;
                        printf("关老化测试!\r\n");
                    }
                    else
                    {
                        aging_test = true;
                        Set_Sc_Extinct_Time(0);
                        sys_bits.aging_md = KEEPING_BRIGHT;
                        sys_cfg.bright_sz = 8;
                        Set_Bright_Grade(0);
                        printf("开老化测试!\r\n");
                    }
				}
				return;
			}
			else if (USART1_RX_BUF[0] == 'B') {
                if (USART1_RX_BUF[1] == 'T') {
                    if(beep_test)
                    {
                        beep_test = false;
                        Beep_Ctr(BEEP_EVENT_STOP_TEST);
                        printf("beep test off!\r\n");
                    }
                    else
                    {
                        beep_test = true;
                        Beep_Ctr(BEEP_EVENT_TEST);
                        printf("beep test on!\r\n");
                    }
				}
				return;
			}
			else if (USART1_RX_BUF[0] == 'M') {
                if (USART1_RX_BUF[1] == 'T') {
                    if(test_cmd)
                    {
                        test_cmd = false;
                        DoseRate_SetInputMode(DR_INPUT_MODE_REAL);
                        printf("geiger cnt test off!\r\n");
                    }
                    else
                    {
                        test_cmd = true;
                        DoseRate_SetInputMode(DR_INPUT_MODE_SIM);
                        printf("geiger cnt test on!\r\n");
                    }
				}
				return;
			}
			else if (USART1_RX_BUF[0] == 'C' && USART1_RX_BUF[1] == 'M') {
                one_second_cnt_func = !one_second_cnt_func;
                dose_rate_print_func = one_second_cnt_func;
                printf("Measure print %s!\r\n", one_second_cnt_func ? "on" : "off");
                return;
			}
			else if (USART1_RX_BUF[0] == 'C') {
				if (USART1_RX_BUF[1] == 'C') {
                    if(up_sta)
                    {
                        printf("取消");
                        Clr_Program_Update();
                        up_sta = false;
                    }
                    else
                    {
                        printf("更新程序!\r\n");
                        up_sta = true;
                        Req_Program_Update();
                        JumpToIAP();
                    }
				}
				if (USART1_RX_BUF[1] == 'T') {
                    if(one_second_cnt_func)
                        one_second_cnt_func = false;
                    else
                        one_second_cnt_func = true;
                    printf("Cnt test %s!\r\n",one_second_cnt_func ? "on":"off");
				}
				else if (USART1_RX_BUF[1] == 'P') {
					sys_cfg.power_tk = 0;
					Save_Sys_Config();
					printf("功测时长清零!\r\n");
				}
				else if (USART1_RX_BUF[1] == 'U') {
					STMDATAEEPROM_Write(FIRST_USE_STA_ADDR,(uint32_t *)(&get_use_sta),1);
					printf("Reset ok!\r\n");
				}
//				else if (USART1_RX_BUF[1] == 'H') {
//					UART_CLEAR_HISTORY();
//					printf("清除历史记录！\r\n");
//				}
				return;
			}
			else if (USART1_RX_BUF[0] == 'R') {
				if (USART1_RX_BUF[1] == 'E') {
					Sys_Reset();
					printf("恢复默认设置!\r\n");
				}
				return;
			}
			else if (USART1_RX_BUF[0] == 'S') {
				if (USART1_RX_BUF[1] == 'N') {
                    STMDATAEEPROM_Read(TH_REAL_RATE_ADDR,(uint32_t *)(&sys_cfg.th_real_rate),6);
                    printf("SEN: %.2f cpm/uSv/h\r\n",DataUnit_To_Float(sys_cfg.sensitivity));
					printf("SN: %s\r\n",sys_cfg.dev.u8_SN);
                    printf("Ver. %02x%08x\r\n",DEVICE_TYPE,SOFTWARE_VERSION);
                    printf("PT: %d min\r\n",sys_cfg.power_tk*5);
                    printf("compile: %s %s\r\n",__DATE__,__TIME__);
				}
				else if (USART1_RX_BUF[1] == 'T') {
					Flash_Save_Test();
					printf("模拟完毕！\r\n");
				}
				return;
			}
		}
		if(len != 0)
		{
			i = 0;
			p = strtok((char*)USART1_RX_BUF, ",");
			while(p)
			{
                if(i >= 9)
                    break;
                strcpy(gstr[i], p);
				i++;
				p = strtok(NULL, ",");
			}
			
			memset(USART1_RX_BUF,0,USART_REC_LEN);
			
			if((i >= 3) && !strcasecmp(gstr[0], "setsen") && !strcasecmp(gstr[2], "end"))
			{
                Float_To_DataUnit(atof(gstr[1]),DAY_DOSE_SW);
                sys_cfg.sensitivity.data = udata.day.sum_data;
                sys_cfg.sensitivity.unit = udata.day.sum_unit;
                DoseRate_SetSensitivity(DataUnit_To_Float(sys_cfg.sensitivity));
				STMDATAEEPROM_Write(TH_CRT_DOSE_ADDR,(uint32_t *)(&sys_cfg.th_crt_dose),1);
				printf("灵敏度: %.2f cpm/uSv/h\r\n",DataUnit_To_Float(sys_cfg.sensitivity));
                return;
			}
			else if((i >= 3) && !strcasecmp(gstr[0], "setoff") && !strcasecmp(gstr[2], "end"))
			{
				sys_cfg.sd_func = atof(gstr[1]);
                Save_Sys_Config();
				printf("自动关机: %d (0/1: 开/关)\r\n",sys_cfg.sd_func);
                return;
			}
			else if((i >= 2) && !strcasecmp(gstr[0], "drcfg") && !strcasecmp(gstr[1], "end"))
			{
                DoseRate_PrintConfig();
                return;
			}
			else if((i >= 3) && !strcasecmp(gstr[0], "setdrmode") && !strcasecmp(gstr[2], "end"))
			{
                uint8_t mode = (uint8_t)atoi(gstr[1]);
                if(!DoseRate_SetInputMode(mode))
                {
                    printf("setdrmode err! mode:0/1/2\r\n");
                    return;
                }
                test_cmd = (mode == DR_INPUT_MODE_SIM);
                printf("dr mode: %d (0-real 1-sim 2-manual)\r\n", mode);
                return;
			}
			else if((i >= 3) && !strcasecmp(gstr[0], "setdrcps") && !strcasecmp(gstr[2], "end"))
			{
                uint32_t cps = (uint32_t)atoi(gstr[1]);
                DoseRate_SetManualCps(cps);
                printf("manual cps: %lu\r\n", (unsigned long)DoseRate_GetManualCps());
                return;
			}
			else if((i >= 3) && !strcasecmp(gstr[0], "setdrsens") && !strcasecmp(gstr[2], "end"))
			{
                float sens = (float)atof(gstr[1]);
                if(!DoseRate_SetSensitivity(sens))
                {
                    printf("setdrsens err! sens>0\r\n");
                    return;
                }
                Float_To_DataUnit(sens,DAY_DOSE_SW);
                sys_cfg.sensitivity.data = udata.day.sum_data;
                sys_cfg.sensitivity.unit = udata.day.sum_unit;
                STMDATAEEPROM_Write(TH_CRT_DOSE_ADDR,(uint32_t *)(&sys_cfg.th_crt_dose),1);
                printf("sensitivity: %.2f cpm/uSv/h\r\n", G_SENSITIVITY_CPM_PER_USVH);
                return;
			}
			else if((i >= 3) && !strcasecmp(gstr[0], "setdrth") && !strcasecmp(gstr[2], "end"))
			{
                if(!DoseRate_SetThresholdCps(atoi(gstr[1])))
                {
                    printf("setdrth err! th>=0\r\n");
                    return;
                }
                printf("threshold cps: %d\r\n", G_EWMA_CONFIG.threshold_cps);
                return;
			}
			else if((i >= 3) && !strcasecmp(gstr[0], "setdrdelta") && !strcasecmp(gstr[2], "end"))
			{
                if(!DoseRate_SetThresholdDelta(atoi(gstr[1])))
                {
                    printf("setdrdelta err! delta>=0\r\n");
                    return;
                }
                printf("threshold delta: %d\r\n", G_EWMA_CONFIG.threshold_delta);
                return;
			}
			else if((i >= 3) && !strcasecmp(gstr[0], "setdralow") && !strcasecmp(gstr[2], "end"))
			{
                if(!DoseRate_SetAlphaLow((float)atof(gstr[1])))
                {
                    printf("setdralow err! 0<alpha<=1\r\n");
                    return;
                }
                printf("alpha low: %.3f\r\n", G_EWMA_CONFIG.alpha_low);
                return;
			}
			else if((i >= 3) && !strcasecmp(gstr[0], "setdrahigh") && !strcasecmp(gstr[2], "end"))
			{
                if(!DoseRate_SetAlphaHigh((float)atof(gstr[1])))
                {
                    printf("setdrahigh err! 0<alpha<=1\r\n");
                    return;
                }
                printf("alpha high: %.3f\r\n", G_EWMA_CONFIG.alpha_high);
                return;
			}
			else if((i >= 3) && !strcasecmp(gstr[0], "setdrboost") && !strcasecmp(gstr[2], "end"))
			{
                if(!DoseRate_SetBoostDuration(atoi(gstr[1])))
                {
                    printf("setdrboost err! 0<=boost<=600\r\n");
                    return;
                }
                printf("boost duration: %d s\r\n", G_EWMA_CONFIG.boost_duration);
                return;
			}
			else if((i >= 7) && !strcasecmp(gstr[0], "setdrall") && !strcasecmp(gstr[6], "end"))
			{
                if(!DoseRate_SetThresholdCps(atoi(gstr[1])) ||
                   !DoseRate_SetThresholdDelta(atoi(gstr[2])) ||
                   !DoseRate_SetAlphaLow((float)atof(gstr[3])) ||
                   !DoseRate_SetAlphaHigh((float)atof(gstr[4])) ||
                   !DoseRate_SetBoostDuration(atoi(gstr[5])))
                {
                    printf("setdrall err! t>=0 d>=0 0<a<=1 0<a<=1 0<=b<=600\r\n");
                    return;
                }
                DoseRate_PrintConfig();
                return;
			}
			else if((i >= 2) && !strcasecmp(gstr[0], "setdrreset") && !strcasecmp(gstr[1], "end"))
			{
                DoseRate_ResetFilter();
                printf("ewma state reset ok\r\n");
                return;
			}
			else if((i >= 4) && !strcasecmp(gstr[0], "setptime") && !strcasecmp(gstr[3], "end"))
			{
                struct time_type__ date_time_buf;
                
				date_buf = atof(gstr[2]);
				date_time_buf.hour = date_buf / 10000;
				date_time_buf.minute = (date_buf / 100) % 100;
				date_time_buf.second = date_buf % 100;
                date_time_buf.week = 0;
                
				date_buf = atof(gstr[1]);
				date_time_buf.year = date_buf / 10000;
				date_time_buf.month = (date_buf / 100) % 100;
				date_time_buf.day = date_buf % 100;
                
				pcf8563_set_cur_time(&date_time_buf);
				pcf8563_get_cur_time(&data_time);
				
				printf("Time：%d %d:%d:%d\r\n",date_buf,date_time_buf.hour,date_time_buf.minute,date_time_buf.second);

                if(memcmp(&date_time_buf,&data_time,7))
                {
                    printf("Set err!\r\n");
                    return;
                }

                data_var.day_date = date_buf;
                STMDATAEEPROM_Write(DAY_DATE_ADDR,(uint32_t *)(&data_var.day_date),1);
                
                /* 调整历史数据保存 */
                Adjust_History(Get_Date_uint());
                
                ref_sta = true;
                return;
			}
			else if((i >= 3) && !strcasecmp(gstr[0], "setSN") && !strcasecmp(gstr[2], "end"))
			{
                gstr[1][11] = 0;
				Set_SN(gstr[1]);
				printf("SN: %s\r\n",sys_cfg.dev.u8_SN);

				Get_SN(SN_temp.u32_SN);
				if(strcmp(SN_temp.u8_SN,(char *)sys_cfg.dev.u8_SN))
					printf("Set err!\r\n");
                return;
			}
//			else if(!strcasecmp(gstr[0], "alpha") && !strcasecmp(gstr[8], "end"))
//			{
//                alpha_arge[0] = atof(gstr[1]);
//                alpha_arge[1] = atof(gstr[2]);
//                alpha_arge[2] = atof(gstr[3]);
//                alpha_arge[3] = atof(gstr[4]);
//                alpha_arge[4] = atof(gstr[5]);
//                alpha_arge[5] = atof(gstr[6]);
//                alpha_arge[6] = atof(gstr[7]);
//				
//                return;
//			}
//			else if(!strcasecmp(gstr[0], "setp1") && !strcasecmp(gstr[2], "end"))
//			{                
//				hd_param.over_cnt = atoi(gstr[1]);
//                printf("高剂量模式 -> 设置 n 秒内采集的总计数 ≥ xxx个（当前值：%d）\r\n",hd_param.over_cnt);
//                return;
//			}
//			else if(!strcasecmp(gstr[0], "setp2") && !strcasecmp(gstr[2], "end"))
//			{                
//				hd_param.muti_cnt = atoi(gstr[1]);
//                printf("高剂量模式 -> 设置连续 n 秒内，每次采集的计数都 ≥ xxx个（当前值：%d）\r\n",hd_param.muti_cnt);
//                return;
//			}
//			else if(!strcasecmp(gstr[0], "setp3") && !strcasecmp(gstr[2], "end"))
//			{
//				hd_param.once_cnt = atoi(gstr[1]);
//                printf("高剂量模式 -> 设置单次采集的计数 ≥ xxx个（当前值：%d）\r\n",hd_param.once_cnt);
//                return;
//			}
//			else if(!strcasecmp(gstr[0], "setp4") && !strcasecmp(gstr[2], "end"))
//			{
//				hd_param.keep_cnt = atoi(gstr[1]);
//                printf("维持高剂量模式 -> 设置单次采集计数 ≥ xxx 个（处于高剂量模式时）（当前值：%d）\r\n",hd_param.keep_cnt);
//                return;
//			}
		}
		Usart_Cmd_Tip();
	}
}

/********************************************************************************************
* 函数名：UART_Date_Printf
* 描述  ：打印日期
* 输入  ：date -> 实际时间   
* 调用  ：外部调用
********************************************************************************************/
void UART_Date_Printf(uint32_t date)
{
    printf("%02d/%02d/%02d - ",date % 100,date / 10000,(date / 100) % 100);
}

/********************************************************************************************
* 函数名：UART_Dose_Printf
* 描述  ：打印剂量值
* 输入  ：data_type -> 数据类型（true -> 当日剂量累计/ false -> 总剂量累计）
* 输出  ：无
* 调用  ：外部调用
********************************************************************************************/
void UART_Dose_Printf(bool data_type)
{
	float temp = (data_type ? udata.day.sum_data : udata.dose.sum_data) / 100.0f;   
    
    switch((data_type ? udata.day.sum_unit : udata.dose.sum_unit))
    {
        case UNIT_USV_H: 
            if(temp < 0.1f)
                printf("<0.10 uSv");
            else
                printf("%.2f uSv", temp);
            break;
        case UNIT_MSV_H: 
            printf("%.2f mSv", temp);
            break;
        case UNIT_SV_H:
            printf("%.2f Sv", temp);
            break;
    }
}

/********************************************************************************************
* 函数名：UART_AVER_PRINTF
* 描述  ：剂量率打印
* 输入  : 无
* 输出  ：单位选择
* 调用  ：外部调用
********************************************************************************************/
void UART_AVER_PRINTF(void)
{
    float temp = udata.day.rate_data / 100.0f;
    
    switch(udata.day.rate_unit)
    {
        case UNIT_USV_H: 
            if(temp < 0.1f)
                printf("<0.10 uSv/h");
            else
                printf("%.2f uSv/h", temp);
            break;
        case UNIT_MSV_H: 
            printf("%.2f mSv/h", temp);
            break;
        case UNIT_SV_H:
            printf("%.2f Sv/h", temp);
            break;
    }
}

/********************************************************************************************
* 函数名：UART_PRINTF_HISTORY
* 描述  ：串口打印历史记录
********************************************************************************************/
void UART_PRINTF_HISTORY(void)
{
	uint32_t read_addr,ofs_read = 0xFFFFFFFF;
	
	sys_bits.rec_rg_prep = 0;
	data_var.crt_page = 0;
	data_var.rec_rg_offset = 0;
	
	if(data_var.history_data_num == 0)    //无数据保存
	{
		null_deal:
		printf("Empty!\r\n");
		return;
	}
	else
	{
		data_var.rec_rg_valid_page = Rec_Mode_Get_Valid_Page();
		if(!data_var.rec_rg_valid_page)  //有数据保存，但是保存的数据不符合显示条件
            goto null_deal;
	}
	
    while(data_var.crt_page < data_var.rec_rg_valid_page)
    {
        for(uint8_t i = 0;i < 3;i++)
        {
            uint8_t read_ofs = i + data_var.crt_page * 3;
            if(((1+rec_offset_addr[read_ofs]) > data_var.history_data_num) || (read_ofs >= ALL_DATA_NUM))
            {
                printf("\r\n数据已输出!\r\n");
                return;
            }
            
            if(!sys_cfg.rec_cir)  //未保存一轮数据
                read_addr = DATA_BASE_ADDR+rec_offset_addr[read_ofs] * HIS_DATA_SIZE;
            else
                read_addr = DATA_BASE_ADDR+data_var.data_ofs_num * HIS_DATA_SIZE
                        +rec_offset_addr[read_ofs] * HIS_DATA_SIZE;
            
            if(ofs_read == read_addr)
                return;
            ofs_read = read_addr;
            
            if(read_addr > DATA_MAX_ADDR)
                read_addr -= (ALL_DATA_NUM * HIS_DATA_SIZE);

            STMDATAEEPROM_Read(read_addr,(uint32_t *)(&udata),2);
            
            UART_Date_Printf(udata.day.rec_date);
            if(udata.day.rec_type)     //每日累计的数据记录
            {
                UART_Dose_Printf(true);
                printf(" - ");
                UART_AVER_PRINTF();
            }
            else     //当前累计总剂量值的数据记录
            {
                UART_Date_Printf(udata.dose.clr_date);
                UART_Dose_Printf(false);
            }
            printf("\r\n");
        }
        data_var.crt_page++;
    }
	printf("\r\n数据已输出!\r\n");
}

/********************************************************************************************
* 函数名：JumpToIAP
* 描述  ：跳转到应用程序
********************************************************************************************/
//void JumpToIAP(void) {
//    typedef void (*pFunction)(void);
//    pFunction JumpToIap;

//    uint32_t JumpAddress;
//    // 检查应用程序地址是否有效
//    if (((*(__IO uint32_t *)0x08000000) & 0x2FFE0000) == 0x20000000) {
//       
//        // 必须禁用所有中断，APP程序开启所有中断，否则跳转可能出错
//        __disable_irq();

//        // 清除所有中断挂起标志（可选）
//        for (int irq = 0; irq < LPUART1_IRQn; irq++) {
//            NVIC_ClearPendingIRQ((IRQn_Type)irq);
//        }
//        
//        // 设置向量表偏移寄存器为系统存储器地址
////        SCB->VTOR = 0x08000000;
//        
//        JumpAddress = *(__IO uint32_t *)(0x08000000 + 4);
//        JumpToIap = (pFunction)JumpAddress;
//        
//        // 设置堆栈指针
//        __set_MSP(*(__IO uint32_t *)0x08000000);
//        
//        // 设置程序计数器
////        JumpToApp = (pFunction)(*(uint32_t *)(FLASH_APP_ADDRESS + 4));
//        JumpToIap();
//    }

//}


void JumpToIAP(void) {
    // 1. 关闭中断
    __disable_irq();

    Update_DayData_To_EEPROM(false);
    Update_CrtData_To_EEPROM(false);
    
    // 2. 复位外设（以HAL库为例）
//    HAL_RCC_DeInit();
    HAL_DeInit();

    // 3. 设置IAP的堆栈指针
    uint32_t *iap_vector_table = (uint32_t*)0x08000000;
    __set_MSP(iap_vector_table[0]);

    // 4. 跳转到IAP
    uint32_t iap_reset_handler = iap_vector_table[1];
    ((void (*)(void))iap_reset_handler)();
}


#include "control.h"
#include "measure_test.h"

__IO Meas_Data_var_st      meas_data_var;
__IO Meas_Opration_Sta_st  Meas_Sys_sta;

/*
 * 函数名：flash_test
 * 描述  ：比较两个缓冲区中的数据是否相等
 * 返回  ：-PASSED pBuffer1 等于   pBuffer2
 *         -FAILED pBuffer1 不同于 pBuffer2
 */
uint8_t flash_test(void)
{
	uint16_t i,BufferLength = 0; 
	
	static uint32_t Tx_Buffer[FLASH_TESTSIZE]={0};
  static uint32_t Rx_Buffer[FLASH_TESTSIZE]={0};
	
	BufferLength = FLASH_TESTSIZE;
/************************内部flash读写测试实验************************/
	/* 调用格式化输出函数打印输出数据 */
  printf("这是一个内部flash读写测试实验\n");  
  
	for(i = 0;i < BufferLength;i++)
		Tx_Buffer[i] = i+1;
	
  /* 向内部Flash写入数据 */
  STMFLASH_Write(FLASH_WriteAddress,Tx_Buffer,FLASH_TESTSIZE);
  /* 小延时 */
  HAL_Delay(10);
  /* 从内部Flash读取数据 */
	STMFLASH_Read(FLASH_ReadAddress,Rx_Buffer,FLASH_TESTSIZE);
  /* 小延时 */
	HAL_Delay(10);
	
	i = 0;
  while(BufferLength--)
  {
		printf("data%d:%d - %d \r\n",i/3,Tx_Buffer[i] , Rx_Buffer[i]);
    if(Tx_Buffer[i] != Rx_Buffer[i])
    {
      return 0;
    }
    i++;
  }
  return 1;
/************************内部flash读写测试实验************************/
}

/********************************************************************************************
* 函数名：Meas_Base_Init（测试用）
* 描述  ：临时测量模式基本初始化配置
********************************************************************************************/
void Meas_Base_Init(void)
{
	uint32_t init_time = 0;
	
	pcf8563_get_cur_time(&data_time);
	init_time = data_time.hour*10000+data_time.minute*100+data_time.second;
	
	STMDATAEEPROM_Write(MEAS_INIT_TIME_ADD,(uint32_t *)(&init_time),1);
	/************************测试************************/
//	STMDATAEEPROM_Read(MEAS_INIT_TIME_ADD,(uint32_t *)(&init_time),1);
//	printf("起始时间：%d",init_time);
	/************************测试************************/
}
/********************************************************************************************
* 函数名：Meas_Recovery_Factory（测试用）
* 描述  ：临时测量模式恢复出产设置（清空保存的历史记录）
********************************************************************************************/
void Meas_Recovery_Factory(void)
{
	uint32_t init_time = 0;
	
	pcf8563_get_cur_time(&data_time);
	init_time = data_time.hour*10000+data_time.minute*100+data_time.second;
	
	Meas_Sys_sta.meas_rec_one_round = 0;
	meas_data_var.meas_valid_data_num = 0;
	meas_data_var.meas_data_offset_num = 0;
	
	STMDATAEEPROM_Write(MEAS_INIT_TIME_ADD,(uint32_t *)(&init_time),1);
	STMDATAEEPROM_Write(MEAS_REC_ONE_ROUND_ADD,(uint32_t *)(&Meas_Sys_sta.meas_rec_one_round),1);
	STMDATAEEPROM_Write(MEAS_VALID_DATA_NUM_ADD,(uint32_t *)(&meas_data_var.meas_valid_data_num),1);
	STMDATAEEPROM_Write(MEAS_SAVE_DATA_NUM_ADD,(uint32_t *)(&meas_data_var.meas_data_offset_num),1);
	
//	STMDATAEEPROM_Read(MEAS_REC_ONE_ROUND_ADD,(uint32_t *)(&Meas_Sys_sta.meas_rec_one_round),1);
//	STMDATAEEPROM_Read(MEAS_VALID_DATA_NUM_ADD,(uint32_t *)(&meas_data_var.meas_valid_data_num),1);
//	STMDATAEEPROM_Read(MEAS_SAVE_DATA_NUM_ADD,(uint32_t *)(&meas_data_var.meas_data_offset_num),1);
//	printf("%d-%d-%d\r\n\r\n",Meas_Sys_sta.meas_rec_one_round,meas_data_var.meas_valid_data_num,meas_data_var.meas_data_offset_num);
}

/********************************************************************************************
* 函数名：Meas_Data_Num_Add（测试用）
* 描述  ：保存的有效数量加1，偏移地址加1
********************************************************************************************/
void Meas_Data_Num_Add(void)
{
	if(meas_data_var.meas_valid_data_num < MEAS_FLASH_SAVE_DATA_NUM)
	{
		meas_data_var.meas_valid_data_num++;
		STMDATAEEPROM_Write(MEAS_VALID_DATA_NUM_ADD,(uint32_t *)(&meas_data_var.meas_valid_data_num),1);
		
		if((meas_data_var.meas_valid_data_num == MEAS_FLASH_SAVE_DATA_NUM) && !Meas_Sys_sta.meas_rec_one_round)
		{
			Meas_Sys_sta.meas_rec_one_round = 1;
			STMDATAEEPROM_Write(MEAS_REC_ONE_ROUND_ADD,(uint32_t *)(&Meas_Sys_sta.meas_rec_one_round),1);
		}
	}
	
	if(meas_data_var.meas_data_offset_num < MEAS_FLASH_SAVE_DATA_NUM)
	{
		if(!meas_data_var.meas_data_offset_num && Meas_Sys_sta.meas_rec_one_round)
			meas_data_var.meas_valid_data_num -= 7;
		
		meas_data_var.meas_data_offset_num++;
		STMDATAEEPROM_Write(MEAS_SAVE_DATA_NUM_ADD,(uint32_t *)(&meas_data_var.meas_data_offset_num),1);
		
		if(meas_data_var.meas_data_offset_num == MEAS_FLASH_SAVE_DATA_NUM)
		{
			meas_data_var.meas_data_offset_num = 0;
			STMDATAEEPROM_Write(MEAS_SAVE_DATA_NUM_ADD,(uint32_t *)(&meas_data_var.meas_data_offset_num),1);
			STMDATAEEPROM_Write(MEAS_VALID_DATA_NUM_ADD,(uint32_t *)(&meas_data_var.meas_valid_data_num),1);
		}
	}
}

/********************************************************************************************
* 函数名：Meas_Test_Mode（测试用）
* 描述  ：临时测量模式（清空主界面的平均剂量率的数值和DOSE值）
* 输出  ：1：刷新OLED显示屏上的数据     0：不刷新OLED显示屏上的数据
********************************************************************************************/
//int Meas_Test_Mode(void)
//{
//	char str_buf[18] = {0};
//	
//	struct meas_data_struct{
//		uint32_t date;
//		uint32_t init_time;
//		uint32_t clear_time;
//		float aver_dose_rate;
//	}meas_data;
//	/******************保存累计数据******************/
//	pcf8563_get_cur_time(&data_time);
//	meas_data.date = data_time.year*10000+data_time.month*100+data_time.day;
//	STMDATAEEPROM_Read(MEAS_INIT_TIME_ADD,(uint32_t *)(&meas_data.init_time),1);
//	meas_data.clear_time = data_time.hour*10000+data_time.minute*100+data_time.second;
//	STMDATAEEPROM_Write(MEAS_INIT_TIME_ADD,(uint32_t *)(&meas_data.clear_time),1);
////	meas_data.aver_dose_rate = data_var.Aver_dose_rate;
//	
//	STMFLASH_Write(MEAS_DATA_WRITE_BASE_ADD+meas_data_var.meas_data_offset_num*16,(uint32_t *)(&meas_data.date),1);
//	STMFLASH_Write(MEAS_DATA_WRITE_BASE_ADD+meas_data_var.meas_data_offset_num*16+4,(uint32_t *)(&meas_data.init_time),1);
//	STMFLASH_Write(MEAS_DATA_WRITE_BASE_ADD+meas_data_var.meas_data_offset_num*16+8,(uint32_t *)(&meas_data.clear_time),1);
//	STMFLASH_Write(MEAS_DATA_WRITE_BASE_ADD+meas_data_var.meas_data_offset_num*16+12,(uint32_t *)(&meas_data.aver_dose_rate),1);

//	Meas_Data_Num_Add();
//	/******************保存累计数据******************/
//	data_var.main_dose = 0;
//	TIM_Var.startup_time = 0;
//	
////	Sys_sta.cal_mode = AVG;
////	OLED_ShowString(16,24," AVG ",16,0);
//	
////	if(Sys_sta.cal_uint == CPM)
////		OLED_Draw_Fill(64,16,76,32,0x00,0);
//	Sys_sta.cal_uint = CPS;
//	
//	unit_sta = uint_deal(str_dose_rate,data_var.dose_rate);
//	Unit_Show(64,16,32,144,28,16,str_dose_rate,unit_sta,0);      //刷新屏幕实时剂量率
//	OLED_Dose_Show(149,48,16,str_buf,data_var.main_dose);
//	Sys_sta.meas_prep_sta = 1;
////	Meas_Print_History();     //测试
//	return 0;
//}

/********************************************************************************************
* 函数名：Meas_Usart_Time_Print
* 描述  ：打印日期
* 输入  ：time -> 实际时间   
* 调用  ：外部调用
********************************************************************************************/
void Meas_Usart_Time_Print(uint32_t time)
{
	struct time_type__ time_buf;
	
	time_buf.hour = (time / 10000) % 100;
	time_buf.minute = (time / 100) % 100;
	time_buf.second = time % 100;
	
	if(time_buf.hour < 10)
	{
		if(time_buf.minute < 10)
		{
			if(time_buf.second < 10)
				printf("0%d:0%d:0%d - ",time_buf.hour,time_buf.minute,time_buf.second);
			else
				printf("0%d:0%d:%d - ",time_buf.hour,time_buf.minute,time_buf.second);
		}
		else{
			if(time_buf.second < 10)
				printf("0%d:%d:0%d - ",time_buf.hour,time_buf.minute,time_buf.second);
			else
				printf("0%d:%d:%d - ",time_buf.hour,time_buf.minute,time_buf.second);
		}
	}
	else
	{
		if(time_buf.minute < 10)
		{
			if(time_buf.second < 10)
				printf("%d:0%d:0%d - ",time_buf.hour,time_buf.minute,time_buf.second);
			else
				printf("%d:0%d:%d - ",time_buf.hour,time_buf.minute,time_buf.second);
		}
		else{
			if(time_buf.second < 10)
				printf("%d:%d:0%d - ",time_buf.hour,time_buf.minute,time_buf.second);
			else
				printf("%d:%d:%d - ",time_buf.hour,time_buf.minute,time_buf.second);
		}
	}
}

/********************************************************************************************
* 函数名：Meas_Usart_Aver_Print
* 描述  ：剂量率打印
********************************************************************************************/
void Meas_Usart_Aver_Print(float dose_rate)
{
	if(dose_rate < 100)
		printf("%.2f uSv/h\r\n", dose_rate);
	else if(dose_rate < 1E5)
		printf("%.2f mSv/h\r\n", dose_rate/1E3);
	else if(dose_rate < 1E8)
		printf("%.2f Sv/h\r\n", dose_rate/1E6);
}

/********************************************************************************************
* 函数名：Meas_Print_History
* 描述  ：串口打印历史记录
********************************************************************************************/
void Meas_Print_History(void)
{
	struct meas_data_struct{
		uint32_t date;
		uint32_t init_time;
		uint32_t clear_time;
		float aver_dose_rate;
	}meas_data_buf;
	
	uint8_t printf_sta = 1;
	uint16_t all_valid_data = 0;
	uint8_t invalid_data = 0;
	uint8_t current_offset = 0;
	uint32_t read_addr = 0,temp = 0;
	
	printf("\r\n");
	if(meas_data_var.meas_valid_data_num == 0)
	{
		printf("未保存任何数据！\r\n\r\n");
		return;
	}
	
	if(!Meas_Sys_sta.meas_rec_one_round)
	{
		while(current_offset < meas_data_var.meas_valid_data_num)
		{
			read_addr = FLASH_WriteAddress + (current_offset * 16);
			
			STMFLASH_Read(read_addr,(uint32_t *)(&meas_data_buf),4);
			
			UART_Date_Printf(meas_data_buf.date);
			Meas_Usart_Time_Print(meas_data_buf.init_time);
			Meas_Usart_Time_Print(meas_data_buf.clear_time);
			Meas_Usart_Aver_Print(meas_data_buf.aver_dose_rate);
			current_offset++;
			all_valid_data = current_offset;
		}
		if(current_offset == meas_data_var.meas_valid_data_num)
		{
			printf_sta = 0;
			printf("\r\n1：共%d组数据，全部打印完毕！\r\n\r\n",all_valid_data);
		}
	}
	else
	{
		while(current_offset < meas_data_var.meas_valid_data_num)
		{
			read_addr = FLASH_WriteAddress+meas_data_var.meas_data_offset_num*16
						 +(MEAS_FLASH_SAVE_DATA_NUM - meas_data_var.meas_valid_data_num)*16
						 +(current_offset*16);	
//			printf("\r\naddr: %#X",read_addr);
			if(read_addr > MEAS_DATA_WRITE_MAX_ADDR)
			{
				temp = read_addr-(MEAS_FLASH_SAVE_DATA_NUM * 16);
				STMFLASH_Read(temp,(uint32_t *)(&meas_data_buf),4);
			}
			else
				STMFLASH_Read(read_addr,(uint32_t *)(&meas_data_buf),4);
			
			if(meas_data_buf.date == 0)
			{
				invalid_data++;
				if(invalid_data > 8)
					break;
				else
				{
					current_offset++;
					continue;
				}
			}
			
			UART_Date_Printf(meas_data_buf.date);
			Meas_Usart_Time_Print(meas_data_buf.init_time);
			Meas_Usart_Time_Print(meas_data_buf.clear_time);
			Meas_Usart_Aver_Print(meas_data_buf.aver_dose_rate);
			current_offset++;
		}
		if(current_offset == meas_data_var.meas_valid_data_num)
		{
			printf_sta = 0;
			all_valid_data = current_offset - invalid_data;
			invalid_data = 0;
			printf("\r\n2：共%d组数据，全部打印完毕！\r\n\r\n",all_valid_data);
		}
	}
	if(printf_sta)
		printf("\r\n3：共%d组数据，全部打印完毕！\r\n\r\n",meas_data_var.meas_valid_data_num);
}

/********************************************************************************************
* 函数名：Meas_Print_dose_rate
* 描述  ：串口打印平均剂量率和dose剂量值
********************************************************************************************/
void Meas_Print_dose_rate(void)
{
	/*************************测试*************************/
//	int sta = 0;
	char str_buf[18] = {0};
	Dose_To_Str(str_buf,data_var.main_dose);
	printf("%s\r\n",str_buf);
	
//	data_var.Aver_dose_rate = (float)data_var.main_dose * 3600000 / TIM_Var.startup_time;
//	aver_unit_sta = uint_deal(str_ave_dose_rate,data_var.Aver_dose_rate);
//	printf("%s ",str_ave_dose_rate);
//	if(aver_unit_sta <= 1)
//			sta = 0;
//		else if(aver_unit_sta <= 5)
//			sta = 2;
//		else if(aver_unit_sta <= 8)
//			sta = 6;
//		
//		switch(sta)
//		{
//			case 0:	printf("uSv/h\r\n\r\n");
//							break;
//			case 2:	printf("mSv/h\r\n\r\n");
//							break;
//			case 6:	printf(" Sv/h\r\n\r\n");
//							break;
//			default:break;
//		}
	/*************************测试*************************/
}



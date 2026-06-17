/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
#include "spi.h"
#include "control.h"
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "lptim.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "iwdg.h"
#include "beep.h"
#include "key.h"
#include "dose_rate.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define FLASH_APP_ADDRESS 0x08001000  // 新的起始地址
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
    /* USER CODE BEGIN 1 */
    // 设置向量表偏移寄存器（IAPD的APP程序，直接烧录貌似是跑不了的）
//    SCB->VTOR = FLASH_APP_ADDRESS;  // 修改system_stm32l0xx.c的VECT_TAB_OFFSET宏定义
    
    /* USER CODE END 1 */

    /* MCU Configuration--------------------------------------------------------*/

    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();
    /* USER CODE BEGIN Init */
    /* USER CODE END Init */

    /* Configure the system clock */
    SystemClock_Config();
    /* USER CODE BEGIN SysInit */
    __enable_irq();
    /* USER CODE END SysInit */

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_ADC_Init();
    MX_TIM2_Init();
    MX_USART1_UART_Init();
    MX_LPTIM1_Init();
    MX_SPI1_Init();
    /* USER CODE BEGIN 2 */
    MX_IWDG_Init();
    IIC_Init();
    Btn_GPIO_Init();
    sys_bits.power_sta = POWER_OFF; //目前为关机状态
    sys_bits.run_md = RUN_MODE;     //进入正常运行模式
    // key_s[0].KeyStatus = KeyUp;     // 避免开机后就立马自动关机
    /* 一键开关 */
    while(1)
    {
//        BLUE_LED_ON();
//        ALARM_LED_ON();
//        HAL_Delay(500);
//        BLUE_LED_OFF();
//        ALARM_LED_OFF();
//        HAL_Delay(500);
        
        if(Get_vol() > 2.9f)            //电压大于2.9V才能开机
        {
            BtnOpenPower();             //开机按键检测
            if(sys_bits.power_sta == POWER_ON)
                break;
        }
        HAL_IWDG_Refresh(&hiwdg);
    }
    /* 一键开关 */

    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
//    Detect_first_use();     //检测首次使用，初始化基础配置
    /**************************************测试**************************************/
    /* 使用一键开关后屏蔽 */
//	  BAT_LINK();    //换成一键开机后屏蔽
//	  OLED_Init();        //换成一键开机后屏蔽
//	  Base_Oper();        //换成一键开机后屏蔽
    
////	while(1);
    /* 使用一键开关后屏蔽 */


//	  Sys_Reset();
//	  DBGMCU->CR |= DBGMCU_CR_DBG_STOP; /* 为了能够在低功耗模式下调试 */
//    printf("Flash读写测试：%d  （0：失败  1：成功）\r\n\r\n",flash_test());
//	  Beep_Off();
//    data_var.switch_bound = SWITCH_BOUND_VAL/sys_cfg.geiger_coef;
//	  STMDATAEEPROM_Write(SWITCH_BOUND_ADDR,(uint32_t *)(&data_var.switch_bound),1);
    /**************************************测试**************************************/
    
//    key_s[0].KeyStatus = KeyUp;     // 避免开机后就立马自动关机
//    Req_Program_Update();
    Clr_Program_Update();
    DoseRate_Init();
    
    
    
    
    
    while(1)
    {
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
        
//      USART1->TDR = 'R';
//      while(!(USART1->ISR & 0X40));//等待发送结束
//      USART1->TDR = 'A';
//      while(!(USART1->ISR & 0X40));//等待发送结束
//      USART1->TDR = 'Y';
//      while(!(USART1->ISR & 0X40));//等待发送结束
//      USART1->TDR = 'D';
//      while(!(USART1->ISR & 0X40));//等待发送结束
//      USART1->TDR = 'O';
//      while(!(USART1->ISR & 0X40));//等待发送结束
//      USART1->TDR = 'S';
//      while(!(USART1->ISR & 0X40));//等待发送结束
//      USART1->TDR = 'E';
//      while(!(USART1->ISR & 0X40));//等待发送结束

        Data_Cal();             //读取、处理数据
        Gap_Execute();          //定时进行电池检测，刷新日期时间、数据
        ref_sta = false;

        if(sys_bits.run_md == RUN_MODE)
        {
            Beep_Ctr(beep_event);   //蜂鸣器报警
            Uasrt_Cmd_Rx();         //串口指令
            KeyOperate();           //按键操作、判定进入低功耗
        }
        else if(sys_bits.run_md == EXIT_LPR_MODE)    // 准备退出低功耗模式
            Exit_LPR(KEY_BRIGHT);
        else    // 低功耗模式
            LPR_LED_Twinkle();  //低功耗模式下，灯每5s闪烁一次

        Aging_Test();           //老化测试
        Power_Test();           //电池功耗测试
        Timing_Mode_Time_Up();  //计时模式下，刷新累计时间
        HAL_IWDG_Refresh(&hiwdg);   //喂狗
    }
    /* USER CODE END 3 */
}

/**q
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
//	/*---------------使用内部晶振---------------*/
    // RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    // RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

//    /** Configure the main internal regulator output voltage
//    */
//    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

//    /** Initializes the RCC Oscillators according to the specified parameters
//    * in the RCC_OscInitTypeDef structure.
//    */
//    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
//    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
//    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
//    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
//    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
//    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLLMUL_4;
//    RCC_OscInitStruct.PLL.PLLDIV = RCC_PLLDIV_2;
//    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
//    {
//        Error_Handler();
//    }

//    /** Initializes the CPU, AHB and APB buses clocks
//    */
//    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
//                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
//    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
//    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
//    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
//    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

//    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
//    {
//        Error_Handler();
//    }
//	/*---------------使用内部晶振---------------*/
    
    /*---------------使用外部高速晶振+内部低速晶振---------------*/
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    /** Configure the main internal regulator output voltage
    */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /** Initializes the RCC Oscillators according to the specified parameters
    * in the RCC_OscInitTypeDef structure.
    */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.LSIState = RCC_LSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
    */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSE;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
    {
        Error_Handler();
    }
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1|RCC_PERIPHCLK_RTC
                                |RCC_PERIPHCLK_LPTIM1;
    PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
    PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
    PeriphClkInit.LptimClockSelection = RCC_LPTIM1CLKSOURCE_PCLK;

    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
        Error_Handler();
    }
    /*---------------使用外部高速晶振+内部低速晶振---------------*/
}

/* USER CODE BEGIN 4 */

/*
 * 函数名：Buffercmp
 * 描述  ：比较两个缓冲区中的数据是否相等
 * 输入  ：-sta   1：清空dataeeprom  
                    0：测试模式
 * 输出  ：false 测试失败
            true 测试成功
 */
//bool Buffercmp(int sta)
//{
//	uint16_t i,BufferLength = 0; 
//	uint32_t buf = 0;
//	
//	static uint32_t Tx_Buffer[DATAEEPROM_TESTSIZE]={0};
//  static uint32_t Rx_Buffer[DATAEEPROM_TESTSIZE]={0};
//	
///*****************************清空DATAEEPROM*****************************/
//	if(sta)
//	{
//		HAL_FLASHEx_DATAEEPROM_Unlock();						//解锁
//		for(i=0;i<512;i++)
//		{ 
//			HAL_FLASHEx_DATAEEPROM_Erase(DATA_BASE_ADDR+4*i);   //清除要写入地址的数据
////			printf("\r\n%d----%d\r\n",i+1,status);
//		}
//		i = 0;
//		for(i=0;i<512;i++)
//		{ 
//			STMDATAEEPROM_Read(DATA_BASE_ADDR+4*i,&buf,1);   //清除要写入地址的数据
////			printf("\r\ndd%d----%d\r\n",i+1,buf);
//			if(buf != 0)
//			{
//				HAL_FLASHEx_DATAEEPROM_Lock();//上锁
//				return false;
//			}
//		}
//		HAL_FLASHEx_DATAEEPROM_Lock();//上锁
//		return true;
//	}
///*****************************清空DATAEEPROM*****************************/
//	
///************************内部EEPROM读写测试实验************************/
////  printf("这是一个内部EEPROM读写测试实验\n");  
//	BufferLength = DATAEEPROM_TESTSIZE;
//	for(i=0;i<BufferLength;++i)
//  {
////		Tx_Buffer[i]=0;
//		if((i+1)%7 == 0)
//			Tx_Buffer[i]=i+230201+1000000;
//    else// if((i+1)%3 == 0)
//			Tx_Buffer[i]=i+1;
////		printf("da1:%d\r\n",Tx_Buffer[i]);
//		Rx_Buffer[i]=0;
//  }	
//	
//	/* 向内部Flash写入数据 */
//  STMDATAEEPROM_Write(DATAEEPROM_WriteAddress,Tx_Buffer,BufferLength);
//  /* 从内部Flash读取数据 */
//	STMDATAEEPROM_Read(DATAEEPROM_ReadAddress,Rx_Buffer,BufferLength);

//	i = 0;
//  while(BufferLength--)
//  {
////		printf("da2:%d-%d\r\n",Tx_Buffer[i] , Rx_Buffer[i]);
//    if(Tx_Buffer[i] != Rx_Buffer[i])
//    {
//      return false;
//    }
//    i++;
//  }
//	return true;
///************************内部EEPROM读写测试实验************************/
//}

void HAL_Delay_us(uint32_t us)   //延时us函数
{
    //需要cubemx配置时SYS时基源选择SysTick
    __IO uint32_t currentTicks = SysTick->VAL;
    /* Number of ticks per millisecond */
    const uint32_t tickPerMs = SysTick->LOAD + 1;
    /* Number of ticks to count */
    const uint32_t nbTicks = ((us - ((us > 0) ? 1 : 0)) * tickPerMs) / 1000;
    /* Number of elapsed ticks */
    uint32_t elapsedTicks = 0;
    __IO uint32_t oldTicks = currentTicks;
    do {
        currentTicks = SysTick->VAL;
        elapsedTicks += (oldTicks < currentTicks) ? tickPerMs + oldTicks - currentTicks :
                        oldTicks - currentTicks;
        oldTicks = currentTicks;
    }while (nbTicks > elapsedTicks);
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1)
    {
        return ;  /*************************暂时这样修改，避免程序卡死**************/
    }
    /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
        ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

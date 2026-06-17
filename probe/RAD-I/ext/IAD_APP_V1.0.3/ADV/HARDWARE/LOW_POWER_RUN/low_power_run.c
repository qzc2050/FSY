#include "control.h"
#include "ui_menu.h"

uint32_t LPR_Time_Cnt;
//uint32_t old_lpr_bat_time = 0;

//extern int th_unit_sta;
//extern char th_str_buf[];
extern uint8_t USART1_RX_BUF[];        //接收缓冲,最大USART_REC_LEN个字节.
extern __IO uint16_t  USART1_RX_STA;   //接收状态标记
extern SPI_HandleTypeDef hspi1;

/********************************************************************************************
* 函数名：SystemClock_MSI_131
* 描述  ：配置系统时钟为131KhZ
* 调用  ：外部调用
********************************************************************************************/
void SystemClock_MSI_131(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_1;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

void WAKEUP_PB_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE(); 
    __HAL_RCC_GPIOB_CLK_ENABLE();

	GPIO_InitStruct.Pin =  KEY_INT_Pin;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
	HAL_GPIO_Init(KEY_INT_GPIO_Port, &GPIO_InitStruct);
	
	GPIO_InitStruct.Pin =  KEY2_Pin;
	HAL_GPIO_Init(KEY2_GPIO_Port, &GPIO_InitStruct);
	
	GPIO_InitStruct.Pin =  KEY1_Pin;
	HAL_GPIO_Init(KEY1_GPIO_Port, &GPIO_InitStruct);
	
	__HAL_GPIO_EXTI_CLEAR_IT(KEY_INT_Pin);
	__HAL_GPIO_EXTI_CLEAR_IT(KEY2_Pin);
	__HAL_GPIO_EXTI_CLEAR_IT(KEY1_Pin);
	
	HAL_GPIO_WritePin(LCD_EN_GPIO_Port, LCD_EN_Pin, GPIO_PIN_RESET);
	
	GPIO_InitStruct.Pin =  GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_12;   // UART_TX、RX、SPI_MOSI
    GPIO_InitStruct.Mode  = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
	
	GPIO_InitStruct.Pin = GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_10;     // SPI1_SCK、CS、PWM_BUZZER_PIN(TIM2_CH3)
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
	
//	HAL_TIM_Base_MspDeInit(&htim2);
    __HAL_RCC_TIM2_CLK_DISABLE();    // 关闭蜂鸣器PWM引脚对应的定时器
	__HAL_RCC_USART1_CLK_DISABLE();
//	HAL_NVIC_DisableIRQ(USART1_IRQn);
	
	HAL_NVIC_EnableIRQ(EXTI0_1_IRQn);
	HAL_NVIC_EnableIRQ(EXTI2_3_IRQn);
	HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
}

/********************************************************************************************
* 函数名：Sys_LPR_Setting
* 描述  ：系统低功耗设置
* 调用  ：外部调用
********************************************************************************************/
void Sys_LPR_Setting(void)
{
	RCC->APB1ENR |= RCC_APB1ENR_PWREN;
	SystemClock_MSI_131();        //降低时钟频率为131.072KHz，调整电压调节器电压范围为范围2
	PWR->CR &= ~PWR_CR_LPRUN;     //确保系统不在低功耗运行模式
	HAL_PWREx_EnableLowPowerRunMode();   //进入低功耗运行模式
}

/********************************************************************************************
* 函数名：ENTER_LOW_POWER_RUN_MODE
* 描述  ：进入低功耗运行模式
* 调用  ：外部调用
********************************************************************************************/
void ENTER_LOW_POWER_RUN_MODE(void)
{
	OLED_WR_REG(0xAE);            //设置OLED进入低功耗模式
	
    Request_Twinkle(NULL,NULL,NULL,NULL,true);  // 关闭闪烁
    key_ctr.muti_long = false;  //关闭多次触发长按
    WAKEUP_PB_Init();
//	printf("Enter LPR mode!\r\n");
	Sys_LPR_Setting();
}

/********************************************************************************************
* 函数名：LPR_LED_Twinkle
* 描述  ：低功耗运行模式下LED定时闪烁
* 调用  ：外部调用
********************************************************************************************/
void LPR_LED_Twinkle(void)
{
	static uint32_t twinkle_tk = 0;

	if(System_Time_Wait(LPR_LED_TWINKLE_TIME,twinkle_tk))
	{
		System_Time_Init(&twinkle_tk);
		BLUE_LED_ON();      //开灯
	}
	else
	{
		if(System_Time_Wait(100,twinkle_tk))
			BLUE_LED_OFF(); //关灯
	}
}

/********************************************************************************************
* 函数名：EXIT_LOW_POWER_RUN_MODE
* 描述  ：退出低功耗运行模式
* 调用  ：外部调用
********************************************************************************************/
void EXIT_LOW_POWER_RUN_MODE(void)
{
	__disable_irq();       //关闭所有中断，系统滴答定时器中断可能导致执行HAL_RCC_ClockConfig时出现卡死
	HAL_PWREx_DisableLowPowerRunMode();
	SystemClock_Config();
	__enable_irq();
	
	HAL_GPIO_WritePin(LED_ALARM_GPIO_Port, LED_ALARM_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(LCD_EN_GPIO_Port, LCD_EN_Pin, GPIO_PIN_SET);
	
	Btn_GPIO_Init();
	
	IIC_SCL(GPIO_PIN_SET);
	IIC_SDA(GPIO_PIN_SET);
	
	HAL_SPI_MspInit(&hspi1);
	OLED_Init();
	Set_Bright_Grade(0);

	MX_USART1_UART_Init();
	HAL_UART_MspInit(&huart1);
	
//	Timing_Mode_Add_Time();    // 暂时去掉，观察效果
    menu_func(NULL,MENU_HOME_1);
	key_ctr.up_tk = 0;
	
	__HAL_RCC_TIM2_CLK_ENABLE();
	MX_TIM2_Init();
	
	BLUE_LED_OFF();     //关灯
    
	USART1_RX_STA = 0;
    memset(USART1_RX_BUF,0,30);
//	printf("Exit LPR mode!\r\n");
}

/********************************************************************************************
* 函数名：LPR_Critical_Execute
* 描述  ：低功耗运行模式下LED定时闪烁
* 调用  ：外部调用
********************************************************************************************/
void LPR_Critical_Execute(void (*execute_fun)(),void *p)
{
    if(sys_bits.run_md != RUN_MODE)   //当前为低功耗模式下则退出再保存（是因为保存耗时吗？如果IIC读取不耗时则屏蔽）
    {
        __disable_irq();
        HAL_PWREx_DisableLowPowerRunMode();
        SystemClock_Config();
    }
    
    if(p)
        execute_fun(p);
    else
        execute_fun();
    
    if(sys_bits.run_md != RUN_MODE)    //重新进入低功耗模式
    {
        Sys_LPR_Setting();
        __enable_irq();
    }
}




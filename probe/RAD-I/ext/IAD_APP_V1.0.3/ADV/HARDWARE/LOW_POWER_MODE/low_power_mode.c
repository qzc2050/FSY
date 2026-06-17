#include "control.h"
#include "lptim.h"
#include "power_ctr.h"

extern __IO TIM_Opration_st   TIM_Var;

/********************************************************************************************
* 函数名：Sys_Standby
* 描述  ：进入待机模式
* 输入  : 无
* 输出  ：无
* 调用  ：外部调用
********************************************************************************************/
void Sys_Standby(void)
{
  __HAL_RCC_PWR_CLK_ENABLE();
  
  HAL_PWR_DisableWakeUpPin(PWR_WAKEUP_PIN1);
  __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
  HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1);
	__HAL_RCC_LPTIM1_CLK_ENABLE();
  HAL_PWR_EnterSTANDBYMode();
}

//系统进入待机模式
void Sys_Enter_Standby(void)
{	
  __HAL_RCC_APB2_FORCE_RESET();
	Sys_Standby();
}


/********************************************************************************************
* 函数名：Sys_Stop_IO_Config
* 描述  ：进入停机模式
* 输入  : 无
* 输出  ：无
* 调用  ：外部调用
********************************************************************************************/
void Sys_Stop_IO_Config(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_RCC_LPTIM1_CLK_ENABLE();
  
	HAL_UART_DeInit(&huart1);  ///注销USART1时钟   无此项会影响唤醒
	HAL_PWREx_EnableUltraLowPower();
  HAL_PWREx_EnableFastWakeUp();
  __HAL_RCC_WAKEUPSTOP_CLK_CONFIG(RCC_STOP_WAKEUPCLOCK_HSI);
	
//  GPIO_InitStruct.Pin =  GPIO_PIN_All;
//  GPIO_InitStruct.Mode  = GPIO_MODE_ANALOG;    
//  GPIO_InitStruct.Pull = GPIO_NOPULL;
	
	HAL_GPIO_WritePin(LCD_EN_GPIO_Port, LCD_EN_Pin, GPIO_PIN_RESET);
	GPIO_InitStruct.Pin =  GPIO_PIN_7;
  GPIO_InitStruct.Mode  = GPIO_MODE_ANALOG;    
  GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
//  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
	/**LPTIM1 GPIO Configuration
	PB5     ------> LPTIM1_IN1
	*/
//	GPIO_InitStruct.Pin = GPIO_PIN_5;
//	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
//	GPIO_InitStruct.Pull = GPIO_PULLUP;
//	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
//	GPIO_InitStruct.Alternate = GPIO_AF2_LPTIM1;
//	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	
//	__HAL_RCC_GPIOA_CLK_DISABLE();
//	__HAL_RCC_GPIOB_CLK_DISABLE();
//	__HAL_RCC_ADC1_CLK_DISABLE();
//	__HAL_RCC_USART1_CLK_ENABLE();
	
}

void BSP_PB_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct;
  /* Enable the BUTTON Clock */
 
  __HAL_RCC_GPIOA_CLK_ENABLE(); 
  __HAL_RCC_GPIOB_CLK_ENABLE();
   
	GPIO_InitStruct.Pin =  GPIO_PIN_0;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
	__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_0);    //必须立刻清除，否则会进入中断

	HAL_NVIC_SetPriority(EXTI0_1_IRQn, 2, 0);
	HAL_NVIC_EnableIRQ(EXTI0_1_IRQn);

	GPIO_InitStruct.Pin =  GPIO_PIN_7;
	GPIO_InitStruct.Pull = GPIO_PULLDOWN;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	HAL_GPIO_Init( GPIOB, &GPIO_InitStruct);
	
	HAL_NVIC_SetPriority(EXTI4_15_IRQn, 2, 0);
	HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
}



/********************************************************************************************
* 函数名：Sys_Stop
* 描述  ：进入停机模式
* 输入  : 无
* 输出  ：无
* 调用  ：外部调用
********************************************************************************************/
void Sys_Stop(void)
{
//  HAL_PWR_DisableWakeUpPin(PWR_WAKEUP_PIN1);
//  HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1);
//  __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
	
	BSP_PB_Init();
	TIM_Var.add_time = 0;  //计时清0
  HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON,PWR_STOPENTRY_WFI);	
}


/********************************************************************************************
* 函数名：Exit_Sys_Stop
* 描述  ：退出停机模式
* 输入  : 无
* 输出  ：无
* 调用  ：外部调用
********************************************************************************************/
void Exit_Sys_Stop(void)
{
	static uint8_t battery_percent = 0;
//	SystemClock_Config();  /* RTC超时函数（或中断函数）中已调用 */
	MX_GPIO_Init();
//	HAL_GPIO_TogglePin(GPIOB,GPIO_PIN_0);
	
//	STOP_Data_Cal();       //唤醒后的数据处理，再根据计算值判断是否退出停止模式
	TIM_Var.add_time = 0;  //计时清0
	battery_percent = max17048_get_permille();  //获取剩余电量
  printf("剩余电量%d\r\n",battery_percent);
//	printf("退出停机模式！\r\n");
	
//	MX_ADC_Init();
}


/********************************************************************************************
* 函数名：Sys_Enter_Stop
* 描述  ：进入停机模式
* 输入  : 无
* 输出  ：无
* 调用  ：外部调用
********************************************************************************************/
void Sys_Enter_Stop(void)
{
	
	OLED_WR_REG(0xAE); //OLED进入低功耗模式。
  Sys_Stop_IO_Config();
	Sys_Stop();
}

void WKUP_Init(void)
{	
	static int starting_up_flag = 0;
	
//  GPIO_InitTypeDef GPIO_InitStruct = {0};
  
  __HAL_RCC_GPIOA_CLK_ENABLE();
  
//  GPIO_InitStruct.Pin = GPIO_PIN_0;
//  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
//  GPIO_InitStruct.Pull = GPIO_NOPULL;
//  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  
  if(starting_up_flag) 
    Sys_Standby();    //不是开机,进入待机模式
	else
		starting_up_flag = 1;
}

/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"
/* USER CODE BEGIN Includes */
#include "debug.h"
#include "usart.h"
#include "pm25.h"
#include "ens160.h"
#include "aht20.h"
#include "bmp280.h"
#include "fan.h"
#include "tim.h"
#include "protec_protocol.h"
#include "config_flash.h"
#include "protocol.h"
#include "ota.h"
#include "usart.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* 上电传感器自检串口打印：联调/生产保持 0 */
#define SENSOR_DEBUG_BOOT_CHECK  0
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Definitions for sensorTask */
osThreadId_t sensorTaskHandle;
const osThreadAttr_t sensorTask_attributes = {
  .name = "sensorTask",
  .stack_size = 384 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Definitions for fanTask */
osThreadId_t fanTaskHandle;
const osThreadAttr_t fanTask_attributes = {
  .name = "fanTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};

/* Definitions for doorTask */
osThreadId_t doorTaskHandle;
const osThreadAttr_t doorTask_attributes = {
  .name = "doorTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
#if SENSOR_DEBUG_BOOT_CHECK
static void Sensor_PrintBootCheck(void);
#endif
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartSensorTask(void *argument);
void StartFanTask(void *argument);
void StartDoorTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
  USART1_TxInit();
  USART2_TxInit();
  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of sensorTask */
  sensorTaskHandle = osThreadNew(StartSensorTask, NULL, &sensorTask_attributes);

  /* creation of fanTask */
  fanTaskHandle = osThreadNew(StartFanTask, NULL, &fanTask_attributes);

  /* creation of doorTask */
  doorTaskHandle = osThreadNew(StartDoorTask, NULL, &doorTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  for(;;)
  {
    Protocol_OnUart1Bytes();
    Protocol_OnUart2Bytes();
    Protocol_OnCanFrames();

   // printf("hello world\r\n");

//    n = USART1_Rx_GetCount();
//    if (n > 0U)
//    {
//      if (n > sizeof(rx_tmp))
//        n = sizeof(rx_tmp);
//      n = USART1_Rx_Read(rx_tmp, n);
//      printf("rx %u bytes: ", (unsigned)n);
//      for (i = 0U; i < n; i++)
//        printf("%02X ", (unsigned)rx_tmp[i]);
//      printf("\r\n");
//    }

    osDelay(20);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartSensorTask */
/**
  * @brief  Function implementing the sensorTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartSensorTask */
#if SENSOR_DEBUG_BOOT_CHECK
static void Sensor_PrintBootCheck(void)
{
  ENS160_Data_t ens;
  AHT20_Data_t aht;
  BMP280_Data_t bmp;
  PM25_Data_t pm25;

  AHT20_Update();
  BMP280_Update();

  AHT20_GetData(&aht);
  if (aht.online != 0U)
  {
    ENS160_SetCompensation(aht.temperature_c, aht.humidity_rh);
  }
  ENS160_Update();

  ENS160_GetData(&ens);
  BMP280_GetData(&bmp);
  PM25_GetData(&pm25);

  printf("=== SENSOR BOOT CHECK ===\r\n");
  printf("AHT20  online=%u  T=%.1fC  RH=%.1f%%\r\n",
         (unsigned)aht.online,
         (double)aht.temperature_c,
         (double)aht.humidity_rh);
  printf("BMP280 online=%u  P=%.0fPa\r\n",
         (unsigned)bmp.online,
         (double)bmp.pressure_pa);
  printf("ENS160 online=%u  eCO2=%u  TVOC=%u  status=0x%02X  phase=%s\r\n",
         (unsigned)ens.online,
         (unsigned)ens.eco2,
         (unsigned)ens.tvoc,
         (unsigned)ens.device_status,
         ENS160_WarmupText(ens.warmup_phase));
  printf("PM25   online=%u  pm2_5=%u (未装传感器时为0)\r\n",
         (unsigned)pm25.online,
         (unsigned)pm25.pm2_5);

  if ((aht.online == 0U) && (bmp.online == 0U) && (ens.online == 0U))
  {
    printf("WARN: I2C sensors all offline, check PB6/PB7 pull-up & solder\r\n");
  }

  printf("=========================\r\n");
}
#endif

void StartSensorTask(void *argument)
{
  /* USER CODE BEGIN StartSensorTask */
  PM25_Data_t pm25_data;
  PM25_Data_t pm25_last;
  AHT20_Data_t aht_data;
  uint32_t last_i2c_update = 0U;

  /* 上电后等待传感器/总线稳定，减少偶发 I2C 首读失败 */
  osDelay(300);

  PM25_Rx_Start();
  ENS160_Init();
  AHT20_Init();
  BMP280_Init();
  Protec_Init();

#if SENSOR_DEBUG_BOOT_CHECK
  Sensor_PrintBootCheck();
#endif

  pm25_last.last_update_tick = 0U;
  pm25_last.pm1_0 = 0U;
  pm25_last.pm2_5 = 0U;
  pm25_last.pm10 = 0U;
  pm25_last.online = 0U;

  for(;;)
  {
    OTA_Service();

    PM25_GetData(&pm25_data);

    if (pm25_data.last_update_tick != pm25_last.last_update_tick)
    {
//      printf("PM frame: pm1_0=%u pm2_5=%u pm10=%u online=%u tick=%lu\r\n",
//             (unsigned)pm25_data.pm1_0,
//             (unsigned)pm25_data.pm2_5,
//             (unsigned)pm25_data.pm10,
//             (unsigned)pm25_data.online,
//             (unsigned long)pm25_data.last_update_tick);

      pm25_last = pm25_data;
    }

    if ((HAL_GetTick() - last_i2c_update) >= 1000U)
    {
      last_i2c_update = HAL_GetTick();

      AHT20_Update();
      BMP280_Update();

      AHT20_GetData(&aht_data);
      if (aht_data.online != 0U)
      {
        ENS160_SetCompensation(aht_data.temperature_c, aht_data.humidity_rh);
      }
      ENS160_Update();

      if (OTA_IsRealtimeMuted() == 0U)
      {
        Protec_SendRealtime();
      }
    }

    osDelay(200);
  }
  /* USER CODE END StartSensorTask */
}

/* USER CODE BEGIN Header_StartFanTask */
/**
  * @brief  Function implementing the fanTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartFanTask */
void StartFanTask(void *argument)
{
  /* USER CODE BEGIN StartFanTask */
  uint32_t period;
  uint32_t pulse;
  uint32_t last_print_tick = 0U;
  uint8_t on;

  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_4);
  period = htim4.Init.Period;
  for (;;)
  {
    on = Fan_Get();                         // 0: ?, 1: ?
    pulse = (on != 0U) ? period : 0U;       // ?? or ??
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, pulse);

    if ((HAL_GetTick() - last_print_tick) >= 1000U)
    {
      last_print_tick = HAL_GetTick();
     // printf("FAN: state=%u pulse=%lu\r\n",
     //        (unsigned)on,
     //        (unsigned long)pulse);
    }

    osDelay(100);
  }
  /* USER CODE END StartFanTask */
}

/* USER CODE BEGIN Header_StartDoorTask */
/**
  * @brief  Function implementing the doorTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDoorTask */
void StartDoorTask(void *argument)
{
  /* USER CODE BEGIN StartDoorTask */
  GPIO_PinState last_state;
  GPIO_PinState cur_state;
  uint32_t stable_count = 0U;
  uint32_t last_print_tick = 0U;

  last_state = HAL_GPIO_ReadPin(DOOR_SW_GPIO_Port, DOOR_SW_Pin);

  for(;;)
  {
    cur_state = HAL_GPIO_ReadPin(DOOR_SW_GPIO_Port, DOOR_SW_Pin);

    if (cur_state == last_state)
    {
      if (stable_count < 5U)
      {
        stable_count++;
      }
    }
    else
    {
      stable_count = 0U;
      last_state = cur_state;
    }

    if ((HAL_GetTick() - last_print_tick) >= 1000U)
    {
      last_print_tick = HAL_GetTick();
     // printf("DOOR state: %s\r\n", (cur_state == GPIO_PIN_SET) ? "OPEN" : "CLOSED");
    }

    osDelay(10);
  }
  /* USER CODE END StartDoorTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */


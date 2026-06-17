/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "main.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ota_bl.h"
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

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
static uint32_t BL_Crc32(const uint8_t *data, uint32_t len);
static int      BL_ReadOtaFlag(OtaFlag_t *out);
static int      BL_ClearOtaFlag(void);
static int      BL_CopyDownloadToApp(uint32_t app_size);
static int      BL_IsAppVectorValid(void);
static void     BL_JumpToApp(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static uint32_t BL_Crc32(const uint8_t *data, uint32_t len)
{
  uint32_t crc = 0xFFFFFFFFU;
  uint32_t i;
  uint8_t  j;

  for (i = 0U; i < len; i++)
  {
    crc ^= ((uint32_t)data[i] << 24);
    for (j = 0U; j < 8U; j++)
    {
      if ((crc & 0x80000000U) != 0U)
        crc = (crc << 1) ^ 0x04C11DB7U;
      else
        crc <<= 1;
    }
  }
  return crc;
}

static int BL_ReadOtaFlag(OtaFlag_t *out)
{
  if (out == NULL)
    return -1;

  const OtaFlag_t *pf = (const OtaFlag_t *)OTA_FLAG_FLASH_ADDR;
  memcpy(out, pf, sizeof(OtaFlag_t));

  if (out->magic != OTA_FLAG_MAGIC)
    return -1;

  uint32_t calc = BL_Crc32((const uint8_t *)out, sizeof(OtaFlag_t) - sizeof(uint32_t));
  if (calc != out->flag_crc)
    return -1;

  return 0;
}

static int BL_ClearOtaFlag(void)
{
  HAL_FLASH_Unlock();

  FLASH_EraseInitTypeDef erase;
  uint32_t page_error = 0U;
  erase.TypeErase   = FLASH_TYPEERASE_PAGES;
  erase.PageAddress = OTA_FLAG_FLASH_ADDR;
  erase.NbPages     = 1U;

  if (HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK)
  {
    HAL_FLASH_Lock();
    return -1;
  }

  /* 仅擦除即可清空标记（变为 0xFF..），App 下次升级会重写 */
  HAL_FLASH_Lock();
  return 0;
}

static int BL_CopyDownloadToApp(uint32_t app_size)
{
  if ((app_size == 0U) || (app_size > DOWNLOAD_FLASH_SIZE) || (app_size > APP_FLASH_SIZE))
    return -1;

  uint32_t pages = (app_size + FLASH_PAGE_SIZE - 1U) / FLASH_PAGE_SIZE;
  uint32_t i;

  HAL_FLASH_Unlock();

  /* 擦除 App 区 */
  FLASH_EraseInitTypeDef erase;
  uint32_t page_error = 0U;
  erase.TypeErase   = FLASH_TYPEERASE_PAGES;
  erase.PageAddress = APP_FLASH_ADDR;
  erase.NbPages     = pages;
  if (HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK)
  {
    HAL_FLASH_Lock();
    return -1;
  }

  /* 以 32-bit 写入拷贝 */
  const uint8_t *src = (const uint8_t *)DOWNLOAD_FLASH_ADDR;
  uint32_t dst       = APP_FLASH_ADDR;
  uint32_t words     = (app_size + 3U) / 4U;

  for (i = 0U; i < words; i++)
  {
    uint32_t word_val;
    uint32_t off = i * 4U;
    uint32_t remain = (app_size > off) ? (app_size - off) : 0U;

    if (remain >= 4U)
    {
      memcpy(&word_val, &src[off], 4U);
    }
    else
    {
      uint8_t tmp[4];
      memset(tmp, 0xFF, sizeof(tmp));
      if (remain > 0U)
        memcpy(tmp, &src[off], remain);
      memcpy(&word_val, tmp, 4U);
    }

    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, dst + off, word_val) != HAL_OK)
    {
      HAL_FLASH_Lock();
      return -1;
    }
  }

  HAL_FLASH_Lock();
  return 0;
}

static int BL_IsAppVectorValid(void)
{
  uint32_t sp = *(__IO uint32_t *)(APP_FLASH_ADDR + 0U);
  uint32_t rh = *(__IO uint32_t *)(APP_FLASH_ADDR + 4U);

  /* MSP 必须落在 0x20000000-0x20005000（与你工程 RAM 配置一致） */
  if ((sp < 0x20000000U) || (sp > (0x20000000U + 0x00005000U)))
    return 0;

  /* Reset_Handler 必须落在 App Flash 范围内（Thumb 地址最低位为1不强制检查） */
  uint32_t rh_addr = rh & 0xFFFFFFFEU;
  if ((rh_addr < APP_FLASH_ADDR) || (rh_addr >= (APP_FLASH_ADDR + APP_FLASH_SIZE)))
    return 0;

  return 1;
}

static void BL_JumpToApp(void)
{
  uint32_t appStack = *(__IO uint32_t *)(APP_FLASH_ADDR + 0U);
  uint32_t appReset = *(__IO uint32_t *)(APP_FLASH_ADDR + 4U);

  __disable_irq();

  /* 关 SysTick，避免跳转后触发异常 */
  SysTick->CTRL = 0U;
  SysTick->LOAD = 0U;
  SysTick->VAL  = 0U;

  /* 复位外设状态（尽量干净） */
  HAL_DeInit();

  /* 设置向量表、MSP、并跳转 */
  SCB->VTOR = APP_FLASH_ADDR;
  __set_MSP(appStack);
  __enable_irq();

  void (*AppResetHandler)(void) = (void (*)(void))appReset;
  AppResetHandler();

  while (1)
  {
    /* 不应返回 */
  }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  OtaFlag_t flag;
  int has_pending = 0;

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  /* USER CODE BEGIN 2 */

  /* 1) 如 OTA 标记有效且 pending，则搬运 Download -> App */
  if (BL_ReadOtaFlag(&flag) == 0)
  {
    if (flag.status == OTA_STATUS_PENDING)
    {
      has_pending = 1;

      /* 先校验 Download 区 CRC，避免写坏 App */
      uint32_t calc = BL_Crc32((const uint8_t *)DOWNLOAD_FLASH_ADDR, flag.app_size);
      if (calc == flag.app_crc32)
      {
        if (BL_CopyDownloadToApp(flag.app_size) == 0)
        {
          /* 拷贝后可选再校验一次 App 区 CRC */
          uint32_t app_calc = BL_Crc32((const uint8_t *)APP_FLASH_ADDR, flag.app_size);
          if (app_calc == flag.app_crc32)
          {
            (void)BL_ClearOtaFlag();
          }
        }
      }
    }
  }

  /* 2) 正常跳转到 App（如果有效） */
  if (BL_IsAppVectorValid() != 0)
  {
    BL_JumpToApp();
  }

  (void)has_pending;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

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
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
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

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
#include "stmflash.h"

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

static uint32_t BL_FlashAlignUp32(uint32_t byte_len)
{
  if((byte_len % 32U) == 0U)
    return byte_len;
  return byte_len + (32U - (byte_len % 32U));
}

/* 拷贝前一次性擦除 App 区（避免分块 Reg_Flash_Write 重复擦整扇区抹掉已写数据） */
static int BL_EraseAppRegion(uint32_t app_size)
{
  uint32_t erase_bytes = BL_FlashAlignUp32(app_size);
  uint32_t addr = APP_FLASH_ADDR;
  uint32_t end = APP_FLASH_ADDR + erase_bytes;
  uint8_t res;

  if (Reg_Flash_WaitDone(0, 0xFFFFFFFFU) != 0U ||
      Reg_Flash_WaitDone(1, 0xFFFFFFFFU) != 0U)
    return -1;

  INTX_DISABLE();
  Reg_Flash_Unlock(0);

  while (addr < end)
  {
    res = Reg_Flash_EraseSector(addr);
    if (res != 0U)
    {
      Reg_Flash_Lock(0);
      INTX_ENABLE();
      return -1;
    }
    if (Reg_Flash_WaitDone(0, 0xFFFFFFFFU) != 0U)
    {
      Reg_Flash_Lock(0);
      INTX_ENABLE();
      return -1;
    }
    addr += FLASH_SECTOR_SIZE;
  }

  Reg_Flash_Lock(0);
  INTX_ENABLE();
  return 0;
}

static int BL_ReadOtaFlag(OtaFlag_t *out)
{
  if (out == NULL)
    return -1;

//  const OtaFlag_t *pf = (const OtaFlag_t *)OTA_FLAG_FLASH_ADDR;
//  memcpy(out, pf, sizeof(OtaFlag_t));
  
  
//    OtaFlag_t verify_flag = {0};
//    memcpy(&verify_flag, (void *)OTA_FLAG_FLASH_ADDR, sizeof(OtaFlag_t));
//    
//    if(verify_flag.magic != OTA_FLAG_MAGIC)
//        ;
//    if(verify_flag.status != OTA_STATUS_PENDING)
//        ;
//    uint32_t flag_crc = 0;
//    
//    /* 计算标志 CRC（不包含 flag_crc 本身） */
//    flag_crc = BL_Crc32((const uint8_t *)&verify_flag, 32);
//    (void)flag_crc;
  
  
  
  
  
  
  memcpy(out, (void *)OTA_FLAG_FLASH_ADDR, sizeof(OtaFlag_t));

  if (out->magic != OTA_FLAG_MAGIC)
    return -1;

  uint32_t calc = BL_Crc32((const uint8_t *)out, 32);
  if (calc != out->flag_crc)
    return -1;

  return 0;
}

static int BL_ClearOtaFlag(void)
{
  /* 清除整个 OTA 标志结构（64 字节 = 16 word） */
  uint32_t clear_data[16];
  uint32_t i;

  for (i = 0U; i < 16U; i++)
    clear_data[i] = 0xFFFFFFFFU;

  if (!Reg_Flash_Write(OTA_FLAG_FLASH_ADDR, clear_data, 16U))
    return -1;
  return 0;
}

static int BL_CopyDownloadToApp(uint32_t app_size)
{
  uint32_t offset = 0U;
  uint32_t flash_buf[64]; /* 256 字节 RAM；Reg_Flash_Write 内部分 8 word 做 PG */

  if ((app_size == 0U) || (app_size > DOWNLOAD_FLASH_SIZE) || (app_size > APP_FLASH_SIZE))
    return -1;

  /* 必须先整区擦除：旧 App 非 0xFF，分块写时 Reg_Flash_Write 会擦整扇区并抹掉前面已拷贝块 */
  if (BL_EraseAppRegion(app_size) != 0)
    return -1;

  while (offset < app_size)
  {
    uint32_t chunk = app_size - offset;
    uint32_t write_len;
    uint32_t pad_len;

    if (chunk > sizeof(flash_buf))
      chunk = (uint32_t)sizeof(flash_buf);

    memcpy(flash_buf, (const void *)(DOWNLOAD_FLASH_ADDR + offset), chunk);

    write_len = chunk;
    if ((write_len % 32U) != 0U)
    {
      pad_len = 32U - (write_len % 32U);
      memset((uint8_t *)flash_buf + chunk, 0xFF, pad_len);
      write_len = chunk + pad_len;
    }

    if (!Reg_Flash_Write(APP_FLASH_ADDR + offset, flash_buf, write_len / 4U))
      return -1;

    offset += chunk;
  }

  return 0;
}

static int BL_IsAppVectorValid(void)
{
  uint32_t sp = *(__IO uint32_t *)(APP_FLASH_ADDR + 0U);
  uint32_t rh = *(__IO uint32_t *)(APP_FLASH_ADDR + 4U);

  /* MSP 必须落在有效的 RAM 范围内
   * 根据链接脚本配置:
   *   - RW_IRAM1: 0x20000000-0x2001FFFF (DTCM RAM, 128KB)
   *   - RW_IRAM2: 0x24000000-0x2407FFFF (AXI SRAM, 512KB)
   */
  if (!((sp >= 0x20000000U && sp < (0x20000000U + 0x00020000U)) ||
        (sp >= 0x24000000U && sp < (0x24000000U + 0x00080000U))))
    return 0;

  /* Reset_Handler 必须落在 App Flash 范围内（Thumb 地址最低位为 1 不强制检查） */
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
//  __enable_irq();

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
    OtaFlag_t flag = {0};
  
    /* Enable I-Cache---------------------------------------------------------*/
//    SCB_EnableICache();

    /* Enable D-Cache---------------------------------------------------------*/
//    SCB_EnableDCache();
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
  
  /* Flash 读写测试 - 使用新移植的 STMFLASH 函数 */
//  {
//    uint32_t write_buf[8] = {0x11110101, 0x22220202, 0x33333333, 0x44444444, 
//                             0x5555025, 0x66666666, 0x77777777, 0x88888888};
//    uint32_t read_buf[8] = {0};
//    
//    // 测试 1: 使用 STMFLASH_Write_NoErase 直接写入 (需要先擦除)
//    // 注意：这里只是示例，实际使用时需要先擦除对应扇区
//    
//    // 测试 2: 使用普通写入（带擦除检测）
////    uint32_t data = *(__IO uint32_t *)0x080E0000;
////    data = 0;
////    printf("%#x\r\n", data);
//    STMFLASH_Write(0x081E0000, write_buf, 8);
//    
//    // 读取验证
//    STMFLASH_Read(0x081E0000, read_buf, 8);
//  }
  
  
  
  
  
  
//    uint32_t write_tt[512] = {0};
//    
//    for(uint32_t i = 0;i < 512;i++)
//        write_tt[i] = i + 1;
//    for(uint32_t i = 0;i < 0x400;i++)    // 一次似乎最多可以写入24*4 = 96字节，否则写入失败
//        Reg_Flash_Write(0x080E0000 + (i * 512), write_tt, 128);
  
  
  
  
  
  /* 1) 如 OTA 标记有效且 pending，则搬运 Download -> App */
  if (BL_ReadOtaFlag(&flag) == 0)
  {
    if (flag.status == OTA_STATUS_PENDING)
    {
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

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
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

/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32h7xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "stm32h7xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>

#include "lvgl.h"
#include "FreeRTOS.h"
#include "task.h"
#include "core_cm7.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

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
/* USER CODE BEGIN PFP */
extern void xPortSysTickHandler(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/** HardFault 等异常中禁止 printf/互斥锁，直接轮询 USART1 */
static void fault_uart_putc(char c)
{
    while ((USART1->ISR & 0x40U) == 0U) {
    }
    USART1->TDR = (uint8_t)c;
}

static void fault_uart_puts(const char *s)
{
    if (s == NULL) {
        return;
    }
    while (*s != '\0') {
        fault_uart_putc(*s++);
    }
}

static void fault_uart_put_hex32(uint32_t v)
{
    static const char hex[] = "0123456789ABCDEF";
    int shift;

    fault_uart_puts("0x");
    for (shift = 28; shift >= 0; shift -= 4) {
        fault_uart_putc(hex[(v >> shift) & 0xFU]);
    }
}

static void fault_dump_cfsr_detail(uint32_t cfsr)
{
    fault_uart_puts("  MMFSR=");
    fault_uart_put_hex32(cfsr & 0xFFU);
    if (cfsr & (1U << 0)) fault_uart_puts(" IACCVIOL");
    if (cfsr & (1U << 1)) fault_uart_puts(" DACCVIOL");
    if (cfsr & (1U << 3)) fault_uart_puts(" MUNSTKERR");
    if (cfsr & (1U << 4)) fault_uart_puts(" MSTKERR");
    if (cfsr & (1U << 5)) fault_uart_puts(" MLSPERR");
    if (cfsr & (1U << 7)) fault_uart_puts(" MMARVALID");
    fault_uart_puts("\r\n  BFSR=");
    fault_uart_put_hex32((cfsr >> 8) & 0xFFU);
    if (cfsr & (1U << 8))  fault_uart_puts(" IBUSERR");
    if (cfsr & (1U << 9))  fault_uart_puts(" PRECISERR");
    if (cfsr & (1U << 10)) fault_uart_puts(" IMPRECISERR");
    if (cfsr & (1U << 11)) fault_uart_puts(" UNSTKERR");
    if (cfsr & (1U << 12)) fault_uart_puts(" STKERR");
    if (cfsr & (1U << 13)) fault_uart_puts(" LSPERR");
    if (cfsr & (1U << 15)) fault_uart_puts(" BFARVALID");
    fault_uart_puts("\r\n  UFSR=");
    fault_uart_put_hex32((cfsr >> 16) & 0xFFFFU);
    if (cfsr & (1U << 16)) fault_uart_puts(" UNDEFINSTR");
    if (cfsr & (1U << 17)) fault_uart_puts(" INVSTATE");
    if (cfsr & (1U << 18)) fault_uart_puts(" INVPC");
    if (cfsr & (1U << 19)) fault_uart_puts(" NOCP");
    if (cfsr & (1U << 24)) fault_uart_puts(" UNALIGNED");
    if (cfsr & (1U << 25)) fault_uart_puts(" DIVBYZERO");
    fault_uart_puts("\r\n");
}

static void fault_dump_stack_frame(uint32_t *stack, uint32_t exc_lr)
{
    if (stack == NULL) {
        fault_uart_puts("Stack frame: (null)\r\n");
        return;
    }
    fault_uart_puts("Stack frame:\r\n");
    fault_uart_puts(" R0 = "); fault_uart_put_hex32(stack[0]); fault_uart_puts("\r\n");
    fault_uart_puts(" R1 = "); fault_uart_put_hex32(stack[1]); fault_uart_puts("\r\n");
    fault_uart_puts(" R2 = "); fault_uart_put_hex32(stack[2]); fault_uart_puts("\r\n");
    fault_uart_puts(" R3 = "); fault_uart_put_hex32(stack[3]); fault_uart_puts("\r\n");
    fault_uart_puts(" R12= "); fault_uart_put_hex32(stack[4]); fault_uart_puts("\r\n");
    fault_uart_puts(" LR = "); fault_uart_put_hex32(stack[5]); fault_uart_puts("\r\n");
    fault_uart_puts(" PC = "); fault_uart_put_hex32(stack[6]); fault_uart_puts("\r\n");
    fault_uart_puts(" PSR= "); fault_uart_put_hex32(stack[7]); fault_uart_puts("\r\n");
    fault_uart_puts("EXC_LR = "); fault_uart_put_hex32(exc_lr); fault_uart_puts("\r\n");
}

static void fault_dump_scs_regs(const char *tag)
{
    uint32_t cfsr = SCB->CFSR;
    uint32_t hfsr = SCB->HFSR;
    uint32_t mmfar = SCB->MMFAR;
    uint32_t bfar = SCB->BFAR;
    uint32_t shcsr = SCB->SHCSR;
    uint32_t ccr = SCB->CCR;
    uint32_t afsr = SCB->AFSR;

    fault_uart_puts("\r\n===== ");
    fault_uart_puts(tag);
    fault_uart_puts(" =====\r\n");
    fault_uart_puts("CFSR  = "); fault_uart_put_hex32(cfsr); fault_uart_puts("\r\n");
    fault_dump_cfsr_detail(cfsr);
    fault_uart_puts("HFSR  = "); fault_uart_put_hex32(hfsr); fault_uart_puts("\r\n");
    if (hfsr & (1U << 1))  fault_uart_puts("  FORCED (Mem/Bus/Usage escalated)\r\n");
    if (hfsr & (1U << 30)) fault_uart_puts("  DEBUGEVT\r\n");
    if (hfsr & (1U << 31)) fault_uart_puts("  VECTTBL\r\n");
    fault_uart_puts("SHCSR = "); fault_uart_put_hex32(shcsr); fault_uart_puts("\r\n");
    fault_uart_puts("CCR   = "); fault_uart_put_hex32(ccr); fault_uart_puts("\r\n");
    if (ccr & SCB_CCR_UNALIGN_TRP_Msk) {
        fault_uart_puts("  UNALIGN_TRP=1 (unaligned access traps)\r\n");
    }
    fault_uart_puts("AFSR  = "); fault_uart_put_hex32(afsr); fault_uart_puts("\r\n");
    fault_uart_puts("MMFAR = "); fault_uart_put_hex32(mmfar);
    if (cfsr & (1U << 7)) {
        fault_uart_puts("  (MMARVALID)\r\n");
    } else {
        fault_uart_puts("  (MMARVALID=0, ignore if not MemManage)\r\n");
    }
    fault_uart_puts("BFAR  = "); fault_uart_put_hex32(bfar);
    if (cfsr & (1U << 15)) {
        fault_uart_puts("  (BFARVALID)\r\n");
    } else {
        fault_uart_puts("  (BFARVALID=0, ignore if not BusFault)\r\n");
    }
}

void Fault_Debug_Init(void)
{
    /* 使能 MemManage/BusFault/UsageFault，优先进入对应 Handler 打印寄存器 */
    SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk
                | SCB_SHCSR_BUSFAULTENA_Msk
                | SCB_SHCSR_USGFAULTENA_Msk;
}

static void fault_handler_common(uint32_t *stack, uint32_t exc_lr, const char *tag)
{
    fault_dump_scs_regs(tag);
    fault_dump_stack_frame(stack, exc_lr);
    fault_uart_puts("=====================\r\n");
    while (1) {
    }
}

void HardFault_Handler_C(uint32_t *stack, uint32_t exc_lr)
{
    fault_handler_common(stack, exc_lr, "HardFault");
}

void MemManage_Handler_C(uint32_t *stack, uint32_t exc_lr)
{
    fault_handler_common(stack, exc_lr, "MemManage");
}

void BusFault_Handler_C(uint32_t *stack, uint32_t exc_lr)
{
    fault_handler_common(stack, exc_lr, "BusFault");
}

void UsageFault_Handler_C(uint32_t *stack, uint32_t exc_lr)
{
    fault_handler_common(stack, exc_lr, "UsageFault");
}

#if defined(__GNUC__)
__attribute__((naked, noreturn))
void HardFault_Handler(void)
{
    __asm volatile(
        "tst lr, #4          \n"
        "ite eq              \n"
        "mrseq r0, msp       \n"
        "mrsne r0, psp       \n"
        "mov r1, lr          \n"
        "b HardFault_Handler_C \n"
    );
}

__attribute__((naked, noreturn))
void MemManage_Handler(void)
{
    __asm volatile(
        "tst lr, #4          \n"
        "ite eq              \n"
        "mrseq r0, msp       \n"
        "mrsne r0, psp       \n"
        "mov r1, lr          \n"
        "b MemManage_Handler_C \n"
    );
}

__attribute__((naked, noreturn))
void BusFault_Handler(void)
{
    __asm volatile(
        "tst lr, #4          \n"
        "ite eq              \n"
        "mrseq r0, msp       \n"
        "mrsne r0, psp       \n"
        "mov r1, lr          \n"
        "b BusFault_Handler_C \n"
    );
}

__attribute__((naked, noreturn))
void UsageFault_Handler(void)
{
    __asm volatile(
        "tst lr, #4          \n"
        "ite eq              \n"
        "mrseq r0, msp       \n"
        "mrsne r0, psp       \n"
        "mov r1, lr          \n"
        "b UsageFault_Handler_C \n"
    );
}
#elif defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
__attribute__((naked, noreturn))
void HardFault_Handler(void)
{
    __asm volatile(
        "tst lr, #4          \n"
        "ite eq              \n"
        "mrseq r0, msp       \n"
        "mrsne r0, psp       \n"
        "mov r1, lr          \n"
        "b HardFault_Handler_C \n"
    );
}

__attribute__((naked, noreturn))
void MemManage_Handler(void)
{
    __asm volatile(
        "tst lr, #4          \n"
        "ite eq              \n"
        "mrseq r0, msp       \n"
        "mrsne r0, psp       \n"
        "mov r1, lr          \n"
        "b MemManage_Handler_C \n"
    );
}

__attribute__((naked, noreturn))
void BusFault_Handler(void)
{
    __asm volatile(
        "tst lr, #4          \n"
        "ite eq              \n"
        "mrseq r0, msp       \n"
        "mrsne r0, psp       \n"
        "mov r1, lr          \n"
        "b BusFault_Handler_C \n"
    );
}

__attribute__((naked, noreturn))
void UsageFault_Handler(void)
{
    __asm volatile(
        "tst lr, #4          \n"
        "ite eq              \n"
        "mrseq r0, msp       \n"
        "mrsne r0, psp       \n"
        "mov r1, lr          \n"
        "b UsageFault_Handler_C \n"
    );
}
#else
__asm void HardFault_Handler(void)
{
    IMPORT HardFault_Handler_C
    TST LR, #4
    ITE EQ
    MRSEQ R0, MSP
    MRSNE R0, PSP
    MOV R1, LR
    B HardFault_Handler_C
}

__asm void MemManage_Handler(void)
{
    IMPORT MemManage_Handler_C
    TST LR, #4
    ITE EQ
    MRSEQ R0, MSP
    MRSNE R0, PSP
    MOV R1, LR
    B MemManage_Handler_C
}

__asm void BusFault_Handler(void)
{
    IMPORT BusFault_Handler_C
    TST LR, #4
    ITE EQ
    MRSEQ R0, MSP
    MRSNE R0, PSP
    MOV R1, LR
    B BusFault_Handler_C
}

__asm void UsageFault_Handler(void)
{
    IMPORT UsageFault_Handler_C
    TST LR, #4
    ITE EQ
    MRSEQ R0, MSP
    MRSNE R0, PSP
    MOV R1, LR
    B UsageFault_Handler_C
}
#endif

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern DMA_HandleTypeDef hdma_adc2;
extern DMA_HandleTypeDef hdma_spi1_tx;
extern DMA_HandleTypeDef hdma_spi1_rx;
extern SPI_HandleTypeDef hspi1;
extern DMA_HandleTypeDef hdma_tim4_ch1;
extern TIM_HandleTypeDef htim4;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart3;
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */
    printf("NMI_Handler! \r\n");
  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

#if 0 /* HardFault_Handler 见 USER CODE BEGIN 0（含 CFSR/BFAR/MMFAR 打印） */
/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */
    printf("HardFault_Handler! \r\n");
  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}
#endif

/**
  * @brief This function handles System service call via SWI instruction.
  */
// void SVC_Handler(void)
// {
//   /* USER CODE BEGIN SVCall_IRQn 0 */
//     printf("SVC错误！\r\n");
//   /* USER CODE END SVCall_IRQn 0 */
//   /* USER CODE BEGIN SVCall_IRQn 1 */

//   /* USER CODE END SVCall_IRQn 1 */
// }

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */
    printf("DebugMon_Handler! \r\n");
  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/**
  * @brief This function handles Pendable request for system service.
  */
// void PendSV_Handler(void)
// {
//   /* USER CODE BEGIN PendSV_IRQn 0 */
//     printf("PendSV错误！\r\n");
//   /* USER CODE END PendSV_IRQn 0 */
//   /* USER CODE BEGIN PendSV_IRQn 1 */

//   /* USER CODE END PendSV_IRQn 1 */
// }

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */

  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  /* USER CODE BEGIN SysTick_IRQn 1 */
  lv_tick_inc(1);

  if(xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
  {
    xPortSysTickHandler();
  }
  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32H7xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32h7xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles DMA1 stream0 global interrupt.
  */
void DMA1_Stream0_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Stream0_IRQn 0 */

  /* USER CODE END DMA1_Stream0_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_adc2);
  /* USER CODE BEGIN DMA1_Stream0_IRQn 1 */

  /* USER CODE END DMA1_Stream0_IRQn 1 */
}

/* USART3 使用中断方式，不需要 DMA 中断处理 */

/**
  * @brief This function handles DMA1 stream3 global interrupt.
  */
void DMA1_Stream3_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Stream3_IRQn 0 */

  /* USER CODE END DMA1_Stream3_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_tim4_ch1);
  /* USER CODE BEGIN DMA1_Stream3_IRQn 1 */

  /* USER CODE END DMA1_Stream3_IRQn 1 */
}

/**
  * @brief This function handles DMA1 stream4 global interrupt.
  */
void DMA1_Stream4_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Stream4_IRQn 0 */

  /* USER CODE END DMA1_Stream4_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_spi1_tx);
  /* USER CODE BEGIN DMA1_Stream4_IRQn 1 */

  /* USER CODE END DMA1_Stream4_IRQn 1 */
}

/**
  * @brief This function handles DMA1 stream5 global interrupt.
  */
void DMA1_Stream5_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Stream5_IRQn 0 */

  /* USER CODE END DMA1_Stream5_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_spi1_rx);
  /* USER CODE BEGIN DMA1_Stream5_IRQn 1 */

  /* USER CODE END DMA1_Stream5_IRQn 1 */
}

/**
  * @brief This function handles TIM4 global interrupt.
  */
void TIM4_IRQHandler(void)
{
  /* USER CODE BEGIN TIM4_IRQn 0 */

  /* USER CODE END TIM4_IRQn 0 */
  HAL_TIM_IRQHandler(&htim4);
  /* USER CODE BEGIN TIM4_IRQn 1 */

  /* USER CODE END TIM4_IRQn 1 */
}

/**
  * @brief This function handles SPI1 global interrupt.
  */
void SPI1_IRQHandler(void)
{
  /* USER CODE BEGIN SPI1_IRQn 0 */

  /* USER CODE END SPI1_IRQn 0 */
  HAL_SPI_IRQHandler(&hspi1);
  /* USER CODE BEGIN SPI1_IRQn 1 */

  /* USER CODE END SPI1_IRQn 1 */
}

///**
//  * @brief This function handles USART1 global interrupt.
//  */
//void USART1_IRQHandler(void)
//{
//  /* USER CODE BEGIN USART1_IRQn 0 */

//  /* USER CODE END USART1_IRQn 0 */
//  HAL_UART_IRQHandler(&huart1);
//  /* USER CODE BEGIN USART1_IRQn 1 */

//  /* USER CODE END USART1_IRQn 1 */
//}

/**
  * @brief This function handles USART3 global interrupt.
  */
// void USART3_IRQHandler(void)
// {
//   /* USER CODE BEGIN USART3_IRQn 0 */

//   /* USER CODE END USART3_IRQn 0 */
//   HAL_UART_IRQHandler(&huart3);
//   /* USER CODE BEGIN USART3_IRQn 1 */

//   /* USER CODE END USART3_IRQn 1 */
// }

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

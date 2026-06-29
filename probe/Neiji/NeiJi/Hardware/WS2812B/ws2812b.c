#include "ws2812b.h"
#include "tim.h"
#include "FreeRTOS.h"
#include "task.h"

#include "stm32h7xx_hal_rcc.h"
#include "stm32h7xx_hal_dma.h"
#include "cmsis_os.h"
#include "uart_diag.h"

extern DMA_HandleTypeDef hdma_tim4_ch1;


uint32_t ws2812_code0;
uint32_t ws2812_code1;

/*Some Static Colors------------------------------*/

/*   24  PWM    = 1  LED  24 bit         CCR=0   RES */
uint32_t State_Pixel_Buf[WS2812_STATE_BUF_ROWS][24];

uint32_t rgb_color_array[COLOR_COUNT] = {
    0x00EE0011,
    0x00FF0000,
    0x00FF2211,

    0x00FF4400,
    0x00FF7700, 
    0x00FF9900,

    0x00FFCC00,
    0x00DDFF00,
    0x00BBFF00,

    0x0099FF00,
    0x0066FF00,
    0x0033FF00,

    0x0000FF11,
    0x0000FF33,
    0x0000FF55,

    0x0000DDFF,
    0x0022AAFF,
    0x005533EE,

    0x00991199,
    0x00BB0055,
    0x00DD0022
};

rgb_sta_t rgb_ctrl = {
    .bits.shutdown = 1,
    .bits.lpr = 0,
    .bits.keep_1 = 0,
    .bits.keep_2 = 0
};

uint32_t color_idx = 0;
uint32_t rgb_color[TOTAL_LED_COUNT];
uint32_t rgb_color_upload[TOTAL_LED_COUNT];


/*
  :  
  :  
*/
void ws2812b_init(void)
{
  RCC_ClkInitTypeDef clkcfg = {0};
  uint32_t fl = 0U;
  uint64_t timhz;
  uint32_t arrp1;
  uint32_t c0;
  uint32_t c1;

  HAL_RCC_GetClockConfig(&clkcfg, &fl);
  timhz = (uint64_t)HAL_RCC_GetPCLK1Freq();
  /* STM32H7 APB1    1   TIM4     = 2 PCLK1 */
  if (clkcfg.APB1CLKDivider != RCC_HCLK_DIV1)
  {
    timhz *= 2ULL;
  }
  timhz /= (uint64_t)(htim4.Init.Prescaler + 1U);

  /*   1500 ns T1H+T1L   T0H=340ns T1H=750ns -> CCR T0L=1500 T0H */
  arrp1 = (uint32_t)((timhz * 1500ULL + 500000000ULL) / 1000000000ULL);
  if (arrp1 < 4U)
  {
    arrp1 = 4U;
  }
  c0 = (uint32_t)((timhz * 340ULL + 500000000ULL) / 1000000000ULL);
  c1 = (uint32_t)((timhz * 750ULL + 500000000ULL) / 1000000000ULL);
  if (c0 + 1U >= arrp1)
  {
    c0 = arrp1 - 2U;
  }
  if (c1 + 1U >= arrp1)
  {
    c1 = arrp1 - 2U;
  }

  ws2812_code0 = c0;
  ws2812_code1 = c1;
  __HAL_TIM_SET_AUTORELOAD(&htim4, arrp1 - 1U);
  
  
}

// void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
// {
//     if (htim->Instance == TIM2 || htim->Instance == TIM15)
//     {
//         dma_busy_flag = 0; // DMA    
//     }
// }

/*
  :     
  :Pixel_Len :    ,color :   ,led_num :    ,bright :  
*/
void State_Led_Show(uint16_t Pixel_Len, RGB_Color_TypeDef Color, uint8_t led_num)
{
	uint8_t i, j;
	uint16_t row;
	uint32_t dma_len;

	if (Pixel_Len > led_num)
	{
		return;
	}
	if ((uint32_t)led_num + (uint32_t)WS2812_RESET_ROWS > WS2812_STATE_BUF_ROWS)
	{
		return;
	}

	for (i = 0; i < Pixel_Len; i++)
	{
		for (j = 0; j < 8; j++)
			State_Pixel_Buf[i][j] = (((uint8_t)(Color.G * Breath_Bright_State) & (1U << (7U - j))) != 0U) ? ws2812_code1 : ws2812_code0;
		for (j = 8; j < 16; j++)
			State_Pixel_Buf[i][j] = (((uint8_t)(Color.R * Breath_Bright_State) & (1U << (15U - j))) != 0U) ? ws2812_code1 : ws2812_code0;
		for (j = 16; j < 24; j++)
			State_Pixel_Buf[i][j] = (((uint8_t)(Color.B * Breath_Bright_State) & (1U << (23U - j))) != 0U) ? ws2812_code1 : ws2812_code0;
	}
	for (i = Pixel_Len; i < led_num; i++)
	{
		for (j = 0; j < 24; j++)
			State_Pixel_Buf[i][j] = ws2812_code0;
	}
	for (row = led_num; row < led_num + WS2812_RESET_ROWS; row++)
	{
		for (j = 0; j < 24; j++)
			State_Pixel_Buf[row][j] = 0U;
	}

    SCB_CleanDCache_by_Addr(&State_Pixel_Buf[0][0], WS2812_STATE_BUF_ROWS * 24);

	dma_len = (uint32_t)(led_num + WS2812_RESET_ROWS) * 24U;
	HAL_TIM_PWM_Start_DMA(&htim4, TIM_CHANNEL_1, (uint32_t *)State_Pixel_Buf, dma_len);
}




void ws2812_shutdown(void)
{
    memset(rgb_color_upload, 0, TOTAL_LED_COUNT * 4);
}

void rgb_led_flush(void)
{
    uint8_t i;
    RGB_Color_TypeDef led_colors[TOTAL_LED_COUNT];
    
    for (i = 0; i < TOTAL_LED_COUNT; i++)
    {
        uint32_t color = rgb_color_upload[i];
        led_colors[i].G = (color >> 8) & 0xFF;
        led_colors[i].R = (color >> 16) & 0xFF;
        led_colors[i].B = color & 0xFF;
    }
    
    for (i = 0; i < TOTAL_LED_COUNT; i++)
    {
        uint8_t j;
        for (j = 0; j < 8; j++)
            State_Pixel_Buf[i][j] = (((uint8_t)(led_colors[i].G * Breath_Bright_State) & (1U << (7U - j))) != 0U) ? ws2812_code1 : ws2812_code0;
        for (j = 8; j < 16; j++)
            State_Pixel_Buf[i][j] = (((uint8_t)(led_colors[i].R * Breath_Bright_State) & (1U << (15U - j))) != 0U) ? ws2812_code1 : ws2812_code0;
        for (j = 16; j < 24; j++)
            State_Pixel_Buf[i][j] = (((uint8_t)(led_colors[i].B * Breath_Bright_State) & (1U << (23U - j))) != 0U) ? ws2812_code1 : ws2812_code0;
    }
    
    uint16_t row;
    for (row = TOTAL_LED_COUNT; row < TOTAL_LED_COUNT + WS2812_RESET_ROWS; row++)
    {
        uint8_t j;
        for (j = 0; j < 24; j++)
            State_Pixel_Buf[row][j] = 0U;
    }
    
    SCB_CleanDCache_by_Addr(&State_Pixel_Buf[0][0], WS2812_STATE_BUF_ROWS * 24);
    HAL_TIM_PWM_Start_DMA(&htim4, TIM_CHANNEL_1, (uint32_t *)State_Pixel_Buf,
                          (uint32_t)(TOTAL_LED_COUNT + WS2812_RESET_ROWS) * 24U);
}

void mode_keep1(float light)
{
    uint8_t r, g, b;
    uint8_t r_t, g_t, b_t;
    
    for(uint8_t i = 0; i < TOTAL_LED_COUNT - 1; i++)
        rgb_color[i] = rgb_color[i + 1];
    rgb_color[TOTAL_LED_COUNT - 1] = rgb_color_array[color_idx];
    
    if(++color_idx >= COLOR_COUNT)
        color_idx = 0;
    
    memcpy(rgb_color_upload, rgb_color, TOTAL_LED_COUNT * 4);
    
    for(uint8_t i = 0;i < TOTAL_LED_COUNT;i++)
    {
        uint32_t color_tp = rgb_color[i];
        
        r_t = (color_tp >> 16) & 0xff;
        g_t = (color_tp >> 8) & 0xff;
        b_t = color_tp & 0xff;
        
        r = (float)r_t * light;
        g = (float)g_t * light;
        b = (float)b_t * light;
        rgb_color_upload[i] = ((uint32_t)r << 16) | ((uint16_t)g << 8) | b;
    }
}

void ws2812_set_mode(rgbled_mode_t mode)
{
    rgb_ctrl.rgb_sta &= ~((uint8_t)1 << mode);
    rgb_ctrl.rgb_sta |= ((uint8_t)1 << mode);
}

void ws2812_clr_mode(rgbled_mode_t mode)
{
    rgb_ctrl.rgb_sta &= ~((uint8_t)1 << mode);
}

static void ws2812_task(void *pvParameters);

void ws2812b_TaskInit(void)
{
    static osThreadId_t handle;
    static const osThreadAttr_t attr = {
        .name = "ws2812Task",
        .stack_size = 512 * 4,
        .priority = osPriorityNormal,
    };
    int i;

    printf("[APP] ws2812b_init start\r\n");
    ws2812b_init();
    printf("[APP] ws2812b_init done\r\n");

    for (i = 0; i < TOTAL_LED_COUNT; i++)
        rgb_color[i] = rgb_color_array[i];
    color_idx = TOTAL_LED_COUNT;

    ws2812_clr_mode(RGBLED_SHUTDOWN_MODE);
    ws2812_set_mode(RGBLED_KEEP_1_MODE);
    printf("[APP] ws2812 LED ON\r\n");

    handle = osThreadNew(ws2812_task, NULL, &attr);
    (void)handle;
}

static void ws2812_task(void *pvParameters)
{
    uint8_t idx_val = 0;
    TickType_t last_wake = xTaskGetTickCount();

    (void)pvParameters;

    printf("[WS2812] task start\r\n");

    vTaskDelay(pdMS_TO_TICKS(1000));

    for (;;) {
        if (rgb_ctrl.rgb_sta) {
            idx_val = rgb_ctrl.rgb_sta;
            for (uint8_t idx = 0; idx < 8; idx++) {
                if (idx_val & 0x01) {
                    switch (idx) {
                        case RGBLED_SHUTDOWN_MODE:
                            idx = 8;
                            ws2812_shutdown();
                            ws2812_clr_mode(RGBLED_SHUTDOWN_MODE);
                            break;
                        case RGBLED_LPR_MODE:
                            ws2812_shutdown();
                            ws2812_clr_mode(RGBLED_LPR_MODE);
                            break;
                        case RGBLED_KEEP_1_MODE:
                            mode_keep1(0.05f);
                            break;
                        case RGBLED_KEEP_2_MODE:
                            rgb_ctrl.bits.keep_2 = 0;
                            break;
                        default:
                            break;
                    }
                }
                idx_val >>= 1;
            }
            rgb_led_flush();
        }
        vTaskDelayUntil(&last_wake, COMMON_DELAY);
    }
}

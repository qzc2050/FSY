#ifndef __LCD_RGB_H
#define	__LCD_RGB_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>


#define WHITE                            0xFFFF
#define BLACK                            0x0000
#define BLUE                             0x001F
#define BRED                             0xF81F
#define GRED                             0xFFE0
#define GBLUE                            0x07FF
#define RED                              0xF800
#define MAGENTA                          0xF81F
#define GREEN                            0x07E0
#define CYAN                             0x7FFF
#define YELLOW                           0xFFE0
#define BROWN                            0xBC40
#define BRRED                            0xFC07
#define GRAY                             0x8430


/*-------------------------------------------------------- LCD 参数 -------------------------------------------------------*/
#define HBP  40
#define VBP  40
#define HSW  1
#define VSW  1
#define HFP  200
#define VFP  22

/* UI / LVGL 横屏逻辑分辨率 */
#define LCD_WIDTH                       854U
#define LCD_HEIGHT                      480U

/* LVGL 双缓冲行数：与屏高相同，整屏一次 flush */
#define LCD_LVGL_BUF_LINES              LCD_HEIGHT

/* LTDC / 面板物理显存：竖屏 480x854 */
#define LCD_PHYS_WIDTH                  480U
#define LCD_PHYS_HEIGHT                 854U

#define LCD_Width                       LCD_PHYS_WIDTH
#define LCD_Height                      LCD_PHYS_HEIGHT

#define LTDC_ACCUMULATED_HBP            80U
#define LTDC_ACCUMULATED_ACTIVE_W       (LTDC_ACCUMULATED_HBP + LCD_PHYS_WIDTH)
#define LTDC_TOTAL_WIDTH                (LTDC_ACCUMULATED_ACTIVE_W + HFP)
#define LTDC_ACCUMULATED_VBP            40U
#define LTDC_ACCUMULATED_ACTIVE_H       (LTDC_ACCUMULATED_VBP + LCD_PHYS_HEIGHT)
#define LTDC_TOTAL_HEIGHT               (LTDC_ACCUMULATED_ACTIVE_H + VFP)

/* GC9503 MADCTL(0xB1): bit0 SS, bit1 GS, bit5 BGR */
#define GC9503_CMD_MADCTL               0xB1U
#define GC9503_MADCTL_VALUE             0x10U

	
/*-------------------------------------------------------- LCD背光引脚 -------------------------------------------------------*/
#define HAL_RCC_GPIO_EM_SCK_CLK_ENABLE() 	__HAL_RCC_GPIOI_CLK_ENABLE()
#define EM_SCK_GPIO_PIN						GPIO_PIN_1
#define EM_SCK_GPIO_PORT					GPIOI

#define HAL_RCC_GPIO_EM_SDA_CLK_ENABLE() 	__HAL_RCC_GPIOC_CLK_ENABLE()
#define EM_SDA_GPIO_PIN						GPIO_PIN_1
#define EM_SDA_GPIO_PORT					GPIOC

#define HAL_RCC_GPIO_EM_CS_CLK_ENABLE() 	__HAL_RCC_GPIOH_CLK_ENABLE()
#define EM_CS_GPIO_PIN						GPIO_PIN_5
#define EM_CS_GPIO_PORT						GPIOH

#define HAL_RCC_GPIO_EM_RST_CLK_ENABLE() 	__HAL_RCC_GPIOH_CLK_ENABLE()
#define EM_RST_GPIO_PIN						GPIO_PIN_4
#define EM_RST_GPIO_PORT					GPIOH
	
#define mSPI_SCK_LOW()						(EM_SCK_GPIO_PORT->BSRR=(uint32_t)EM_SCK_GPIO_PIN<<16U)
#define mSPI_SCK_HI()						(EM_SCK_GPIO_PORT->BSRR=(uint32_t)EM_SCK_GPIO_PIN)

#define mSPI_SDA_LOW()						(EM_SDA_GPIO_PORT->BSRR=(uint32_t)EM_SDA_GPIO_PIN<<16U)
#define mSPI_SDA_HI()						(EM_SDA_GPIO_PORT->BSRR=(uint32_t)EM_SDA_GPIO_PIN)

#define mSPI_CS_LOW()						(EM_CS_GPIO_PORT->BSRR=(uint32_t)EM_CS_GPIO_PIN<<16U)
#define mSPI_CS_HI()						(EM_CS_GPIO_PORT->BSRR=(uint32_t)EM_CS_GPIO_PIN)

#define mSPI_RST_LOW()						(EM_RST_GPIO_PORT->BSRR=(uint32_t)EM_RST_GPIO_PIN<<16U)
#define mSPI_RST_HI()						(EM_RST_GPIO_PORT->BSRR=(uint32_t)EM_RST_GPIO_PIN)


#define LCD_WRITE_COMMAND					0
#define LCD_WRITE_DATA						1
	



/*---------------------------------------------------------- 函数声明 -------------------------------------------------------*/
void LCD_GC9503V_init(void);
void LCD_Clear(uint32_t color);
void LCD_DrawPoint(uint16_t x,uint16_t y,uint16_t color);
void LCD_BlitPhysArea(uint16_t px, uint16_t py, uint16_t width, uint16_t height, const uint16_t *src);
void LCD_Dma2dInit(void);
void LCD_FlushWait(void);
void LCD_SetFlushDoneCallback(void (*cb)(void));
void LCD_BlitPhysAreaAsync(uint16_t px, uint16_t py, uint16_t width, uint16_t height, const uint16_t *src);
bool LCD_BlitAreaAsync(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *src);
void LCD_BlitArea(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *src);
void ShowPic_XY(uint16_t x, uint16_t y, uint16_t width, uint16_t height,uint32_t Image);

#endif //__LCD_RGB_H

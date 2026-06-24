#ifndef __LCD_RGB_H
#define __LCD_RGB_H

#include <stdint.h>
#include <stdbool.h>

#define WHITE                            0xFFFFU
#define BLACK                            0x0000U
#define BLUE                             0x001FU
#define RED                              0xF800U
#define GREEN                            0x07E0U
#define LCD_UI_BG                        0x2104U   /* RGB565 #202020，与 LVGL 背景一致 */

#define LCD_PHYS_WIDTH                   480U
#define LCD_PHYS_HEIGHT                  854U

#define LCD_WIDTH                        854U
#define LCD_HEIGHT                       480U
#define LCD_SCRATCH_BUF_LINES            LCD_HEIGHT
#define LCD_LVGL_BUF_LINES               LCD_HEIGHT

#define GC9503_CMD_MADCTL                0xB1U
#define GC9503_MADCTL_VALUE              0x10U

#define HAL_RCC_GPIO_EM_SCK_CLK_ENABLE() __HAL_RCC_GPIOI_CLK_ENABLE()
#define EM_SCK_GPIO_PIN                  GPIO_PIN_1
#define EM_SCK_GPIO_PORT                 GPIOI

#define HAL_RCC_GPIO_EM_SDA_CLK_ENABLE() __HAL_RCC_GPIOC_CLK_ENABLE()
#define EM_SDA_GPIO_PIN                  GPIO_PIN_1
#define EM_SDA_GPIO_PORT                 GPIOC

#define HAL_RCC_GPIO_EM_CS_CLK_ENABLE()  __HAL_RCC_GPIOH_CLK_ENABLE()
#define EM_CS_GPIO_PIN                   GPIO_PIN_5
#define EM_CS_GPIO_PORT                  GPIOH

#define HAL_RCC_GPIO_EM_RST_CLK_ENABLE() __HAL_RCC_GPIOH_CLK_ENABLE()
#define EM_RST_GPIO_PIN                  GPIO_PIN_4
#define EM_RST_GPIO_PORT                 GPIOH

#define mSPI_SCK_LOW()                   (EM_SCK_GPIO_PORT->BSRR = (uint32_t)EM_SCK_GPIO_PIN << 16U)
#define mSPI_SCK_HI()                    (EM_SCK_GPIO_PORT->BSRR = (uint32_t)EM_SCK_GPIO_PIN)
#define mSPI_SDA_LOW()                   (EM_SDA_GPIO_PORT->BSRR = (uint32_t)EM_SDA_GPIO_PIN << 16U)
#define mSPI_SDA_HI()                    (EM_SDA_GPIO_PORT->BSRR = (uint32_t)EM_SDA_GPIO_PIN)
#define mSPI_CS_LOW()                    (EM_CS_GPIO_PORT->BSRR = (uint32_t)EM_CS_GPIO_PIN << 16U)
#define mSPI_CS_HI()                     (EM_CS_GPIO_PORT->BSRR = (uint32_t)EM_CS_GPIO_PIN)
#define mSPI_RST_LOW()                   (EM_RST_GPIO_PORT->BSRR = (uint32_t)EM_RST_GPIO_PIN << 16U)
#define mSPI_RST_HI()                    (EM_RST_GPIO_PORT->BSRR = (uint32_t)EM_RST_GPIO_PIN)

#define LCD_WRITE_COMMAND                0U
#define LCD_WRITE_DATA                   1U

void LCD_Dma2dInit(void);
void LCD_GC9503V_init(void);
void LCD_DrawBufClean(const void *addr, uint32_t pixel_count);
void LCD_Clear(uint32_t color);
void LCD_FlushWait(void);
void LCD_SetFlushDoneCallback(void (*cb)(void));
bool LCD_BlitAreaAsync(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *src);
void LCD_FillPhysRect(uint16_t px, uint16_t py, uint16_t width, uint16_t height, uint16_t color);
void LCD_LtdcReloadFramebuf(void);
void LCD_LtdcLogState(const char *tag);
uint32_t LCD_LtdcLayer0CFBAR(void);
uint32_t LCD_FramebufBaseAddr(void);
void LCD_InvalidateFramebuf(void);
uint16_t LCD_ReadFramebufPixel(uint16_t px, uint16_t py);

#endif /* __LCD_RGB_H */

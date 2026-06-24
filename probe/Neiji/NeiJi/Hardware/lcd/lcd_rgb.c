#include "lcd_rgb.h"

#include "main.h"
#include "ltdc.h"
#include "uart_diag.h"
#include "stm32h7xx_hal_dma2d.h"
#include "core_cm7.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

extern __align(4) DEV_MALLOC_EXSRAM uint16_t ltdc_lcd_framebuf[LCD_PHYS_WIDTH * LCD_PHYS_HEIGHT];

static __align(4) DEV_MALLOC_EXSRAM uint16_t lcd_rotate_buf[LCD_WIDTH * LCD_SCRATCH_BUF_LINES];

static volatile bool lcd_dma2d_busy = false;
static uint32_t lcd_dma2d_clean_addr;
static uint32_t lcd_dma2d_clean_size;
static void (*lcd_flush_done_cb)(void);

static void lcd_dma2d_fill(uint32_t dst, uint16_t width, uint16_t height,
                           uint16_t dst_line_offset, uint32_t color);

static uint32_t lcd_phys_addr(uint16_t px, uint16_t py)
{
	return (uint32_t)&ltdc_lcd_framebuf + 2U * ((uint32_t)py * LCD_PHYS_WIDTH + px);
}

static void lcd_logical_to_phys(uint16_t lx, uint16_t ly, uint16_t *px, uint16_t *py)
{
	/* 横屏 UI -> 竖屏显存：90° 旋转 + 消除左右镜像 */
	*px = (uint16_t)(LCD_PHYS_WIDTH - 1U - ly);
	*py = lx;
}

static void lcd_dma2d_wait(void)
{
	uint32_t timeout = 0U;

	while(DMA2D->CR & DMA2D_CR_START) {
		if(++timeout > 1000000U) {
			DMA2D->CR &= ~DMA2D_CR_START;
			break;
		}
	}
}

void LCD_FlushWait(void)
{
	lcd_dma2d_wait();

	while(lcd_dma2d_busy) {
		taskYIELD();
	}
}

void LCD_SetFlushDoneCallback(void (*cb)(void))
{
	lcd_flush_done_cb = cb;
}

void LCD_Dma2dInit(void)
{
	__HAL_RCC_DMA2D_CLK_ENABLE();
	HAL_NVIC_SetPriority(DMA2D_IRQn, 6U, 0U);
	HAL_NVIC_EnableIRQ(DMA2D_IRQn);
}

static void lcd_dcache_clean(const void *addr, uint32_t size)
{
	if((SCB->CCR & SCB_CCR_DC_Msk) == 0U) {
		return;
	}

	uint32_t start = (uint32_t)addr & ~31U;
	uint32_t end = (uint32_t)addr + size;
	uint32_t len = ((end - start + 31U) & ~31U);

	SCB_CleanDCache_by_Addr((uint32_t *)start, len);
}

void LCD_DrawBufClean(const void *addr, uint32_t pixel_count)
{
	if((addr == NULL) || (pixel_count == 0U)) {
		return;
	}

	lcd_dcache_clean(addr, pixel_count * 2U);
}

static void lcd_dcache_invalidate(const void *addr, uint32_t size)
{
	if((SCB->CCR & SCB_CCR_DC_Msk) == 0U) {
		return;
	}

	uint32_t start = (uint32_t)addr & ~31U;
	uint32_t end = (uint32_t)addr + size;
	uint32_t len = ((end - start + 31U) & ~31U);

	SCB_InvalidateDCache_by_Addr((uint32_t *)start, len);
}

uint32_t LCD_FramebufBaseAddr(void)
{
	return (uint32_t)(uintptr_t)&ltdc_lcd_framebuf;
}

void LCD_FillPhysRect(uint16_t px, uint16_t py, uint16_t width, uint16_t height, uint16_t color)
{
	uint32_t addr;
	uint32_t size;

	if((width == 0U) || (height == 0U)) {
		return;
	}

	if(((uint32_t)px + width) > LCD_PHYS_WIDTH || ((uint32_t)py + height) > LCD_PHYS_HEIGHT) {
		return;
	}

	addr = lcd_phys_addr(px, py);
	size = (uint32_t)height * ((uint32_t)LCD_PHYS_WIDTH * 2U);
	lcd_dma2d_fill(addr, width, height, (uint16_t)(LCD_PHYS_WIDTH - width), color);
	lcd_dcache_clean((const void *)addr, size);
}

uint32_t LCD_LtdcLayer0CFBAR(void)
{
	return LTDC_Layer1->CFBAR & LTDC_LxCFBAR_CFBADD_Msk;
}

void LCD_LtdcReloadFramebuf(void)
{
	(void)HAL_LTDC_SetAddress(&hltdc, LCD_FramebufBaseAddr(), 0U);
	(void)HAL_LTDC_Reload(&hltdc, LTDC_SRCR_VBR);
}

void LCD_LtdcLogState(const char *tag)
{
	char msg[160];
	uint32_t cfbar = LTDC_Layer1->CFBAR & LTDC_LxCFBAR_CFBADD_Msk;
	uint32_t cfblr = LTDC_Layer1->CFBLR;
	uint32_t cfblnbr = LTDC_Layer1->CFBLNR & LTDC_LxCFBLNR_CFBLNBR_Msk;
	uint32_t whpcr = LTDC_Layer1->WHPCR;
	uint32_t wvpcr = LTDC_Layer1->WVPCR;
	uint32_t isr = LTDC->ISR;

	(void)snprintf(msg, sizeof(msg),
	               "[ltdc] %s CFBAR=0x%08lX CFBLR=0x%08lX CFBLNBR=%lu WHPCR=0x%08lX WVPCR=0x%08lX ISR=0x%lX%s\r\n",
	               tag,
	               (unsigned long)cfbar,
	               (unsigned long)cfblr,
	               (unsigned long)cfblnbr,
	               (unsigned long)whpcr,
	               (unsigned long)wvpcr,
	               (unsigned long)isr,
	               (isr & LTDC_ISR_FUIF) ? " FUIF" : "");
	UartDiag_Write(msg);
	if ((isr & LTDC_ISR_FUIF) != 0U) {
		LTDC->ICR = LTDC_ICR_CFUIF;
	}
}

void LCD_InvalidateFramebuf(void)
{
	lcd_dcache_invalidate((const void *)LCD_FramebufBaseAddr(),
	                      (uint32_t)LCD_PHYS_WIDTH * LCD_PHYS_HEIGHT * 2U);
}

uint16_t LCD_ReadFramebufPixel(uint16_t px, uint16_t py)
{
	if(px >= LCD_PHYS_WIDTH || py >= LCD_PHYS_HEIGHT) {
		return 0U;
	}

	return ltdc_lcd_framebuf[(uint32_t)py * LCD_PHYS_WIDTH + px];
}

static void lcd_dma2d_start_async(uint32_t src, uint32_t dst, uint16_t width, uint16_t height,
                                  uint16_t src_line_offset, uint16_t dst_line_offset,
                                  uint32_t dst_cache_addr, uint32_t dst_cache_size)
{
	LCD_FlushWait();

	DMA2D->IFCR = DMA2D_IFCR_CTCIF;
	DMA2D->CR &= ~DMA2D_CR_START;
	DMA2D->CR = DMA2D_M2M | DMA2D_CR_TCIE;
	DMA2D->FGMAR = src;
	DMA2D->FGOR = src_line_offset;
	DMA2D->FGPFCCR = DMA2D_INPUT_RGB565;
	DMA2D->OMAR = dst;
	DMA2D->OOR = dst_line_offset;
	DMA2D->OPFCCR = DMA2D_OUTPUT_RGB565;
	DMA2D->NLR = ((uint32_t)width << 16) | height;
	lcd_dma2d_clean_addr = dst_cache_addr;
	lcd_dma2d_clean_size = dst_cache_size;
	lcd_dma2d_busy = true;
	DMA2D->CR |= DMA2D_CR_START;
}

void DMA2D_IRQHandler(void)
{
	if((DMA2D->ISR & DMA2D_ISR_TCIF) != 0U) {
		DMA2D->IFCR = DMA2D_IFCR_CTCIF;
		DMA2D->CR &= ~(DMA2D_CR_START | DMA2D_CR_TCIE);
		lcd_dcache_clean((const void *)lcd_dma2d_clean_addr, lcd_dma2d_clean_size);
		lcd_dma2d_busy = false;

		if(lcd_flush_done_cb != NULL) {
			lcd_flush_done_cb();
		}
	}
}

static void lcd_dma2d_blit_m2m(uint32_t src, uint32_t dst, uint16_t width, uint16_t height,
                               uint16_t src_line_offset, uint16_t dst_line_offset)
{
	LCD_FlushWait();

	DMA2D->IFCR = DMA2D_IFCR_CTCIF;
	DMA2D->CR &= ~DMA2D_CR_START;
	DMA2D->CR = DMA2D_M2M;
	DMA2D->FGMAR = src;
	DMA2D->FGOR = src_line_offset;
	DMA2D->FGPFCCR = DMA2D_INPUT_RGB565;
	DMA2D->OMAR = dst;
	DMA2D->OOR = dst_line_offset;
	DMA2D->OPFCCR = DMA2D_OUTPUT_RGB565;
	DMA2D->NLR = ((uint32_t)width << 16) | height;
	DMA2D->CR |= DMA2D_CR_START;
	lcd_dma2d_wait();
	lcd_dcache_clean((const void *)dst,
	                 (uint32_t)height * (((uint32_t)width + dst_line_offset) * 2U));
}

/* 逻辑 w×h -> 物理布局（按行读 src 顺序访问，利于 Cache） */
#if defined(__ARMCC_VERSION)
#pragma push
#pragma Otime
#endif
static void lcd_transpose_to_phys(const uint16_t *src, uint16_t *dst, uint16_t w, uint16_t h)
{
	uint16_t row;
	uint16_t col;
	const uint32_t hm1 = (uint32_t)h - 1U;

	if(h == 1U) {
		memcpy(dst, src, (uint32_t)w * 2U);
		return;
	}

	for(row = 0U; row < h; row++) {
		const uint16_t *srow = src + (uint32_t)row * w;
		uint16_t *dcol = dst + hm1 - row;

		for(col = 0U; col < w; col++) {
			dcol[(uint32_t)col * h] = srow[col];
		}
	}
}
#if defined(__ARMCC_VERSION)
#pragma pop
#endif

static void lcd_dma2d_fill(uint32_t dst, uint16_t width, uint16_t height,
                             uint16_t dst_line_offset, uint32_t color)
{
	DMA2D->IFCR = DMA2D_IFCR_CTCIF;
	DMA2D->CR &= ~DMA2D_CR_START;
	DMA2D->CR = DMA2D_R2M;
	DMA2D->OPFCCR = DMA2D_OUTPUT_RGB565;
	DMA2D->OOR = dst_line_offset;
	DMA2D->OMAR = dst;
	DMA2D->NLR = ((uint32_t)width << 16) | height;
	DMA2D->OCOLR = color;
	DMA2D->CR |= DMA2D_CR_START;
	lcd_dma2d_wait();
}

void delay_us(uint32_t us)
{
	uint32_t u=us*10000U;
	while(u--);
}

void EMSPI9BitWrite(uint8_t DCsel,uint8_t data)
{
	mSPI_CS_LOW();
	delay_us(1);
	mSPI_SCK_LOW();
	delay_us(1);
	
	if(DCsel)
		mSPI_SDA_HI();
	else
		mSPI_SDA_LOW();

	delay_us(1);
	mSPI_SCK_HI();

	for(uint8_t i=0;i<8U;i++){
		delay_us(1);
		mSPI_SCK_LOW();
		if(data&0x80)
			{mSPI_SDA_HI();}
		else
			{mSPI_SDA_LOW();}

		delay_us(1);
		mSPI_SCK_HI();

		data<<=1;
	}

	mSPI_SCK_LOW();
	mSPI_CS_HI();
	delay_us(1);
}

static void LCD_GC9503V__InitGPIOMomentum(void)
{
	GPIO_InitTypeDef GPIO_InitStruct={0};

	HAL_RCC_GPIO_EM_RST_CLK_ENABLE();
	HAL_RCC_GPIO_EM_SCK_CLK_ENABLE();
	HAL_RCC_GPIO_EM_SDA_CLK_ENABLE();
	HAL_RCC_GPIO_EM_CS_CLK_ENABLE();

    __HAL_RCC_DMA2D_CLK_ENABLE();
    
	GPIO_InitStruct.Mode=GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Speed=GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Pull=GPIO_PULLUP;

	GPIO_InitStruct.Pin=EM_RST_GPIO_PIN;
	HAL_GPIO_Init(EM_RST_GPIO_PORT,&GPIO_InitStruct);

	GPIO_InitStruct.Pin=EM_SCK_GPIO_PIN;
	HAL_GPIO_Init(EM_SCK_GPIO_PORT,&GPIO_InitStruct);

	GPIO_InitStruct.Pin=EM_SDA_GPIO_PIN;
	HAL_GPIO_Init(EM_SDA_GPIO_PORT,&GPIO_InitStruct);

	GPIO_InitStruct.Pin=EM_CS_GPIO_PIN;
	HAL_GPIO_Init(EM_CS_GPIO_PORT,&GPIO_InitStruct);

	mSPI_SCK_LOW();
	mSPI_SDA_LOW();
	mSPI_CS_HI();

	mSPI_RST_LOW();
	HAL_Delay(50U);
	mSPI_RST_HI();
	HAL_Delay(150U);
}

void SPI_WriteComm(uint8_t data)
{
	EMSPI9BitWrite(LCD_WRITE_COMMAND,data);
}

void SPI_WriteData(uint8_t data)
{
	EMSPI9BitWrite(LCD_WRITE_DATA,data);
}

static void LCD_GC9503V_SetLandscapeScan(void)
{
	SPI_WriteComm(GC9503_CMD_MADCTL);
	SPI_WriteData(GC9503_MADCTL_VALUE);
}

void LCD_GC9503V_init(void)
{
    LCD_GC9503V__InitGPIOMomentum();
    HAL_Delay(10U);

    SPI_WriteComm(0xF0);SPI_WriteData(0x55);SPI_WriteData(0xAA);SPI_WriteData(0x52);SPI_WriteData(0x08);SPI_WriteData(0x00);
    SPI_WriteComm(0xF6);SPI_WriteData(0x5A);SPI_WriteData(0x87);
    SPI_WriteComm(0xC1);SPI_WriteData(0x3F);
    SPI_WriteComm(0xC2);SPI_WriteData(0x0E);
    SPI_WriteComm(0xC6);SPI_WriteData(0xF8);
    SPI_WriteComm(0xCD);SPI_WriteData(0x25);
    SPI_WriteComm(0xC9);SPI_WriteData(0x10);
    SPI_WriteComm(0xAC);SPI_WriteData(0x45);
    SPI_WriteComm(0xF8);SPI_WriteData(0x8A);
    SPI_WriteComm(0x86);SPI_WriteData(0x99);SPI_WriteData(0xA3);SPI_WriteData(0xA3);SPI_WriteData(0x51);
    SPI_WriteComm(0xFA);SPI_WriteData(0x08);SPI_WriteData(0x08);SPI_WriteData(0x00);SPI_WriteData(0x04);
    SPI_WriteComm(0x71);SPI_WriteData(0x48);
    SPI_WriteComm(0x72);SPI_WriteData(0x48);
    SPI_WriteComm(0x73);SPI_WriteData(0x00);SPI_WriteData(0x44);
    SPI_WriteComm(0xA3);SPI_WriteData(0x22);
    SPI_WriteComm(0xFD);SPI_WriteData(0x28);SPI_WriteData(0x3C);SPI_WriteData(0x00);
    SPI_WriteComm(0x97);SPI_WriteData(0xEE);
    SPI_WriteComm(0x83);SPI_WriteData(0x93);
    SPI_WriteComm(0xA7);SPI_WriteData(0x47);
    SPI_WriteComm(0xA0);SPI_WriteData(0xDD);
    SPI_WriteComm(0x9A);SPI_WriteData(0x6f);
    SPI_WriteComm(0x9B);SPI_WriteData(0x3f);
    SPI_WriteComm(0x82);SPI_WriteData(0x4A);SPI_WriteData(0x4A);
    SPI_WriteComm(0xB1);SPI_WriteData(0x04);
    SPI_WriteComm(0x7A);SPI_WriteData(0x13);SPI_WriteData(0x15);
    SPI_WriteComm(0x7B);SPI_WriteData(0x13);SPI_WriteData(0x15);
    SPI_WriteComm(0x6D);SPI_WriteData(0x1E);SPI_WriteData(0x1E);SPI_WriteData(0x1E);SPI_WriteData(0x03);SPI_WriteData(0x01);SPI_WriteData(0x09);SPI_WriteData(0x0A);SPI_WriteData(0x0C);SPI_WriteData(0x0E);SPI_WriteData(0x05);SPI_WriteData(0x07);SPI_WriteData(0x1E);SPI_WriteData(0x1E);SPI_WriteData(0x1E);SPI_WriteData(0x1E);SPI_WriteData(0x1E);SPI_WriteData(0x1E);SPI_WriteData(0x1E);SPI_WriteData(0x1E);SPI_WriteData(0x1E);SPI_WriteData(0x1E);SPI_WriteData(0x08);SPI_WriteData(0x06);SPI_WriteData(0x0F);SPI_WriteData(0x0D);SPI_WriteData(0x0B);SPI_WriteData(0x10);SPI_WriteData(0x02);SPI_WriteData(0x04);SPI_WriteData(0x1E);SPI_WriteData(0x1E);SPI_WriteData(0x1E);
    SPI_WriteComm(0x64);SPI_WriteData(0x38);SPI_WriteData(0x07);SPI_WriteData(0x03);SPI_WriteData(0x59);SPI_WriteData(0x03);SPI_WriteData(0x03);SPI_WriteData(0x38);SPI_WriteData(0x05);SPI_WriteData(0x03);SPI_WriteData(0x5B);SPI_WriteData(0x03);SPI_WriteData(0x03);SPI_WriteData(0x7D);SPI_WriteData(0x7D);SPI_WriteData(0x7D);SPI_WriteData(0x7D);
    SPI_WriteComm(0x65);SPI_WriteData(0x38);SPI_WriteData(0x04);SPI_WriteData(0x03);SPI_WriteData(0x5C);SPI_WriteData(0x03);SPI_WriteData(0x03);SPI_WriteData(0x38);SPI_WriteData(0x03);SPI_WriteData(0x03);SPI_WriteData(0x5D);SPI_WriteData(0x03);SPI_WriteData(0x03);SPI_WriteData(0x7D);SPI_WriteData(0x7D);SPI_WriteData(0x7D);SPI_WriteData(0x7D);
    SPI_WriteComm(0x66);SPI_WriteData(0x38);SPI_WriteData(0x02);SPI_WriteData(0x03);SPI_WriteData(0x5E);SPI_WriteData(0x03);SPI_WriteData(0x03);SPI_WriteData(0x38);SPI_WriteData(0x01);SPI_WriteData(0x03);SPI_WriteData(0x5F);SPI_WriteData(0x03);SPI_WriteData(0x03);SPI_WriteData(0x7D);SPI_WriteData(0x7D);SPI_WriteData(0x7D);SPI_WriteData(0x7D);
    SPI_WriteComm(0x67);SPI_WriteData(0x38);SPI_WriteData(0x00);SPI_WriteData(0x03);SPI_WriteData(0x60);SPI_WriteData(0x03);SPI_WriteData(0x03);SPI_WriteData(0x38);SPI_WriteData(0x06);SPI_WriteData(0x03);SPI_WriteData(0x5A);SPI_WriteData(0x03);SPI_WriteData(0x03);SPI_WriteData(0x7D);SPI_WriteData(0x7D);SPI_WriteData(0x7D);SPI_WriteData(0x7D);
    SPI_WriteComm(0x60);SPI_WriteData(0x38);SPI_WriteData(0x09);SPI_WriteData(0x7D);SPI_WriteData(0x7D);SPI_WriteData(0x38);SPI_WriteData(0x08);SPI_WriteData(0x7D);SPI_WriteData(0x7D);
    SPI_WriteComm(0x61);SPI_WriteData(0x38);SPI_WriteData(0x07);SPI_WriteData(0x7D);SPI_WriteData(0x7D);SPI_WriteData(0x38);SPI_WriteData(0x06);SPI_WriteData(0x7D);SPI_WriteData(0x7D);
    SPI_WriteComm(0x62);SPI_WriteData(0x33);SPI_WriteData(0x51);SPI_WriteData(0x7D);SPI_WriteData(0x7D);SPI_WriteData(0x33);SPI_WriteData(0x52);SPI_WriteData(0x7D);SPI_WriteData(0x7D);
    SPI_WriteComm(0x63);SPI_WriteData(0x33);SPI_WriteData(0x53);SPI_WriteData(0x7D);SPI_WriteData(0x7D);SPI_WriteData(0x33);SPI_WriteData(0x54);SPI_WriteData(0x7D);SPI_WriteData(0x7D);
    SPI_WriteComm(0x69);SPI_WriteData(0x14);SPI_WriteData(0x22);SPI_WriteData(0x14);SPI_WriteData(0x22);SPI_WriteData(0x44);SPI_WriteData(0x22);SPI_WriteData(0x08);
    SPI_WriteComm(0x6B);SPI_WriteData(0x07);
    SPI_WriteComm(0xD1);SPI_WriteData(0x00);SPI_WriteData(0x00);SPI_WriteData(0x00);SPI_WriteData(0x11);SPI_WriteData(0x00);SPI_WriteData(0x3C);SPI_WriteData(0x00);SPI_WriteData(0x4F);SPI_WriteData(0x00);SPI_WriteData(0x6D);SPI_WriteData(0x00);SPI_WriteData(0x9E);SPI_WriteData(0x00);SPI_WriteData(0xC0);SPI_WriteData(0x01);SPI_WriteData(0x03);SPI_WriteData(0x01);SPI_WriteData(0x35);SPI_WriteData(0x01);SPI_WriteData(0x7A);SPI_WriteData(0x01);SPI_WriteData(0xAA);SPI_WriteData(0x01);SPI_WriteData(0xF5);SPI_WriteData(0x02);SPI_WriteData(0x32);SPI_WriteData(0x02);SPI_WriteData(0x34);SPI_WriteData(0x02);SPI_WriteData(0x6E);SPI_WriteData(0x02);SPI_WriteData(0xA7);SPI_WriteData(0x02);SPI_WriteData(0xD1);SPI_WriteData(0x03);SPI_WriteData(0x00);SPI_WriteData(0x03);SPI_WriteData(0x33);SPI_WriteData(0x03);SPI_WriteData(0x3F);SPI_WriteData(0x03);SPI_WriteData(0x4A);SPI_WriteData(0x03);SPI_WriteData(0x4F);SPI_WriteData(0x03);SPI_WriteData(0x5B);SPI_WriteData(0x03);SPI_WriteData(0x6B);SPI_WriteData(0x03);SPI_WriteData(0x90);SPI_WriteData(0x03);SPI_WriteData(0xFF);
    SPI_WriteComm(0xD2);SPI_WriteData(0x00);SPI_WriteData(0x00);SPI_WriteData(0x00);SPI_WriteData(0x11);SPI_WriteData(0x00);SPI_WriteData(0x3C);SPI_WriteData(0x00);SPI_WriteData(0x4F);SPI_WriteData(0x00);SPI_WriteData(0x6D);SPI_WriteData(0x00);SPI_WriteData(0x9E);SPI_WriteData(0x00);SPI_WriteData(0xC0);SPI_WriteData(0x01);SPI_WriteData(0x03);SPI_WriteData(0x01);SPI_WriteData(0x35);SPI_WriteData(0x01);SPI_WriteData(0x7A);SPI_WriteData(0x01);SPI_WriteData(0xAA);SPI_WriteData(0x01);SPI_WriteData(0xF5);SPI_WriteData(0x02);SPI_WriteData(0x32);SPI_WriteData(0x02);SPI_WriteData(0x34);SPI_WriteData(0x02);SPI_WriteData(0x6E);SPI_WriteData(0x02);SPI_WriteData(0xA7);SPI_WriteData(0x02);SPI_WriteData(0xD1);SPI_WriteData(0x03);SPI_WriteData(0x00);SPI_WriteData(0x03);SPI_WriteData(0x33);SPI_WriteData(0x03);SPI_WriteData(0x3F);SPI_WriteData(0x03);SPI_WriteData(0x4A);SPI_WriteData(0x03);SPI_WriteData(0x4F);SPI_WriteData(0x03);SPI_WriteData(0x5B);SPI_WriteData(0x03);SPI_WriteData(0x6B);SPI_WriteData(0x03);SPI_WriteData(0x90);SPI_WriteData(0x03);SPI_WriteData(0xFF);
    SPI_WriteComm(0xD3);SPI_WriteData(0x00);SPI_WriteData(0x00);SPI_WriteData(0x00);SPI_WriteData(0x11);SPI_WriteData(0x00);SPI_WriteData(0x3C);SPI_WriteData(0x00);SPI_WriteData(0x4F);SPI_WriteData(0x00);SPI_WriteData(0x6D);SPI_WriteData(0x00);SPI_WriteData(0x9E);SPI_WriteData(0x00);SPI_WriteData(0xC0);SPI_WriteData(0x01);SPI_WriteData(0x03);SPI_WriteData(0x01);SPI_WriteData(0x35);SPI_WriteData(0x01);SPI_WriteData(0x7A);SPI_WriteData(0x01);SPI_WriteData(0xAA);SPI_WriteData(0x01);SPI_WriteData(0xF5);SPI_WriteData(0x02);SPI_WriteData(0x32);SPI_WriteData(0x02);SPI_WriteData(0x34);SPI_WriteData(0x02);SPI_WriteData(0x6E);SPI_WriteData(0x02);SPI_WriteData(0xA7);SPI_WriteData(0x02);SPI_WriteData(0xD1);SPI_WriteData(0x03);SPI_WriteData(0x00);SPI_WriteData(0x03);SPI_WriteData(0x33);SPI_WriteData(0x03);SPI_WriteData(0x3F);SPI_WriteData(0x03);SPI_WriteData(0x4A);SPI_WriteData(0x03);SPI_WriteData(0x4F);SPI_WriteData(0x03);SPI_WriteData(0x5B);SPI_WriteData(0x03);SPI_WriteData(0x6B);SPI_WriteData(0x03);SPI_WriteData(0x90);SPI_WriteData(0x03);SPI_WriteData(0xFF);
    SPI_WriteComm(0xD4);SPI_WriteData(0x00);SPI_WriteData(0x00);SPI_WriteData(0x00);SPI_WriteData(0x11);SPI_WriteData(0x00);SPI_WriteData(0x3C);SPI_WriteData(0x00);SPI_WriteData(0x4F);SPI_WriteData(0x00);SPI_WriteData(0x6D);SPI_WriteData(0x00);SPI_WriteData(0x9E);SPI_WriteData(0x00);SPI_WriteData(0xC0);SPI_WriteData(0x01);SPI_WriteData(0x03);SPI_WriteData(0x01);SPI_WriteData(0x35);SPI_WriteData(0x01);SPI_WriteData(0x7A);SPI_WriteData(0x01);SPI_WriteData(0xAA);SPI_WriteData(0x01);SPI_WriteData(0xF5);SPI_WriteData(0x02);SPI_WriteData(0x32);SPI_WriteData(0x02);SPI_WriteData(0x34);SPI_WriteData(0x02);SPI_WriteData(0x6E);SPI_WriteData(0x02);SPI_WriteData(0xA7);SPI_WriteData(0x02);SPI_WriteData(0xD1);SPI_WriteData(0x03);SPI_WriteData(0x00);SPI_WriteData(0x03);SPI_WriteData(0x33);SPI_WriteData(0x03);SPI_WriteData(0x3F);SPI_WriteData(0x03);SPI_WriteData(0x4A);SPI_WriteData(0x03);SPI_WriteData(0x4F);SPI_WriteData(0x03);SPI_WriteData(0x5B);SPI_WriteData(0x03);SPI_WriteData(0x6B);SPI_WriteData(0x03);SPI_WriteData(0x90);SPI_WriteData(0x03);SPI_WriteData(0xFF);
    SPI_WriteComm(0xD5);SPI_WriteData(0x00);SPI_WriteData(0x00);SPI_WriteData(0x00);SPI_WriteData(0x11);SPI_WriteData(0x00);SPI_WriteData(0x3C);SPI_WriteData(0x00);SPI_WriteData(0x4F);SPI_WriteData(0x00);SPI_WriteData(0x6D);SPI_WriteData(0x00);SPI_WriteData(0x9E);SPI_WriteData(0x00);SPI_WriteData(0xC0);SPI_WriteData(0x01);SPI_WriteData(0x03);SPI_WriteData(0x01);SPI_WriteData(0x35);SPI_WriteData(0x01);SPI_WriteData(0x7A);SPI_WriteData(0x01);SPI_WriteData(0xAA);SPI_WriteData(0x01);SPI_WriteData(0xF5);SPI_WriteData(0x02);SPI_WriteData(0x32);SPI_WriteData(0x02);SPI_WriteData(0x34);SPI_WriteData(0x02);SPI_WriteData(0x6E);SPI_WriteData(0x02);SPI_WriteData(0xA7);SPI_WriteData(0x02);SPI_WriteData(0xD1);SPI_WriteData(0x03);SPI_WriteData(0x00);SPI_WriteData(0x03);SPI_WriteData(0x33);SPI_WriteData(0x03);SPI_WriteData(0x3F);SPI_WriteData(0x03);SPI_WriteData(0x4A);SPI_WriteData(0x03);SPI_WriteData(0x4F);SPI_WriteData(0x03);SPI_WriteData(0x5B);SPI_WriteData(0x03);SPI_WriteData(0x6B);SPI_WriteData(0x03);SPI_WriteData(0x90);SPI_WriteData(0x03);SPI_WriteData(0xFF);
    SPI_WriteComm(0xD6);SPI_WriteData(0x00);SPI_WriteData(0x00);SPI_WriteData(0x00);SPI_WriteData(0x11);SPI_WriteData(0x00);SPI_WriteData(0x3C);SPI_WriteData(0x00);SPI_WriteData(0x4F);SPI_WriteData(0x00);SPI_WriteData(0x6D);SPI_WriteData(0x00);SPI_WriteData(0x9E);SPI_WriteData(0x00);SPI_WriteData(0xC0);SPI_WriteData(0x01);SPI_WriteData(0x03);SPI_WriteData(0x01);SPI_WriteData(0x35);SPI_WriteData(0x01);SPI_WriteData(0x7A);SPI_WriteData(0x01);SPI_WriteData(0xAA);SPI_WriteData(0x01);SPI_WriteData(0xF5);SPI_WriteData(0x02);SPI_WriteData(0x32);SPI_WriteData(0x02);SPI_WriteData(0x34);SPI_WriteData(0x02);SPI_WriteData(0x6E);SPI_WriteData(0x02);SPI_WriteData(0xA7);SPI_WriteData(0x02);SPI_WriteData(0xD1);SPI_WriteData(0x03);SPI_WriteData(0x00);SPI_WriteData(0x03);SPI_WriteData(0x33);SPI_WriteData(0x03);SPI_WriteData(0x3F);SPI_WriteData(0x03);SPI_WriteData(0x4A);SPI_WriteData(0x03);SPI_WriteData(0x4F);SPI_WriteData(0x03);SPI_WriteData(0x5B);SPI_WriteData(0x03);SPI_WriteData(0x6B);SPI_WriteData(0x03);SPI_WriteData(0x90);SPI_WriteData(0x03);SPI_WriteData(0xFF);

    SPI_WriteComm(0x35);SPI_WriteData(0x00);
    SPI_WriteComm(0x3A);SPI_WriteData(0x77);
    LCD_GC9503V_SetLandscapeScan();

    SPI_WriteComm(0x11);
    HAL_Delay(120);
    SPI_WriteComm(0x29);
    HAL_Delay(50);
}

void LCD_Clear(uint32_t color)
{
	uint32_t fb = (uint32_t)&ltdc_lcd_framebuf;

	lcd_dma2d_fill(fb,
	               LCD_PHYS_WIDTH,
	               LCD_PHYS_HEIGHT,
	               0U,
	               color);
	lcd_dcache_clean((const void *)fb,
	                 (uint32_t)LCD_PHYS_WIDTH * LCD_PHYS_HEIGHT * 2U);
}

void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color)
{
	uint16_t px;
	uint16_t py;

	if(x >= LCD_WIDTH || y >= LCD_HEIGHT) {
		return;
	}

	lcd_logical_to_phys(x, y, &px, &py);
	*(__IO uint16_t *)lcd_phys_addr(px, py) = color;
}

void LCD_BlitPhysAreaAsync(uint16_t px, uint16_t py, uint16_t width, uint16_t height, const uint16_t *src)
{
	uint32_t dst_addr;
	uint32_t clean_size;

	if((src == NULL) || (width == 0U) || (height == 0U)) {
		return;
	}

	if(((uint32_t)px + width) > LCD_PHYS_WIDTH || ((uint32_t)py + height) > LCD_PHYS_HEIGHT) {
		return;
	}

	lcd_dcache_clean(src, (uint32_t)width * height * 2U);

	dst_addr = lcd_phys_addr(px, py);
	clean_size = (uint32_t)height * ((uint32_t)LCD_PHYS_WIDTH * 2U);

	lcd_dma2d_start_async((uint32_t)src,
	                      dst_addr,
	                      width,
	                      height,
	                      0U,
	                      (uint16_t)(LCD_PHYS_WIDTH - width),
	                      dst_addr,
	                      clean_size);
}

void LCD_BlitPhysArea(uint16_t px, uint16_t py, uint16_t width, uint16_t height, const uint16_t *src)
{
	LCD_BlitPhysAreaAsync(px, py, width, height, src);
	LCD_FlushWait();
}

bool LCD_BlitAreaAsync(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *src)
{
	uint16_t px_bottom;
	uint32_t pixel_count;
	uint32_t dst_addr;
	uint32_t clean_size;
	const uint16_t *dma_src;
	uint16_t dma_w;
	uint16_t dma_h;
	uint16_t dst_oor;

	if((src == NULL) || (width == 0U) || (height == 0U)) {
		return false;
	}

	if(((uint32_t)x + width) > LCD_WIDTH || ((uint32_t)y + height) > LCD_HEIGHT) {
		return false;
	}

	if(height > LCD_LVGL_BUF_LINES) {
		return false;
	}

	pixel_count = (uint32_t)width * height;
	px_bottom = (uint16_t)(LCD_PHYS_WIDTH - height - y);
	dst_addr = lcd_phys_addr(px_bottom, x);
	if(height == 1U) {
		uint16_t px = (uint16_t)(LCD_PHYS_WIDTH - 1U - y);

		lcd_dcache_clean(src, (uint32_t)width * 2U);
		dst_addr = lcd_phys_addr(px, x);
		clean_size = (uint32_t)LCD_PHYS_WIDTH * 2U;
		lcd_dma2d_start_async((uint32_t)src,
		                      dst_addr,
		                      1U,
		                      width,
		                      0U,
		                      (uint16_t)(LCD_PHYS_WIDTH - 1U),
		                      dst_addr,
		                      clean_size);
		return true;
	}

	clean_size = (uint32_t)width * ((uint32_t)LCD_PHYS_WIDTH * 2U);

	lcd_dcache_clean(src, pixel_count * 2U);
	lcd_transpose_to_phys(src, lcd_rotate_buf, width, height);
	lcd_dcache_clean(lcd_rotate_buf, pixel_count * 2U);

	dma_src = lcd_rotate_buf;
	dma_w = height;
	dma_h = width;
	dst_oor = (uint16_t)(LCD_PHYS_WIDTH - height);

	lcd_dma2d_start_async((uint32_t)dma_src,
	                      dst_addr,
	                      dma_w,
	                      dma_h,
	                      0U,
	                      dst_oor,
	                      dst_addr,
	                      clean_size);
	return true;
}

void LCD_BlitArea(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *src)
{
	LCD_BlitAreaAsync(x, y, width, height, src);
	LCD_FlushWait();
}

void ShowPic_XY(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint32_t Image)
{
	LCD_BlitArea(x, y, width, height, (const uint16_t *)Image);
}

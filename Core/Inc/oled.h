#ifndef __OLED_H
#define __OLED_H

#include "main.h"

#define OLED_I2C_ADDR   0x78        /* SSD1306：STM32F1 的 HAL 不左移，要传 8 位地址（7位=0x3C） */
#define OLED_WIDTH      128
#define OLED_HEIGHT     64

void OLED_Init(void);
void OLED_Clear(void);
void OLED_Update(void);
void OLED_ShowString(uint8_t x, uint8_t y, uint8_t *str, uint8_t size);
void OLED_ShowText(uint8_t x, uint8_t y, const char *str, uint8_t size);   /* 上者的 const 包装，用起来不用强转 */
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size);
void OLED_DisPlayOn(void);
void OLED_DisPlayOff(void);

#endif

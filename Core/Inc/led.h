#ifndef __LED_H
#define __LED_H

#include "main.h"

/* LED 的三种工作模式 */
typedef enum
{
    LED_MODE_OFF = 0,       /* 两个都灭 */
    LED_MODE_ON,            /* 两个都亮 */
    LED_MODE_BLINK          /* 红绿交替闪 */
} LedMode;

void    Led_Init(void);
void    Led_SetMode(LedMode mode);
LedMode Led_GetMode(void);
void    Led_Task(void);         /* 必须在主循环里不停调用（非阻塞） */

/* 返回"引脚此刻真实电平"换算出的亮灭，给 OLED 显示用 */
uint8_t Led_IsRedOn(void);
uint8_t Led_IsGreenOn(void);

#endif /* __LED_H */

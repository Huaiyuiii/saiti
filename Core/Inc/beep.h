#ifndef __BEEP_H
#define __BEEP_H

#include "main.h"

/* 蜂鸣器（低电平触发模块）
 *
 * 全部接口都是非阻塞的：Beep_Beep() 只登记"要响多久"，真正的关断
 * 在 Beep_Task() 里按时间戳完成。这样主循环不会被 HAL_Delay 卡住，
 * 按键和 OLED 刷新照常跑。 */

void    Beep_Init(void);            /* 上电先静音 */
void    Beep_On(void);              /* 一直响，直到 Beep_Off */
void    Beep_Off(void);             /* 立刻停 */
void    Beep_Beep(uint16_t ms);     /* 响 ms 毫秒后自动停 */
void    Beep_BeepN(uint8_t count, uint16_t on_ms, uint16_t off_ms);
                                    /* 响 count 声，每声 on_ms，间隔 off_ms */
void    Beep_Task(void);            /* 必须在主循环里不停调用（非阻塞） */
uint8_t Beep_IsBusy(void);          /* 1 = 正在响或还在序列中 */

#endif /* __BEEP_H */

#ifndef __KEY_H
#define __KEY_H

#include "main.h"

/* 按键编号：和赛题里的"按键1/2/3/4"一一对应
 * 具体引脚见 app_config.h 的 KEY_PIN_LIST（当前 PA0 / PA1 / PA4 / PA5） */
typedef enum
{
    KEY_ID_NONE = 0,
    KEY_ID_1    = 1,        /* K1 */
    KEY_ID_2    = 2,        /* K2 */
    KEY_ID_3    = 3,        /* K3 */
    KEY_ID_4    = 4         /* K4 */
} KeyId;

void    Key_Init(void);

/* 在主循环里不停调用。只有"按下的那一刻"返回一次键号，其余时候返回 KEY_ID_NONE */
uint8_t Key_Scan(void);

#endif /* __KEY_H */

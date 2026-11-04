#ifndef __KEY_H
#define __KEY_H

#include "main.h"

/* 按键编号：和赛题里的"按键1/2/3/4"一一对应 */
typedef enum
{
    KEY_ID_NONE = 0,
    KEY_ID_1    = 1,        /* PA8  */
    KEY_ID_2    = 2,        /* PA11 */
    KEY_ID_3    = 3,        /* PA12 */
    KEY_ID_4    = 4         /* PA15 */
} KeyId;

void    Key_Init(void);

/* 在主循环里不停调用。只有"按下的那一刻"返回一次键号，其余时候返回 KEY_ID_NONE */
uint8_t Key_Scan(void);

#endif /* __KEY_H */

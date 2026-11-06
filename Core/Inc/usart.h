#ifndef __USART_H
#define __USART_H

#include "main.h"

/* USART1：PA9 = TX，PA10 = RX
 * 基础部分 3b / 3c 用 USB-TTL 接电脑（上位机）
 * 拓展 6.3 蓝牙也可以先挂这里试，两个不要同时插 */

extern UART_HandleTypeDef huart1;

void Usart_Init(void);
void Usart_Task(void);                          /* 主循环里调，解析收到的命令 */

void Usart_SendString(const char *s);           /* 发一串字符 */
void Usart_SendLine(const char *s);             /* 发一串字符 + \r\n */

#endif /* __USART_H */

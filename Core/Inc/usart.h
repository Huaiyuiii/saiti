#ifndef __USART_H
#define __USART_H

#include "main.h"

/* 上位机通信口。具体用 USART1（PA9/PA10）还是 USART2（PA2/PA3）
 * 由 app_config.h 里的 USART_PORT_SEL 决定，这里不用管。
 *
 * 基础部分 3b / 3c 用 USB-TTL 接电脑（上位机）
 * 拓展 6.3 蓝牙也可以挂这里，两个不要同时插 */

extern UART_HandleTypeDef huart_com;

void Usart_Init(void);
void Usart_Task(void);                          /* 主循环里调，解析收到的命令 */

void Usart_SendString(const char *s);           /* 发一串字符 */
void Usart_SendLine(const char *s);             /* 发一串字符 + \r\n */

#endif /* __USART_H */

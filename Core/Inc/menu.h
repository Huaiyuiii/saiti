#ifndef __MENU_H
#define __MENU_H

#include "main.h"

/* 页面编号。注意：PAGE_LED ~ PAGE_EXT 必须和主菜单 4 个选项的顺序一致 */
typedef enum
{
    PAGE_MAIN = 0,
    PAGE_LED,           /* 第 1 项：LED 控制（2a/2b/2c/2d） */
    PAGE_INFO,          /* 第 3a 项：信息显示 */
    PAGE_LINE,          /* 第 4a 项：传感器显示 */
    PAGE_EXT            /* 拓展功能入口 */
} PageId;

void Menu_Init(void);
void Menu_Task(void);       /* 主循环里不停调用：读按键 + 刷新 OLED */

#endif /* __MENU_H */

#ifndef __APP_CONFIG_H
#define __APP_CONFIG_H

#include "main.h"

/* =====================================================================
 * 全部可调参数集中在这里。老师当面改参数时，只改这个文件。
 * ===================================================================== */

/* -------------------- 1. LED -------------------- */
#define LED_RED_PORT            GPIOB
#define LED_RED_PIN             GPIO_PIN_3
#define LED_GREEN_PORT          GPIOB
#define LED_GREEN_PIN           GPIO_PIN_4

/* 0 = 高电平点亮（阳极接引脚，阴极接 GND）
 * 1 = 低电平点亮（阴极接引脚，阳极接 3.3V） */
#define LED_ACTIVE_LOW          0

/* 闪烁半周期（ms）。250 = 亮 250ms 灭 250ms，完整周期 500ms
 * 改大会闪得慢，改小会闪得快；小于 50 人眼看不出"交替" */
#define LED_BLINK_HALF_MS       250

/* -------------------- 2. 按键 -------------------- */
#define KEY_PORT                GPIOA
#define KEY_COUNT               4

/* 内部上拉 + 另一端接 GND，所以按下读到低电平 */
#define KEY_PRESSED_LEVEL       GPIO_PIN_RESET

/* 扫描周期与消抖时间（ms） */
#define KEY_SCAN_MS             10
#define KEY_DEBOUNCE_MS         20

/* -------------------- 3. 蜂鸣器（4b 发车提示用，现在先空着）-------------------- */
#define BEEP_PORT               GPIOB
#define BEEP_PIN                GPIO_PIN_5
#define BEEP_ACTIVE_LOW         1       /* 1 = 低电平响 */

/* -------------------- 4. OLED -------------------- */
#define OLED_SCL_PORT           GPIOB
#define OLED_SCL_PIN            GPIO_PIN_6
#define OLED_SDA_PORT           GPIOB
#define OLED_SDA_PIN            GPIO_PIN_7

/* 软件 I2C 的延时循环次数，决定 SCL 频率
 * 20 次约 300kHz；调大 → 变慢；调小 → 变快（太快会通信失败） */
#define OLED_I2C_DELAY_LOOPS    20

/* 整屏刷新周期（ms）。赛题 2d 要求 1 秒内更新，100ms 留了 10 倍余量 */
#define MENU_REFRESH_MS         100

/* -------------------- 5. 循迹传感器（8 路）-------------------- */
#define TRACK_PORT              GPIOA

/* 8 路引脚，顺序 = 从左到右 X1~X8，都在 GPIOA。
 * 方向已在 MX_GPIO_Init() 里配成「输入 + 上拉」，不需要重复配置 */
#define TRACK_PIN_LIST          { GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_2, GPIO_PIN_3, \
                                  GPIO_PIN_4, GPIO_PIN_5, GPIO_PIN_6, GPIO_PIN_7 }

/* 1 = 检测到黑线时该路输出高电平（多数模块默认）
 * 0 = 检测到黑线时输出低电平
 * 判断方法：进 LINE 页面，把车放在白纸上应该全是 0；
 * 把某个探头压到黑线上，对应位变 1 就是对的。如果正好相反，把这里改成 0 */
#define TRACK_ACTIVE_HIGH       1

/* -------------------- 6. 个人信息（3a 要显示，改成你自己的）-------------------- */
/* ⚠️ 把下面一串 0 换成你的真学号，否则 3a 那 1 分拿不到 */
#define INFO_NAME               "ZHANGTIANYU"
#define INFO_STUDENT_ID         "32602237"

#endif /* __APP_CONFIG_H */

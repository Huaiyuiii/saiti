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

/* -------------------- 3. 蜂鸣器（低电平触发模块，带三极管）-------------------- */
#define BEEP_PORT               GPIOB
#define BEEP_PIN                GPIO_PIN_5
#define BEEP_ACTIVE_LOW         1       /* 1 = 低电平响（模块丝印"低电触发"） */

/* 上电提示音：响多久（ms）。0 = 上电不响 */
#define BEEP_BOOT_MS            120

/* 发车提示音：响 3 声，每声 80ms，中间停 120ms */
#define BEEP_START_COUNT        3
#define BEEP_START_ON_MS        80
#define BEEP_START_OFF_MS       120

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
/* 模块只支持 5V 供电，输出信号也是 5V。
 * PA0~PA7 是 TTa 类型，不承受 5V，所以改用 GPIOB 的 PB8~PB15，
 * 这 8 个脚是 FT（5V 容忍），可以直接接 5V 信号。
 * 引脚方向在 Track_Init() 里自己配置，不依赖 CubeMX 生成。 */
#define TRACK_PORT              GPIOB

/* 8 路引脚，顺序 = 从左到右 X1~X8 */
#define TRACK_PIN_LIST          { GPIO_PIN_8,  GPIO_PIN_9,  GPIO_PIN_10, GPIO_PIN_11, \
                                  GPIO_PIN_12, GPIO_PIN_13, GPIO_PIN_14, GPIO_PIN_15 }

/* 1 = 检测到黑线时该路输出高电平（多数模块默认）
 * 0 = 检测到黑线时输出低电平
 * 判断方法：进 LINE 页面，把车放在白纸上应该全是 0；
 * 把某个探头压到黑线上，对应位变 1 就是对的。如果正好相反，把这里改成 0 */
#define TRACK_ACTIVE_HIGH       1

/* 1 = 把偏差的正负号反过来。
 * 如果实测发现「车偏左时 ERR 显示正数」，说明模块装的方向和代码假设相反，
 * 把这个改成 1 即可，不用重新接线 */
#define TRACK_REVERSE           0

/* -------------------- 6. 个人信息（3a 要显示，改成你自己的）-------------------- */
/* ⚠️ 把下面一串 0 换成你的真学号，否则 3a 那 1 分拿不到 */
#define INFO_NAME               "ZHANGTIANYU"
#define INFO_STUDENT_ID         "32602237"

/* -------------------- 7. 串口（3b / 3c 上位机通信）-------------------- */
/* 用哪个串口：
 *   1 = USART1，PA9 = TX，PA10 = RX
 *       两个都是 FT（5V 容忍），5V 的 USB-TTL 可以直接接，不用电阻。
 *       缺点是 PA8、PA11 已经插了按键线，PA9/PA10 夹在中间，母头容易顶住。
 *   2 = USART2，PA2 = TX，PA3 = RX
 *       位置宽松，但这两个脚是 TTa（只承受 3.3V）。
 *       ⚠️ USB-TTL 的 TXD 是 5V，接 PA3 必须分压，否则会打穿输入保护二极管。
 *       分压：TXD --[1k]-- PA3 --[2k]-- GND，得到 3.3V。
 *       PA2（单片机发）输出 3.3V，直接接 USB-TTL 的 RXD 就行，不用处理。
 *
 * 改完这个宏，usart.c 会自动切换引脚、时钟和中断向量，别的地方不用动。 */
#define USART_PORT_SEL          2

#define USART_BAUDRATE          115200      /* 串口助手上的波特率必须和这里一致 */

/* 一帧最多收多少字节。"GET A" 才 5 个字节，64 已经很宽松 */
#define USART_RX_BUF_SIZE       64

/* 对方不发换行时，静默多少毫秒就把已有内容当成一帧。
 * 改小 → 反应快，但发送慢的设备可能被截断；改大 → 更宽容但延迟高 */
#define USART_FRAME_IDLE_MS     50

/* 发送一帧的超时保护（ms）。115200 下发 64 字节约 6ms */
#define USART_TX_TIMEOUT_MS     100

/* 变量 a 的取值范围。屏幕上是 4 位数字，超出就显示不全，所以卡在这里 */
#define USART_VAR_A_MIN         (-9999)
#define USART_VAR_A_MAX         (9999)

#endif /* __APP_CONFIG_H */

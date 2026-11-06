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
/* 原来在 PA8 / PA11 / PA12 / PA15，但这 4 个脚和串口 PA9/PA10 挨着，
 * 杜邦线母头挤在一起插不进去。所以整体挪到 PA 口前半段。
 *
 * 选 PA0 / PA1 / PA4 / PA5 的理由：
 *   - 中间空出 PA2 / PA3，正好留给蓝牙（USART2），互不干扰
 *   - 按键是无源开关接 GND，只走 3.3V，落在 TTa 脚上完全安全
 *   - 这 4 个脚不挨着，母头之间有空隙 */
#define KEY_PORT                GPIOA
#define KEY_COUNT               4

/* 4 个按键引脚，顺序 = K1 / K2 / K3 / K4 */
#define KEY_PIN_LIST            { GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_4, GPIO_PIN_5 }

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

/* -------------------- 6. 电机（TB6612FNG 双路驱动）-------------------- */
/* TB6612 每路电机要 3 根线：PWM（调速）+ IN1/IN2（定方向）。
 * 两路共 6 根，加 STBY 待机脚。
 *
 * 为什么 PWM 必须放 PA6 / PA7：
 *   这两个脚是 TIM3 的 CH1 / CH2 默认通道，不用重映射就能出硬件 PWM。
 *   换到别的脚就只能软件模拟 PWM，主循环会被占满，屏幕和按键全卡。
 *
 * 方向脚随便挑，但要避开已占用的：
 *   PA0/PA1/PA4/PA5 按键，PA2/PA3 蓝牙，PA9/PA10 串口，
 *   PA13/PA14 下载口，PB3~PB15 灯/蜂鸣器/OLED/循迹。
 * 所以只剩 PA8、PA11、PA12、PB0、PB1、PB2、PA15。
 *   - PA15 是 JTAG 的 JTDI，做输出不可靠，排除
 *   - PB2 是 BOOT1，上电瞬间被采样，排除
 * 最后取 PA8 / PA11 / PA12 / PB0。 */
#define MOTOR_PWM_PORT          GPIOA

#define MOTOR_PWMA_PIN          GPIO_PIN_6      /* TIM3_CH1 */
#define MOTOR_PWMB_PIN          GPIO_PIN_7      /* TIM3_CH2 */

#define MOTOR_AIN1_PORT         GPIOA
#define MOTOR_AIN1_PIN          GPIO_PIN_8
#define MOTOR_AIN2_PORT         GPIOA
#define MOTOR_AIN2_PIN          GPIO_PIN_11

#define MOTOR_BIN1_PORT         GPIOA
#define MOTOR_BIN1_PIN          GPIO_PIN_12
#define MOTOR_BIN2_PORT         GPIOB
#define MOTOR_BIN2_PIN          GPIO_PIN_0

/* STBY 待机脚：低电平芯片关断，电机自由滑行。
 * 现在直接接 3.3V（常使能），代码里不管它。
 * 如果以后想软件急停，把它挪到 PB1，再把下面改成 1 */
#define MOTOR_USE_STBY_PIN      0
#define MOTOR_STBY_PORT         GPIOB
#define MOTOR_STBY_PIN          GPIO_PIN_1

/* PWM 频率 = 72MHz / (ARR + 1) = 72MHz / 3600 = 20kHz
 * 20kHz 在人耳听觉上限之外，电机不会发出"滋滋"啸叫。
 * 改成 10kHz 也能用，但会听到轻微的高频噪音 */
#define MOTOR_PWM_ARR           3599

/* 速度满量程。Motor_Set 的参数范围是 -1000 ~ +1000，
 * 符号表示方向，绝对值表示占空比（1000 = 100%） */
#define MOTOR_FULL_SCALE        1000

/* 死区补偿：TB6612 在占空比很低时电机不转，只有"嗡嗡"声还发热。
 * 实测一般 15%~20% 才起转，这里给 180（18%）。
 * 效果：只要给了非零速度，就至少输出 180，越过死区。
 * 如果发现低速时车"窜一下"，把这个调小；如果低速干脆不动，调大 */
#define MOTOR_DEADZONE          180

/* 软启动步进周期（ms）。Motor_SetRamp 每隔这么久推进一步，
 * 让 PWM 从当前值平滑爬到目标值。
 * 为什么要软启动：电机静止时启动电流接近堵转电流（1~2A），
 * 瞬间把电源拉垮。缓慢加速能把冲击电流压下去。
 * 改小 → 加速快但冲击大；改大 → 加速柔和但迟钝 */
#define MOTOR_RAMP_TICK_MS      10

/* 默认直行速度（循迹用）。0~1000，600 大约是中等偏快 */
#define MOTOR_BASE_SPEED        600

/* ---- 上电自测：验证接线用，测通后改回 0 ---- */
/* 1 = 上电后等 3 秒，两轮正转 2 秒，然后停
 * 0 = 正常模式，上电不转
 * ⚠️ 测试时必须把车架空，否则会直接窜出去 */
#define MOTOR_SELFTEST          1
#define MOTOR_SELFTEST_WAIT_MS  3000    /* 上电等多久再转，留时间松手 */
#define MOTOR_SELFTEST_RUN_MS   2000    /* 转多久 */
#define MOTOR_SELFTEST_SPEED    400     /* 测试速度，400 够看清转向又不至于太快 */
#define MOTOR_SELFTEST_RAMP_MS  1200    /* 从 0 爬到目标速度用多久，压启动电流 */

/* -------------------- 7. 个人信息（3a 要显示，改成你自己的）-------------------- */
/* ⚠️ 把下面一串 0 换成你的真学号，否则 3a 那 1 分拿不到 */
#define INFO_NAME               "ZHANGTIANYU"
#define INFO_STUDENT_ID         "32602237"

/* -------------------- 8. 串口（3b / 3c 上位机通信）-------------------- */
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
#define USART_PORT_SEL          1

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

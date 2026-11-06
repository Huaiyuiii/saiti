#ifndef __PARAM_H
#define __PARAM_H

#include "main.h"

/* =====================================================================
 * 参数保存（赛题 6.2）
 *
 * 把 KP / KD / 基准速度 / 圈数 存到 Flash 最后一页，掉电不丢。
 * 好处：现场调参不用改代码重烧，按键调完按保存就行。
 *
 * STM32F103C8T6 的 Flash：
 *   总容量 64KB，页大小 1KB，共 64 页
 *   程序从 0x08000000 开始，我们占最后一页 0x0800FC00
 *   当前程序约 15KB，离最后一页还很远，不会冲突
 *
 * 为什么不用 EEPROM：C8T6 内部没有 EEPROM，只能用 Flash 模拟。
 * Flash 擦写寿命约 1 万次，调参完全够用，但别在循环里频繁写。
 * ===================================================================== */

/* 参数结构体。必须 4 字节对齐，否则 Flash 按字写入会错位 */
typedef struct
{
    uint32_t magic;         /* 固定值，用来判断这块 Flash 有没有写过 */
    int16_t  kp;            /* 循迹比例系数 */
    int16_t  kd;            /* 循迹微分系数 */
    int16_t  base_speed;    /* 直行基准速度 0~1000 */
    uint8_t  laps;          /* 默认圈数 1 或 2 */
    uint8_t  beep_on;       /* 1 = 允许提示音 */
    uint16_t reserved;      /* 占位，保证结构体是 4 的倍数 */
    uint32_t sum;           /* 前面所有字节的累加和，用来校验 */
} Param_t;

/* 从 Flash 加载。Flash 是空的或校验不过，就自动装载默认值（不写 Flash） */
void Param_Init(void);

/* 把当前参数写进 Flash。内部会先擦页，耗时约 20~40ms，期间 CPU 会停顿 */
void Param_Save(void);

/* 恢复出厂默认值（只改内存，要持久化还得再调 Param_Save） */
void Param_Reset(void);

/* 取参数指针。调用方直接读写里面的字段，改完调 Param_Save 才生效到 Flash */
Param_t *Param_Get(void);

#endif /* __PARAM_H */

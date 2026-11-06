#ifndef __LINE_H
#define __LINE_H

#include "main.h"

/* =====================================================================
 * 循迹闭环控制（赛题 4b / 4c）
 *
 * 原理：
 *   8 路探头 → Track_GetError() 得到 -7~+7 的偏差
 *   → PD 运算得到"转向量"
 *   → 左轮 = 基准速度 + 转向量，右轮 = 基准速度 - 转向量
 *
 * 为什么用 PD 不用 PID：
 *   积分项 I 在循迹里几乎没有用武之地——偏差是位置量，不是累积误差，
 *   加上 I 反而会在弯道里积出超调，车会画蛇。
 *   P 负责"拉回来"，D 负责"别冲过头"，这两个就够了。
 * ===================================================================== */

/* 循迹状态 */
typedef enum
{
    LINE_IDLE = 0,      /* 待机，没在跑 */
    LINE_RUNNING,       /* 正在跑 */
    LINE_DONE           /* 跑完一圈或多圈，成绩已定格 */
} LineState;

void        Line_Init(void);

/* 发车。laps = 1（4b）或 2（4c）。会先响发车提示音 */
void        Line_Start(uint8_t laps);

/* 立刻停车并定格成绩。跑完最后一圈时内部会自动调用 */
void        Line_Stop(void);

/* 主循环里调用。内部按 LINE_CTRL_MS 限速，不会拖慢别的东西 */
void        Line_Task(void);

LineState   Line_GetState(void);
uint32_t    Line_GetElapsedMs(void);    /* 本次（或上次）用时 */
uint8_t     Line_GetLapDone(void);      /* 已经跑完几圈 */
uint8_t     Line_GetLapTarget(void);    /* 目标几圈 */
int8_t      Line_GetError(void);        /* 最近一次偏差，调试用 */
uint8_t     Line_GetMask(void);         /* 最近一次探头掩码，调试用 */

#endif /* __LINE_H */

#ifndef __MOTOR_H
#define __MOTOR_H

#include "main.h"

/* TB6612FNG 双路电机驱动
 *
 * 速度用有符号整数表示：
 *     0        = 停（滑行，电机不受力）
 *     +1~+1000 = 正转，绝对值是占空比
 *     -1~-1000 = 反转
 *
 * 左右轮的"正转"方向由电机线怎么插决定。如果装好后发现车往后跑，
 * 不用改代码，把该电机在驱动板上的两根线对调即可。 */

/* 初始化：配引脚 + TIM3 双路 PWM（20kHz） */
void Motor_Init(void);

/* 设置左右轮速度，范围 -1000 ~ +1000，超出自动截断 */
void Motor_Set(int16_t left, int16_t right);

/* 两轮滑行停止（IN1=IN2=0，电机不受力，车会靠惯性溜一段） */
void Motor_Stop(void);

/* 两轮刹车（IN1=IN2=1，电机绕组短路，立刻停住，但电流大、发热快） */
void Motor_Brake(void);

/* 读回当前设定的速度，调试和显示用 */
void Motor_GetSpeed(int16_t *left, int16_t *right);

#endif /* __MOTOR_H */

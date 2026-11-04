#ifndef __TRACK_H
#define __TRACK_H

#include <stdint.h>

/*
 * 8 路循迹传感器
 *
 * 位序：bit0 = X1（最左），bit7 = X8（最右）
 * 位为 1 表示该路压在黑线上
 */

#define TRACK_CH_COUNT          8
#define TRACK_MASK_NONE         0x00u
#define TRACK_MASK_ALL          0xFFu

/* 初始化（引脚方向已在 CubeMX 里配好，这里只做状态清零） */
void    Track_Init(void);

/* 一次读出 8 路状态，返回 0x00~0xFF 的位掩码 */
uint8_t Track_ReadMask(void);

/* 黑线相对车头中心的偏差：-7（偏左）~ +7（偏右），0 = 正对中心
 * 一路都没检测到时返回 0，调用方应先用 Track_IsLost() 判断 */
int8_t  Track_GetError(void);

/* 8 路全部没压到黑线 = 脱轨，返回 1 */
uint8_t Track_IsLost(void);

#endif /* __TRACK_H */

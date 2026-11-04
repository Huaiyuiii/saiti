/*
 * track.c - 8 路循迹传感器
 *
 * 8 个探头从左到右挂在 PA0~PA7，每个探头只输出高低电平
 * （黑线 / 白底），没有模拟量，所以不需要 ADC。
 *
 * 用法：
 *   uint8_t m = Track_ReadMask();   一次拿到 8 路状态
 *   int8_t  e = Track_GetError();   偏差 -7~+7，负数偏左，正数偏右
 */

#include "track.h"
#include "app_config.h"

/* 8 路引脚表，顺序必须和探头从左到右一致 */
static const uint16_t s_pin[TRACK_CH_COUNT] = TRACK_PIN_LIST;

/* 读某一路，并按配置统一成「1 = 压在黑线上」 */
static uint8_t track_read_one(uint8_t ch)
{
    uint8_t level = (HAL_GPIO_ReadPin(TRACK_PORT, s_pin[ch]) == GPIO_PIN_SET) ? 1 : 0;

#if TRACK_ACTIVE_HIGH
    return level;
#else
    return (uint8_t)(!level);
#endif
}

void Track_Init(void)
{
    /* 引脚方向在 MX_GPIO_Init() 里已经配成输入上拉，这里不用重复配置，
     * 只读一次让内部状态有个初值 */
    (void)Track_ReadMask();
}

uint8_t Track_ReadMask(void)
{
    uint8_t i;
    uint8_t mask = 0;

    for (i = 0; i < TRACK_CH_COUNT; i++)
    {
        if (track_read_one(i) != 0)
        {
            mask |= (uint8_t)(1u << i);
        }
    }

    return mask;
}

int8_t Track_GetError(void)
{
    uint8_t mask = Track_ReadMask();
    int16_t sum  = 0;
    uint8_t cnt  = 0;
    uint8_t i;

    for (i = 0; i < TRACK_CH_COUNT; i++)
    {
        if ((mask & (uint8_t)(1u << i)) != 0)
        {
            /* 通道 0 → -7，通道 7 → +7，中心在 3.5，所以左右对称 */
            sum += (int16_t)i * 2 - 7;
            cnt++;
        }
    }

    if (cnt == 0)
    {
        return 0;           /* 一路都没压到，脱轨，由 Track_IsLost() 判断 */
    }

    return (int8_t)(sum / (int16_t)cnt);
}

uint8_t Track_IsLost(void)
{
    return (Track_ReadMask() == TRACK_MASK_NONE) ? 1 : 0;
}

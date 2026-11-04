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
    GPIO_InitTypeDef gi = {0};
    uint16_t pins = 0;
    uint8_t i;

    /* 引脚方向在这里自己配，不依赖 CubeMX 生成，
     * 以后换引脚只改 app_config.h 里的 TRACK_PORT / TRACK_PIN_LIST */
    for (i = 0; i < TRACK_CH_COUNT; i++)
    {
        pins |= s_pin[i];
    }

    gi.Pin  = pins;
    gi.Mode = GPIO_MODE_INPUT;
    gi.Pull = GPIO_PULLUP;      /* 模块没接时读高电平，不会乱跳 */
    HAL_GPIO_Init(TRACK_PORT, &gi);

    (void)Track_ReadMask();     /* 读一次，让内部状态有个初值 */
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
    int8_t  err;

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

    err = (int8_t)(sum / (int16_t)cnt);

#if TRACK_REVERSE
    err = (int8_t)(-err);   /* 模块装反了时用这个开关纠正符号 */
#endif

    return err;
}

uint8_t Track_IsLost(void)
{
    return (Track_ReadMask() == TRACK_MASK_NONE) ? 1 : 0;
}

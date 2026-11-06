/*
 * key.c - 4 个按键的扫描 + 消抖 + 边沿检测
 *
 * 思路：每 KEY_SCAN_MS 采样一次；电平变化后要稳定 KEY_DEBOUNCE_MS 才算数；
 *       只有"从不按下变成按下"的那一次才产生事件（长按不会连续触发）。
 */

#include "key.h"
#include "app_config.h"

/* 4 个按键的引脚表：顺序必须和 KeyId 对应 */
static const uint16_t s_key_pin[KEY_COUNT] = KEY_PIN_LIST;

static uint8_t  s_raw[KEY_COUNT];       /* 最近一次采样到的电平：1 = 按下 */
static uint8_t  s_stable[KEY_COUNT];    /* 消抖之后的稳定状态：1 = 按下 */
static uint32_t s_change_tick[KEY_COUNT];
static uint32_t s_scan_tick = 0;

void Key_Init(void)
{
    GPIO_InitTypeDef gi = {0};
    uint16_t pins = 0;
    uint8_t  i;

    /* 自己配引脚，不依赖 MX_GPIO_Init 里那批配置，
     * 以后换脚只改 app_config.h 的 KEY_PIN_LIST 就行 */
    for (i = 0; i < KEY_COUNT; i++)
    {
        pins |= s_key_pin[i];
    }
    gi.Pin  = pins;
    gi.Mode = GPIO_MODE_INPUT;
    gi.Pull = GPIO_PULLUP;              /* 按键另一头接 GND，没按的时候靠上拉拉高 */
    HAL_GPIO_Init(KEY_PORT, &gi);

    for (i = 0; i < KEY_COUNT; i++)
    {
        s_raw[i]         = 0;
        s_stable[i]      = 0;
        s_change_tick[i] = 0;
    }
    s_scan_tick = HAL_GetTick();
}

uint8_t Key_Scan(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t  press = KEY_ID_NONE;
    uint8_t  i;

    /* 还没到扫描周期，直接返回 */
    if ((now - s_scan_tick) < KEY_SCAN_MS)
    {
        return KEY_ID_NONE;
    }
    s_scan_tick = now;

    for (i = 0; i < KEY_COUNT; i++)
    {
        uint8_t raw = (HAL_GPIO_ReadPin(KEY_PORT, s_key_pin[i]) == KEY_PRESSED_LEVEL) ? 1 : 0;

        if (raw != s_raw[i])
        {
            /* 电平刚变，重新开始算稳定时间 */
            s_raw[i]         = raw;
            s_change_tick[i] = now;
        }
        else if ((now - s_change_tick[i]) >= KEY_DEBOUNCE_MS)
        {
            /* 已经稳定足够久，才承认这个状态 */
            if (s_stable[i] != raw)
            {
                s_stable[i] = raw;

                if (raw != 0)       /* 只报"按下"那一次 */
                {
                    press = (uint8_t)(i + 1);
                }
            }
        }
    }

    return press;
}

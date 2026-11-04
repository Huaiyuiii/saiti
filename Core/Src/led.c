/*
 * led.c - 两个 LED 的三种状态：常灭 / 常亮 / 交替闪烁
 *
 * 关键：闪烁用 HAL_GetTick() 做时间戳，绝不用 HAL_Delay()。
 *       因为 2b 要求连续闪 30 秒×3 轮不停，同时还要响应按键、刷新 OLED。
 */

#include "led.h"
#include "app_config.h"

/* 引脚输出电平：根据 LED_ACTIVE_LOW 换算 */
#if (LED_ACTIVE_LOW == 0)
  #define LED_ON_LEVEL      GPIO_PIN_SET
  #define LED_OFF_LEVEL     GPIO_PIN_RESET
#else
  #define LED_ON_LEVEL      GPIO_PIN_RESET
  #define LED_OFF_LEVEL     GPIO_PIN_SET
#endif

static LedMode  s_mode = LED_MODE_OFF;
static uint8_t  s_phase = 0;            /* 闪烁相位：0 = 红亮绿灭，1 = 红灭绿亮 */
static uint32_t s_blink_tick = 0;

/* 把两个灯的电平一次性写下去 */
static void led_write(uint8_t red_on, uint8_t green_on)
{
    HAL_GPIO_WritePin(LED_RED_PORT,   LED_RED_PIN,
                      red_on   ? LED_ON_LEVEL : LED_OFF_LEVEL);
    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN,
                      green_on ? LED_ON_LEVEL : LED_OFF_LEVEL);
}

void Led_Init(void)
{
    s_mode  = LED_MODE_OFF;
    s_phase = 0;
    led_write(0, 0);                    /* 上电先确保两个都灭 */
}

void Led_SetMode(LedMode mode)
{
    s_mode = mode;

    if (mode == LED_MODE_BLINK)
    {
        s_phase      = 0;
        s_blink_tick = HAL_GetTick();   /* 重新开始计时 */
    }
}

LedMode Led_GetMode(void)
{
    return s_mode;
}

void Led_Task(void)
{
    switch (s_mode)
    {
    case LED_MODE_ON:
        led_write(1, 1);
        break;

    case LED_MODE_BLINK:
        /* 到点就翻相位。用 += 累加而不是 = now，避免长时间运行后慢慢丢节拍 */
        if (HAL_GetTick() - s_blink_tick >= LED_BLINK_HALF_MS)
        {
            s_blink_tick += LED_BLINK_HALF_MS;
            s_phase = (uint8_t)(!s_phase);
        }
        /* 一个亮一个灭 —— 结构上就不可能"同时亮/同时灭" */
        led_write(s_phase, (uint8_t)(!s_phase));
        break;

    case LED_MODE_OFF:
    default:
        led_write(0, 0);
        break;
    }
}

uint8_t Led_IsRedOn(void)
{
    return (HAL_GPIO_ReadPin(LED_RED_PORT, LED_RED_PIN) == LED_ON_LEVEL) ? 1 : 0;
}

uint8_t Led_IsGreenOn(void)
{
    return (HAL_GPIO_ReadPin(LED_GREEN_PORT, LED_GREEN_PIN) == LED_ON_LEVEL) ? 1 : 0;
}

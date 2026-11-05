/*
 * beep.c - 蜂鸣器驱动（低电平触发模块，带三极管）
 *
 * 为什么不用 HAL_Delay 关断：
 *   主循环里同时要跑按键扫描和 OLED 刷新。如果 Beep_Beep(500) 直接
 *   HAL_Delay(500)，这半秒内按键完全没响应，菜单会"卡一下"。
 *   所以这里只记录起始时间戳，由 Beep_Task() 每轮检查是否到点。
 *
 * 三声序列（Beep_BeepN）也是同一个思路：用一个小状态机在
 *   响 -> 停 -> 响 之间切换，全程不阻塞。
 */

#include "beep.h"
#include "app_config.h"

/* 引脚输出电平：根据 BEEP_ACTIVE_LOW 换算 */
#if (BEEP_ACTIVE_LOW == 0)
  #define BEEP_ON_LEVEL     GPIO_PIN_SET       /* 高电平响 */
  #define BEEP_OFF_LEVEL    GPIO_PIN_RESET
#else
  #define BEEP_ON_LEVEL     GPIO_PIN_RESET     /* 低电平响 */
  #define BEEP_OFF_LEVEL    GPIO_PIN_SET
#endif

/* 自动序列的状态 */
typedef enum
{
    BEEP_IDLE = 0,      /* 空闲，什么都不做 */
    BEEP_SOUND,         /* 正在响 */
    BEEP_GAP            /* 两声之间的静音间隔 */
} BeepState;

static BeepState s_state    = BEEP_IDLE;
static uint16_t  s_on_ms    = 0;        /* 每声持续多久 */
static uint16_t  s_off_ms   = 0;        /* 两声之间停多久 */
static uint16_t  s_interval = 0;        /* 当前阶段的时长 */
static uint8_t   s_left     = 0;        /* 还剩几声没响 */
static uint32_t  s_tick     = 0;        /* 当前阶段的起始时间戳 */

/* 把电平写下去 */
static void beep_write(uint8_t on)
{
    HAL_GPIO_WritePin(BEEP_PORT, BEEP_PIN, on ? BEEP_ON_LEVEL : BEEP_OFF_LEVEL);
}

void Beep_Init(void)
{
    s_state    = BEEP_IDLE;
    s_left     = 0;
    s_interval = 0;
    s_tick     = 0;
    beep_write(0);                      /* 上电先确保不响 */
}

void Beep_On(void)
{
    s_state = BEEP_IDLE;                /* 手动模式打断自动序列 */
    s_left  = 0;
    beep_write(1);
}

void Beep_Off(void)
{
    s_state = BEEP_IDLE;
    s_left  = 0;
    beep_write(0);
}

void Beep_BeepN(uint8_t count, uint16_t on_ms, uint16_t off_ms)
{
    if ((count == 0) || (on_ms == 0))
    {
        Beep_Off();
        return;
    }

    s_on_ms    = on_ms;
    s_off_ms   = off_ms;
    s_interval = on_ms;
    s_left     = count;
    s_tick     = HAL_GetTick();
    s_state    = BEEP_SOUND;

    beep_write(1);                      /* 第一声立刻开始 */
}

void Beep_Beep(uint16_t ms)
{
    Beep_BeepN(1, ms, 0);
}

void Beep_Task(void)
{
    if (s_state == BEEP_IDLE)
    {
        return;
    }

    /* 还没到点 */
    if ((HAL_GetTick() - s_tick) < s_interval)
    {
        return;
    }

    /* 用 += 累加而不是 = now，避免长时间运行后节拍越飘越远 */
    s_tick += s_interval;

    if (s_state == BEEP_SOUND)
    {
        beep_write(0);
        s_left--;

        if (s_left == 0)
        {
            s_state = BEEP_IDLE;
            return;
        }

        /* 还有下一声，先停一会儿 */
        s_state    = BEEP_GAP;
        s_interval = s_off_ms;
    }
    else /* BEEP_GAP */
    {
        beep_write(1);
        s_state    = BEEP_SOUND;
        s_interval = s_on_ms;
    }
}

uint8_t Beep_IsBusy(void)
{
    return (s_state != BEEP_IDLE) ? 1 : 0;
}

/*
 * menu.c - 多级菜单 + LED 子界面
 *
 * 赛题 1a：上电显示主菜单，箭头默认第一行，按键1/2 移动箭头，至少 4 个菜单项
 * 赛题 1b：按键3 进入子菜单，按键4 返回主菜单
 * 赛题 2a：LED 子界面里 按键1 → 两个灯 常灭↔常亮
 * 赛题 2b：按键2 → 交替闪烁
 * 赛题 2c：按键4 返回主菜单，同时两个灯变常灭
 * 赛题 2d：子界面显示两个 LED 的真实亮灭状态
 */

#include "menu.h"
#include "app_config.h"
#include "led.h"
#include "key.h"
#include "oled.h"
#include "track.h"
#include "line.h"

#define MENU_ITEM_COUNT     4

/* 主菜单文字（字库只有 ASCII，所以用英文） */
static const char *const s_item_text[MENU_ITEM_COUNT] =
{
    "LED",      /* 对应 PAGE_LED  */
    "INFO",     /* 对应 PAGE_INFO */
    "LINE",     /* 对应 PAGE_LINE */
    "EXT"       /* 对应 PAGE_EXT  */
};

static PageId   s_page   = PAGE_MAIN;
static uint8_t  s_cursor = 0;           /* 主菜单箭头所在行，上电指第一行 */
static uint32_t s_draw_tick = 0;
static uint8_t  s_dirty = 1;            /* 1 = 下一轮必须重画 */

/* 变量 a：3a 显示，3b 上位机改，3c 上位机读 */
static int32_t  s_var_a = 0;

/* LINE 页面选定的圈数：1 = 4b，2 = 4c */
static uint8_t  s_laps_sel = LINE_DEFAULT_LAPS;

/* ---------------- 文字小工具 ---------------- */

static const char *mode_text(LedMode m)
{
    switch (m)
    {
    case LED_MODE_ON:    return "ON";
    case LED_MODE_BLINK: return "BLNK";
    default:             return "OFF";
    }
}

static const char *state_text(uint8_t on)
{
    return (on != 0) ? "ON " : "OFF";
}

/* ---------------- 各个页面 ---------------- */

static void draw_main(void)
{
    uint8_t i;

    OLED_Clear();
    for (i = 0; i < MENU_ITEM_COUNT; i++)
    {
        uint8_t y = (uint8_t)(i * 16);
        if (i == s_cursor)
        {
            OLED_ShowText(0, y, ">", 16);           /* 箭头 */
        }
        OLED_ShowText(16, y, s_item_text[i], 16);
    }
    OLED_Update();
}

static void draw_led(void)
{
    uint8_t red, green;

    /* 直接读引脚，保证"显示 = 实际" */
    red   = Led_IsRedOn();
    green = Led_IsGreenOn();

    OLED_Clear();
    OLED_ShowText(0,  0,  "LED MODE:", 16);
    OLED_ShowText(72, 0,  mode_text(Led_GetMode()), 16);

    OLED_ShowText(0,  16, "RED  :", 16);
    OLED_ShowText(48, 16, state_text(red), 16);

    OLED_ShowText(0,  32, "GREEN:", 16);
    OLED_ShowText(48, 32, state_text(green), 16);

    OLED_ShowText(0,  48, "K1:ON/OFF K2:BLINK", 8);
    OLED_ShowText(0,  56, "K4:BACK", 8);
    OLED_Update();
}

static void draw_info(void)
{
    OLED_Clear();
    OLED_ShowText(0, 0, "INFO", 16);

    OLED_ShowText(0,  16, "NAME:", 8);
    OLED_ShowText(36, 16, INFO_NAME, 8);

    OLED_ShowText(0,  24, "ID:", 8);
    OLED_ShowText(24, 24, INFO_STUDENT_ID, 8);

    OLED_ShowText(0,  32, "A=", 8);
    if (s_var_a < 0)                    /* 负数要单独画符号，OLED_ShowNum 只收无符号 */
    {
        OLED_ShowText(12, 32, "-", 8);
        OLED_ShowNum(18, 32, (uint32_t)(-s_var_a), 4, 8);
    }
    else
    {
        OLED_ShowNum(18, 32, (uint32_t)s_var_a, 4, 8);
    }

    OLED_ShowText(0,  56, "K4:BACK", 8);
    OLED_Update();
}

/* 在 (x,y) 处画 "秒.两位小数S"，例如 "12.34S"
 * 16px 字体每字符 8px 宽，8px 字体每字符 6px 宽，按 size 自动算位置 */
static void draw_time(uint8_t x, uint8_t y, uint32_t ms, uint8_t size)
{
    uint32_t sec  = ms / 1000u;
    uint32_t cent = (ms % 1000u) / 10u;
    uint8_t  w    = (size >= 16) ? 8 : 6;

    OLED_ShowNum(x, y, sec, 2, size);
    OLED_ShowText((uint8_t)(x + 2 * w), y, ".", size);

    if (cent < 10u)                     /* 补前导零，否则 5 会显示成 " 5" */
    {
        OLED_ShowText((uint8_t)(x + 3 * w), y, "0", size);
        OLED_ShowNum((uint8_t)(x + 4 * w), y, cent, 1, size);
    }
    else
    {
        OLED_ShowNum((uint8_t)(x + 3 * w), y, cent, 2, size);
    }

    OLED_ShowText((uint8_t)(x + 5 * w), y, "S", size);
}

static void draw_line(void)
{
    LineState st = Line_GetState();
    char      bits[TRACK_CH_COUNT + 1];
    uint8_t   mask;
    uint8_t   i;
    int8_t    err;

    /* ---------- 正在跑：只显示成绩相关的信息 ---------- */
    if (st == LINE_RUNNING)
    {
        OLED_Clear();
        OLED_ShowText(0, 0, "RUN  ", 16);
        OLED_ShowNum(40, 0, Line_GetLapDone() + 1, 1, 16);
        OLED_ShowText(48, 0, "/", 16);
        OLED_ShowNum(56, 0, Line_GetLapTarget(), 1, 16);

        draw_time(16, 20, Line_GetElapsedMs(), 16);

        OLED_ShowText(0, 40, "ERR:", 8);
        err = Line_GetError();
        OLED_ShowText(24, 40, (err < 0) ? "-" : "+", 8);
        OLED_ShowNum(30, 40, (uint32_t)((err < 0) ? -err : err), 1, 8);

        OLED_ShowText(48, 40, "M:", 8);
        mask = Line_GetMask();
        for (i = 0; i < TRACK_CH_COUNT; i++)
        {
            OLED_ShowText((uint8_t)(60 + i * 6), 40,
                          ((mask & (uint8_t)(1u << i)) != 0) ? "1" : "0", 8);
        }

        OLED_ShowText(0, 56, "K4:STOP", 8);
        OLED_Update();
        return;
    }

    /* ---------- 跑完了：定格成绩 ---------- */
    if (st == LINE_DONE)
    {
        OLED_Clear();
        OLED_ShowText(0, 0, "DONE", 16);
        draw_time(16, 20, Line_GetElapsedMs(), 16);

        OLED_ShowText(0, 40, "LAP ", 8);
        OLED_ShowNum(24, 40, Line_GetLapDone(), 1, 8);
        OLED_ShowText(30, 40, "/", 8);
        OLED_ShowNum(36, 40, Line_GetLapTarget(), 1, 8);

        OLED_ShowText(0, 52, "K3:AGAIN K4:BACK", 8);
        OLED_Update();
        return;
    }

    /* ---------- 待机：4a 传感器显示 + 发车提示 ---------- */
    mask = Track_ReadMask();
    for (i = 0; i < TRACK_CH_COUNT; i++)
    {
        bits[i] = ((mask & (uint8_t)(1u << i)) != 0) ? '1' : '0';
    }
    bits[TRACK_CH_COUNT] = '\0';

    OLED_Clear();
    OLED_ShowText(0, 0, "SENSOR", 16);
    OLED_ShowText(0, 16, bits, 16);

    OLED_ShowText(0, 36, "ERR:", 8);
    if (mask == TRACK_MASK_NONE)
    {
        OLED_ShowText(24, 36, "LOST", 8);
    }
    else
    {
        err = Track_GetError();
        OLED_ShowText(24, 36, (err < 0) ? "-" : "+", 8);
        OLED_ShowNum(30, 36, (uint32_t)((err < 0) ? -err : err), 1, 8);
    }

    OLED_ShowText(48, 36, "LAP:", 8);
    OLED_ShowNum(72, 36, s_laps_sel, 1, 8);

    OLED_ShowText(0, 48, "K3:START K1:LAP", 8);
    OLED_ShowText(0, 56, "K4:BACK", 8);
    OLED_Update();
}

static void draw_ext(void)
{
    OLED_Clear();
    OLED_ShowText(0, 0, "EXT", 16);
    OLED_ShowText(0, 16, "6.1 RANDOM MAP", 8);
    OLED_ShowText(0, 24, "6.2 PARAM SAVE", 8);
    OLED_ShowText(0, 32, "6.3 BLUETOOTH", 8);
    OLED_ShowText(0, 40, "6.4 SPEED", 8);
    OLED_ShowText(0, 56, "K4:BACK", 8);
    OLED_Update();
}

static void draw_page(void)
{
    switch (s_page)
    {
    case PAGE_LED:  draw_led();  break;
    case PAGE_INFO: draw_info(); break;
    case PAGE_LINE: draw_line(); break;
    case PAGE_EXT:  draw_ext();  break;
    case PAGE_MAIN:
    default:        draw_main(); break;
    }
}

/* ---------------- 按键处理 ---------------- */

static void handle_key(uint8_t key)
{
    switch (s_page)
    {
    case PAGE_MAIN:
        if (key == KEY_ID_1)            /* 上移，循环 */
        {
            s_cursor = (uint8_t)((s_cursor + MENU_ITEM_COUNT - 1) % MENU_ITEM_COUNT);
        }
        else if (key == KEY_ID_2)       /* 下移，循环 */
        {
            s_cursor = (uint8_t)((s_cursor + 1) % MENU_ITEM_COUNT);
        }
        else if (key == KEY_ID_3)       /* 进入子菜单 */
        {
            s_page = (PageId)(PAGE_LED + s_cursor);
        }
        break;

    case PAGE_LED:
        if (key == KEY_ID_1)            /* 2a：常灭 ↔ 常亮 */
        {
            Led_SetMode((Led_GetMode() == LED_MODE_ON) ? LED_MODE_OFF : LED_MODE_ON);
        }
        else if (key == KEY_ID_2)       /* 2b：交替闪烁 */
        {
            Led_SetMode(LED_MODE_BLINK);
        }
        else if (key == KEY_ID_4)       /* 2c：返回主菜单，灯必须灭 */
        {
            Led_SetMode(LED_MODE_OFF);
            s_page = PAGE_MAIN;
        }
        break;

    case PAGE_LINE:
    {
        LineState st = Line_GetState();

        if (st == LINE_RUNNING)
        {
            if (key == KEY_ID_4)            /* 跑动中：K4 手动停车 */
            {
                Line_Stop();
            }
        }
        else if (st == LINE_DONE)
        {
            if (key == KEY_ID_3)            /* 再来一次 */
            {
                Line_Start(s_laps_sel);
            }
            else if (key == KEY_ID_4)       /* 返回主菜单，同时清掉成绩 */
            {
                Line_Stop();
                Line_Init();
                s_page = PAGE_MAIN;
            }
        }
        else                                /* LINE_IDLE */
        {
            if (key == KEY_ID_3)            /* 4b/4c：发车 */
            {
                Line_Start(s_laps_sel);
            }
            else if (key == KEY_ID_1)       /* 切换 1 圈 / 2 圈 */
            {
                s_laps_sel = (s_laps_sel == 1) ? 2 : 1;
            }
            else if (key == KEY_ID_4)
            {
                s_page = PAGE_MAIN;
            }
        }
        break;
    }

    default:                            /* 其它子界面：按键4 返回 */
        if (key == KEY_ID_4)
        {
            s_page = PAGE_MAIN;
        }
        break;
    }
}

/* ---------------- 对外接口 ---------------- */

void Menu_Init(void)
{
    s_page      = PAGE_MAIN;
    s_cursor    = 0;
    s_dirty     = 1;
    s_draw_tick = HAL_GetTick();
    s_laps_sel  = LINE_DEFAULT_LAPS;
}

int32_t Menu_GetVarA(void)
{
    return s_var_a;
}

void Menu_SetVarA(int32_t v)
{
    s_var_a = v;
    s_dirty = 1;                        /* 值变了，下一轮立刻重画，不用等 100ms */
}

void Menu_Task(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t  key = Key_Scan();

    if (key != KEY_ID_NONE)
    {
        handle_key(key);
        s_dirty = 1;                    /* 状态变了，立刻重画 */
    }

    if ((s_dirty != 0) || ((now - s_draw_tick) >= MENU_REFRESH_MS))
    {
        s_draw_tick = now;
        s_dirty     = 0;
        draw_page();
    }
}

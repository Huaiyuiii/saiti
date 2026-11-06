#include "line.h"
#include "app_config.h"
#include "track.h"
#include "motor.h"
#include "beep.h"

/* =====================================================================
 * 循迹闭环
 *
 * 数据流：
 *   Track_ReadMask() → Track_GetError() → PD → 左右轮速度 → Motor_Set()
 *
 * 三个必须处理好的边界情况：
 *   1. 丢线（8 路全白）——不能直接停车，要沿上次方向找一小段线
 *   2. 起跑线（8 路全黑）——计圈依据，但发车瞬间车就压在线上，要屏蔽
 *   3. 十字路口（也是全黑）——和起跑线长得一样，靠 LINE_MARK_CONFIRM 滤掉瞬时干扰
 * ===================================================================== */

static LineState s_state = LINE_IDLE;

static uint8_t   s_laps_target = LINE_DEFAULT_LAPS;
static uint8_t   s_lap_done    = 0;

static uint32_t  s_start_tick    = 0;
static uint32_t  s_stop_tick     = 0;
static uint32_t  s_last_ctrl_tick = 0;
static uint32_t  s_arm_tick      = 0;   /* 到这个时刻之后才开始检测起跑线 */

static int8_t    s_prev_error      = 0; /* 上一周期的偏差，算微分用 */
static int8_t    s_last_good_error = 0; /* 最近一次有信号时的偏差，丢线时沿用 */
static int8_t    s_cur_error       = 0;
static uint8_t   s_cur_mask        = 0;

static uint8_t   s_lost_count = 0;
static uint16_t  s_mark_count = 0;

static uint8_t   s_ramping  = 0;
static uint32_t  s_ramp_end = 0;

/* ---------------- 对外接口 ---------------- */

void Line_Init(void)
{
    s_state        = LINE_IDLE;
    s_laps_target  = LINE_DEFAULT_LAPS;
    s_lap_done     = 0;
    s_start_tick   = 0;
    s_stop_tick    = 0;
    s_prev_error   = 0;
    s_last_good_error = 0;
    s_cur_error    = 0;
    s_cur_mask     = 0;
    s_lost_count   = 0;
    s_mark_count   = 0;
    s_ramping      = 0;
}

void Line_Start(uint8_t laps)
{
    uint32_t now = HAL_GetTick();

    s_laps_target = (laps == 0) ? 1 : laps;
    s_lap_done    = 0;

    s_prev_error      = 0;
    s_last_good_error = 0;
    s_cur_error       = 0;
    s_cur_mask        = Track_ReadMask();

    s_lost_count     = 0;
    s_mark_count     = 0;
    s_ramping        = 1;
    s_ramp_end       = now + LINE_START_RAMP_MS;
    s_start_tick     = now;
    s_last_ctrl_tick = now;
    s_arm_tick       = now + LINE_ARM_MS;

    s_state = LINE_RUNNING;

    /* 软启动：从 0 平滑爬到基准速度，压住启动电流
     * 实际推进由主循环的 Motor_Task() 负责，这里只登记目标 */
    Motor_SetRamp(MOTOR_BASE_SPEED, MOTOR_BASE_SPEED, LINE_START_RAMP_MS);

    /* 4b：发车提示音 */
    Beep_BeepN(BEEP_START_COUNT, BEEP_START_ON_MS, BEEP_START_OFF_MS);
}

void Line_Stop(void)
{
    if (s_state == LINE_RUNNING)
    {
        s_stop_tick = HAL_GetTick();
        s_state     = LINE_DONE;
    }
    Motor_Stop();
}

void Line_Task(void)
{
    uint32_t now;
    uint8_t  mask;
    int8_t   err;
    int32_t  turn, left, right;

    if (s_state != LINE_RUNNING) return;

    now = HAL_GetTick();
    if ((uint32_t)(now - s_last_ctrl_tick) < LINE_CTRL_MS) return;
    s_last_ctrl_tick = now;

    /* 软启动阶段：不做闭环，等速度爬上来再说。
     * 斜坡的推进交给主循环的 Motor_Task()，这里不要重复调用 */
    if (s_ramping)
    {
        if ((int32_t)(now - s_ramp_end) >= 0)
        {
            s_ramping = 0;
        }
        return;
    }

    mask       = Track_ReadMask();
    s_cur_mask = mask;
    err        = Track_GetError();

    /* ---- 丢线处理 ---- */
    if (mask == TRACK_MASK_NONE)
    {
        s_lost_count++;
        if (s_lost_count > LINE_LOST_MAX)
        {
            /* 找不回来了，停车并报警 */
            Line_Stop();
            Beep_BeepN(2, 200, 100);
            return;
        }
        /* 沿最后一次有效偏差的方向继续走，给车一个"找回来"的机会 */
        err = s_last_good_error;
    }
    else
    {
        s_lost_count      = 0;
        s_last_good_error = err;
    }
    s_cur_error = err;

    /* ---- PD 运算 ---- */
    /* P 项：偏差越大，修正越猛
     * D 项：偏差变化越快，反向修正越强，用来压住直道上的左右摇摆
     * error 为负表示黑线在左（车偏右），此时左轮减速右轮加速，车左转 */
    turn = (int32_t)LINE_KP * (int32_t)err
         + (int32_t)LINE_KD * (int32_t)(err - s_prev_error);
    s_prev_error = err;

    left  = (int32_t)MOTOR_BASE_SPEED + turn;
    right = (int32_t)MOTOR_BASE_SPEED - turn;

    if (left  >  MOTOR_FULL_SCALE) left  =  MOTOR_FULL_SCALE;
    if (left  < -MOTOR_FULL_SCALE) left  = -MOTOR_FULL_SCALE;
    if (right >  MOTOR_FULL_SCALE) right =  MOTOR_FULL_SCALE;
    if (right < -MOTOR_FULL_SCALE) right = -MOTOR_FULL_SCALE;

    Motor_Set((int16_t)left, (int16_t)right);

    /* ---- 计圈：检测起跑线（8 路全黑）---- */
    if ((int32_t)(now - s_arm_tick) >= 0)
    {
        if (mask == TRACK_MASK_ALL)
        {
            s_mark_count++;
        }
        else
        {
            s_mark_count = 0;       /* 中间断了一帧就重新计数，滤掉杂点 */
        }

        if (s_mark_count >= LINE_MARK_CONFIRM)
        {
            s_mark_count = 0;
            s_lap_done++;

            if (s_lap_done >= s_laps_target)
            {
                /* 跑完最后一圈：停车、定格成绩、响结束提示音 */
                Line_Stop();
                Beep_BeepN(3, 100, 80);
            }
            else
            {
                /* 4c：过第一圈只响一声，不停车 */
                Beep_Beep(80);
            }
        }
    }
}

/* ---------------- 查询接口 ---------------- */

LineState Line_GetState(void)
{
    return s_state;
}

uint32_t Line_GetElapsedMs(void)
{
    if (s_state == LINE_RUNNING) return (uint32_t)(HAL_GetTick() - s_start_tick);
    if (s_state == LINE_DONE)    return (uint32_t)(s_stop_tick  - s_start_tick);
    return 0;
}

uint8_t Line_GetLapDone(void)
{
    return s_lap_done;
}

uint8_t Line_GetLapTarget(void)
{
    return s_laps_target;
}

int8_t Line_GetError(void)
{
    return s_cur_error;
}

uint8_t Line_GetMask(void)
{
    return s_cur_mask;
}

#include "motor.h"
#include "app_config.h"

/* =====================================================================
 * TB6612FNG 真值表（每路独立，下面以 A 路为例）
 *
 *   STBY  AIN1  AIN2  PWMA     结果
 *    0     x     x     x       待机，输出关断
 *    1     0     0     x       滑行停止（输出浮空）
 *    1     1     0    PWM      正转，占空比 = PWM
 *    1     0     1    PWM      反转
 *    1     1     1     x       刹车（输出短路到地）
 *
 * STBY 现在是硬件直接接 3.3V，所以代码里只管 AIN1/AIN2/PWM。
 * ===================================================================== */

static TIM_HandleTypeDef s_htim3;

/* 缓存最近一次设定的速度，供 Motor_GetSpeed 和调试界面读取 */
static int16_t s_left  = 0;
static int16_t s_right = 0;

/* ---------------- 方向脚 ---------------- */

static void motor_dir_gpio_init(void)
{
    GPIO_InitTypeDef gi = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    gi.Mode  = GPIO_MODE_OUTPUT_PP;
    gi.Pull  = GPIO_NOPULL;
    gi.Speed = GPIO_SPEED_FREQ_HIGH;

    /* AIN1(PA8) AIN2(PA11) BIN1(PA12) 都在 GPIOA */
    gi.Pin = MOTOR_AIN1_PIN | MOTOR_AIN2_PIN | MOTOR_BIN1_PIN;
    HAL_GPIO_Init(MOTOR_AIN1_PORT, &gi);

    /* BIN2(PB0) 在 GPIOB */
    gi.Pin = MOTOR_BIN2_PIN;
    HAL_GPIO_Init(MOTOR_BIN2_PORT, &gi);

    /* 上电先全部拉低 = 两路都滑行，车不会一通电就窜出去 */
    HAL_GPIO_WritePin(MOTOR_AIN1_PORT, MOTOR_AIN1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR_AIN2_PORT, MOTOR_AIN2_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR_BIN1_PORT, MOTOR_BIN1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR_BIN2_PORT, MOTOR_BIN2_PIN, GPIO_PIN_RESET);

#if MOTOR_USE_STBY_PIN
    gi.Pin = MOTOR_STBY_PIN;
    HAL_GPIO_Init(MOTOR_STBY_PORT, &gi);
    HAL_GPIO_WritePin(MOTOR_STBY_PORT, MOTOR_STBY_PIN, GPIO_PIN_RESET);
#endif
}

/* ---------------- PWM ---------------- */

static void motor_pwm_init(void)
{
    TIM_OC_InitTypeDef oc = {0};
    GPIO_InitTypeDef   gi = {0};

    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA6 = TIM3_CH1，PA7 = TIM3_CH2，复用推挽 */
    gi.Pin   = MOTOR_PWMA_PIN | MOTOR_PWMB_PIN;
    gi.Mode  = GPIO_MODE_AF_PP;
    gi.Pull  = GPIO_NOPULL;
    gi.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(MOTOR_PWM_PORT, &gi);

    /* TIM3 挂在 APB1 上。APB1 分频 2，但定时器时钟会被硬件自动 ×2，
     * 所以 TIM3 实际还是 72MHz。
     * 72MHz / (PSC+1) / (ARR+1) = 72MHz / 1 / 3600 = 20kHz */
    s_htim3.Instance               = TIM3;
    s_htim3.Init.Prescaler         = 0;
    s_htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
    s_htim3.Init.Period            = MOTOR_PWM_ARR;
    s_htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    s_htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_PWM_Init(&s_htim3) != HAL_OK)
    {
        Error_Handler();
    }

    oc.OCMode     = TIM_OCMODE_PWM1;      /* 计数小于 CCR 时输出高 */
    oc.Pulse      = 0;                    /* 初始占空比 0 */
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;

    HAL_TIM_PWM_ConfigChannel(&s_htim3, &oc, TIM_CHANNEL_1);
    HAL_TIM_PWM_ConfigChannel(&s_htim3, &oc, TIM_CHANNEL_2);

    HAL_TIM_PWM_Start(&s_htim3, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&s_htim3, TIM_CHANNEL_2);
}

/* ---------------- 内部：驱动单路 ---------------- */

static void motor_drive_one(uint8_t ch, int16_t cmd)
{
    GPIO_TypeDef *port1, *port2;
    uint16_t      pin1,  pin2;
    uint32_t      tim_ch;
    uint16_t      mag, ccr;

    /* 超量程截断 */
    if (cmd >  MOTOR_FULL_SCALE) cmd =  MOTOR_FULL_SCALE;
    if (cmd < -MOTOR_FULL_SCALE) cmd = -MOTOR_FULL_SCALE;

    if (ch == 0)
    {
        port1  = MOTOR_AIN1_PORT;  pin1 = MOTOR_AIN1_PIN;
        port2  = MOTOR_AIN2_PORT;  pin2 = MOTOR_AIN2_PIN;
        tim_ch = TIM_CHANNEL_1;
    }
    else
    {
        port1  = MOTOR_BIN1_PORT;  pin1 = MOTOR_BIN1_PIN;
        port2  = MOTOR_BIN2_PORT;  pin2 = MOTOR_BIN2_PIN;
        tim_ch = TIM_CHANNEL_2;
    }

    /* 速度为 0：IN1=IN2=0，滑行。PWM 也清零，免得电机嗡嗡响还发热 */
    if (cmd == 0)
    {
        HAL_GPIO_WritePin(port1, pin1, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(port2, pin2, GPIO_PIN_RESET);
        __HAL_TIM_SET_COMPARE(&s_htim3, tim_ch, 0);
        return;
    }

    if (cmd > 0)                          /* 正转：IN1=1，IN2=0 */
    {
        HAL_GPIO_WritePin(port1, pin1, GPIO_PIN_SET);
        HAL_GPIO_WritePin(port2, pin2, GPIO_PIN_RESET);
        mag = (uint16_t)cmd;
    }
    else                                  /* 反转：IN1=0，IN2=1 */
    {
        HAL_GPIO_WritePin(port1, pin1, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(port2, pin2, GPIO_PIN_SET);
        mag = (uint16_t)(-cmd);
    }

    /* 死区补偿：占空比太低电机不转，只发热。
     * 只要不是 0，就至少给到 MOTOR_DEADZONE */
    if (mag < MOTOR_DEADZONE)
    {
        mag = MOTOR_DEADZONE;
    }

    /* 把 0~1000 映射到 0~3600（ARR+1），3600 对应 100% */
    ccr = (uint16_t)(((uint32_t)mag * (MOTOR_PWM_ARR + 1)) / MOTOR_FULL_SCALE);
    __HAL_TIM_SET_COMPARE(&s_htim3, tim_ch, ccr);
}

/* ---------------- 对外接口 ---------------- */

void Motor_Init(void)
{
    motor_dir_gpio_init();
    motor_pwm_init();

    s_left  = 0;
    s_right = 0;

#if MOTOR_USE_STBY_PIN
    HAL_GPIO_WritePin(MOTOR_STBY_PORT, MOTOR_STBY_PIN, GPIO_PIN_SET);
#endif
}

void Motor_Set(int16_t left, int16_t right)
{
    s_left  = left;
    s_right = right;

    motor_drive_one(0, left);
    motor_drive_one(1, right);
}

void Motor_Stop(void)
{
    Motor_Set(0, 0);
}

void Motor_Brake(void)
{
    /* IN1=IN2=1 同时成立 = 输出短路制动。
     * 不走 Motor_Set，因为那个函数会把 0 解释成滑行 */
    HAL_GPIO_WritePin(MOTOR_AIN1_PORT, MOTOR_AIN1_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(MOTOR_AIN2_PORT, MOTOR_AIN2_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(MOTOR_BIN1_PORT, MOTOR_BIN1_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(MOTOR_BIN2_PORT, MOTOR_BIN2_PIN, GPIO_PIN_SET);

    __HAL_TIM_SET_COMPARE(&s_htim3, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(&s_htim3, TIM_CHANNEL_2, 0);

    s_left  = 0;
    s_right = 0;
}

void Motor_GetSpeed(int16_t *left, int16_t *right)
{
    if (left  != 0) *left  = s_left;
    if (right != 0) *right = s_right;
}

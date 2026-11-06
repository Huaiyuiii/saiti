/*
 * usart.c - USART1 串口通信（基础部分 3b / 3c）
 *
 * 赛题 3b：上位机发数字 → 变量 a 随之变化
 * 赛题 3c：上位机发 "GET A" → 返回当前 a 的值
 *
 * 设计要点：
 * 1. 接收用中断逐字节收，收到 \r 或 \n 算一帧结束。
 *    如果对方不发换行，静默 USART_FRAME_IDLE_MS 毫秒后也当一帧结束，
 *    这样串口助手勾不勾"发送新行"都能用。
 * 2. 解析在主循环 Usart_Task() 里做，中断里只搬字节，不干活。
 * 3. 不用 printf —— 那要在 Keil 里勾 MicroLIB，容易忘。
 *    自己写 fmt_int()，不依赖任何库。
 */

#include "usart.h"
#include "app_config.h"
#include "menu.h"
#include <string.h>

UART_HandleTypeDef huart1;

/* ---------------- 接收缓冲 ---------------- */

static char             s_rx_buf[USART_RX_BUF_SIZE];
static volatile uint8_t s_rx_len;        /* 当前这一帧已经收了多少字节 */
static volatile uint8_t s_rx_ready;      /* 1 = 已经凑齐一帧，等主循环处理 */
static volatile uint32_t s_rx_tick;      /* 最后一个字节到达的时刻 */
static uint8_t          s_rx_byte;       /* HAL 逐字节接收的落脚点 */

/* ---------------- 小工具 ---------------- */

/* 整数转字符串，返回长度。不依赖 sprintf */
static uint8_t fmt_int(char *dst, int32_t v)
{
    char     tmp[12];
    uint8_t  n = 0, i = 0;
    uint32_t u;

    if (v < 0)
    {
        dst[n++] = '-';
        u = (uint32_t)(-v);
    }
    else
    {
        u = (uint32_t)v;
    }

    do
    {
        tmp[i++] = (char)('0' + (u % 10u));
        u /= 10u;
    } while (u != 0);

    while (i > 0)
    {
        dst[n++] = tmp[--i];
    }
    dst[n] = '\0';
    return n;
}

/* 忽略大小写比较两个字符串（POSIX 的 strcasecmp 在 ARMCC 里不一定有） */
static uint8_t str_eq_ci(const char *a, const char *b)
{
    while ((*a != '\0') && (*b != '\0'))
    {
        char ca = *a;
        char cb = *b;
        if ((ca >= 'a') && (ca <= 'z')) ca = (char)(ca - 'a' + 'A');
        if ((cb >= 'a') && (cb <= 'z')) cb = (char)(cb - 'a' + 'A');
        if (ca != cb) return 0;
        a++;
        b++;
    }
    return ((*a == '\0') && (*b == '\0')) ? 1 : 0;
}

/* 把整串解析成整数。成功返回 1，失败返回 0。
 * 只接受可选的 +/- 号加纯数字，中间夹字母就算失败 */
static uint8_t parse_int(const char *s, int32_t *out)
{
    int32_t v      = 0;
    uint8_t neg    = 0;
    uint8_t digits = 0;

    while (*s == ' ') s++;

    if ((*s == '+') || (*s == '-'))
    {
        neg = (uint8_t)(*s == '-');
        s++;
    }

    while ((*s >= '0') && (*s <= '9'))
    {
        v = v * 10 + (int32_t)(*s - '0');
        if (v > 99999) return 0;        /* 超范围，别让 int32 溢出 */
        s++;
        digits++;
    }

    while (*s == ' ') s++;

    if ((digits == 0) || (*s != '\0')) return 0;

    *out = (neg != 0) ? -v : v;
    return 1;
}

/* ---------------- 命令解析 ---------------- */

static void usart_handle_line(char *line)
{
    char    out[24];
    char   *p   = line;
    char   *end;
    int32_t v;
    uint8_t n;

    /* 去掉首尾空格 */
    while ((*p == ' ') || (*p == '\t')) p++;
    end = p;
    while (*end != '\0') end++;
    while ((end > p) && ((end[-1] == ' ') || (end[-1] == '\t'))) end--;
    *end = '\0';

    if (*p == '\0') return;             /* 空帧，忽略（比如只按了个回车） */

    /* 3c：查询变量 a */
    if (str_eq_ci(p, "GET A") || str_eq_ci(p, "GETA") || str_eq_ci(p, "GET"))
    {
        out[0] = 'A';
        out[1] = '=';
        n = (uint8_t)(2u + fmt_int(&out[2], Menu_GetVarA()));
        out[n++] = '\r';
        out[n++] = '\n';
        out[n]   = '\0';
        Usart_SendString(out);
        return;
    }

    /* 连通性测试，第一次接线时用这个确认串口通了 */
    if (str_eq_ci(p, "PING"))
    {
        Usart_SendLine("OK");
        return;
    }

    if (str_eq_ci(p, "?") || str_eq_ci(p, "HELP"))
    {
        Usart_SendLine("CMD: <number> | GET A | PING");
        return;
    }

    /* 3b：发数字 → 改变量 a */
    if (parse_int(p, &v) != 0)
    {
        if ((v < USART_VAR_A_MIN) || (v > USART_VAR_A_MAX))
        {
            Usart_SendLine("ERR RANGE");
            return;
        }

        Menu_SetVarA(v);

        out[0] = 'A';
        out[1] = '=';
        n = (uint8_t)(2u + fmt_int(&out[2], v));
        out[n++] = '\r';
        out[n++] = '\n';
        out[n]   = '\0';
        Usart_SendString(out);
        return;
    }

    Usart_SendLine("ERR");
}

/* ---------------- 初始化 ---------------- */

/* HAL 在 HAL_UART_Init() 内部回调这个函数，用来配引脚和中断。
 * 放在这里而不是 stm32f1xx_hal_msp.c，是为了让串口相关的东西都集中在一个文件。
 * 以后加 USART2 接蓝牙时，在这里按 huart->Instance 分支加就行 */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef gi = {0};

    if (huart->Instance != USART1) return;

    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA9 = TX，复用推挽输出 */
    gi.Pin   = GPIO_PIN_9;
    gi.Mode  = GPIO_MODE_AF_PP;
    gi.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gi);

    /* PA10 = RX，浮空输入（内部上拉会和模块输出打架，这里用浮空） */
    gi.Pin  = GPIO_PIN_10;
    gi.Mode = GPIO_MODE_INPUT;
    gi.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gi);

    HAL_NVIC_SetPriority(USART1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

void Usart_Init(void)
{
    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = USART_BAUDRATE;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        Error_Handler();
    }

    s_rx_len   = 0;
    s_rx_ready = 0;
    s_rx_tick  = HAL_GetTick();

    /* 开中断接收，从此每来一个字节进一次 HAL_UART_RxCpltCallback */
    HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1);

    Usart_SendLine("READY");
}

/* ---------------- 接收中断 ---------------- */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    uint8_t b;

    if (huart->Instance != USART1) return;

    b          = s_rx_byte;
    s_rx_tick  = HAL_GetTick();

    if ((b == '\r') || (b == '\n'))
    {
        if (s_rx_len > 0)
        {
            s_rx_ready = 1;             /* 凑齐一帧 */
        }
    }
    else if (s_rx_ready != 0)
    {
        /* 上一帧还没被主循环取走，这一帧直接丢弃，别把缓冲搞乱 */
    }
    else if (s_rx_len < (uint8_t)(USART_RX_BUF_SIZE - 1))
    {
        s_rx_buf[s_rx_len] = (char)b;
        s_rx_len++;
    }
    else
    {
        s_rx_len = 0;                   /* 太长，丢掉重来 */
    }

    HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1);
}

/* ---------------- 发送 ---------------- */

void Usart_SendString(const char *s)
{
    uint16_t len = (uint16_t)strlen(s);
    if (len == 0) return;
    HAL_UART_Transmit(&huart1, (const uint8_t *)s, len, USART_TX_TIMEOUT_MS);
}

void Usart_SendLine(const char *s)
{
    Usart_SendString(s);
    Usart_SendString("\r\n");
}

/* ---------------- 主循环任务 ---------------- */

void Usart_Task(void)
{
    char    line[USART_RX_BUF_SIZE];
    uint8_t len;

    if (s_rx_ready == 0)
    {
        /* 没收到换行，但对方已经停手了，也当成一帧 */
        if (s_rx_len == 0) return;
        if ((HAL_GetTick() - s_rx_tick) < USART_FRAME_IDLE_MS) return;
    }

    __disable_irq();
    len = s_rx_len;
    if (len > 0)
    {
        memcpy(line, s_rx_buf, len);
    }
    line[len] = '\0';
    s_rx_len   = 0;
    s_rx_ready = 0;
    __enable_irq();

    if (len == 0) return;

    usart_handle_line(line);
}

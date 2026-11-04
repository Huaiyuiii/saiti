/*
 * oled.c - SSD1306 0.96 寸 OLED 驱动（128x64）
 *
 * 底层通信：软件 I2C，SCL = PB6，SDA = PB7，两根都配成开漏输出。
 * 只用 BSRR / BRR 置位复位，比 HAL_GPIO_WritePin 快，soft I2C 才跑得起来。
 *
 * 为什么不用硬件 I2C1：本工程 PB6/PB7 配的是普通 GPIO，没有开 I2C1 外设。
 * 软件 I2C 只依赖引脚，工程配置不用改。
 */

#include "oled.h"
#include "app_config.h"
#include "font8x16.h"
#include "font6x8.h"
#include <string.h>

/* 帧缓冲：按页排列，8 行 1 页，共 8 页 * 128 列 */
static uint8_t gImage[OLED_WIDTH * OLED_HEIGHT / 8];

/*-------------------- 底层：软件 I2C --------------------*/

/* BSRR 低 16 位置 1 = 输出高；BRR 对应位置 1 = 输出低。
 * 开漏输出下"输出高"等于把线放开，由上拉电阻拉高。 */
#define SCL_H()     (OLED_SCL_PORT->BSRR = OLED_SCL_PIN)
#define SCL_L()     (OLED_SCL_PORT->BRR  = OLED_SCL_PIN)
#define SDA_H()     (OLED_SDA_PORT->BSRR = OLED_SDA_PIN)
#define SDA_L()     (OLED_SDA_PORT->BRR  = OLED_SDA_PIN)

static void oled_i2c_delay(void)
{
    volatile uint32_t n = OLED_I2C_DELAY_LOOPS;
    while (n--)
    {
        /* 空循环，靠它产生 I2C 的时序间隔 */
    }
}

static void oled_i2c_start(void)
{
    SDA_H(); SCL_H(); oled_i2c_delay();     /* 空闲时两根线都是高 */
    SDA_L();          oled_i2c_delay();     /* SCL 高时 SDA 下降 = 起始 */
    SCL_L();          oled_i2c_delay();
}

static void oled_i2c_stop(void)
{
    SDA_L();          oled_i2c_delay();
    SCL_H();          oled_i2c_delay();
    SDA_H();          oled_i2c_delay();     /* SCL 高时 SDA 上升 = 停止 */
}

static void oled_i2c_write_byte(uint8_t byte)
{
    uint8_t i;

    for (i = 0; i < 8; i++)
    {
        if ((byte & 0x80) != 0)
        {
            SDA_H();
        }
        else
        {
            SDA_L();
        }
        byte = (uint8_t)(byte << 1);
        oled_i2c_delay();

        SCL_H(); oled_i2c_delay();          /* 从机在这时把数据读走 */
        SCL_L(); oled_i2c_delay();
    }

    /* 第 9 个时钟：应答位。只发时钟不读结果，SSD1306 不需要主机检查 ACK */
    SDA_H();          oled_i2c_delay();
    SCL_H();          oled_i2c_delay();
    SCL_L();          oled_i2c_delay();
}

static void OLED_WriteCmd(uint8_t cmd)
{
    oled_i2c_start();
    oled_i2c_write_byte(OLED_I2C_ADDR);
    oled_i2c_write_byte(0x00);              /* 控制字节：后面是命令 */
    oled_i2c_write_byte(cmd);
    oled_i2c_stop();
}

static void OLED_WriteData(uint8_t *data, uint16_t len)
{
    uint16_t i;

    oled_i2c_start();
    oled_i2c_write_byte(OLED_I2C_ADDR);
    oled_i2c_write_byte(0x40);              /* 控制字节：后面是显示数据 */
    for (i = 0; i < len; i++)
    {
        oled_i2c_write_byte(data[i]);
    }
    oled_i2c_stop();
}

/*-------------------- 对外接口 --------------------*/

void OLED_Clear(void)
{
    memset(gImage, 0, sizeof(gImage));
}

void OLED_Update(void)
{
    uint8_t page;

    for (page = 0; page < (OLED_HEIGHT / 8); page++)
    {
        OLED_WriteCmd((uint8_t)(0xB0 + page));   /* 设置页地址 */
        OLED_WriteCmd(0x00);                     /* 列地址低 4 位 = 0 */
        OLED_WriteCmd(0x10);                     /* 列地址高 4 位 = 0 */
        OLED_WriteData(&gImage[page * OLED_WIDTH], OLED_WIDTH);
    }
}

static void OLED_GpioInit(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();   /* SCL/SDA 若改到别的端口，这里要一并开对应时钟 */

    HAL_GPIO_WritePin(OLED_SCL_PORT, OLED_SCL_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(OLED_SDA_PORT, OLED_SDA_PIN, GPIO_PIN_SET);

    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;   /* I2C 必须开漏 */
    GPIO_InitStruct.Pull  = GPIO_PULLUP;           /* 模块自带 4.7k，这里再兜一层 */
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

    GPIO_InitStruct.Pin = OLED_SCL_PIN;
    HAL_GPIO_Init(OLED_SCL_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = OLED_SDA_PIN;
    HAL_GPIO_Init(OLED_SDA_PORT, &GPIO_InitStruct);
}

void OLED_Init(void)
{
    OLED_GpioInit();
    HAL_Delay(100);

    OLED_WriteCmd(0xAE);   /* 关闭显示 */
    OLED_WriteCmd(0x20);   /* 设置内存寻址模式 */
    OLED_WriteCmd(0x02);   /* 页寻址：后面 0xB0/0x00/0x10 才有效 */
    OLED_WriteCmd(0xB0);   /* 设置页起始地址 */
    OLED_WriteCmd(0xC8);   /* COM 扫描方向翻转 */
    OLED_WriteCmd(0x00);   /* 设置列地址低 4 位 */
    OLED_WriteCmd(0x10);   /* 设置列地址高 4 位 */
    OLED_WriteCmd(0x40);   /* 设置显示起始行 */
    OLED_WriteCmd(0x81);   /* 设置对比度 */
    OLED_WriteCmd(0xCF);
    OLED_WriteCmd(0xA1);   /* 段重映射 */
    OLED_WriteCmd(0xA6);   /* 正常显示（非反白） */
    OLED_WriteCmd(0xA8);   /* 设置多路复用比 */
    OLED_WriteCmd(0x3F);
    OLED_WriteCmd(0xA4);   /* 显示全部像素 */
    OLED_WriteCmd(0xD3);   /* 设置显示偏移 */
    OLED_WriteCmd(0x00);
    OLED_WriteCmd(0xD5);   /* 设置时钟分频 */
    OLED_WriteCmd(0x80);
    OLED_WriteCmd(0xD9);   /* 设置预充电周期 */
    OLED_WriteCmd(0xF1);
    OLED_WriteCmd(0xDA);   /* 设置 COM 引脚硬件配置 */
    OLED_WriteCmd(0x12);
    OLED_WriteCmd(0xDB);   /* 设置 VCOMH 取消选择电平 */
    OLED_WriteCmd(0x40);
    OLED_WriteCmd(0x8D);   /* 使能电荷泵 */
    OLED_WriteCmd(0x14);
    OLED_WriteCmd(0xAF);   /* 打开显示 */

    OLED_Clear();
    OLED_Update();
}

void OLED_DisPlayOn(void)
{
    OLED_WriteCmd(0xAF);
}

void OLED_DisPlayOff(void)
{
    OLED_WriteCmd(0xAE);
}

/*
 * 显示字符串
 *   x, y    : 起始坐标（像素），y 是 0~63
 *   str     : 字符串
 *   size    : 字号，>=16 用 8x16 字库，否则用 6x8 字库
 */
void OLED_ShowString(uint8_t x, uint8_t y, uint8_t *str, uint8_t size)
{
    const unsigned char *font;
    uint8_t charW, charH;
    uint8_t code, row, col, bits, px, py;

    if (size >= 16)
    {
        font  = gFont_8X16;
        charW = FONT_8X16_W;
        charH = FONT_8X16_H;
    }
    else
    {
        font  = gFont_6X8;
        charW = FONT_6X8_W;
        charH = FONT_6X8_H;
    }

    if (str == 0)
    {
        return;
    }

    while (*str != '\0')
    {
        code = *str;
        if (code < FONT_8X16_FIRST || code > FONT_8X16_LAST)
        {
            code = ' ';
        }

        for (row = 0; row < charH; row++)
        {
            bits = font[(uint8_t)(code - FONT_8X16_FIRST) * charH + row];
            py = (uint8_t)(y + row);
            if (py >= OLED_HEIGHT)
            {
                break;
            }

            for (col = 0; col < charW; col++)
            {
                if ((bits & (1 << (7 - col))) == 0)
                {
                    continue;
                }
                px = (uint8_t)(x + col);
                if (px >= OLED_WIDTH)
                {
                    continue;
                }
                /* 帧缓冲按 SSD1306 的页格式排列：
                 *   字节下标 = 页号 * 128 + 列号(px)   ← 一个字节管一整列的 8 个像素
                 *   位       = py % 8                  ← bit0 是页内最上面那一行
                 * 写成 px/8 和 7-(px%8) 会把整幅图横向压扁成 1/8，屏幕上就是一堆竖条 */
                gImage[(py / 8) * OLED_WIDTH + px] |= (uint8_t)(1 << (py % 8));
            }
        }

        x = (uint8_t)(x + charW);
        str++;
    }
}

/*
 * 显示数字
 *   num : 要显示的数
 *   len : 位数（不足前面补空格）
 */
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size)
{
    uint8_t tbuf[12];
    uint8_t i = 0;
    uint8_t charW;

    do
    {
        tbuf[i++] = (uint8_t)('0' + (num % 10));
        num /= 10;
    } while ((num != 0) && (i < 10));

    while (i < len)
    {
        tbuf[i++] = ' ';
    }

    charW = (size >= 16) ? FONT_8X16_W : FONT_6X8_W;

    for (i = 0; i < len; i++)
    {
        uint8_t ch = tbuf[len - 1 - i];
        uint8_t str1[2];

        str1[0] = ch;
        str1[1] = '\0';
        OLED_ShowString((uint8_t)(x + i * charW), y, str1, size);
    }
}

/*
 * 字符串显示（const 版本）
 *   OLED_ShowString 的参数是 uint8_t *，每次调用都要强转，很难看。
 *   这里包一层，menu.c 里就可以直接写 OLED_ShowText(0, 0, "LED", 16);
 */
void OLED_ShowText(uint8_t x, uint8_t y, const char *str, uint8_t size)
{
    OLED_ShowString(x, y, (uint8_t *)str, size);
}

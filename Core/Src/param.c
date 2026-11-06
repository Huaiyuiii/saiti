#include "param.h"
#include "app_config.h"

/* Flash 最后一页：0x0800FC00 ~ 0x0800FFFF，共 1KB */
#define PARAM_FLASH_ADDR        0x0800FC00u

/* "PARA" 的 ASCII，用来判断这一页是不是我们写过的 */
#define PARAM_MAGIC             0x50415241u

static Param_t s_param;

/* 校验和：把 sum 字段之前的字节全部累加。
 * 不用 CRC32 是因为这里只有十几个字节，累加和足够发现"没写过"和"写坏了"，
 * 而且代码短，答辩时好讲 */
static uint32_t param_checksum(const Param_t *p)
{
    const uint8_t *b = (const uint8_t *)p;
    uint16_t i;
    uint32_t sum = 0;

    for (i = 0; i < (uint16_t)(sizeof(Param_t) - sizeof(uint32_t)); i++)
    {
        sum += b[i];
    }
    return sum;
}

/* 把参数限制在合理范围。Flash 里可能是乱码，不能直接信 */
static void param_clamp(void)
{
    if ((s_param.kp < 5) || (s_param.kp > 200))          s_param.kp         = LINE_KP;
    if ((s_param.kd < 0) || (s_param.kd > 100))          s_param.kd         = LINE_KD;
    if ((s_param.base_speed < 100) ||
        (s_param.base_speed > MOTOR_FULL_SCALE))         s_param.base_speed = MOTOR_BASE_SPEED;
    if ((s_param.laps < 1) || (s_param.laps > 5))        s_param.laps       = LINE_DEFAULT_LAPS;
    if (s_param.beep_on > 1)                             s_param.beep_on    = 1;
}

void Param_Init(void)
{
    const Param_t *p = (const Param_t *)PARAM_FLASH_ADDR;

    if ((p->magic == PARAM_MAGIC) && (p->sum == param_checksum(p)))
    {
        s_param = *p;
        param_clamp();
    }
    else
    {
        /* Flash 是空的（新片子）或者校验不过（写过但坏了），用默认值。
         * 注意这里不写 Flash，避免每次上电都擦一次 */
        Param_Reset();
    }
}

void Param_Save(void)
{
    FLASH_EraseInitTypeDef er = {0};
    uint32_t page_err = 0;
    uint32_t addr     = PARAM_FLASH_ADDR;
    const uint16_t *src;
    uint16_t halfwords, i;

    param_clamp();

    s_param.magic = PARAM_MAGIC;
    s_param.sum   = param_checksum(&s_param);

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);

    er.TypeErase   = FLASH_TYPEERASE_PAGES;
    er.PageAddress = PARAM_FLASH_ADDR;
    er.NbPages     = 1;

    if (HAL_FLASHEx_Erase(&er, &page_err) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return;
    }

    /* 按半字（16 位）写入。结构体大小是 20 字节，正好 10 个半字 */
    src       = (const uint16_t *)&s_param;
    halfwords = (uint16_t)((sizeof(Param_t) + 1u) / 2u);

    for (i = 0; i < halfwords; i++)
    {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr, src[i]) != HAL_OK)
        {
            break;
        }
        addr += 2u;
    }

    HAL_FLASH_Lock();
}

void Param_Reset(void)
{
    s_param.magic      = PARAM_MAGIC;
    s_param.kp         = LINE_KP;
    s_param.kd         = LINE_KD;
    s_param.base_speed = MOTOR_BASE_SPEED;
    s_param.laps       = LINE_DEFAULT_LAPS;
    s_param.beep_on    = 1;
    s_param.reserved   = 0;
    s_param.sum        = param_checksum(&s_param);
}

Param_t *Param_Get(void)
{
    return &s_param;
}

/*!
    \file    device_init.c
    \brief   Device initialization implementation
    
    \version 2025-08-29, V1.0.0, PMU project for GD32F4xx
*/

#include "device_init.h"
#include <stdio.h>

/* =================== 私有变量 =================== */
static uint32_t g_usart_periph = USART0;  /* 当前使用的串口 */

/* =================== 私有函数声明 =================== */
static uint8_t clock_init_irc16m(void);
static uint8_t clock_init_irc16m_pll72m(void);
static uint8_t clock_init_hxtal_pll168m(const clock_config_t *config);
static void _usart_send_char(char ch);

/* =================== 公共函数实现 =================== */

/*!
    \brief      获取默认时钟配置
    \param[out] config: 时钟配置结构体指针
    \param[in]  none
    \retval     none
*/
void device_get_default_clock_config(clock_config_t *config)
{
    if (config == NULL) return;
    
    config->source = CLOCK_SRC_IRC16M;      /* 默认使用内部16MHz */
    config->hxtal_freq = 8000000;           /* 8MHz外部晶振 */
    config->timeout_ms = 1000;              /* 1秒超时 */
}

/*!
    \brief      系统时钟初始化
    \param[in]  config: 时钟配置结构体指针
    \param[out] none
    \retval     DEVICE_INIT_OK: 初始化成功
                DEVICE_INIT_ERROR: 初始化失败
*/
uint8_t device_clock_init(const clock_config_t *config)
{
    if (config == NULL) return DEVICE_INIT_ERROR;
    
    uint8_t result = DEVICE_INIT_ERROR;
    
    switch (config->source) {
        case CLOCK_SRC_IRC16M:
            result = clock_init_irc16m();
            break;
            
        case CLOCK_SRC_IRC16M_PLL72M:
            result = clock_init_irc16m_pll72m();
            break;
            
        case CLOCK_SRC_HXTAL_PLL168M:
            result = clock_init_hxtal_pll168m(config);
            break;
            
        default:
            result = DEVICE_INIT_ERROR;
            break;
    }
    
    if (result == DEVICE_INIT_OK) {
        /* 更新系统时钟频率变量 */
        SystemCoreClockUpdate();
        
        /* 短暂延时让时钟稳定 */
        device_soft_delay(1000);
    }
    
    return result;
}

/*!
    \brief      获取默认串口配置
    \param[out] config: 串口配置结构体指针
    \param[in]  none
    \retval     none
*/
void device_get_default_usart_config(usart_config_t *config)
{
    if (config == NULL) return;
    
    config->usart_periph = USART0;
    config->baudrate = 115200;
    config->word_length = USART_WL_8BIT;
    config->stop_bit = USART_STB_1BIT;
    config->parity = USART_PM_NONE;
    
    /* USART0: PA9(TX), PA10(RX) */
    config->gpio_periph = GPIOA;
    config->tx_pin = GPIO_PIN_9;
    config->rx_pin = GPIO_PIN_10;
    config->gpio_af = GPIO_AF_7;
}

/*!
    \brief      串口初始化
    \param[in]  config: 串口配置结构体指针
    \param[out] none
    \retval     DEVICE_INIT_OK: 初始化成功
                DEVICE_INIT_ERROR: 初始化失败
*/
uint8_t device_usart_init(const usart_config_t *config)
{
    if (config == NULL) return DEVICE_INIT_ERROR;
    
    /* 保存当前使用的串口 */
    g_usart_periph = config->usart_periph;
    
    /* 使能GPIO和USART时钟 */
    if (config->gpio_periph == GPIOA) {
        rcu_periph_clock_enable(RCU_GPIOA);
    } else if (config->gpio_periph == GPIOB) {
        rcu_periph_clock_enable(RCU_GPIOB);
    } else if (config->gpio_periph == GPIOC) {
        rcu_periph_clock_enable(RCU_GPIOC);
    }
    
    if (config->usart_periph == USART0) {
        rcu_periph_clock_enable(RCU_USART0);
    } else if (config->usart_periph == USART1) {
        rcu_periph_clock_enable(RCU_USART1);
    } else if (config->usart_periph == USART2) {
        rcu_periph_clock_enable(RCU_USART2);
    }
    
    /* 配置GPIO */
    gpio_af_set(config->gpio_periph, config->gpio_af, config->tx_pin);
    gpio_af_set(config->gpio_periph, config->gpio_af, config->rx_pin);
    
    gpio_mode_set(config->gpio_periph, GPIO_MODE_AF, GPIO_PUPD_PULLUP, config->tx_pin);
    gpio_output_options_set(config->gpio_periph, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, config->tx_pin);
    
    gpio_mode_set(config->gpio_periph, GPIO_MODE_AF, GPIO_PUPD_PULLUP, config->rx_pin);
    gpio_output_options_set(config->gpio_periph, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, config->rx_pin);
    
    /* 配置USART */
    usart_deinit(config->usart_periph);
    usart_baudrate_set(config->usart_periph, config->baudrate);
    usart_word_length_set(config->usart_periph, config->word_length);
    usart_stop_bit_set(config->usart_periph, config->stop_bit);
    usart_parity_config(config->usart_periph, config->parity);
    usart_hardware_flow_rts_config(config->usart_periph, USART_RTS_DISABLE);
    usart_hardware_flow_cts_config(config->usart_periph, USART_CTS_DISABLE);
    usart_receive_config(config->usart_periph, USART_RECEIVE_ENABLE);
    usart_transmit_config(config->usart_periph, USART_TRANSMIT_ENABLE);
    usart_enable(config->usart_periph);
    
    /* 配置NVIC中断优先级和使能USART接收中断 */
    if (config->usart_periph == USART0) {
        nvic_irq_enable(USART0_IRQn, 5, 0);  /* 优先级5符合FreeRTOS要求 */
        usart_interrupt_enable(USART0, USART_INT_RBNE);
    } else if (config->usart_periph == USART1) {
        nvic_irq_enable(USART1_IRQn, 5, 0);
        usart_interrupt_enable(USART1, USART_INT_RBNE);
    } else if (config->usart_periph == USART2) {
        nvic_irq_enable(USART2_IRQn, 5, 0);
        usart_interrupt_enable(USART2, USART_INT_RBNE);
    }
    
    return DEVICE_INIT_OK;
}

/*!
    \brief      获取默认LED配置
    \param[out] config: LED配置结构体指针
    \param[in]  none
    \retval     none
*/
void device_get_default_led_config(led_config_t *config)
{
    if (config == NULL) return;
    
    config->gpio_periph = GPIOC;
    config->pin = GPIO_PIN_13;
    config->mode = GPIO_MODE_OUTPUT;
    config->speed = GPIO_OSPEED_50MHZ;
    config->active_level = 0;  /* 低电平有效 */
}

/*!
    \brief      LED初始化
    \param[in]  config: LED配置结构体指针
    \param[out] none
    \retval     DEVICE_INIT_OK: 初始化成功
                DEVICE_INIT_ERROR: 初始化失败
*/
uint8_t device_led_init(const led_config_t *config)
{
    if (config == NULL) return DEVICE_INIT_ERROR;
    
    /* 使能GPIO时钟 */
    if (config->gpio_periph == GPIOA) {
        rcu_periph_clock_enable(RCU_GPIOA);
    } else if (config->gpio_periph == GPIOB) {
        rcu_periph_clock_enable(RCU_GPIOB);
    } else if (config->gpio_periph == GPIOC) {
        rcu_periph_clock_enable(RCU_GPIOC);
    }
    
    /* 配置GPIO为输出模式 */
    gpio_mode_set(config->gpio_periph, config->mode, GPIO_PUPD_NONE, config->pin);
    gpio_output_options_set(config->gpio_periph, GPIO_OTYPE_PP, config->speed, config->pin);
    
    /* 初始状态：LED关闭 */
    if (config->active_level == 0) {
        gpio_bit_set(config->gpio_periph, config->pin);    /* 高电平关闭 */
    } else {
        gpio_bit_reset(config->gpio_periph, config->pin);  /* 低电平关闭 */
    }
    
    return DEVICE_INIT_OK;
}

/*!
    \brief      系统设备初始化 (时钟+串口+LED)
    \param[in]  none
    \param[out] none
    \retval     DEVICE_INIT_OK: 初始化成功
                DEVICE_INIT_ERROR: 初始化失败
*/
uint8_t device_system_init(void)
{
    clock_config_t clock_config;
    usart_config_t usart_config;
    led_config_t led_config;
    
    /* 获取默认配置 */
    device_get_default_clock_config(&clock_config);
    device_get_default_usart_config(&usart_config);
    device_get_default_led_config(&led_config);
    
    /* 初始化时钟 */
    if (device_clock_init(&clock_config) != DEVICE_INIT_OK) {
        return DEVICE_INIT_ERROR;
    }
    
    /* 初始化LED */
    if (device_led_init(&led_config) != DEVICE_INIT_OK) {
        return DEVICE_INIT_ERROR;
    }
    
    /* 初始化串口 */
    if (device_usart_init(&usart_config) != DEVICE_INIT_OK) {
        return DEVICE_INIT_ERROR;
    }
    
    return DEVICE_INIT_OK;
}

/*!
    \brief      获取当前时钟源信息
    \param[in]  none
    \param[out] none
    \retval     当前时钟源类型
*/
clock_source_t device_get_clock_source(void)
{
    uint32_t clock_source = (RCU_CFG0 & RCU_CFG0_SCSS) >> 2;
    
    switch(clock_source) {
        case 0: return CLOCK_SRC_IRC16M;
        case 1: return CLOCK_SRC_HXTAL_PLL168M;  /* HXTAL直接或通过PLL */
        case 2: 
            /* 需要检查PLL源来确定具体类型 */
            if (RCU_PLL & RCU_PLLSRC_HXTAL) {
                return CLOCK_SRC_HXTAL_PLL168M;
            } else {
                return CLOCK_SRC_IRC16M_PLL72M;
            }
        default: 
            return CLOCK_SRC_IRC16M;
    }
}

/*!
    \brief      获取系统时钟频率
    \param[in]  none
    \param[out] none
    \retval     系统时钟频率 (Hz)
*/
uint32_t device_get_system_clock_freq(void)
{
    /* 确保SystemCoreClock变量是最新的 */
    SystemCoreClockUpdate();
    return SystemCoreClock;
}

/*!
    \brief      打印系统信息
    \param[in]  none
    \param[out] none
    \retval     none
*/
void device_print_system_info(void)
{
    printf("\r\n=== PMU System Information ===\r\n");
    printf("MCU: GD32F4xx\r\n");
    printf("System Clock: %lu MHz\r\n", SystemCoreClock / 1000000);
    
    clock_source_t src = device_get_clock_source();
    printf("Clock Source: ");
    switch(src) {
        case CLOCK_SRC_IRC16M:        printf("IRC16M\r\n"); break;
        case CLOCK_SRC_IRC16M_PLL72M: printf("IRC16M->PLL(72MHz)\r\n"); break;
        case CLOCK_SRC_HXTAL_PLL168M: printf("HXTAL->PLL(168MHz)\r\n"); break;
        default:                      printf("Unknown\r\n"); break;
    }
    
    printf("AHB Clock: %lu MHz\r\n", rcu_clock_freq_get(CK_AHB) / 1000000);
    printf("APB1 Clock: %lu MHz\r\n", rcu_clock_freq_get(CK_APB1) / 1000000);
    printf("APB2 Clock: %lu MHz\r\n", rcu_clock_freq_get(CK_APB2) / 1000000);
    printf("==============================\r\n\r\n");
}

/*!
    \brief      软件延时函数
    \param[in]  time: 延时时间 (相对单位)
    \param[out] none
    \retval     none
*/
void device_soft_delay(uint32_t time)
{
    __IO uint32_t i;
    for(i = 0; i < time * 10; i++){
        /* 空循环延时 */
    }
}

/*!
    \brief      串口发送字符
    \param[in]  ch: 要发送的字符
    \param[out] none
    \retval     none
*/
void _usart_send_char(char ch)
{
    usart_data_transmit(g_usart_periph, (uint8_t)ch);
    while(RESET == usart_flag_get(g_usart_periph, USART_FLAG_TBE));
}

/*!
    \brief      串口发送字符串
    \param[in]  str: 要发送的字符串
    \param[out] none
    \retval     none
*/
void device_usart_send_string(const char* str)
{
    while(*str) {
        _usart_send_char(*str);
        str++;
    }
}

/*!
    \brief      串口发送原始数据
    \param[in]  data: 要发送的数据指针
    \param[in]  length: 数据长度
    \param[out] none
    \retval     none
*/
void usart_send_raw_data(const uint8_t* data, uint32_t length)
{
    if (data == NULL) return;
    
    for (uint32_t i = 0; i < length; i++) {
        usart_data_transmit(g_usart_periph, data[i]);
        while(RESET == usart_flag_get(g_usart_periph, USART_FLAG_TBE));
    }
}

/* =================== 私有函数实现 =================== */

/*!
    \brief      IRC16M时钟初始化
    \param[in]  none
    \param[out] none
    \retval     DEVICE_INIT_OK: 成功
                DEVICE_INIT_ERROR: 失败
*/
static uint8_t clock_init_irc16m(void)
{
    /* 确保IRC16M已经使能 */
    RCU_CTL |= RCU_CTL_IRC16MEN;
    
    /* 等待IRC16M稳定 */
    uint32_t timeout = 0;
    while(0U == (RCU_CTL & RCU_CTL_IRC16MSTB)) {
        timeout++;
        if (timeout > 100000) {
            return DEVICE_INIT_ERROR;
        }
    }
    
    /* 配置时钟分频器 */
    /* AHB = SYSCLK = 16MHz */
    RCU_CFG0 &= ~RCU_CFG0_AHBPSC;
    RCU_CFG0 |= RCU_AHB_CKSYS_DIV1;
    
    /* APB2 = AHB = 16MHz */
    RCU_CFG0 &= ~RCU_CFG0_APB2PSC;
    RCU_CFG0 |= RCU_APB2_CKAHB_DIV1;
    
    /* APB1 = AHB/2 = 8MHz */
    RCU_CFG0 &= ~RCU_CFG0_APB1PSC;
    RCU_CFG0 |= RCU_APB1_CKAHB_DIV2;
    
    /* 选择IRC16M作为系统时钟 */
    RCU_CFG0 &= ~RCU_CFG0_SCS;
    RCU_CFG0 |= RCU_CKSYSSRC_IRC16M;
    
    /* 等待时钟切换完成 */
    timeout = 0;
    while((RCU_CFG0 & RCU_CFG0_SCSS) != RCU_SCSS_IRC16M) {
        timeout++;
        if (timeout > 100000) {
            return DEVICE_INIT_ERROR;
        }
    }
    
    return DEVICE_INIT_OK;
}

/*!
    \brief      IRC16M PLL 72MHz时钟初始化
    \param[in]  none
    \param[out] none
    \retval     DEVICE_INIT_OK: 成功
                DEVICE_INIT_ERROR: 失败
*/
static uint8_t clock_init_irc16m_pll72m(void)
{
    /* 先确保IRC16M稳定 */
    if (clock_init_irc16m() != DEVICE_INIT_OK) {
        return DEVICE_INIT_ERROR;
    }
    
    /* 配置时钟分频器 */
    RCU_CFG0 &= ~RCU_CFG0_AHBPSC;
    RCU_CFG0 |= RCU_AHB_CKSYS_DIV1;        /* AHB = SYSCLK */
    RCU_CFG0 &= ~RCU_CFG0_APB2PSC;
    RCU_CFG0 |= RCU_APB2_CKAHB_DIV1;       /* APB2 = AHB */
    RCU_CFG0 &= ~RCU_CFG0_APB1PSC;
    RCU_CFG0 |= RCU_APB1_CKAHB_DIV2;       /* APB1 = AHB/2 */
    
    /* 配置PLL: IRC16M/16 * 144 / 2 = 72MHz */
    RCU_PLL = (16U | (144U << 6U) | (((2U >> 1U) - 1U) << 16U) | (RCU_PLLSRC_IRC16M));
    
    /* 使能PLL */
    RCU_CTL |= RCU_CTL_PLLEN;
    
    /* 等待PLL稳定 */
    uint32_t timeout = 0;
    while(0U == (RCU_CTL & RCU_CTL_PLLSTB)) {
        timeout++;
        if (timeout > 100000) {
            return DEVICE_INIT_ERROR;
        }
    }
    
    /* 切换到PLL时钟 */
    __IO uint32_t reg_temp = RCU_CFG0;
    reg_temp &= ~RCU_CFG0_SCS;
    reg_temp |= RCU_CKSYSSRC_PLLP;
    RCU_CFG0 = reg_temp;
    
    /* 等待时钟切换完成 */
    timeout = 0;
    while(0U == (RCU_CFG0 & RCU_SCSS_PLLP)) {
        timeout++;
        if (timeout > 100000) {
            return DEVICE_INIT_ERROR;
        }
    }
    
    return DEVICE_INIT_OK;
}

/*!
    \brief      HXTAL PLL 168MHz时钟初始化
    \param[in]  config: 时钟配置
    \param[out] none
    \retval     DEVICE_INIT_OK: 成功
                DEVICE_INIT_ERROR: 失败
*/
static uint8_t clock_init_hxtal_pll168m(const clock_config_t *config)
{
    /* 先切换到IRC16M，避免时钟切换时的波动 */
    if (clock_init_irc16m() != DEVICE_INIT_OK) {
        return DEVICE_INIT_ERROR;
    }
    
    /* 使能HXTAL */
    RCU_CTL |= RCU_CTL_HXTALEN;
    
    /* 等待HXTAL稳定 */
    uint32_t timeout = 0;
    uint32_t max_timeout = config->timeout_ms * 1000;  /* 简单的超时计算 */
    
    while(0U == (RCU_CTL & RCU_CTL_HXTALSTB)) {
        timeout++;
        if (timeout > max_timeout) {
            /* HXTAL启动失败，回退到IRC16M PLL 72MHz */
            return clock_init_irc16m_pll72m();
        }
    }
    
    /* 配置时钟分频器 */
    __IO uint32_t reg_temp = RCU_CFG0;
    reg_temp &= ~RCU_CFG0_AHBPSC;
    reg_temp |= RCU_AHB_CKSYS_DIV1;        /* AHB = SYSCLK */
    reg_temp &= ~RCU_CFG0_APB2PSC;
    reg_temp |= RCU_APB2_CKAHB_DIV2;       /* APB2 = AHB/2 = 84MHz */
    reg_temp &= ~RCU_CFG0_APB1PSC;
    reg_temp |= RCU_APB1_CKAHB_DIV4;       /* APB1 = AHB/4 = 42MHz */
    RCU_CFG0 = reg_temp;
    
    /* 配置PLL: HXTAL(8MHz) / 8 * 336 / 2 = 168MHz */
    RCU_PLL = (8U | (336U << 6U) | (((2U >> 1U) - 1U) << 16U) | (RCU_PLLSRC_HXTAL));
    
    /* 使能PLL */
    RCU_CTL |= RCU_CTL_PLLEN;
    
    /* 等待PLL稳定 */
    timeout = 0;
    while(0U == (RCU_CTL & RCU_CTL_PLLSTB)) {
        timeout++;
        if (timeout > 100000) {
            return DEVICE_INIT_ERROR;
        }
    }
    
    /* 切换到PLL时钟前再次确认配置 */
    device_soft_delay(100);
    
    /* 切换到PLL时钟 */
    reg_temp = RCU_CFG0;
    reg_temp &= ~RCU_CFG0_SCS;
    reg_temp |= RCU_CKSYSSRC_PLLP;
    RCU_CFG0 = reg_temp;
    
    /* 等待时钟切换完成 */
    timeout = 0;
    while(0U == (RCU_CFG0 & RCU_SCSS_PLLP)) {
        timeout++;
        if (timeout > 100000) {
            return DEVICE_INIT_ERROR;
        }
    }
    
    return DEVICE_INIT_OK;
}

/* printf 重定向到串口 */
int _write(int fd, char *ptr, int len)
{
    for(int i = 0; i < len; i++) {
        _usart_send_char(ptr[i]);
    }
    return len;
}

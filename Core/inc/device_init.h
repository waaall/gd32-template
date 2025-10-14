/*!
    \file    device_init.h
    \brief   Device initialization header file
    
    \version 2025-08-29, V1.0.0, PMU project for GD32F4xx
*/

#ifndef DEVICE_INIT_H
#define DEVICE_INIT_H

#include "gd32f4xx.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =================== 宏定义 =================== */
#define DEVICE_INIT_OK          0
#define DEVICE_INIT_ERROR       1

/* 时钟配置选项 */
typedef enum {
    CLOCK_SRC_IRC16M = 0,           /*!< 使用内部16MHz RC振荡器 */
    CLOCK_SRC_IRC16M_PLL72M,        /*!< 使用IRC16M通过PLL倍频到72MHz */
    CLOCK_SRC_HXTAL_PLL168M         /*!< 使用外部8MHz晶振通过PLL倍频到168MHz */
} clock_source_t;

/* 时钟配置结构体 */
typedef struct {
    clock_source_t source;          /*!< 时钟源选择 */
    uint32_t hxtal_freq;           /*!< 外部晶振频率 (Hz) */
    uint32_t timeout_ms;           /*!< 晶振稳定超时时间 (ms) */
} clock_config_t;

/* 串口配置结构体 */
typedef struct {
    uint32_t usart_periph;         /*!< USART外设 */
    uint32_t baudrate;             /*!< 波特率 */
    uint32_t word_length;          /*!< 数据位长度 */
    uint32_t stop_bit;             /*!< 停止位 */
    uint32_t parity;               /*!< 奇偶校验 */
    
    /* GPIO配置 */
    uint32_t gpio_periph;          /*!< GPIO端口 */
    uint32_t tx_pin;               /*!< TX引脚 */
    uint32_t rx_pin;               /*!< RX引脚 */
    uint32_t gpio_af;              /*!< 复用功能 */
} usart_config_t;

/* LED配置结构体 */
typedef struct {
    uint32_t gpio_periph;          /*!< GPIO端口 */
    uint32_t pin;                  /*!< 引脚 */
    uint32_t mode;                 /*!< 输出模式 */
    uint32_t speed;                /*!< 输出速度 */
    uint8_t active_level;          /*!< 有效电平 (0=低电平有效, 1=高电平有效) */
} led_config_t;

/* =================== 函数声明 =================== */

/*!
    \brief      获取默认时钟配置
    \param[out] config: 时钟配置结构体指针
    \param[in]  none
    \retval     none
*/
void device_get_default_clock_config(clock_config_t *config);

/*!
    \brief      系统时钟初始化
    \param[in]  config: 时钟配置结构体指针
    \param[out] none
    \retval     DEVICE_INIT_OK: 初始化成功
                DEVICE_INIT_ERROR: 初始化失败
*/
uint8_t device_clock_init(const clock_config_t *config);

/*!
    \brief      获取默认串口配置
    \param[out] config: 串口配置结构体指针
    \param[in]  none
    \retval     none
*/
void device_get_default_usart_config(usart_config_t *config);

/*!
    \brief      串口初始化
    \param[in]  config: 串口配置结构体指针
    \param[out] none
    \retval     DEVICE_INIT_OK: 初始化成功
                DEVICE_INIT_ERROR: 初始化失败
*/
uint8_t device_usart_init(const usart_config_t *config);

/*!
    \brief      获取默认LED配置
    \param[out] config: LED配置结构体指针
    \param[in]  none
    \retval     none
*/
void device_get_default_led_config(led_config_t *config);

/*!
    \brief      LED初始化
    \param[in]  config: LED配置结构体指针
    \param[out] none
    \retval     DEVICE_INIT_OK: 初始化成功
                DEVICE_INIT_ERROR: 初始化失败
*/
uint8_t device_led_init(const led_config_t *config);

/*!
    \brief      系统设备初始化 (时钟+串口+LED)
    \param[in]  none
    \param[out] none
    \retval     DEVICE_INIT_OK: 初始化成功
                DEVICE_INIT_ERROR: 初始化失败
*/
uint8_t device_system_init(void);

/*!
    \brief      获取当前时钟源信息
    \param[in]  none
    \param[out] none
    \retval     当前时钟源类型
*/
clock_source_t device_get_clock_source(void);

/*!
    \brief      获取系统时钟频率
    \param[in]  none
    \param[out] none
    \retval     系统时钟频率 (Hz)
*/
uint32_t device_get_system_clock_freq(void);

/*!
    \brief      打印系统信息
    \param[in]  none
    \param[out] none
    \retval     none
*/
void device_print_system_info(void);

/*!
    \brief      软件延时函数
    \param[in]  time: 延时时间 (相对单位)
    \param[out] none
    \retval     none
*/
void device_soft_delay(uint32_t time);

/*!
    \brief      串口发送字符串
    \param[in]  str: 要发送的字符串
    \param[out] none
    \retval     none
*/
void device_usart_send_string(const char* str);

#ifdef __cplusplus
}
#endif

#endif /* DEVICE_INIT_H */

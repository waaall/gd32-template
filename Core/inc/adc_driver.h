/*!
    \file    adc_driver.h
    \brief   ADC driver header for PMU application
    
    \version 2025-08-29, V1.0.0, PMU project for GD32F4xx
*/

#ifndef ADC_DRIVER_H
#define ADC_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "gd32f4xx.h"
#include "FreeRTOS.h"
#include "queue.h"
#include <stdbool.h>

/* =================== 配置参数 =================== */
#define ADC_SAMPLE_RATE_HZ      10000.0f
#define ADC_FRAME_SIZE          200
#define ADC_CHANNELS_NUM        6

#define ADC_BUF_HALF_WORDS      (ADC_CHANNELS_NUM * ADC_FRAME_SIZE)
#define ADC_DMA_TOTAL_WORDS     (ADC_BUF_HALF_WORDS * 2)

/* ADC通道定义 */
typedef enum {
    ADC_CH_UA = 0,  // 电压A相
    ADC_CH_UB = 1,  // 电压B相
    ADC_CH_UC = 2,  // 电压C相
    ADC_CH_IA = 3,  // 电流A相
    ADC_CH_IB = 4,  // 电流B相
    ADC_CH_IC = 5   // 电流C相
} adc_channel_t;

/* ADC缓冲区状态 */
typedef enum {
    ADC_BUFFER_HALF = 0,    // 第一半缓冲区准备好
    ADC_BUFFER_FULL = 1     // 第二半缓冲区准备好
} adc_buffer_status_t;

/* ADC数据转换结构体 */
typedef struct {
    float voltage_scaling;      // 电压缩放因子
    float current_scaling;      // 电流缩放因子
    float voltage_offset;       // 电压偏移
    float current_offset;       // 电流偏移
} adc_scaling_config_t;

/* =================== 函数声明 =================== */

/*!
    \brief      初始化ADC驱动
    \param[in]  dma_queue: DMA完成通知队列
    \param[out] none
    \retval     pdPASS: 成功, pdFAIL: 失败
*/
BaseType_t adc_driver_init(QueueHandle_t dma_queue);

/*!
    \brief      配置GPIO为ADC模拟输入
    \param[in]  none
    \param[out] none
    \retval     none
*/
void adc_gpio_config(void);

/*!
    \brief      获取ADC原始数据缓冲区指针
    \param[in]  buffer_status: 缓冲区状态（半/全）
    \param[out] none
    \retval     指向ADC数据缓冲区的指针
*/
uint16_t* adc_get_buffer_ptr(adc_buffer_status_t buffer_status);

/*!
    \brief      转换ADC原始数据为物理量
    \param[in]  raw_data: 原始ADC数据缓冲区
    \param[in]  config: 转换配置参数
    \param[out] channel_data: 转换后的通道数据（通道主序格式）
    \retval     none
*/
void adc_convert_to_physical(const uint16_t* raw_data, 
                           const adc_scaling_config_t* config,
                           float* channel_data);

/*!
    \brief      启动ADC采样
    \param[in]  none
    \param[out] none
    \retval     none
*/
void adc_start_sampling(void);

/*!
    \brief      停止ADC采样
    \param[in]  none
    \param[out] none
    \retval     none
*/
void adc_stop_sampling(void);

/*!
    \brief      获取ADC采样状态
    \param[in]  none
    \param[out] none
    \retval     true: 正在采样, false: 停止采样
*/
bool adc_is_sampling(void);

#ifdef __cplusplus
}
#endif

#endif /* ADC_DRIVER_H */

/*!
    \file    adc_driver.c
    \brief   ADC driver implementation for PMU application
    
    \version 2025-08-29, V1.0.0, PMU project for GD32F4xx
*/

#include "adc_driver.h"
#include "gd32f4xx_rcu.h"
#include "gd32f4xx_dma.h"
#include "gd32f4xx_adc.h"
#include "gd32f4xx_timer.h"
#include "gd32f4xx_gpio.h"
#include <stdbool.h>

/* =================== 私有变量 =================== */
static volatile uint16_t adc_dma_buf[ADC_DMA_TOTAL_WORDS] __attribute__((aligned(4)));
static QueueHandle_t g_dma_queue = NULL;
static bool g_sampling_active = false;

/* =================== 私有函数声明 =================== */
static void adc_timer_config(void);
static void adc_dma_config(void);
static void adc_peripheral_config(void);

/* =================== 公共函数实现 =================== */

BaseType_t adc_driver_init(QueueHandle_t dma_queue)
{
    g_dma_queue = dma_queue;
    
    /* 使能相关外设时钟 */
    rcu_periph_clock_enable(RCU_DMA1);
    rcu_periph_clock_enable(RCU_ADC0);
    rcu_periph_clock_enable(RCU_TIMER1);
    rcu_periph_clock_enable(RCU_GPIOA);
    
    /* 配置GPIO */
    adc_gpio_config();
    
    /* 配置定时器触发源 */
    adc_timer_config();
    
    /* 配置DMA */
    adc_dma_config();
    
    /* 配置ADC外设 */
    adc_peripheral_config();
    
    g_sampling_active = false;
    
    return pdPASS;
}

void adc_gpio_config(void)
{
    /* 配置PA0-PA5为ADC模拟输入 */
    gpio_mode_set(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO_PIN_0);
    gpio_mode_set(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO_PIN_1);
    gpio_mode_set(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO_PIN_2);
    gpio_mode_set(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO_PIN_3);
    gpio_mode_set(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO_PIN_4);
    gpio_mode_set(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO_PIN_5);
}

uint16_t* adc_get_buffer_ptr(adc_buffer_status_t buffer_status)
{
    if (buffer_status == ADC_BUFFER_HALF) {
        return (uint16_t*)&adc_dma_buf[0];
    } else {
        return (uint16_t*)&adc_dma_buf[ADC_BUF_HALF_WORDS];
    }
}

void adc_convert_to_physical(const uint16_t* raw_data, 
                           const adc_scaling_config_t* config,
                           float* channel_data)
{
    /* 转换数据格式：从交错格式转为通道主序格式 */
    for (int n = 0; n < ADC_FRAME_SIZE; ++n) {
        for (int ch = 0; ch < ADC_CHANNELS_NUM; ++ch) {
            uint16_t raw = raw_data[n * ADC_CHANNELS_NUM + ch];
            float voltage = (float)raw * (3.3f / 4095.0f); // ADC基本转换
            
            /* 根据通道类型应用不同的缩放 */
            if (ch < 3) { // 电压通道 (UA, UB, UC)
                channel_data[ch * ADC_FRAME_SIZE + n] = 
                    (voltage - config->voltage_offset) * config->voltage_scaling;
            } else { // 电流通道 (IA, IB, IC)
                channel_data[ch * ADC_FRAME_SIZE + n] = 
                    (voltage - config->current_offset) * config->current_scaling;
            }
        }
    }
}

void adc_start_sampling(void)
{
    if (!g_sampling_active) {
        /* 启动定时器触发ADC */
        timer_enable(TIMER1);
        g_sampling_active = true;
    }
}

void adc_stop_sampling(void)
{
    if (g_sampling_active) {
        /* 停止定时器 */
        timer_disable(TIMER1);
        g_sampling_active = false;
    }
}

bool adc_is_sampling(void)
{
    return g_sampling_active;
}

/* =================== 私有函数实现 =================== */

static void adc_timer_config(void)
{
    timer_parameter_struct timer_initpara;
    timer_deinit(TIMER1);
    timer_struct_para_init(&timer_initpara);

    /* 配置定时器产生10kHz触发频率 */
    uint32_t timer_freq = 180000000; // 假设系统时钟180MHz
    uint32_t prescaler = (timer_freq / 1000000) - 1; // 1MHz基准
    uint32_t period = (1000000 / (uint32_t)ADC_SAMPLE_RATE_HZ) - 1; // 10kHz更新

    timer_initpara.prescaler = prescaler;
    timer_initpara.alignedmode = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection = TIMER_COUNTER_UP;
    timer_initpara.period = period;
    timer_initpara.clockdivision = TIMER_CKDIV_DIV1;
    timer_init(TIMER1, &timer_initpara);

    /* 配置TRGO输出 */
    timer_master_output_trigger_source_select(TIMER1, TIMER_TRI_OUT_SRC_UPDATE);
}

static void adc_dma_config(void)
{
    dma_deinit(DMA1, DMA_CH0);
    
    dma_single_data_parameter_struct dma_init_struct;
    dma_single_data_para_struct_init(&dma_init_struct);

    dma_init_struct.periph_addr = (uint32_t)&ADC_RDATA(ADC0);
    dma_init_struct.memory0_addr = (uint32_t)adc_dma_buf;
    dma_init_struct.direction = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.number = ADC_DMA_TOTAL_WORDS;
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_16BIT;
    dma_init_struct.circular_mode = DMA_CIRCULAR_MODE_ENABLE;
    dma_init_struct.priority = DMA_PRIORITY_HIGH;

    dma_single_data_mode_init(DMA1, DMA_CH0, &dma_init_struct);

    /* 使能DMA中断 */
    dma_interrupt_enable(DMA1, DMA_CH0, DMA_INT_FTF);
    dma_interrupt_enable(DMA1, DMA_CH0, DMA_INT_HTF);

    /* 配置NVIC */
    nvic_irq_enable(DMA1_Channel0_IRQn, 1, 0);

    /* 使能DMA通道 */
    dma_channel_enable(DMA1, DMA_CH0);
}

static void adc_peripheral_config(void)
{
    adc_deinit();

    /* ADC模式配置 */
    adc_sync_mode_config(ADC_SYNC_MODE_INDEPENDENT);
    adc_special_function_config(ADC0, ADC_SCAN_MODE, ENABLE);
    adc_special_function_config(ADC0, ADC_CONTINUOUS_MODE, DISABLE);
    adc_data_alignment_config(ADC0, ADC_DATAALIGN_RIGHT);
    adc_resolution_config(ADC0, ADC_RESOLUTION_12B);

    /* 配置常规序列 */
    adc_channel_length_config(ADC0, ADC_ROUTINE_CHANNEL, ADC_CHANNELS_NUM);
    
    /* 配置每个通道 */
    adc_routine_channel_config(ADC0, 0, ADC_CHANNEL_0, ADC_SAMPLETIME_84);
    adc_routine_channel_config(ADC0, 1, ADC_CHANNEL_1, ADC_SAMPLETIME_84);
    adc_routine_channel_config(ADC0, 2, ADC_CHANNEL_2, ADC_SAMPLETIME_84);
    adc_routine_channel_config(ADC0, 3, ADC_CHANNEL_3, ADC_SAMPLETIME_84);
    adc_routine_channel_config(ADC0, 4, ADC_CHANNEL_4, ADC_SAMPLETIME_84);
    adc_routine_channel_config(ADC0, 5, ADC_CHANNEL_5, ADC_SAMPLETIME_84);

    /* 外部触发配置 */
    adc_external_trigger_source_config(ADC0, ADC_ROUTINE_CHANNEL, ADC_EXTTRIG_ROUTINE_T1_TRGO);
    adc_external_trigger_config(ADC0, ADC_ROUTINE_CHANNEL, EXTERNAL_TRIGGER_RISING);

    /* DMA模式使能 */
    adc_dma_request_after_last_enable(ADC0);
    adc_dma_mode_enable(ADC0);

    /* 使能ADC */
    adc_enable(ADC0);
    
    /* 等待ADC稳定 */
    for(volatile uint32_t i = 0; i < 1000; i++);
}

/* =================== 中断处理函数 =================== */

void DMA1_Channel0_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    /* 半传输完成中断 */
    if(dma_interrupt_flag_get(DMA1, DMA_CH0, DMA_INT_FLAG_HTF) == SET) {
        dma_interrupt_flag_clear(DMA1, DMA_CH0, DMA_INT_FLAG_HTF);
        uint8_t idx = ADC_BUFFER_HALF;
        if (g_dma_queue != NULL) {
            xQueueSendFromISR(g_dma_queue, &idx, &xHigherPriorityTaskWoken);
        }
    }
    
    /* 全传输完成中断 */
    if(dma_interrupt_flag_get(DMA1, DMA_CH0, DMA_INT_FLAG_FTF) == SET) {
        dma_interrupt_flag_clear(DMA1, DMA_CH0, DMA_INT_FLAG_FTF);
        uint8_t idx = ADC_BUFFER_FULL;
        if (g_dma_queue != NULL) {
            xQueueSendFromISR(g_dma_queue, &idx, &xHigherPriorityTaskWoken);
        }
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

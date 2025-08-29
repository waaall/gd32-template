/*!
    \file    fft_phasor_task.h
    \brief   FFT and phasor calculation task header for PMU application
    
    \version 2025-08-29, V1.0.0, PMU project for GD32F4xx
*/

#ifndef FFT_PHASOR_TASK_H
#define FFT_PHASOR_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "gd32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "arm_math.h"
#include "adc_driver.h"
#include <stdbool.h>

/* =================== 配置参数 =================== */
#define FFT_SIZE                256
#define PHASOR_TASK_STACK_SIZE  4096
#define PHASOR_TASK_PRIORITY    (tskIDLE_PRIORITY + 3)

/* 相量测量结果结构体 */
typedef struct {
    uint32_t frame_index;       // 帧索引
    uint32_t timestamp_ms;      // 时间戳（毫秒）
    float frequency[ADC_CHANNELS_NUM];      // 频率 (Hz)
    float amplitude[ADC_CHANNELS_NUM];      // 幅度
    float phase[ADC_CHANNELS_NUM];          // 相位 (弧度)
    float rocof[ADC_CHANNELS_NUM];          // 频率变化率 (Hz/s)
    bool valid[ADC_CHANNELS_NUM];           // 数据有效性标志
} phasor_result_t;

/* 相量算法配置参数 */
typedef struct {
    float nominal_freq;         // 标称频率 (Hz)
    float freq_tracking_alpha;  // 频率跟踪融合系数 (0-1)
    float window_energy_correction; // 窗函数能量校正因子
    bool enable_phase_unwrap;   // 是否启用相位解包
    float outlier_threshold;    // 异常值检测阈值
} phasor_config_t;

/* =================== 函数声明 =================== */

/*!
    \brief      初始化FFT相量计算任务
    \param[in]  dma_queue: DMA数据就绪队列
    \param[in]  result_queue: 相量结果输出队列
    \param[in]  config: 算法配置参数
    \param[out] none
    \retval     pdPASS: 成功, pdFAIL: 失败
*/
BaseType_t fft_phasor_task_init(QueueHandle_t dma_queue, 
                               QueueHandle_t result_queue,
                               const phasor_config_t* config);

/*!
    \brief      启动FFT相量计算任务
    \param[in]  none
    \param[out] none
    \retval     none
*/
void fft_phasor_task_start(void);

/*!
    \brief      停止FFT相量计算任务
    \param[in]  none
    \param[out] none
    \retval     none
*/
void fft_phasor_task_stop(void);

/*!
    \brief      获取默认算法配置
    \param[in]  none
    \param[out] config: 默认配置参数
    \retval     none
*/
void fft_phasor_get_default_config(phasor_config_t* config);

/*!
    \brief      更新算法配置参数
    \param[in]  config: 新的配置参数
    \param[out] none
    \retval     pdPASS: 成功, pdFAIL: 失败
*/
BaseType_t fft_phasor_update_config(const phasor_config_t* config);

/*!
    \brief      获取任务统计信息
    \param[out] frames_processed: 已处理帧数
    \param[out] avg_process_time_us: 平均处理时间(微秒)
    \param[out] max_process_time_us: 最大处理时间(微秒)
    \retval     none
*/
void fft_phasor_get_statistics(uint32_t* frames_processed,
                              uint32_t* avg_process_time_us,
                              uint32_t* max_process_time_us);

#ifdef __cplusplus
}
#endif

#endif /* FFT_PHASOR_TASK_H */

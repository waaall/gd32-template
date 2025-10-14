/*!
    \file    fft_phasor_task.c
    \brief   FFT and phasor calculation task implementation for PMU application
    
    \version 2025-08-29, V1.0.0, PMU project for GD32F4xx
*/

#include "fft_phasor_task.h"
#include <string.h>
#include <math.h>

/* =================== 私有变量 =================== */
static TaskHandle_t g_phasor_task_handle = NULL;
static QueueHandle_t g_dma_queue = NULL;
static QueueHandle_t g_result_queue = NULL;
static phasor_config_t g_config;

/* FFT相关变量 */
static arm_rfft_fast_instance_f32 g_rfft_inst;
static float g_hann_window[ADC_FRAME_SIZE];
static float g_fft_input[FFT_SIZE];
static float g_fft_output[FFT_SIZE];

/* 历史数据 */
static float g_prev_phase[ADC_CHANNELS_NUM];
static float g_prev_freq[ADC_CHANNELS_NUM];
static volatile uint32_t g_global_frame_idx = 0;

/* 统计信息 */
static uint32_t g_frames_processed = 0;
static uint32_t g_total_process_time_us = 0;
static uint32_t g_max_process_time_us = 0;

/* =================== 私有函数声明 =================== */
static void phasor_task_function(void *pvParameters);
static void init_hann_window(void);
static void get_bin_complex(const float *rfft_out, int k, float *re, float *im);
static void process_channel_data(int channel, const float* channel_data, phasor_result_t* result);
static uint32_t get_timestamp_us(void);
static bool validate_phasor_result(const phasor_result_t* result, int channel);

/* =================== 公共函数实现 =================== */

BaseType_t fft_phasor_task_init(QueueHandle_t dma_queue, 
                               QueueHandle_t result_queue,
                               const phasor_config_t* config)
{
    g_dma_queue = dma_queue;
    g_result_queue = result_queue;
    
    /* 复制配置参数 */
    if (config != NULL) {
        memcpy(&g_config, config, sizeof(phasor_config_t));
    } else {
        fft_phasor_get_default_config(&g_config);
    }
    
    /* 初始化FFT实例 */
    arm_status status = arm_rfft_fast_init_f32(&g_rfft_inst, FFT_SIZE);
    if (status != ARM_MATH_SUCCESS) {
        return pdFAIL;
    }
    
    /* 初始化Hann窗 */
    init_hann_window();
    
    /* 初始化历史数据 */
    for (int ch = 0; ch < ADC_CHANNELS_NUM; ++ch) {
        g_prev_freq[ch] = g_config.nominal_freq;
        g_prev_phase[ch] = 0.0f;
    }
    
    /* 创建任务 */
    BaseType_t result = xTaskCreate(
        phasor_task_function,
        "PhasorTask",
        PHASOR_TASK_STACK_SIZE / 4,
        NULL,
        PHASOR_TASK_PRIORITY,
        &g_phasor_task_handle
    );
    
    return result;
}

void fft_phasor_task_start(void)
{
    if (g_phasor_task_handle != NULL) {
        vTaskResume(g_phasor_task_handle);
    }
}

void fft_phasor_task_stop(void)
{
    if (g_phasor_task_handle != NULL) {
        vTaskSuspend(g_phasor_task_handle);
    }
}

void fft_phasor_get_default_config(phasor_config_t* config)
{
    config->nominal_freq = 50.0f;
    config->freq_tracking_alpha = 0.6f;
    config->window_energy_correction = 2.0f; // Hann窗能量校正因子
    config->enable_phase_unwrap = true;
    config->outlier_threshold = 5.0f; // 5Hz的频率偏差阈值
}

BaseType_t fft_phasor_update_config(const phasor_config_t* config)
{
    if (config == NULL) {
        return pdFAIL;
    }
    
    taskENTER_CRITICAL();
    memcpy(&g_config, config, sizeof(phasor_config_t));
    taskEXIT_CRITICAL();
    
    return pdPASS;
}

void fft_phasor_get_statistics(uint32_t* frames_processed,
                              uint32_t* avg_process_time_us,
                              uint32_t* max_process_time_us)
{
    taskENTER_CRITICAL();
    *frames_processed = g_frames_processed;
    *avg_process_time_us = (g_frames_processed > 0) ? 
                          (g_total_process_time_us / g_frames_processed) : 0;
    *max_process_time_us = g_max_process_time_us;
    taskEXIT_CRITICAL();
}

/* =================== 私有函数实现 =================== */

static void phasor_task_function(void *pvParameters)
{
    uint8_t buffer_status;
    adc_scaling_config_t adc_config = {
        .voltage_scaling = 1.0f,    // 根据实际硬件调整
        .current_scaling = 1.0f,
        .voltage_offset = 1.65f,    // 假设2.5V偏置
        .current_offset = 1.65f
    };
    
    float channel_data[ADC_CHANNELS_NUM * ADC_FRAME_SIZE];
    phasor_result_t result;
    
    for (;;) {
        /* 等待DMA数据就绪 */
        if (xQueueReceive(g_dma_queue, &buffer_status, portMAX_DELAY) == pdPASS) {
            uint32_t start_time = get_timestamp_us();
            
            /* 获取ADC数据缓冲区 */
            uint16_t* raw_data = adc_get_buffer_ptr((adc_buffer_status_t)buffer_status);
            
            /* 转换ADC数据为物理量 */
            adc_convert_to_physical(raw_data, &adc_config, channel_data);
            
            /* 初始化结果结构体 */
            memset(&result, 0, sizeof(phasor_result_t));
            result.frame_index = ++g_global_frame_idx;
            result.timestamp_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
            
            /* 处理每个通道 */
            for (int ch = 0; ch < ADC_CHANNELS_NUM; ++ch) {
                process_channel_data(ch, &channel_data[ch * ADC_FRAME_SIZE], &result);
            }
            
            /* 发送结果 */
            if (g_result_queue != NULL) {
                xQueueSend(g_result_queue, &result, 0); // 非阻塞发送
            }
            
            /* 更新统计信息 */
            uint32_t process_time = get_timestamp_us() - start_time;
            taskENTER_CRITICAL();
            g_frames_processed++;
            g_total_process_time_us += process_time;
            if (process_time > g_max_process_time_us) {
                g_max_process_time_us = process_time;
            }
            taskEXIT_CRITICAL();
        }
    }
}

static void init_hann_window(void)
{
    for (int n = 0; n < ADC_FRAME_SIZE; ++n) {
        g_hann_window[n] = 0.5f * (1.0f - arm_cos_f32(2.0f * PI * (float)n / (float)(ADC_FRAME_SIZE - 1)));
    }
}

static void get_bin_complex(const float *rfft_out, int k, float *re, float *im)
{
    if (k == 0) { 
        *re = rfft_out[0]; 
        *im = 0.0f; 
        return; 
    }
    if (k == FFT_SIZE/2) { 
        *re = rfft_out[1]; 
        *im = 0.0f; 
        return; 
    }
    int idx = 2 * k;
    *re = rfft_out[idx];
    *im = rfft_out[idx + 1];
}

static void process_channel_data(int channel, const float* channel_data, phasor_result_t* result)
{
    /* 1) 应用Hann窗并零填充 */
    for (int n = 0; n < ADC_FRAME_SIZE; ++n) {
        g_fft_input[n] = channel_data[n] * g_hann_window[n];
    }
    for (int n = ADC_FRAME_SIZE; n < FFT_SIZE; ++n) {
        g_fft_input[n] = 0.0f;
    }

    /* 2) FFT */
    arm_rfft_fast_f32(&g_rfft_inst, g_fft_input, g_fft_output, 0);

    /* 3) 基于前一帧频率估算bin */
    float bin_freq_res = ADC_SAMPLE_RATE_HZ / (float)FFT_SIZE;
    int k_est = (int)roundf(g_prev_freq[channel] / bin_freq_res);
    int k0 = k_est;
    if (k0 < 2) k0 = 2;
    if (k0 > (FFT_SIZE/2 - 3)) k0 = (FFT_SIZE/2 - 3);

    /* 4) 获取相邻三个bin */
    float re_km1, im_km1, re_k, im_k, re_kp1, im_kp1;
    get_bin_complex(g_fft_output, k0-1, &re_km1, &im_km1);
    get_bin_complex(g_fft_output, k0,   &re_k,   &im_k);
    get_bin_complex(g_fft_output, k0+1, &re_kp1, &im_kp1);

    float mag_km1 = sqrtf(re_km1*re_km1 + im_km1*im_km1);
    float mag_k   = sqrtf(re_k*re_k + im_k*im_k);
    float mag_kp1 = sqrtf(re_kp1*re_kp1 + im_kp1*im_kp1);

    /* 5) 抛物线插值 */
    float denom = (mag_km1 - 2.0f*mag_k + mag_kp1);
    float delta = 0.0f;
    if (fabsf(denom) > 1e-12f) {
        delta = 0.5f * (mag_km1 - mag_kp1) / denom;
    }
    float freq_ipdft = ((float)k0 + delta) * bin_freq_res;

    /* 6) 相位提取和相位差频率估算 */
    float phase_k = atan2f(im_k, re_k);
    float dphase = phase_k - g_prev_phase[channel];
    
    /* 相位解包 */
    if (g_config.enable_phase_unwrap) {
        if (dphase > PI) dphase -= 2.0f*PI;
        if (dphase < -PI) dphase += 2.0f*PI;
    }
    
    float frame_time_s = (float)ADC_FRAME_SIZE / ADC_SAMPLE_RATE_HZ;
    float freq_phase = g_prev_freq[channel] + (dphase / (2.0f*PI*frame_time_s));

    /* 7) 融合频率估算 */
    float freq_final = g_config.freq_tracking_alpha * freq_ipdft + 
                      (1.0f - g_config.freq_tracking_alpha) * freq_phase;

    /* 8) 幅度校正 */
    float amplitude = mag_k * g_config.window_energy_correction;

    /* 9) ROCOF计算 */
    float rocof = (freq_final - g_prev_freq[channel]) / frame_time_s;

    /* 10) 填充结果 */
    result->frequency[channel] = freq_final;
    result->amplitude[channel] = amplitude;
    result->phase[channel] = phase_k;
    result->rocof[channel] = rocof;
    result->valid[channel] = validate_phasor_result(result, channel);

    /* 11) 更新历史值 */
    g_prev_phase[channel] = phase_k;
    g_prev_freq[channel] = freq_final;
}

static uint32_t get_timestamp_us(void)
{
    /* 简单实现，实际可使用高精度定时器 */
    return (xTaskGetTickCount() * portTICK_PERIOD_MS * 1000);
}

static bool validate_phasor_result(const phasor_result_t* result, int channel)
{
    /* 检查频率是否在合理范围内 */
    float freq_deviation = fabsf(result->frequency[channel] - g_config.nominal_freq);
    if (freq_deviation > g_config.outlier_threshold) {
        return false;
    }
    
    /* 检查幅度是否为正数且在合理范围内 */
    if (result->amplitude[channel] <= 0.0f || result->amplitude[channel] > 1000.0f) {
        return false;
    }
    
    /* 检查ROCOF是否在合理范围内 */
    if (fabsf(result->rocof[channel]) > 10.0f) { // 10 Hz/s的限制
        return false;
    }
    
    return true;
}

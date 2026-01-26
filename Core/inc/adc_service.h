/**
  ******************************************************************************
  * @file    adc_service.h
  * @brief   ADC采样服务模块 - 电压测量与FFT分析
  ******************************************************************************
  * @author  Zheng Xu
  * @date    2025-11-10
  *
  * 功能说明：
  * - 循环缓冲区管理（4096样本/通道，降采样2:1）
  * - 定时FFT/RMS计算（基于零点检测周期）
  * - 三相电压测量（UA, UB, UC）与AHALF参考电压校正
  ******************************************************************************
  */

#ifndef __ADC_SERVICE_H
#define __ADC_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "adc.h"
#include "zero_crossing.h"
#include <stdint.h>

/* Exported constants --------------------------------------------------------*/
// 硬件参数（基于adc.c实际配置）
#define ADC_SRV_VOLTAGE_CHANNELS        3       // 3个电压通道（UC, UA, UB）

/**
 * ADC采样率计算（根据adc.c配置）：
 * - ADC_CLOCK_SYNC_PCLK_DIV8 → ADC_CLK = 4MHz
 * - ADC_SAMPLETIME_56CYCLES = 56 cycles
 * - 12-bit转换 = 12 cycles
 * - 单通道转换时间 = 56 + 12 = 68 cycles
 * - 5通道序列转换时间 = 5 × 68 = 340 cycles
 * - 序列转换频率 = 4MHz / 340 ≈ 11,764.7 Hz
 *
 * 警告：修改adc.c中以下参数需同步更新此处
 */
#define ADC_SRV_DECIMATION_RATIO        2       // 降采样比例（隔1存1）
#define ADC_SRV_EFFECTIVE_RATE_HZ       5882    // 降采样后有效率 (11,764.7/2)
#define ADC_SRV_MIN_SAMPLE_COUNT        512     // 最小计算样本数（保障FFT/RMS稳定）

// 硬件测量链路参数（用于计算基础缩放系数）
#define ADC_SRV_HW_RUI              390.0f      // 输入电压采样电阻（Ω）
#define ADC_SRV_HW_RU0              390.0f      // 零序电压采样电阻（Ω）
#define ADC_SRV_HW_RABC             30.0f       // 相电流采样电阻（Ω）
#define ADC_SRV_HW_R0               300.0f      // 零序电流采样电阻（Ω）
#define ADC_SRV_HW_UIBL             (240000.0f + ADC_SRV_HW_RUI)  // 输入电压互感器倍率
#define ADC_SRV_HW_U0BL             (240000.0f + ADC_SRV_HW_RU0)  // 零序电压互感器倍率
#define ADC_SRV_HW_IABCBL           2000.0f     // 相电流互感器倍率
#define ADC_SRV_HW_I0BL             2000.0f     // 零序电流互感器倍率

// 校准系数参数（整数表示，避免浮点运算）
#define ADC_SRV_COEF_BASE           10000       // 校准系数基准值（表示1.0倍）
#define ADC_SRV_COEF_MIN            5000        // 最小校准系数（0.5倍）
#define ADC_SRV_COEF_MAX            15000       // 最大校准系数（1.5倍）
#define ADC_SRV_COEF_DEFAULT        10000       // 默认校准系数（1.0倍）

// 基础缩放系数（基于硬件分压+互感器）
#define ADC_SRV_BASE_VOLT_SCALE     (ADC_SRV_HW_UIBL / ADC_SRV_HW_RUI)

// 缓冲区参数（4096点 ≈ 0.70s @ 5882S/s有效率）
#define ADC_SRV_BUFFER_SIZE         4096
#define ADC_SRV_BUFFER_MASK         (ADC_SRV_BUFFER_SIZE - 1)

// 计算参数
#define ADC_SRV_CALC_INTERVAL_MS    240     // 计算间隔 240ms
#define ADC_SRV_DEFAULT_PERIODS     10      // 默认计算周期数（10个工频周期）
#define ADC_SRV_MIN_PERIODS         5       // 最小周期数
#define ADC_SRV_MAX_PERIODS         20      // 最大周期数

// FFT参数
#define ADC_SRV_MAX_FFT_SIZE        2048    // FFT最大点数（2的幂，<=4096样本窗口）
#define ADC_SRV_MAX_HARMONICS       10      // 最大谐波次数（2~10次）


/* Exported types ------------------------------------------------------------*/

/**
 * @brief 电压测量结果（单通道）
 */
typedef struct {
    float rms_voltage;          // RMS电压值（V）
    float fundamental_voltage;  // 基波电压（V）
    float phase_angle_deg;      // 基波相位角（度，相对于采样起点）
    float thd_percent;          // 总谐波失真（%）
    float frequency_hz;         // 测量频率（Hz）
    uint32_t sample_count;      // 采样点数
    uint8_t is_valid;           // 数据有效标志（1=有效，0=无效）
} ADC_SRV_VoltageResult_t;

/**
 * @brief 三相电压测量结果
 */
typedef struct {
    ADC_SRV_VoltageResult_t ua; // A相电压
    ADC_SRV_VoltageResult_t ub; // B相电压
    ADC_SRV_VoltageResult_t uc; // C相电压
    uint32_t timestamp_ms;      // 时间戳（ms）
    float avg_frequency_hz;     // 平均频率（Hz）
} ADC_SRV_ThreePhaseResult_t;

/**
 * @brief 硬件校准系数（三相电压）
 */
typedef struct {
    uint16_t coef_ua;           // A相校准系数（5000~15000，10000=1.0倍）
    uint16_t coef_ub;           // B相校准系数
    uint16_t coef_uc;           // C相校准系数
} ADC_SRV_CalibCoef_t;

/**
 * @brief ADC服务配置
 */
typedef struct {
    uint8_t calc_periods;               // 计算周期数（5~20）
    uint16_t calc_interval_ms;          // 计算间隔（ms）
    float voltage_scale_ua;             // A相电压基础缩放系数
    float voltage_scale_ub;             // B相电压基础缩放系数
    float voltage_scale_uc;             // C相电压基础缩放系数
    ADC_SRV_CalibCoef_t calib_coef;     // 校准系数（11000就表示1.1倍）
} ADC_SRV_Config_t;

/**
 * @brief ADC服务状态
 */
typedef enum {
    ADC_SRV_STATE_UNINITIALIZED = 0,
    ADC_SRV_STATE_IDLE,
    ADC_SRV_STATE_RUNNING,
    ADC_SRV_STATE_CALCULATING,
    ADC_SRV_STATE_ERROR
} ADC_SRV_State_t;

/* Exported functions --------------------------------------------------------*/

/**
 * @brief  初始化ADC服务模块
 * @param  config: 配置参数指针（NULL使用默认配置）
 * @retval 0=成功，-1=失败
 */
int8_t ADC_SRV_Init(const ADC_SRV_Config_t *config);

/**
 * @brief  启动ADC采样服务
 * @retval 0=成功，-1=失败
 */
int8_t ADC_SRV_Start(void);

/**
 * @brief  停止ADC采样服务
 * @retval 0=成功，-1=失败
 */
int8_t ADC_SRV_Stop(void);

/**
 * @brief  ADC DMA传输完成回调（需在HAL_ADC_ConvCpltCallback中调用）
 * @param  hadc: ADC句柄指针
 * @param  adc_data: ADC原始数据数组（5通道）
 * @note   此函数在DMA中断上下文中执行，应尽快返回
 */
void ADC_SRV_DMA_ConvCpltCallback(ADC_HandleTypeDef *hadc, const volatile uint16_t *adc_data);

/**
 * @brief  定时计算任务（需在主循环中调用）
 * @retval 0=无新数据，1=计算完成，-1=错误
 */
int8_t ADC_SRV_CalculateTask(void);

/**
 * @brief  获取最新的三相电压测量结果
 * @param  result: 结果存储指针
 * @retval 0=成功，-1=失败或数据无效
 */
int8_t ADC_SRV_GetThreePhaseResult(ADC_SRV_ThreePhaseResult_t *result);

/**
 * @brief  获取单相电压测量结果
 * @param  channel: 通道索引（ADC_CH_UA/UB/UC）
 * @param  result: 结果存储指针
 * @retval 0=成功，-1=失败或通道无效
 */
int8_t ADC_SRV_GetVoltageResult(uint8_t channel, ADC_SRV_VoltageResult_t *result);

/**
 * @brief  获取模块状态
 * @retval 当前状态
 */
ADC_SRV_State_t ADC_SRV_GetState(void);

/**
 * @brief  获取缓冲区填充率
 * @retval 填充率（0~100%）
 */
uint8_t ADC_SRV_GetBufferFillLevel(void);

/**
 * @brief  设置计算周期数
 * @param  periods: 周期数（5~20）
 * @retval 0=成功，-1=参数无效
 */
int8_t ADC_SRV_SetCalcPeriods(uint8_t periods);

/**
 * @brief  设置电压缩放系数
 * @param  channel: 通道索引（ADC_CH_UA/UB/UC）
 * @param  scale: 缩放系数（V/ADC_CODE）
 * @retval 0=成功，-1=参数无效
 */
int8_t ADC_SRV_SetVoltageScale(uint8_t channel, float scale);

/**
 * @brief  设置校准系数
 * @param  channel: 通道索引（ADC_CH_UA/UB/UC）
 * @param  coef: 校准系数（5000~15000，10000=1.0倍）
 * @retval 0=成功，-1=参数无效
 */
int8_t ADC_SRV_SetCalibCoef(uint8_t channel, uint16_t coef);

/**
 * @brief  获取校准系数
 * @param  channel: 通道索引（ADC_CH_UA/UB/UC）
 * @param  coef: 校准系数存储指针
 * @retval 0=成功，-1=参数无效
 */
int8_t ADC_SRV_GetCalibCoef(uint8_t channel, uint16_t *coef);

/**
 * @brief  重置统计信息
 * @retval 0=成功，-1=失败
 */
int8_t ADC_SRV_ResetStats(void);

#ifdef __cplusplus
}
#endif

#endif /* __ADC_SERVICE_H */

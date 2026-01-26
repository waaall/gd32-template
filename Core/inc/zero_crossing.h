/**
  ******************************************************************************
  * @file    zero_crossing.h
  * @brief   过零检测模块头文件
  ******************************************************************************
  * @author Zheng Xu
  * @date   2025-11-06
  * 
  * 硬件配置：
  * - TIM2 CH3 (PB10): FA信号输入（A相过零检测）
  * - TIM2 CH4 (PB11): FC信号输入（B相过零检测）
  *
  ******************************************************************************
  */

#ifndef __ZERO_CROSSING_H__
#define __ZERO_CROSSING_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "tim.h"

/* 默认配置参数 --------------------------------------------------------------*/
/**
 * @brief 默认配置参数
 * @note  所有时间参数单位为TIM2计数值（16MHz时钟，1计数 = 62.5ns）
 */
#define ZC_DEFAULT_FAKE_PERIOD      32000U      // 默认毛刺抑制周期：2ms (2000μs/0.0625μs = 32000)
#define ZC_DEFAULT_MIN_PERIOD       246154U     // 默认最小周期：65Hz (15.385ms → 246154计数)
#define ZC_DEFAULT_MAX_PERIOD       421053U     // 默认最大周期：38Hz (26.316ms → 421053计数)
#define ZC_DEFAULT_WINDOW_PERIODS   10U         // 默认滑动平均窗口：10个周期

/**
 * @brief 数据失活判断相关参数（毫秒）
 */
#define ZC_DEFAULT_INACTIVITY_TIMEOUT_MS  500U   // 默认500ms内无新数据即认定失活
#define ZC_MIN_INACTIVITY_TIMEOUT_MS      50U    // 最小允许的失活检测时间
#define ZC_MAX_INACTIVITY_TIMEOUT_MS      60000U // 最大允许的失活检测时间（60s）

/**
 * @brief 频率计算常数
 * @note  频率 = ZC_FREQ_CALC_CONST / 周期计数值
 *        16MHz时钟: 16,000,000 Hz
 */
#define ZC_FREQ_CALC_CONST          16000000UL

/**
 * @brief 最大滑动平均窗口大小
 */
#define ZC_MAX_WINDOW_PERIODS       30U

/* 通道定义 ------------------------------------------------------------------*/
typedef enum {
    ZC_CHANNEL_A = 0,   // A相（TIM2 CH3, PB10, FA信号）
    ZC_CHANNEL_B = 1,   // B相（TIM2 CH4, PB11, FC信号）
    ZC_CHANNEL_COUNT = 2
} ZC_Channel_t;

/* 配置结构体 ----------------------------------------------------------------*/
/**
 * @brief 过零检测配置结构体
 */
typedef struct {
    uint32_t fake_period;       // 毛刺抑制周期（计数值）：小于此值的脉冲将被忽略
    uint32_t min_period;        // 最小有效周期（计数值）：对应最高频率限制
    uint32_t max_period;        // 最大有效周期（计数值）：对应最低频率限制
    uint8_t  window_periods;    // 滑动平均窗口大小（周期个数）
    uint32_t inactivity_timeout_ms; // 失活超时时间（毫秒），超过该时间未更新则清空测量
} ZC_Config_t;

/* 测量数据结构体 ------------------------------------------------------------*/
/**
 * @brief 过零检测测量数据结构体
 */
typedef struct {
    uint32_t period;            // 当前周期（计数值）
    uint32_t period_avg;        // 平均周期（计数值）
    uint32_t frequency_mhz;     // 实时频率（mHz，1Hz = 1000mHz，保留3位小数精度）
    uint32_t frequency_avg_mhz; // 平均频率（mHz，1Hz = 1000mHz，保留3位小数精度）
    uint32_t valid_count;       // 有效测量计数
    uint32_t error_count;       // 错误计数（超出范围/毛刺）
    uint8_t  is_valid;          // 当前测量是否有效（1=有效，0=无效）
} ZC_Data_t;

/* 公共函数声明 --------------------------------------------------------------*/

/**
 * @brief  初始化过零检测模块
 * @note   必须在TIM2初始化之后调用
 * @retval 0: 成功, -1: 失败
 */
int8_t ZC_Init(void);

/**
 * @brief  启动过零检测
 * @note   启动TIM2输入捕获中断
 * @retval 0: 成功, -1: 失败
 */
int8_t ZC_Start(void);

/**
 * @brief  停止过零检测
 * @retval 0: 成功, -1: 失败
 */
int8_t ZC_Stop(void);

/**
 * @brief  获取指定通道的测量数据
 * @param  channel: 通道选择（ZC_CHANNEL_A 或 ZC_CHANNEL_C）
 * @param  data: 数据存储指针（输出参数）
 * @retval 0: 成功, -1: 失败（参数错误或数据无效）
 */
int8_t ZC_GetData(ZC_Channel_t channel, ZC_Data_t *data);

/**
 * @brief  获取平均频率（双通道平均）
 * @note   返回A相和B相的平均频率
 * @retval 平均频率（mHz），如果无有效数据则返回0
 */
uint32_t ZC_GetAvgFrequency(void);

/**
 * @brief  设置配置参数
 * @param  config: 配置参数指针
 * @retval 0: 成功, -1: 失败（参数无效）
 */
int8_t ZC_SetConfig(const ZC_Config_t *config);

/**
 * @brief  获取当前配置参数
 * @param  config: 配置参数存储指针（输出参数）
 * @retval 0: 成功, -1: 失败
 */
int8_t ZC_GetConfig(ZC_Config_t *config);

/**
 * @brief  设置毛刺抑制周期
 * @param  fake_period_ms: 毛刺抑制时间（毫秒）
 * @retval 0: 成功, -1: 失败
 */
int8_t ZC_SetFakePeriod(float fake_period_ms);

/**
 * @brief  设置频率范围
 * @param  min_freq_hz: 最小频率（Hz）
 * @param  max_freq_hz: 最大频率（Hz）
 * @retval 0: 成功, -1: 失败（参数无效）
 */
int8_t ZC_SetCountFreqRange(float min_freq_hz, float max_freq_hz);

/**
 * @brief  设置滑动平均窗口大小
 * @param  window_periods: 窗口大小（周期个数，1-30）
 * @retval 0: 成功, -1: 失败（参数超出范围）
 */
int8_t ZC_SetWindowSize(uint8_t window_periods);

/**
 * @brief  设置失活超时时间
 * @param  timeout_ms: 毫秒（ZC_MIN_INACTIVITY_TIMEOUT_MS-ZC_MAX_INACTIVITY_TIMEOUT_MS）
 * @retval 0: 成功, -1: 失败
 */
int8_t ZC_SetInactivityTimeout(uint32_t timeout_ms);

/**
 * @brief  获取当前失活超时时间
 * @retval 毫秒
 */
uint32_t ZC_GetInactivityTimeout(void);

/**
 * @brief  复位统计数据
 * @param  channel: 通道选择（ZC_CHANNEL_A 或 ZC_CHANNEL_C）
 * @retval 0: 成功, -1: 失败
 * @note   有必要周期性存储数据并复位（比如30天）
 */
int8_t ZC_ResetStats(ZC_Channel_t channel);

/**
 * @brief  TIM2输入捕获中断回调函数
 * @note   此函数应在stm32f4xx_it.c的TIM2_IRQHandler中调用
 * @param  htim: TIM句柄指针
 */
void ZC_TIM_CaptureCallback(TIM_HandleTypeDef *htim);

/**
 * @brief  获取模块初始化状态（用于故障排查）
 * @param  initialized: 输出初始化标志（输出参数）
 * @param  running: 输出运行状态（输出参数）
 * @param  start_result_ch3: 输出CH3启动结果（输出参数，0=成功，-1=失败，-99=未调用）
 * @param  start_result_ch4: 输出CH4启动结果（输出参数，0=成功，-1=失败，-99=未调用）
 * @retval None
 */
void ZC_GetModuleStatus(uint8_t *initialized,
                        uint8_t *running,
                        int8_t *start_result_ch3,
                        int8_t *start_result_ch4);

#ifdef __cplusplus
}
#endif

#endif /* __ZERO_CROSSING_H__ */

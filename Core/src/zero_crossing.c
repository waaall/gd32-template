/**
  ******************************************************************************
  * @file    zero_crossing.c
  * @brief   过零检测模块实现
  ******************************************************************************
  * @author Zheng Xu
  * @date   2025-11-06
  *
  * 实现说明：
  * - 使用滑动平均算法提高频率测量精度
  * - 自动过滤毛刺和超出范围的信号
  * - 支持运行时配置参数调整
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "zero_crossing.h"
#include <string.h>

/* Private typedef -----------------------------------------------------------*/
/**
 * @brief 通道内部状态结构体（写缓冲，仅在ISR中更新）
 */
typedef struct {
    uint32_t capture_last;          // 上次捕获值
    uint32_t period_buffer[ZC_MAX_WINDOW_PERIODS];  // 周期缓冲区
    uint8_t  buffer_index;          // 缓冲区索引
    uint8_t  buffer_count;          // 缓冲区有效数据个数
    uint64_t period_sum;            // 周期和（用于快速计算平均值）
    uint32_t valid_count;           // 有效测量计数
    uint32_t error_count;           // 错误计数
    uint8_t  first_capture;         // 首次捕获标志（1=首次，0=非首次）
    uint32_t last_update_tick;      // 最近一次有效周期刷新时间（ms）
} ZC_ChannelState_t;

/**
 * @brief 通道结果快照结构体（读缓冲，双缓冲机制）
 */
typedef struct {
    uint32_t period;                // 当前周期（计数值）
    uint32_t period_avg;            // 平均周期（计数值）
    uint32_t frequency_mhz;         // 实时频率（mHz）
    uint32_t frequency_avg_mhz;     // 平均频率（mHz）
    uint32_t valid_count;           // 有效测量计数
    uint32_t error_count;           // 错误计数
    uint8_t  is_valid;              // 当前测量是否有效
} ZC_Result_t;

/* Private variables ---------------------------------------------------------*/
static ZC_Config_t zc_config = {
    .fake_period = ZC_DEFAULT_FAKE_PERIOD,
    .min_period = ZC_DEFAULT_MIN_PERIOD,
    .max_period = ZC_DEFAULT_MAX_PERIOD,
    .window_periods = ZC_DEFAULT_WINDOW_PERIODS,
    .inactivity_timeout_ms = ZC_DEFAULT_INACTIVITY_TIMEOUT_MS
};

static ZC_ChannelState_t zc_state[ZC_CHANNEL_COUNT] = {0};
static uint8_t zc_initialized = 0;
static uint8_t zc_running = 0;

// 双缓冲机制：ISR写入结果，读取函数读取稳定快照
static ZC_Result_t zc_result[ZC_CHANNEL_COUNT][2] = {0};
static volatile uint8_t zc_active_idx[ZC_CHANNEL_COUNT] = {0}; // ISR写入索引（volatile保证可见性）

// 调试：记录启动结果
static int8_t zc_start_result_ch3 = -99;  // -99表示未调用
static int8_t zc_start_result_ch4 = -99;

/* Private function prototypes -----------------------------------------------*/
static void _process_capture(ZC_Channel_t channel, uint32_t capture_value);
static uint8_t _validate_config(const ZC_Config_t *config);
static void _disable_zc_irq(void);
static void _enable_zc_irq(void);
static void _clear_channel_measurements(ZC_ChannelState_t *state);
static void _handle_inactivity(ZC_ChannelState_t *state, ZC_Channel_t channel, uint32_t now_ms);
static void _update_result_snapshot(ZC_Channel_t channel);
static void _reset_channel_runtime(ZC_Channel_t channel);
static void _reset_all_channels(void);

/*-------------------------初始化函数--------------------------*/

int8_t ZC_Init(void)
{
    _reset_all_channels();
    zc_running = 0;

    // 加载默认配置
    zc_config.fake_period = ZC_DEFAULT_FAKE_PERIOD;
    zc_config.min_period = ZC_DEFAULT_MIN_PERIOD;
    zc_config.max_period = ZC_DEFAULT_MAX_PERIOD;
    zc_config.window_periods = ZC_DEFAULT_WINDOW_PERIODS;
    zc_config.inactivity_timeout_ms = ZC_DEFAULT_INACTIVITY_TIMEOUT_MS;

    zc_initialized = 1;
    return 0;
}

int8_t ZC_Start(void)
{
    if (!zc_initialized) {
        return -1;
    }

    // 启动TIM2输入捕获中断（CH3 - A相）
    HAL_StatusTypeDef status_ch3 = HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_3);
    zc_start_result_ch3 = (status_ch3 == HAL_OK) ? 0 : -1;
    if (status_ch3 != HAL_OK) {
        return -1;
    }

    // 启动TIM2输入捕获中断（CH4 - B相）
    HAL_StatusTypeDef status_ch4 = HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_4);
    zc_start_result_ch4 = (status_ch4 == HAL_OK) ? 0 : -1;
    if (status_ch4 != HAL_OK) {
        HAL_TIM_IC_Stop_IT(&htim2, TIM_CHANNEL_3);  // 如果CH4启动失败，停止CH3
        return -1;
    }

    zc_running = 1;
    return 0;
}

int8_t ZC_Stop(void)
{
    // 停止TIM2输入捕获中断
    HAL_TIM_IC_Stop_IT(&htim2, TIM_CHANNEL_3);
    HAL_TIM_IC_Stop_IT(&htim2, TIM_CHANNEL_4);

    zc_running = 0;
    return 0;
}

/*-------------------------Get数据函数--------------------------*/

int8_t ZC_GetData(ZC_Channel_t channel, ZC_Data_t *data)
{
    if (channel >= ZC_CHANNEL_COUNT || data == NULL) {
        return -1;
    }

    // 检查失活超时（需要访问zc_state，在检查后读取快照）
    ZC_ChannelState_t *state = &zc_state[channel];
    uint32_t now_ms = HAL_GetTick();
    _handle_inactivity(state, channel, now_ms);

    // 读取稳定的快照索引（ISR写入active_idx，我们读取另一半）
    uint8_t active = zc_active_idx[channel];
    uint8_t read_idx = 1 - active;  // 读取非活动缓冲区
    ZC_Result_t *snapshot = &zc_result[channel][read_idx];

    // 直接复制快照数据（双缓冲保证数据一致性）
    data->period = snapshot->period;
    data->period_avg = snapshot->period_avg;
    data->frequency_mhz = snapshot->frequency_mhz;
    data->frequency_avg_mhz = snapshot->frequency_avg_mhz;
    data->valid_count = snapshot->valid_count;
    data->error_count = snapshot->error_count;
    data->is_valid = snapshot->is_valid;

    return 0;
}

void ZC_GetModuleStatus(uint8_t *initialized,
                        uint8_t *running,
                        int8_t *start_result_ch3,
                        int8_t *start_result_ch4)
{
    if (initialized != NULL) {
        *initialized = zc_initialized;
    }
    if (running != NULL) {
        *running = zc_running;
    }
    if (start_result_ch3 != NULL) {
        *start_result_ch3 = zc_start_result_ch3;
    }
    if (start_result_ch4 != NULL) {
        *start_result_ch4 = zc_start_result_ch4;
    }
}

/**
 * @brief  获取平均频率（双通道平均）
 */
uint32_t ZC_GetAvgFrequency(void)
{
    ZC_Data_t data_a, data_c;
    uint64_t freq_sum = 0;
    uint8_t valid_count = 0;

    // 获取A相数据
    if (ZC_GetData(ZC_CHANNEL_A, &data_a) == 0 && data_a.is_valid) {
        freq_sum += data_a.frequency_avg_mhz;
        valid_count++;
    }

    // 获取B相数据
    if (ZC_GetData(ZC_CHANNEL_B, &data_c) == 0 && data_c.is_valid) {
        freq_sum += data_c.frequency_avg_mhz;
        valid_count++;
    }

    // 返回平均值（mHz）
    return (valid_count > 0) ? (uint32_t)(freq_sum / valid_count) : 0;
}

int8_t ZC_GetConfig(ZC_Config_t *config)
{
    if (config == NULL) {return -1;}
    *config = zc_config;
    return 0;
}

uint32_t ZC_GetInactivityTimeout(void)
{
    return zc_config.inactivity_timeout_ms;
}

/*-------------------------Set数据函数--------------------------*/

int8_t ZC_SetConfig(const ZC_Config_t *config)
{
    if (config == NULL) {
        return -1;
    }
    ZC_Config_t new_config = *config;
    if (new_config.inactivity_timeout_ms == 0U) {
        new_config.inactivity_timeout_ms = zc_config.inactivity_timeout_ms;
    }
    if (!_validate_config(&new_config)) {
        return -1;
    }

    uint8_t was_running = zc_running;
    if (was_running) {
        ZC_Stop();
    }

    _disable_zc_irq();
    zc_config = new_config;
    _reset_all_channels();
    _enable_zc_irq();

    if (was_running) {
        if (ZC_Start() != 0) {
            return -1;
        }
    }

    return 0;
}

/**
 * @brief  设置毛刺抑制周期
 */
int8_t ZC_SetFakePeriod(float fake_period_ms)
{
    if (fake_period_ms < 0.0f || fake_period_ms > 10.0f) {
        return -1;  // 限制范围：0-10ms
    }

    // 转换为计数值：ms -> μs -> 计数值
    // fake_period_ms * 1000 (μs) / 0.0625 (μs/count) = fake_period_ms * 16000
    ZC_Config_t new_config = zc_config;
    new_config.fake_period = (uint32_t)(fake_period_ms * 16000.0f);

    if (!_validate_config(&new_config)) {
        return -1;
    }

    _disable_zc_irq();
    zc_config = new_config;
    _enable_zc_irq();

    return 0;
}

/**
 * @brief  设置合理频率范围
 */
int8_t ZC_SetCountFreqRange(float min_freq_hz, float max_freq_hz)
{
    if (min_freq_hz <= 0.0f || max_freq_hz <= 0.0f || min_freq_hz >= max_freq_hz) {
        return -1;
    }

    // 频率转周期 再转计数值
    ZC_Config_t new_config = zc_config;
    new_config.max_period = (uint32_t)(ZC_FREQ_CALC_CONST / min_freq_hz);
    new_config.min_period = (uint32_t)(ZC_FREQ_CALC_CONST / max_freq_hz);

    if (!_validate_config(&new_config)) {
        return -1;
    }

    _disable_zc_irq();
    zc_config = new_config;
    _enable_zc_irq();

    return 0;
}

int8_t ZC_SetWindowSize(uint8_t window_periods)
{
    if (window_periods == 0 || window_periods > ZC_MAX_WINDOW_PERIODS) {
        return -1;
    }
    // 临界区外构造新配置并验证
    ZC_Config_t new_config = zc_config;
    new_config.window_periods = window_periods;

    if (!_validate_config(&new_config)) {
        return -1;
    }

    return ZC_SetConfig(&new_config);
}

int8_t ZC_SetInactivityTimeout(uint32_t timeout_ms)
{
    if (timeout_ms == 0U) {
        return -1;
    }
    ZC_Config_t new_config = zc_config;
    new_config.inactivity_timeout_ms = timeout_ms;

    if (!_validate_config(&new_config)) {
        return -1;
    }

    _disable_zc_irq();
    zc_config.inactivity_timeout_ms = timeout_ms;
    _enable_zc_irq();

    return 0;
}

int8_t ZC_ResetStats(ZC_Channel_t channel)
{
    if (channel >= ZC_CHANNEL_COUNT) {
        return -1;
    }

    // 清空缓冲区和统计数据（仅关闭TIM2中断）
    _disable_zc_irq();
    _reset_channel_runtime(channel);

    _enable_zc_irq();

    return 0;
}

/*-------------------------中断回调函数--------------------------*/
/**
 * @note   由HAL_TIM_IC_CaptureCallback调用
 *         HAL库会自动设置htim->Channel为触发中断的通道
 */
void ZC_TIM_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIM2) {
        return;
    }
    // // test
    // HAL_GPIO_TogglePin(TEST1_GPIO_Port, TEST1_Pin);

    // CH3: A相; CH4: B相
    if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_3) {
        uint32_t capture = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_3);
        _process_capture(ZC_CHANNEL_A, capture);
    }
    else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_4) {
        uint32_t capture = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_4);
        _process_capture(ZC_CHANNEL_B, capture);
    }
}

/*------------------------- Private 函数 --------------------------*/
/**
 * @brief  处理捕获值（核心算法）
 * @param  channel: 通道号
 * @param  capture_value: 当前捕获值
 */
static void _process_capture(ZC_Channel_t channel, uint32_t capture_value)
{
    // 边界检查
    if (channel >= ZC_CHANNEL_COUNT) {return;}
    
    ZC_ChannelState_t *state = &zc_state[channel];
    // 首次捕获，仅记录值，不计算周期
    if (state->first_capture) {
        state->capture_last = capture_value;
        state->first_capture = 0;
        return;
    }
    
    uint32_t period; // 计算周期
    if (capture_value >= state->capture_last) {
        period = capture_value - state->capture_last;
    } else {
        // 定时器溢出情况
        period = (0xFFFFFFFF - state->capture_last) + capture_value + 1;
    }

    // 更新上次捕获值
    state->capture_last = capture_value;

    // 毛刺抑制：忽略过短的脉冲
    if (period < zc_config.fake_period) {
        state->error_count++;
        return;
    }
    // 超出有效频率范围的数据丢弃
    if (period < zc_config.min_period || period > zc_config.max_period) {
        state->error_count++;
        return;
    }

    // 有效数据，加入滑动窗口
    state->valid_count++;

    // 如果缓冲区未满，直接添加（buffer_count 负责"热身期"判定）
    if (state->buffer_count < zc_config.window_periods) {
        state->period_buffer[state->buffer_index] = period;
        state->period_sum += period;
        state->buffer_index++;
        state->buffer_count++;

        // 循环索引
        if (state->buffer_index >= zc_config.window_periods) {
            state->buffer_index = 0;
        }
    // 如果缓冲区已满（稳定运行期）
    } else {
        // 替换最旧的period（index位置）和 period_sum
        state->period_sum -= state->period_buffer[state->buffer_index];
        state->period_buffer[state->buffer_index] = period;
        state->period_sum += period;

        // 移动索引
        state->buffer_index++;
        if (state->buffer_index >= zc_config.window_periods) {
            state->buffer_index = 0;
        }
    }
    state->last_update_tick = HAL_GetTick();

    // // test
    // HAL_GPIO_TogglePin(TEST2_GPIO_Port, TEST2_Pin);

    // 更新结果快照（写入到双缓冲区）
    _update_result_snapshot(channel);
}

static uint8_t _validate_config(const ZC_Config_t *config)
{
    if (config->min_period >= config->max_period) {
        return 0;  // 最小周期必须小于最大周期
    }

    if (config->fake_period >= config->min_period) {
        return 0;  // 毛刺周期必须小于最小有效周期
    }

    if (config->window_periods == 0 || config->window_periods > ZC_MAX_WINDOW_PERIODS) {
        return 0; // 检查窗口大小
    }
    if (config->inactivity_timeout_ms < ZC_MIN_INACTIVITY_TIMEOUT_MS ||
        config->inactivity_timeout_ms > ZC_MAX_INACTIVITY_TIMEOUT_MS) {
        return 0;  // 失活时间必须在允许范围内
    }
    return 1;  // 配置有效（注意为了方便，正常返回并不是0）
}

/**
 * @brief  根据窗口变化重新整理各通道缓冲区(不包括result)
 */
static void _clear_channel_measurements(ZC_ChannelState_t *state)
{
    memset(state->period_buffer, 0, sizeof(state->period_buffer));
    state->buffer_index = 0;
    state->buffer_count = 0;
    state->period_sum = 0;
    state->first_capture = 1;
    state->capture_last = 0;
    state->last_update_tick = 0;
}

static void _reset_channel_runtime(ZC_Channel_t channel)
{
    if (channel >= ZC_CHANNEL_COUNT) {
        return;
    }

    ZC_ChannelState_t *state = &zc_state[channel];
    _clear_channel_measurements(state);
    state->valid_count = 0;
    state->error_count = 0;

    zc_active_idx[channel] = 0;
    memset(&zc_result[channel], 0, sizeof(zc_result[channel]));
}

static void _reset_all_channels(void)
{
    for (uint8_t ch = 0; ch < ZC_CHANNEL_COUNT; ch++) {
        _reset_channel_runtime((ZC_Channel_t)ch);
    }
}

static void _handle_inactivity(ZC_ChannelState_t *state, ZC_Channel_t channel, uint32_t now_ms)
{
    uint32_t timeout_ms = zc_config.inactivity_timeout_ms;

    if (timeout_ms == 0U || state->buffer_count == 0U || state->last_update_tick == 0U) {
        return;
    }
    // 无符号减法的模运算特性自动处理HAL_GetTick()溢出
    uint32_t elapsed = now_ms - state->last_update_tick;
    if (elapsed < timeout_ms) {
        return;
    }

    _disable_zc_irq();
    uint32_t latest_now = HAL_GetTick();
    if (state->buffer_count > 0U) {
        uint32_t latest_elapsed = latest_now - state->last_update_tick;
        if (latest_elapsed >= timeout_ms) {
            _clear_channel_measurements(state);
            // 更新结果快照，标记为无效
            _update_result_snapshot(channel);
        }
    }
    _enable_zc_irq();
}

/**
 * @brief  禁用过零检测中断（仅关闭TIM2中断）
 */
static void _disable_zc_irq(void)
{
    NVIC_DisableIRQ(TIM2_IRQn);
}

/**
 * @brief  启用过零检测中断
 */
static void _enable_zc_irq(void)
{
    NVIC_EnableIRQ(TIM2_IRQn);
}

/**
 * @brief  更新结果快照（在ISR中调用，写入到双缓冲区）
 * @param  channel: 通道号
 */
static void _update_result_snapshot(ZC_Channel_t channel)
{
    if (channel >= ZC_CHANNEL_COUNT) {
        return;
    }

    ZC_ChannelState_t *state = &zc_state[channel];
    uint8_t active = zc_active_idx[channel];
    ZC_Result_t *result = &zc_result[channel][active];

    // 填充测量数据到活动缓冲区
    result->valid_count = state->valid_count;
    result->error_count = state->error_count;
    result->is_valid = (state->buffer_count > 0) ? 1 : 0;

    if (state->buffer_count > 0) {
        // 计算平均周期
        result->period_avg = (uint32_t)(state->period_sum / state->buffer_count);

        // 计算平均频率（mHz）
        float freq_avg_hz = (float)ZC_FREQ_CALC_CONST / (float)result->period_avg;
        float freq_avg_mhz_f = freq_avg_hz * 1000.0f;
        result->frequency_avg_mhz = (freq_avg_mhz_f > 4294967295.0f) ?
                                    0xFFFFFFFF : (uint32_t)(freq_avg_mhz_f + 0.5f);

        // 获取最新周期
        uint8_t last_idx = (state->buffer_index == 0) ?
                           (state->buffer_count - 1) : (state->buffer_index - 1);
        result->period = state->period_buffer[last_idx];

        // 计算实时频率（mHz）
        float freq_hz = (float)ZC_FREQ_CALC_CONST / (float)result->period;
        float freq_mhz_f = freq_hz * 1000.0f;
        result->frequency_mhz = (freq_mhz_f > 4294967295.0f) ?
                                0xFFFFFFFF : (uint32_t)(freq_mhz_f + 0.5f);
    } else {
        // 无有效数据
        result->period = 0;
        result->period_avg = 0;
        result->frequency_mhz = 0;
        result->frequency_avg_mhz = 0;
    }

    // 翻转活动索引（原子操作，告诉读取路径切换到刚写好的缓冲区）
    zc_active_idx[channel] = 1 - active;
}

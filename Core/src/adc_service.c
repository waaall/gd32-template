/**
  ******************************************************************************
  * @file    adc_service.c
  * @brief   ADC采样服务模块实现
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "adc_service.h"
#include "arm_math.h"  // CMSIS-DSP库
#include <string.h>
#include <math.h>

/* Private defines -----------------------------------------------------------*/
// PI已在arm_math.h中定义

/* Private typedef -----------------------------------------------------------*/
/**
 * @brief 循环缓冲区（单通道）
 */
typedef struct {
    int16_t data[ADC_SRV_BUFFER_SIZE];  // 缓冲区数据（有符号, 已减AHALF）
    uint16_t write_index;               // 写入索引
    uint32_t total_samples;             // 总采样计数
} ADC_SRV_RingBuffer_t;

/**
 * @brief ADC服务内部状态
 */
typedef struct {
    ADC_SRV_RingBuffer_t buffers[ADC_SRV_VOLTAGE_CHANNELS]; // 三相电压缓冲区
    ADC_SRV_Config_t config;                                // 配置参数
    ADC_SRV_State_t state;                                  // 模块状态
    uint8_t decimation_counter;                             // 降采样计数器
    uint32_t last_calc_tick;                                // 上次计算时间戳
    uint8_t result_ready;                                   // 结果就绪标志
    uint16_t vref_adc_value;                                // VREFINT最新ADC值（用于归一化）
} ADC_SRV_Context_t;

/* Private variables ---------------------------------------------------------*/
static ADC_SRV_Context_t adc_srv_ctx = {0};

// 结果双缓冲区（写入使用adc_active_idx指向的缓冲区，然后翻转索引）
static ADC_SRV_ThreePhaseResult_t adc_results[2] = {0};
static uint8_t adc_result_ready[2] = {0};
static volatile uint8_t adc_active_idx = 0;

// CMSIS-DSP RFFT实例和缓冲区
static arm_rfft_fast_instance_f32 rfft_instance;
static uint32_t rfft_current_size = 0;                  // 当前RFFT实例长度
static float32_t fft_input_buffer[ADC_SRV_MAX_FFT_SIZE];    // 输入缓冲区（实数）
static float32_t fft_output_buffer[ADC_SRV_MAX_FFT_SIZE];   // 输出缓冲区（复数）

// 默认配置
static const ADC_SRV_Config_t DEFAULT_CONFIG = {
    .calc_periods = ADC_SRV_DEFAULT_PERIODS,
    .calc_interval_ms = ADC_SRV_CALC_INTERVAL_MS,
    .voltage_scale_ua = ADC_SRV_BASE_VOLT_SCALE,
    .voltage_scale_ub = ADC_SRV_BASE_VOLT_SCALE,
    .voltage_scale_uc = ADC_SRV_BASE_VOLT_SCALE,
    .calib_coef = {
        .coef_ua = ADC_SRV_COEF_DEFAULT,
        .coef_ub = ADC_SRV_COEF_DEFAULT,
        .coef_uc = ADC_SRV_COEF_DEFAULT
    }
};

/* Private function prototypes -----------------------------------------------*/
static int8_t _calculate_voltage_rms(const int16_t *samples, uint32_t count,
                                     float scale, float sample_rate,
                                     float freq_hz, ADC_SRV_VoltageResult_t *result);
static int8_t _calculate_fft_cmsis(const int16_t *samples, uint32_t count,
                                   float sample_rate, float target_freq_hz,
                                   float dc_offset, float *fundamental, float *phase_deg, float *thd);
static uint32_t _get_samples_from_buffer(uint8_t channel, int16_t *output,
                                         uint32_t max_samples);
static uint32_t _round_down_pow2(uint32_t value);
static int8_t _ensure_rfft_instance(uint32_t fft_size);
static inline void _enter_critical(void);
static inline void _exit_critical(void);
static inline float _apply_calib_coef(float base_scale, uint16_t coef, uint16_t vref_adc);

/* Exported functions --------------------------------------------------------*/

/**
 * @brief 初始化ADC服务模块
 */
int8_t ADC_SRV_Init(const ADC_SRV_Config_t *config)
{
    // 清空上下文
    memset(&adc_srv_ctx, 0, sizeof(adc_srv_ctx));

    // 加载配置
    if (config != NULL) {
        // 参数验证
        if (config->calc_periods < ADC_SRV_MIN_PERIODS ||
            config->calc_periods > ADC_SRV_MAX_PERIODS) {
            return -1;
        }
        if (config->calc_interval_ms == 0U) {
            return -1;
        }
        adc_srv_ctx.config = *config;

        // 如果用户未设置缩放系数, 使用默认宏值
        if (adc_srv_ctx.config.voltage_scale_uc <= 0.0f) {
            adc_srv_ctx.config.voltage_scale_uc = ADC_SRV_BASE_VOLT_SCALE;
        }
        if (adc_srv_ctx.config.voltage_scale_ua <= 0.0f) {
            adc_srv_ctx.config.voltage_scale_ua = ADC_SRV_BASE_VOLT_SCALE;
        }
        if (adc_srv_ctx.config.voltage_scale_ub <= 0.0f) {
            adc_srv_ctx.config.voltage_scale_ub = ADC_SRV_BASE_VOLT_SCALE;
        }
    } else {
        adc_srv_ctx.config = DEFAULT_CONFIG;
    }

    // 延迟初始化CMSIS-DSP RFFT实例, 首次FFT计算时根据实际点数配置
    rfft_current_size = 0;

    adc_srv_ctx.state = ADC_SRV_STATE_IDLE;
    return 0;
}

/**
 * @brief 启动ADC采样服务
 */
int8_t ADC_SRV_Start(void)
{
    if (adc_srv_ctx.state == ADC_SRV_STATE_UNINITIALIZED) {
        return -1;
    }

    // 重置缓冲区
    for (uint8_t i = 0; i < ADC_SRV_VOLTAGE_CHANNELS; i++) {
        memset(&adc_srv_ctx.buffers[i], 0, sizeof(ADC_SRV_RingBuffer_t));
    }

    adc_srv_ctx.decimation_counter = 0;
    adc_srv_ctx.last_calc_tick = HAL_GetTick();
    adc_srv_ctx.result_ready = 0;
    memset(adc_results, 0, sizeof(adc_results));
    memset(adc_result_ready, 0, sizeof(adc_result_ready));
    adc_active_idx = 0;
    adc_srv_ctx.state = ADC_SRV_STATE_RUNNING;

    return 0;
}

/**
 * @brief 停止ADC采样服务
 */
int8_t ADC_SRV_Stop(void)
{
    adc_srv_ctx.state = ADC_SRV_STATE_IDLE;
    return 0;
}

/**
 * @brief ADC DMA传输完成回调
 * @note  此函数在中断上下文中执行
 */
void ADC_SRV_DMA_ConvCpltCallback(ADC_HandleTypeDef *hadc, const volatile uint16_t *adc_data)
{
    if (hadc->Instance != ADC1 || adc_data == NULL) {
        return;
    }

    if (adc_srv_ctx.state != ADC_SRV_STATE_RUNNING) {
        return;
    }

    // 降采样：隔1存1
    if (++adc_srv_ctx.decimation_counter < ADC_SRV_DECIMATION_RATIO) {
        return;
    }
    adc_srv_ctx.decimation_counter = 0;

    // 读取VREFINT用于归一化（每次DMA完成都更新）
    adc_srv_ctx.vref_adc_value = adc_data[ADC_CH_VREF];

    // 读取AHALF参考电压（ADC_CHANNEL_13）
    int16_t ahalf = (int16_t)adc_data[ADC_CH_AHALF];

    // 存储三相电压（减去AHALF）到缓冲区
    // buffers数组布局：[0]=UC, [1]=UA, [2]=UB
    const uint8_t channel_map[ADC_SRV_VOLTAGE_CHANNELS] = {
        ADC_CH_UC,  // buffers[0] <- adc_data[ADC_CH_UC]
        ADC_CH_UA,  // buffers[1] <- adc_data[ADC_CH_UA]
        ADC_CH_UB   // buffers[2] <- adc_data[ADC_CH_UB]
    };

    for (uint8_t i = 0; i < ADC_SRV_VOLTAGE_CHANNELS; i++) {
        ADC_SRV_RingBuffer_t *buf = &adc_srv_ctx.buffers[i];

        // 差分电压 = 通道值 - AHALF
        int16_t voltage = (int16_t)adc_data[channel_map[i]] - ahalf;

        // 环形缓冲区写入
        buf->data[buf->write_index] = voltage;
        buf->write_index = (buf->write_index + 1) & ADC_SRV_BUFFER_MASK;
        buf->total_samples++;
    }
}

/**
 * @brief 定时计算任务
 */
int8_t ADC_SRV_CalculateTask(void)
{
    if (adc_srv_ctx.state != ADC_SRV_STATE_RUNNING) {
        return -1;
    }

    // 检查计算间隔
    uint32_t current_tick = HAL_GetTick();
    if ((current_tick - adc_srv_ctx.last_calc_tick) < adc_srv_ctx.config.calc_interval_ms) {
        return 0;  // 未到计算时间
    }
    adc_srv_ctx.last_calc_tick = current_tick;

    // 获取平均频率（从零点检测模块）
    uint32_t freq_mhz = ZC_GetAvgFrequency();  // mHz
    float freq_hz = (freq_mhz > 0U) ? (freq_mhz / 1000.0f) : 50.0f;  // 默认50Hz
    if (freq_hz <= 0.0f) {
        freq_hz = 50.0f;
    }
    const float effective_sample_rate = (float)ADC_SRV_EFFECTIVE_RATE_HZ;

    // 计算需要的采样点数（基于工频周期数）
    float samples_per_period_f = effective_sample_rate / freq_hz;
    uint32_t samples_per_period = (uint32_t)(samples_per_period_f + 0.5f);
    if (samples_per_period == 0U) {
        samples_per_period = 1U;
    }

    uint32_t requested_samples = samples_per_period * adc_srv_ctx.config.calc_periods;
    if (requested_samples < ADC_SRV_MIN_SAMPLE_COUNT) {
        requested_samples = ADC_SRV_MIN_SAMPLE_COUNT;
    }
    if (requested_samples > ADC_SRV_BUFFER_SIZE) {
        requested_samples = ADC_SRV_BUFFER_SIZE;
    }

    if (adc_srv_ctx.buffers[0].total_samples < requested_samples) {
        return 0;  // 数据不足
    }

    adc_srv_ctx.state = ADC_SRV_STATE_CALCULATING;

    // 临时缓冲区（从循环缓冲区提取数据）
    static int16_t temp_buffer[ADC_SRV_BUFFER_SIZE];

    // 读取VREF用于归一化
    uint16_t vref_adc = adc_srv_ctx.vref_adc_value;

    // 计算三相电压（应用校准系数和VREF归一化）
    float scale_factors[3] = {
        _apply_calib_coef(adc_srv_ctx.config.voltage_scale_uc, adc_srv_ctx.config.calib_coef.coef_uc, vref_adc),
        _apply_calib_coef(adc_srv_ctx.config.voltage_scale_ua, adc_srv_ctx.config.calib_coef.coef_ua, vref_adc),
        _apply_calib_coef(adc_srv_ctx.config.voltage_scale_ub, adc_srv_ctx.config.calib_coef.coef_ub, vref_adc)
    };

    // 选择当前写入的结果缓冲区
    uint8_t write_idx = adc_active_idx;
    ADC_SRV_ThreePhaseResult_t *result_buf = &adc_results[write_idx];

    ADC_SRV_VoltageResult_t *results[3] = {
        &result_buf->uc,
        &result_buf->ua,
        &result_buf->ub
    };

    for (uint8_t ch = 0; ch < ADC_SRV_VOLTAGE_CHANNELS; ch++) {
        // 从缓冲区提取最新数据
        uint32_t actual_samples = _get_samples_from_buffer(ch, temp_buffer, requested_samples);

        if (actual_samples >= ADC_SRV_MIN_SAMPLE_COUNT) {
            // 计算RMS和FFT
            _calculate_voltage_rms(temp_buffer,
                                   actual_samples,
                                   scale_factors[ch],
                                   effective_sample_rate,
                                   freq_hz,
                                   results[ch]);
            results[ch]->frequency_hz = freq_hz;
            results[ch]->is_valid = 1;
        } else {
            results[ch]->is_valid = 0;
        }
    }

    // 更新结果时间戳和平均频率
    result_buf->timestamp_ms = current_tick;
    result_buf->avg_frequency_hz = freq_hz;
    adc_result_ready[write_idx] = 1;
    adc_srv_ctx.result_ready = 1;

    // 确保写入完成后再翻转索引，避免读取到半写入数据
    __DMB();
    adc_active_idx = 1U - write_idx;

    adc_srv_ctx.state = ADC_SRV_STATE_RUNNING;
    return 1;  // 计算完成
}

/**
 * @brief 获取最新三相电压结果
 */
int8_t ADC_SRV_GetThreePhaseResult(ADC_SRV_ThreePhaseResult_t *result)
{
    if (result == NULL || !adc_srv_ctx.result_ready) {
        return -1;
    }

    uint8_t read_idx = 1U - adc_active_idx;
    if (!adc_result_ready[read_idx]) {
        return -1;
    }
    *result = adc_results[read_idx];

    return 0;
}

/**
 * @brief 获取单相电压结果
 */
int8_t ADC_SRV_GetVoltageResult(uint8_t channel, ADC_SRV_VoltageResult_t *result)
{
    if (result == NULL || !adc_srv_ctx.result_ready) {
        return -1;
    }

    uint8_t read_idx = 1U - adc_active_idx;
    if (!adc_result_ready[read_idx]) {
        return -1;
    }
    ADC_SRV_ThreePhaseResult_t *read_buf = &adc_results[read_idx];

    switch (channel) {
        case ADC_CH_UA:
            *result = read_buf->ua;
            break;
        case ADC_CH_UB:
            *result = read_buf->ub;
            break;
        case ADC_CH_UC:
            *result = read_buf->uc;
            break;
        default:
            return -1;
    }

    return 0;
}

/**
 * @brief 获取模块状态
 */
ADC_SRV_State_t ADC_SRV_GetState(void)
{
    return adc_srv_ctx.state;
}

/**
 * @brief 获取缓冲区填充率
 */
uint8_t ADC_SRV_GetBufferFillLevel(void)
{
    uint32_t total = adc_srv_ctx.buffers[0].total_samples;
    if (total >= ADC_SRV_BUFFER_SIZE) {
        return 100;
    }
    return (uint8_t)((total * 100) / ADC_SRV_BUFFER_SIZE);
}

/**
 * @brief 设置计算周期数
 */
int8_t ADC_SRV_SetCalcPeriods(uint8_t periods)
{
    if (periods < ADC_SRV_MIN_PERIODS || periods > ADC_SRV_MAX_PERIODS) {
        return -1;
    }
    adc_srv_ctx.config.calc_periods = periods;
    return 0;
}

/**
 * @brief 设置电压缩放系数
 */
int8_t ADC_SRV_SetVoltageScale(uint8_t channel, float scale)
{
    if (scale <= 0.0f) {
        return -1;
    }

    switch (channel) {
        case ADC_CH_UA:
            adc_srv_ctx.config.voltage_scale_ua = scale;
            break;
        case ADC_CH_UB:
            adc_srv_ctx.config.voltage_scale_ub = scale;
            break;
        case ADC_CH_UC:
            adc_srv_ctx.config.voltage_scale_uc = scale;
            break;
        default:
            return -1;
    }
    return 0;
}

/**
 * @brief 设置校准系数
 */
int8_t ADC_SRV_SetCalibCoef(uint8_t channel, uint16_t coef)
{
    // 范围检查
    if (coef < ADC_SRV_COEF_MIN || coef > ADC_SRV_COEF_MAX) {
        return -1;
    }

    switch (channel) {
        case ADC_CH_UA:
            adc_srv_ctx.config.calib_coef.coef_ua = coef;
            break;
        case ADC_CH_UB:
            adc_srv_ctx.config.calib_coef.coef_ub = coef;
            break;
        case ADC_CH_UC:
            adc_srv_ctx.config.calib_coef.coef_uc = coef;
            break;
        default:
            return -1;
    }

    return 0;
}

/**
 * @brief 获取校准系数
 */
int8_t ADC_SRV_GetCalibCoef(uint8_t channel, uint16_t *coef)
{
    if (coef == NULL) {
        return -1;
    }

    switch (channel) {
        case ADC_CH_UA:
            *coef = adc_srv_ctx.config.calib_coef.coef_ua;
            break;
        case ADC_CH_UB:
            *coef = adc_srv_ctx.config.calib_coef.coef_ub;
            break;
        case ADC_CH_UC:
            *coef = adc_srv_ctx.config.calib_coef.coef_uc;
            break;
        default:
            return -1;
    }

    return 0;
}

/**
 * @brief 重置统计信息
 */
int8_t ADC_SRV_ResetStats(void)
{
    for (uint8_t i = 0; i < ADC_SRV_VOLTAGE_CHANNELS; i++) {
        adc_srv_ctx.buffers[i].total_samples = 0;
        adc_srv_ctx.buffers[i].write_index = 0;
    }
    adc_srv_ctx.result_ready = 0;
    return 0;
}

/* Private functions ---------------------------------------------------------*/

/**
 * @brief 计算电压RMS和THD
 * @note  RMS和FFT使用相同的样本集, 确保结果一致性
 */
static int8_t _calculate_voltage_rms(const int16_t *samples, uint32_t count,
                                     float scale, float sample_rate,
                                     float freq_hz, ADC_SRV_VoltageResult_t *result)
{
    if (samples == NULL || result == NULL || count == 0) {
        return -1;
    }

    // 确定FFT实际使用的样本数（不超过MAX_FFT_SIZE, 向下取2的幂）
    uint32_t usable_samples = (count < ADC_SRV_MAX_FFT_SIZE) ? count : ADC_SRV_MAX_FFT_SIZE;
    uint32_t fft_size = _round_down_pow2(usable_samples);
    if (fft_size < 32U) {
        return -1;  // 样本数太少
    }

    // 使用FFT实际处理的最新样本集进行所有计算（保证一致性）
    const int16_t *fft_samples = samples + (count - fft_size);
    uint32_t effective_count = fft_size;

    // 计算直流分量（去除DC偏移）
    int32_t dc_sum = 0;
    for (uint32_t i = 0; i < effective_count; i++) {
        dc_sum += fft_samples[i];
    }
    float dc_offset = (float)dc_sum / (float)effective_count;

    // 计算RMS（整体频谱, 与FFT使用相同样本）
    uint64_t sum_squares = 0;
    for (uint32_t i = 0; i < effective_count; i++) {
        float val = (float)fft_samples[i] - dc_offset;
        sum_squares += (uint64_t)(val * val);
    }
    float rms_code = sqrtf((float)sum_squares / (float)effective_count);
    result->rms_voltage = rms_code * scale;
    result->sample_count = effective_count;

    // 使用CMSIS-DSP进行FFT计算（基波、相位和THD）
    float fundamental = 0.0f;
    float phase_deg = 0.0f;
    float thd = 0.0f;
    _calculate_fft_cmsis(fft_samples,
                        effective_count,
                        sample_rate,
                        freq_hz,
                        dc_offset,
                        &fundamental,
                        &phase_deg,
                        &thd);

    result->fundamental_voltage = fundamental * scale;
    result->phase_angle_deg = phase_deg;
    result->thd_percent = thd;

    return 0;
}

/**
 * @brief 使用CMSIS-DSP库进行FFT计算（基波、相位和THD）
 * @param samples: 输入样本数组（int16_t, 已确保是2的幂次方点数）
 * @param count: 样本数量（FFT点数, 必须是2的幂）
 * @param sample_rate: 采样率（Hz）
 * @param target_freq_hz: 目标频率（基波频率, Hz）
 * @param dc_offset: 直流偏移量（已计算好）
 * @param fundamental: 输出基波幅值（RMS）
 * @param phase_deg: 输出基波相位角（度）
 * @param thd: 输出总谐波失真（%）
 */
static int8_t _calculate_fft_cmsis(const int16_t *samples, uint32_t count,
                                   float sample_rate, float target_freq_hz,
                                   float dc_offset, float *fundamental, float *phase_deg, float *thd)
{
    if (samples == NULL || fundamental == NULL || phase_deg == NULL || thd == NULL) {
        return -1;
    }
    if (count < 32U || count > ADC_SRV_MAX_FFT_SIZE) {
        return -1;
    }

    // 参数验证和默认值
    if (sample_rate <= 0.0f) {
        sample_rate = (float)ADC_SRV_EFFECTIVE_RATE_HZ;
    }
    if (target_freq_hz <= 0.0f) {
        target_freq_hz = 50.0f;
    }

    // 确保RFFT实例与count匹配
    if (_ensure_rfft_instance(count) != 0) {
        return -1;
    }

    // 转换为float并去除DC偏移
    for (uint32_t i = 0; i < count; i++) {
        fft_input_buffer[i] = (float)samples[i] - dc_offset;
    }

    // 执行实数FFT（arm_rfft_fast_f32输出格式：[Real0, Real1, ..., RealN/2, Imag1, ..., ImagN/2-1]）
    arm_rfft_fast_f32(&rfft_instance, fft_input_buffer, fft_output_buffer, 0);

    // 计算基波频点索引
    float freq_resolution = sample_rate / (float)count;
    uint32_t fundamental_bin = (uint32_t)((target_freq_hz / freq_resolution) + 0.5f);
    if (fundamental_bin == 0U) {
        fundamental_bin = 1U;
    }
    if (fundamental_bin >= (count / 2U)) {
        fundamental_bin = (count / 2U) - 1U;
    }

    // 提取基波实部和虚部（CMSIS-DSP输出格式特殊处理）
    float real_fundamental, imag_fundamental;
    if (fundamental_bin == 0U) {
        // DC分量
        real_fundamental = fft_output_buffer[0];
        imag_fundamental = 0.0f;
    } else if (fundamental_bin == (count / 2U)) {
        // 奈奎斯特频率
        real_fundamental = fft_output_buffer[1];
        imag_fundamental = 0.0f;
    } else {
        // 一般频点：实部在前半部分, 虚部在后半部分
        real_fundamental = fft_output_buffer[fundamental_bin * 2];
        imag_fundamental = fft_output_buffer[fundamental_bin * 2 + 1];
    }

    // 计算基波幅值（转换为RMS）
    float magnitude_fundamental = sqrtf(real_fundamental * real_fundamental +
                                       imag_fundamental * imag_fundamental);
    *fundamental = magnitude_fundamental / sqrtf(2.0f) / (float)count * 2.0f;

    // 计算基波相位角（弧度转度, atan2返回-π到π）
    *phase_deg = atan2f(imag_fundamental, real_fundamental) * 180.0f / PI;

    // 计算谐波能量（2~10次谐波）
    float harmonic_energy = 0.0f;
    for (uint8_t h = 2; h <= ADC_SRV_MAX_HARMONICS; h++) {
        float harmonic_freq = target_freq_hz * (float)h;
        if (harmonic_freq >= (sample_rate * 0.5f)) {
            break;
        }
        uint32_t harmonic_bin = (uint32_t)((harmonic_freq / freq_resolution) + 0.5f);
        if (harmonic_bin == 0U || harmonic_bin >= (count / 2U)) {
            break;
        }

        float real_harmonic, imag_harmonic;
        if (harmonic_bin == (count / 2U)) {
            real_harmonic = fft_output_buffer[1];
            imag_harmonic = 0.0f;
        } else {
            real_harmonic = fft_output_buffer[harmonic_bin * 2];
            imag_harmonic = fft_output_buffer[harmonic_bin * 2 + 1];
        }

        float magnitude_harmonic = sqrtf(real_harmonic * real_harmonic +
                                        imag_harmonic * imag_harmonic);
        float rms_harmonic = magnitude_harmonic / sqrtf(2.0f) / (float)count * 2.0f;
        harmonic_energy += rms_harmonic * rms_harmonic;
    }

    // 计算THD
    if (*fundamental > 0.0f && harmonic_energy > 0.0f) {
        *thd = sqrtf(harmonic_energy) / (*fundamental) * 100.0f;
    } else {
        *thd = 0.0f;
    }

    return 0;
}

/**
 * @brief 计算不超过value的最大2次幂
 */
static uint32_t _round_down_pow2(uint32_t value)
{
    if (value == 0U) {
        return 0U;
    }

    uint32_t pow2 = 1U;
    while ((pow2 << 1U) != 0U && (pow2 << 1U) <= value) {
        pow2 <<= 1U;
    }
    return pow2;
}

/**
 * @brief 确保RFFT实例与指定点数匹配
 */
static int8_t _ensure_rfft_instance(uint32_t fft_size)
{
    if (fft_size == 0U) {
        return -1;
    }

    if (fft_size == rfft_current_size) {
        return 0;
    }

    arm_status status = arm_rfft_fast_init_f32(&rfft_instance, fft_size);
    if (status != ARM_MATH_SUCCESS) {
        rfft_current_size = 0;
        return -1;
    }

    rfft_current_size = fft_size;
    return 0;
}

/**
 * @brief 从循环缓冲区提取最新N个样本
 */
static uint32_t _get_samples_from_buffer(uint8_t channel, int16_t *output,
                                         uint32_t max_samples)
{
    if (channel >= ADC_SRV_VOLTAGE_CHANNELS || output == NULL || max_samples == 0U) {
        return 0;
    }

    ADC_SRV_RingBuffer_t *buf = &adc_srv_ctx.buffers[channel];

    // 快照当前写指针和总样本数, 避免长时间关中断
    _enter_critical();
    uint16_t write_index = buf->write_index;
    uint32_t total_samples = buf->total_samples;
    _exit_critical();

    uint32_t available = (total_samples < ADC_SRV_BUFFER_SIZE) ?
                         total_samples : ADC_SRV_BUFFER_SIZE;
    if (available == 0U) {
        return 0;
    }

    uint32_t to_extract = (max_samples < available) ? max_samples : available;

    uint16_t read_index;
    if (write_index >= to_extract) {
        read_index = write_index - to_extract;
    } else {
        read_index = ADC_SRV_BUFFER_SIZE - (to_extract - write_index);
    }

    _enter_critical();
    if ((uint32_t)read_index + to_extract <= ADC_SRV_BUFFER_SIZE) {
        memcpy(output, &buf->data[read_index], to_extract * sizeof(int16_t));
    } else {
        uint32_t first_chunk = ADC_SRV_BUFFER_SIZE - read_index;
        memcpy(output, &buf->data[read_index], first_chunk * sizeof(int16_t));
        memcpy(output + first_chunk,
               &buf->data[0],
               (to_extract - first_chunk) * sizeof(int16_t));
    }
    _exit_critical();

    return to_extract;
}

/**
 * @brief 进入临界区（仅屏蔽 ADC DMA 中断）
 */
static inline void _enter_critical(void)
{
    NVIC_DisableIRQ(DMA2_Stream4_IRQn);
}

/**
 * @brief 退出临界区
 */
static inline void _exit_critical(void)
{
    NVIC_EnableIRQ(DMA2_Stream4_IRQn);
}

/**
 * @brief 应用校准系数到基础缩放系数
 * @param base_scale: 基础缩放系数（V/V, 电压比）
 * @param coef: 校准系数（5000~15000, 10000=1.0倍）
 * @param vref_adc: VREFINT的ADC采样值（用于归一化）
 * @retval 校准后的缩放系数（V/ADC_CODE）
 * @note 计算公式：calibrated_scale = base_scale * (coef / 10000.0) * (VREFINT / vref_adc)
 */
static inline float _apply_calib_coef(float base_scale, uint16_t coef, uint16_t vref_adc)
{
    // 范围检查
    if (coef < ADC_SRV_COEF_MIN || coef > ADC_SRV_COEF_MAX) {
        coef = ADC_SRV_COEF_DEFAULT;
    }

    if (vref_adc == 0U) {
        vref_adc = 1982U; // 回退到典型值 1.21V/2.5V*4095 //注意这是跟硬件相关
    }

    // VREFINT_CAL 是校准值, 通常存储在地址 0x1FFF7A2A, 用来校正参考电压
    // vrefint_voltage 典型值为 1.21V (在 3.3V VDDA 下, ADC值约为 1501.5)
    // VDDA = vrefint_voltage * 4095 / vref_adc
    uint16_t VREFINT_CAL = *((uint16_t *)0x1FFF7A2AU);
    if (VREFINT_CAL == 0U || VREFINT_CAL == 0xFFFFU) {
        VREFINT_CAL = 1501U;  // 回退到典型值（≈1.21V @ 3.3V）
    }
    float vrefint_voltage = ((float)VREFINT_CAL / 4095.0f) * 3.3f;

    // 每个ADC码对应的实际引脚电压
    float adc_lsb = vrefint_voltage / (float)vref_adc;

    return base_scale * ((float)coef / (float)ADC_SRV_COEF_BASE) * adc_lsb;
}

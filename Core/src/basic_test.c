/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : basic_test.c
  * @brief          : basic_test
  * @author         : zhengxu
  ******************************************************************************
  * @details        ： 所有与测试相关的函数在此文件中定义
  *
  ******************************************************************************
  */

/* USER CODE END Header */
#include "main.h"
#include "usart.h"
#include "adc.h"

#include "basic_driver.h"
#include "basic_test.h"
#include "zero_crossing.h"
#include "adc_service.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>  // va_list支持

/**
  ******************************************************************************
  *
  * 串口输出缓冲区管理
  *
  ******************************************************************************
*/

// 串口输出缓冲区（全局静态变量）
#define UART_BUFFER_SIZE 512
typedef struct {
    UART_HandleTypeDef *huart;
    char buffer[UART_BUFFER_SIZE];
    uint16_t length;
} UART_BufferChannel_t;

static UART_BufferChannel_t uart_buffer_channels[] = {
    { &huart1, {0}, 0 },
    { &huart2, {0}, 0 }
};

#define UART_BUFFER_COUNT (sizeof(uart_buffer_channels) / sizeof(uart_buffer_channels[0]))

/**
  * @brief  根据UART句柄查找缓冲区
  * @param  huart: UART句柄
  * @retval 缓冲区指针，NULL表示未找到
  */
static UART_BufferChannel_t *uart_buffer_get(UART_HandleTypeDef *huart)
{
    for (uint8_t i = 0; i < UART_BUFFER_COUNT; i++) {
        if (uart_buffer_channels[i].huart == huart) {
            return &uart_buffer_channels[i];
        }
    }
    return NULL;
}

/**
  * @brief  重置指定串口缓冲区
  * @param  huart: UART句柄
  * @retval None
  */
static void uart_buffer_reset(UART_HandleTypeDef *huart)
{
    UART_BufferChannel_t *channel = uart_buffer_get(huart);
    if (channel == NULL) {
        return;
    }

    channel->length = 0;
    channel->buffer[0] = '\0';
}

/**
  * @brief  向串口缓冲区追加格式化字符串（带溢出保护）
  * @param  huart: UART句柄
  * @param  format: 格式化字符串（printf风格）
  * @param  ...: 可变参数
  * @retval 0: 成功, -1: 缓冲区溢出或句柄非法
  */
static int uart_buffer_append(UART_HandleTypeDef *huart, const char *format, ...)
{
    UART_BufferChannel_t *channel = uart_buffer_get(huart);
    if (channel == NULL) {
        return -1;
    }

    va_list args;
    va_start(args, format);

    int remaining = UART_BUFFER_SIZE - channel->length - 1;
    if (remaining <= 0) {
        va_end(args);
        return -1;  // 缓冲区已满
    }

    int written = vsnprintf(channel->buffer + channel->length, remaining + 1, format, args);

    va_end(args);

    if (written < 0 || written > remaining) {
        channel->length = UART_BUFFER_SIZE - 1;
        channel->buffer[channel->length] = '\0';
        return -1;
    }

    channel->length += (uint16_t)written;
    return 0;
}

/**
  * @brief  通过DMA发送缓冲区内容
  * @param  huart: UART句柄
  * @retval None
  */
static void uart_buffer_flush(UART_HandleTypeDef *huart)
{
    UART_BufferChannel_t *channel = uart_buffer_get(huart);
    if (channel == NULL || channel->length == 0) {
        return;
    }

    HAL_UART_Transmit_DMA(channel->huart, (uint8_t*)channel->buffer, channel->length);
}

static int format_separator(UART_HandleTypeDef *huart)
{
    return uart_buffer_append(huart, "----------------------------------------\n");
}

/**
  ******************************************************************************
  *
  * 添加测试数据函数
  *
  ******************************************************************************
*/

/**
  * @brief  格式化VREFINT数据到缓冲区
  * @retval 0: 成功, -1: 失败
  */
static int format_vrefint_data(UART_HandleTypeDef *huart)
{
    uint16_t vrefint_adc = adc_dma_buffer[ADC_CH_VREF];
    if (vrefint_adc == 0U) {
        return -1;
    }

    uint16_t vrefint_cal = *((uint16_t *)0x1FFF7A2AU);
    if (vrefint_cal == 0U || vrefint_cal == 0xFFFFU) {
        vrefint_cal = 1501U;  // 典型值回退
    }

    uint32_t vrefint_mv = (uint32_t)vrefint_cal * 3300UL / 4095UL;
    uint32_t vdda_mv = (vrefint_mv * 4095UL) / vrefint_adc;

    return uart_buffer_append(huart,
                              "VREFINT ADC Value: %u, VDDA: %lu.%03luV\n",
                              vrefint_adc, vdda_mv / 1000, vdda_mv % 1000);
}

/**
  * @brief  格式化单个通道的频率数据到缓冲区
  * @param  channel: 通道选择（ZC_CHANNEL_A 或 ZC_CHANNEL_C）
  * @param  channel_name: 通道名称字符串（用于显示）
  * @retval 0: 成功, -1: 失败
  */
static int format_frequency_data(UART_HandleTypeDef *huart,
                                 ZC_Channel_t channel,
                                 const char *channel_name)
{
    ZC_Data_t zc_data;
    int ret;

    // 获取频率数据
    ret = ZC_GetData(channel, &zc_data);

    if (ret == 0 && zc_data.is_valid) {
        // frequency_avg_mhz + 5 配置 %02lu = 四舍五入 0.01Hz
        uint32_t frequency = zc_data.frequency_avg_mhz;
        return uart_buffer_append(huart, "%s:%lu.%03luHz\n",
                                  channel_name, frequency / 1000, frequency % 1000);
    } else {
        // 数据无效时，打印统计计数
        return uart_buffer_append(huart,
                                  "%s freq: data invalid (valid_cnt=%lu, error_cnt=%lu)\n",
                                  channel_name, zc_data.valid_count, zc_data.error_count);
    }
}

/**
  * @brief  返回假频率+功率数据（CSV: frequency,power）
  * @note   签名与 `format_frequency_data` 相同，便于替换调用。
  *         假设函数被周期调用（500ms），用于模拟一次调频场景。
  */
static int format_frequency_data_fake(UART_HandleTypeDef *huart,
                                      ZC_Channel_t channel,
                                      const char *channel_name)
{
    (void)channel;
    (void)channel_name;

    typedef struct {
        uint32_t frequency_mhz;  // 频率(mHz)
        int32_t power_start_x10; // 起始功率(0.1MW)
        int32_t power_end_x10;   // 目标功率(0.1MW)
        uint16_t steps;          // 帧数(500ms/帧)
    } FakeSegment_t;

    /*
     * 基准频率50Hz、额定功率600MW；火电一次调频限幅+/-6%(+/-36MW)。
     * 小频差：|df|<=0.08Hz；大频差：|df|>0.08Hz。
     *
     * 场景覆盖：
     * 1) 10s动作后恢复（正常响应）
     * 2) 延迟过长（>3s）不满足火电要求
     * 3) 出力不足（未达理论90%）
     * 4) 过调节（>120%）
     * 5) 长动作（>60s）
     * 6) 频率上偏（减出力）
     */
#define SEG(freq, p0, p1, n) { (freq), (p0), (p1), (n) }
    static const FakeSegment_t kFakeSegments[] = {
        // 基线：50Hz, 600MW（5s）
        SEG(50000U, 6000, 6000, 10),

        // 1) 10s动作后恢复：df=-0.05Hz (49.950Hz)，目标约+12MW
        SEG(49950U, 6000, 6120, 6),   // 3s内起调并爬坡
        SEG(49950U, 6120, 6120, 14),  // 持续补偿至满10s
        SEG(50000U, 6120, 6000, 6),   // 3s恢复
        SEG(50000U, 6000, 6000, 10),  // 基线缓冲

        // 2) 延迟过长：df=-0.05Hz，延迟>3s后才起调
        SEG(49950U, 6000, 6000, 12),  // 延迟6s
        SEG(49950U, 6000, 6120, 10),  // 5s爬坡
        SEG(49950U, 6120, 6120, 10),  // 5s保持
        SEG(50000U, 6120, 6000, 8),   // 4s恢复
        SEG(50000U, 6000, 6000, 10),  // 基线缓冲

        // 3) 出力不足：df=-0.12Hz (49.880Hz)，理论约+28.8MW，仅到+16MW
        SEG(49880U, 6000, 6160, 12),  // 6s爬坡
        SEG(49880U, 6160, 6160, 20),  // 10s保持
        SEG(50000U, 6160, 6000, 10),  // 5s恢复
        SEG(50000U, 6000, 6000, 10),  // 基线缓冲

        // 4) 过调节：df=-0.05Hz，超过120%（+18MW≈150%）
        SEG(49950U, 6000, 6180, 6),   // 3s爬坡
        SEG(49950U, 6180, 6180, 10),  // 5s保持
        SEG(50000U, 6180, 6000, 6),   // 3s恢复
        SEG(50000U, 6000, 6000, 10),  // 基线缓冲

        // 5) 长动作>60s：df=-0.10Hz (49.900Hz)，目标约+24MW
        SEG(49900U, 6000, 6240, 12),  // 6s爬坡
        SEG(49900U, 6240, 6240, 120), // 60s保持
        SEG(50000U, 6240, 6000, 12),  // 6s恢复
        SEG(50000U, 6000, 6000, 10),  // 基线缓冲

        // 6) 频率上偏（减出力）：df=+0.05Hz (50.050Hz)，目标约-12MW
        SEG(50050U, 6000, 5880, 6),   // 3s爬坡
        SEG(50050U, 5880, 5880, 14),  // 7s保持
        SEG(50000U, 5880, 6000, 6),   // 3s恢复
        SEG(50000U, 6000, 6000, 10)   // 基线缓冲
    };
#undef SEG

    static uint16_t segment_index = 0U;
    static uint16_t segment_step = 0U;

    const uint16_t segment_count
        = (uint16_t)(sizeof(kFakeSegments) / sizeof(kFakeSegments[0]));
    const FakeSegment_t *segment = &kFakeSegments[segment_index];

    uint16_t steps = segment->steps > 0U ? segment->steps : 1U;
    int32_t power_x10 = segment->power_end_x10;
    if (steps > 1U) {
        int32_t delta = segment->power_end_x10 - segment->power_start_x10;
        power_x10 = segment->power_start_x10
                    + (delta * (int32_t)segment_step)
                        / (int32_t)(steps - 1U);
    }

    int res = uart_buffer_append(huart,
                                 "BEGIN:%lu.%03lu,%lu.%01luEND\n",
                                 segment->frequency_mhz / 1000U,
                                 segment->frequency_mhz % 1000U,
                                 (uint32_t)power_x10 / 10U,
                                 (uint32_t)power_x10 % 10U);

    segment_step++;
    if (segment_step >= steps) {
        segment_step = 0U;
        segment_index++;
        if (segment_index >= segment_count) {
            segment_index = 0U;
        }
    }

    return res;
}

/**
  * @brief  格式化模块状态调试信息（用于初始化问题排查）
  * @retval 0: 成功, -1: 失败
  */
static int format_zc_module_status(UART_HandleTypeDef *huart)
{
    uint8_t initialized = 0;
    uint8_t running = 0;
    int8_t start_result_ch3 = -99;
    int8_t start_result_ch4 = -99;

    // 获取模块状态
    ZC_GetModuleStatus(&initialized, &running, &start_result_ch3, &start_result_ch4);

    return uart_buffer_append(
        huart,
        "[DEBUG] Module: initialized=%u, running=%u, CH3_start=%d, CH4_start=%d\n",
        initialized,
        running,
        start_result_ch3,
        start_result_ch4
    );
}

/**
  * @brief  浮点数转换为千分位整数（ + 0.5f 四舍五入; 用于避免printf浮点）
  * @param  value: 浮点输入
  * @retval 千分位整数
  */
static uint32_t float_to_milli(float value)
{
    if (value < 0.0f) {
        value = 0.0f;
    }
    return (uint32_t)(value * 1000.0f + 0.5f);
}

/**
  * @brief  追加单相电压与频率结果
  * @param  phase_name: 相别字符串
  * @param  result: ADC结果
  */
static void append_voltage_measurement(UART_HandleTypeDef *huart,
                                       const char *phase_name,
                                       const ADC_SRV_VoltageResult_t *result)
{
    if (result == NULL) {
        return;
    }

    if (result->is_valid) {
        uint32_t voltage_mv = float_to_milli(result->rms_voltage);
        uint32_t phase_angle = float_to_milli(result->phase_angle_deg);
        uart_buffer_append(huart,
                           "%s: %lu.%03lu V (RMS), %lu.%03lu degrees\n",
                           phase_name,
                           voltage_mv / 1000, voltage_mv % 1000,
                           phase_angle / 1000, phase_angle % 1000);
    } else {
        uart_buffer_append(huart, "%s: data invalid\n", phase_name);
    }
}

/**
  * @brief  格式化三相电压结果
  * @retval 0: 成功, -1: 数据未准备好
  */
static int format_three_phase_voltage_data(UART_HandleTypeDef *huart)
{
    ADC_SRV_ThreePhaseResult_t result;
    if (ADC_SRV_GetThreePhaseResult(&result) != 0) {
        return uart_buffer_append(huart, "[ADC] data not ready\n");
    }

    append_voltage_measurement(huart, "UA", &result.ua);
    // append_voltage_measurement(huart, "UB", &result.ub);
    // append_voltage_measurement(huart, "UC", &result.uc);

    return 0;
}

/**
  ******************************************************************************
  *
  * 主打印函数
  *
  ******************************************************************************
*/

/**
  * @brief  打印测试数据到串口（VREFINT电压 + 过零检测频率 + 调试信息）
  * @note   使用模块化设计，各数据独立格式化
  * @retval None
  */
void print_test_data(void)
{
    UART_HandleTypeDef *table_uart = &huart1;
    UART_HandleTypeDef *view_uart = &huart2;

    uart_buffer_reset(view_uart);
    // format_vrefint_data(uart);                // VREFINT电压数据
    format_frequency_data(view_uart, ZC_CHANNEL_A, "A");  // A相频率数据
    // format_frequency_data(view_uart, ZC_CHANNEL_B, "B");  // B相频率数据

    format_three_phase_voltage_data(view_uart);  // 输出ADC三相电压与频率
    format_separator(view_uart);
    uart_buffer_flush(view_uart);

    uart_buffer_reset(table_uart);
    // format_frequency_data(table_uart, ZC_CHANNEL_A, "A");
    format_frequency_data_fake(table_uart, ZC_CHANNEL_A, "A");  // fake
    uart_buffer_flush(table_uart);
}


/*!
    \file    com_driver.c
    \brief   Communication driver implementation for PMU application
    
    \version 2025-08-29, V1.0.0, PMU project for GD32F4xx
*/

#include "com_driver.h"
#include "gd32f4xx_usart.h"
#include "gd32f4xx_rcu.h"
#include "gd32f4xx_gpio.h"
#include <string.h>
#include <stdio.h>

/* =================== 私有变量 =================== */
static TaskHandle_t g_com_task_handle = NULL;
static QueueHandle_t g_phasor_queue = NULL;
static com_config_t g_config;
static com_statistics_t g_stats;

static uint8_t g_tx_buffer[COM_TX_BUFFER_SIZE];
static uint8_t g_rx_buffer[COM_RX_BUFFER_SIZE];
static volatile uint16_t g_rx_index = 0;

/* =================== 私有函数声明 =================== */
static void com_task_function(void *pvParameters);
static BaseType_t com_uart_send(const uint8_t* data, uint16_t length);
static uint16_t com_calculate_checksum(const uint8_t* data, uint16_t length);
static void com_process_rx_data(void);

/* =================== 公共函数实现 =================== */

BaseType_t com_driver_init(QueueHandle_t phasor_queue, const com_config_t* config)
{
    g_phasor_queue = phasor_queue;
    
    /* 复制配置参数 */
    if (config != NULL) {
        memcpy(&g_config, config, sizeof(com_config_t));
    } else {
        com_get_default_config(&g_config);
    }
    
    /* 初始化统计信息 */
    memset(&g_stats, 0, sizeof(com_statistics_t));
    
    /* 创建通信任务 */
    BaseType_t result = xTaskCreate(
        com_task_function,
        "ComTask",
        COM_TASK_STACK_SIZE / 4,
        NULL,
        COM_TASK_PRIORITY,
        &g_com_task_handle
    );
    
    return result;
}

void com_driver_start(void)
{
    if (g_com_task_handle != NULL) {
        vTaskResume(g_com_task_handle);
    }
}

void com_driver_stop(void)
{
    if (g_com_task_handle != NULL) {
        vTaskSuspend(g_com_task_handle);
    }
}

BaseType_t com_send_phasor_data(const phasor_result_t* phasor_data)
{
    if (phasor_data == NULL) {
        return pdFAIL;
    }
    
    uint16_t length = 0;
    
    /* 根据协议类型编码数据 */
    switch (g_config.protocol) {
        case COM_PROTOCOL_IEEE_C37_118:
            length = com_encode_ieee_c37118(phasor_data, g_tx_buffer, COM_TX_BUFFER_SIZE);
            break;
        case COM_PROTOCOL_CUSTOM:
            length = com_encode_custom(phasor_data, g_tx_buffer, COM_TX_BUFFER_SIZE);
            break;
        default:
            return pdFAIL;
    }
    
    if (length > 0) {
        BaseType_t result = com_uart_send(g_tx_buffer, length);
        if (result == pdPASS) {
            g_stats.tx_packets++;
            g_stats.last_tx_time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        } else {
            g_stats.tx_errors++;
        }
        return result;
    }
    
    return pdFAIL;
}

BaseType_t com_send_status(uint16_t status_code, const char* message)
{
    /* 简单的状态消息格式 */
    uint16_t length = snprintf((char*)g_tx_buffer, COM_TX_BUFFER_SIZE,
                              "STATUS:%04X:%s\r\n", status_code, message ? message : "");
    
    if (length > 0 && length < COM_TX_BUFFER_SIZE) {
        return com_uart_send(g_tx_buffer, length);
    }
    
    return pdFAIL;
}

void com_get_default_config(com_config_t* config)
{
    config->protocol = COM_PROTOCOL_CUSTOM;
    config->pmu_id = 1;
    config->data_rate_ms = 20; // 50Hz数据率
    config->enable_timestamp = true;
    config->enable_checksum = true;
}

void com_get_statistics(com_statistics_t* stats)
{
    taskENTER_CRITICAL();
    memcpy(stats, &g_stats, sizeof(com_statistics_t));
    taskEXIT_CRITICAL();
}

void com_reset_statistics(void)
{
    taskENTER_CRITICAL();
    memset(&g_stats, 0, sizeof(com_statistics_t));
    taskEXIT_CRITICAL();
}

bool com_is_link_active(void)
{
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t timeout_ms = 5000; // 5秒超时
    
    return (current_time - g_stats.last_tx_time_ms) < timeout_ms;
}

/* =================== 协议编码函数 =================== */

uint16_t com_encode_ieee_c37118(const phasor_result_t* phasor_data, 
                               uint8_t* buffer, uint16_t buffer_size)
{
    /* IEEE C37.118-2011标准的简化实现 */
    /* 这里只是一个示例，实际实现需要按照标准的详细格式 */
    
    if (buffer_size < 64) { // 最小包大小检查
        return 0;
    }
    
    uint16_t index = 0;
    
    /* Sync word (2 bytes) */
    buffer[index++] = 0xAA;
    buffer[index++] = 0x01; // Data frame
    
    /* Frame size (2 bytes) */
    uint16_t frame_size = 32 + ADC_CHANNELS_NUM * 8; // 简化计算
    buffer[index++] = (frame_size >> 8) & 0xFF;
    buffer[index++] = frame_size & 0xFF;
    
    /* PMU ID (2 bytes) */
    buffer[index++] = (g_config.pmu_id >> 8) & 0xFF;
    buffer[index++] = g_config.pmu_id & 0xFF;
    
    /* SOC (4 bytes) - 时间戳 */
    uint32_t timestamp = phasor_data->timestamp_ms / 1000;
    buffer[index++] = (timestamp >> 24) & 0xFF;
    buffer[index++] = (timestamp >> 16) & 0xFF;
    buffer[index++] = (timestamp >> 8) & 0xFF;
    buffer[index++] = timestamp & 0xFF;
    
    /* FRACSEC (4 bytes) - 毫秒部分 */
    uint32_t fracsec = (phasor_data->timestamp_ms % 1000) * 1000; // 转换为微秒
    buffer[index++] = (fracsec >> 24) & 0xFF;
    buffer[index++] = (fracsec >> 16) & 0xFF;
    buffer[index++] = (fracsec >> 8) & 0xFF;
    buffer[index++] = fracsec & 0xFF;
    
    /* 相量数据 (每个通道8字节：4字节幅度 + 4字节相位) */
    for (int ch = 0; ch < ADC_CHANNELS_NUM; ch++) {
        /* 幅度 (IEEE 754单精度浮点) */
        union { float f; uint32_t i; } amp;
        amp.f = phasor_data->amplitude[ch];
        buffer[index++] = (amp.i >> 24) & 0xFF;
        buffer[index++] = (amp.i >> 16) & 0xFF;
        buffer[index++] = (amp.i >> 8) & 0xFF;
        buffer[index++] = amp.i & 0xFF;
        
        /* 相位 (IEEE 754单精度浮点) */
        union { float f; uint32_t i; } phase;
        phase.f = phasor_data->phase[ch];
        buffer[index++] = (phase.i >> 24) & 0xFF;
        buffer[index++] = (phase.i >> 16) & 0xFF;
        buffer[index++] = (phase.i >> 8) & 0xFF;
        buffer[index++] = phase.i & 0xFF;
    }
    
    /* 频率和ROCOF (8字节) */
    union { float f; uint32_t i; } freq, rocof;
    freq.f = phasor_data->frequency[0]; // 使用第一个通道的频率
    rocof.f = phasor_data->rocof[0];
    
    buffer[index++] = (freq.i >> 24) & 0xFF;
    buffer[index++] = (freq.i >> 16) & 0xFF;
    buffer[index++] = (freq.i >> 8) & 0xFF;
    buffer[index++] = freq.i & 0xFF;
    
    buffer[index++] = (rocof.i >> 24) & 0xFF;
    buffer[index++] = (rocof.i >> 16) & 0xFF;
    buffer[index++] = (rocof.i >> 8) & 0xFF;
    buffer[index++] = rocof.i & 0xFF;
    
    /* CHK (2 bytes) - 校验和 */
    if (g_config.enable_checksum) {
        uint16_t checksum = com_calculate_checksum(buffer, index);
        buffer[index++] = (checksum >> 8) & 0xFF;
        buffer[index++] = checksum & 0xFF;
    }
    
    return index;
}

uint16_t com_encode_custom(const phasor_result_t* phasor_data, 
                          uint8_t* buffer, uint16_t buffer_size)
{
    /* 自定义协议：简单的文本格式 */
    uint16_t length = snprintf((char*)buffer, buffer_size,
        "PMU,%lu,%lu,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\r\n",
        (unsigned long)phasor_data->frame_index,
        (unsigned long)phasor_data->timestamp_ms,
        phasor_data->frequency[0], phasor_data->amplitude[0], phasor_data->phase[0],
        phasor_data->frequency[1], phasor_data->amplitude[1], phasor_data->phase[1],
        phasor_data->frequency[2], phasor_data->amplitude[2], phasor_data->phase[2],
        phasor_data->frequency[3], phasor_data->amplitude[3], phasor_data->phase[3]
    );
    
    return (length < buffer_size) ? length : 0;
}

/* =================== 私有函数实现 =================== */

static void com_task_function(void *pvParameters)
{
    phasor_result_t phasor_data;
    TickType_t last_send_time = 0;
    
    for (;;) {
        /* 处理接收数据 */
        com_process_rx_data();
        
        /* 检查是否有新的相量数据 */
        if (xQueueReceive(g_phasor_queue, &phasor_data, pdMS_TO_TICKS(10)) == pdPASS) {
            /* 检查发送间隔 */
            TickType_t current_time = xTaskGetTickCount();
            if ((current_time - last_send_time) >= pdMS_TO_TICKS(g_config.data_rate_ms)) {
                com_send_phasor_data(&phasor_data);
                last_send_time = current_time;
            }
        }
        
        /* 发送心跳包 */
        static TickType_t last_heartbeat = 0;
        TickType_t current_time = xTaskGetTickCount();
        if ((current_time - last_heartbeat) >= pdMS_TO_TICKS(5000)) { // 5秒心跳
            com_send_status(0x0000, "HEARTBEAT");
            last_heartbeat = current_time;
        }
    }
}

static BaseType_t com_uart_send(const uint8_t* data, uint16_t length)
{
    for (uint16_t i = 0; i < length; i++) {
        /* 等待发送缓冲区空 */
        while (usart_flag_get(USART0, USART_FLAG_TBE) == RESET);
        usart_data_transmit(USART0, data[i]);
    }
    
    /* 等待发送完成 */
    while (usart_flag_get(USART0, USART_FLAG_TC) == RESET);
    
    return pdPASS;
}

static uint16_t com_calculate_checksum(const uint8_t* data, uint16_t length)
{
    uint16_t checksum = 0;
    for (uint16_t i = 0; i < length; i++) {
        checksum += data[i];
    }
    return checksum;
}

static void com_process_rx_data(void)
{
    /* 简单的接收数据处理 */
    /* 实际应用中可以解析命令和配置请求 */
    if (g_rx_index > 0) {
        g_stats.rx_packets++;
        g_stats.last_rx_time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        g_rx_index = 0; // 重置接收缓冲区
    }
}

/* =================== 中断处理函数 =================== */

void USART0_IRQHandler(void)
{
    if (usart_interrupt_flag_get(USART0, USART_INT_FLAG_RBNE) != RESET) {
        /* 接收数据 */
        uint8_t data = usart_data_receive(USART0);
        if (g_rx_index < COM_RX_BUFFER_SIZE - 1) {
            g_rx_buffer[g_rx_index++] = data;
        }
        usart_interrupt_flag_clear(USART0, USART_INT_FLAG_RBNE);
    }
}

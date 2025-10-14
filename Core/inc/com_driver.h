/*!
    \file    com_driver.h
    \brief   Communication driver header for PMU application
    
    \version 2025-08-29, V1.0.0, PMU project for GD32F4xx
*/

#ifndef COM_DRIVER_H
#define COM_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "gd32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "fft_phasor_task.h"
#include <stdbool.h>

/* =================== 配置参数 =================== */
#define COM_TASK_STACK_SIZE     2048
#define COM_TASK_PRIORITY       (tskIDLE_PRIORITY + 2)

#define COM_TX_BUFFER_SIZE      512
#define COM_RX_BUFFER_SIZE      256

/* 通信协议定义 */
typedef enum {
    COM_PROTOCOL_IEEE_C37_118 = 0,  // IEEE C37.118标准
    COM_PROTOCOL_CUSTOM = 1,        // 自定义协议
    COM_PROTOCOL_MODBUS = 2         // Modbus协议
} com_protocol_t;

/* 消息类型定义 */
typedef enum {
    COM_MSG_PHASOR_DATA = 0x01,     // 相量数据
    COM_MSG_STATUS = 0x02,          // 状态信息
    COM_MSG_CONFIG = 0x03,          // 配置命令
    COM_MSG_HEARTBEAT = 0x04        // 心跳包
} com_message_type_t;

/* 通信配置结构体 */
typedef struct {
    com_protocol_t protocol;        // 通信协议
    uint16_t pmu_id;               // PMU ID
    uint32_t data_rate_ms;         // 数据发送周期(毫秒)
    bool enable_timestamp;         // 是否包含时间戳
    bool enable_checksum;          // 是否启用校验和
} com_config_t;

/* 数据传输统计 */
typedef struct {
    uint32_t tx_packets;           // 发送包数
    uint32_t rx_packets;           // 接收包数
    uint32_t tx_errors;            // 发送错误数
    uint32_t rx_errors;            // 接收错误数
    uint32_t last_tx_time_ms;      // 最后发送时间
    uint32_t last_rx_time_ms;      // 最后接收时间
} com_statistics_t;

/* =================== 函数声明 =================== */

/*!
    \brief      初始化通信驱动
    \param[in]  phasor_queue: 相量数据队列
    \param[in]  config: 通信配置参数
    \param[out] none
    \retval     pdPASS: 成功, pdFAIL: 失败
*/
BaseType_t com_driver_init(QueueHandle_t phasor_queue, const com_config_t* config);

/*!
    \brief      启动通信任务
    \param[in]  none
    \param[out] none
    \retval     none
*/
void com_driver_start(void);

/*!
    \brief      停止通信任务
    \param[in]  none
    \param[out] none
    \retval     none
*/
void com_driver_stop(void);

/*!
    \brief      发送相量数据
    \param[in]  phasor_data: 相量测量结果
    \param[out] none
    \retval     pdPASS: 成功, pdFAIL: 失败
*/
BaseType_t com_send_phasor_data(const phasor_result_t* phasor_data);

/*!
    \brief      发送状态信息
    \param[in]  status_code: 状态码
    \param[in]  message: 状态消息
    \param[out] none
    \retval     pdPASS: 成功, pdFAIL: 失败
*/
BaseType_t com_send_status(uint16_t status_code, const char* message);

/*!
    \brief      获取默认通信配置
    \param[out] config: 默认配置参数
    \retval     none
*/
void com_get_default_config(com_config_t* config);

/*!
    \brief      更新通信配置
    \param[in]  config: 新的配置参数
    \param[out] none
    \retval     pdPASS: 成功, pdFAIL: 失败
*/
BaseType_t com_update_config(const com_config_t* config);

/*!
    \brief      获取通信统计信息
    \param[out] stats: 统计信息
    \retval     none
*/
void com_get_statistics(com_statistics_t* stats);

/*!
    \brief      重置通信统计信息
    \param[in]  none
    \param[out] none
    \retval     none
*/
void com_reset_statistics(void);

/*!
    \brief      检查通信链路状态
    \param[in]  none
    \param[out] none
    \retval     true: 链路正常, false: 链路异常
*/
bool com_is_link_active(void);

/* =================== 协议相关函数 =================== */

/*!
    \brief      IEEE C37.118协议编码
    \param[in]  phasor_data: 相量数据
    \param[out] buffer: 编码后的数据缓冲区
    \param[in]  buffer_size: 缓冲区大小
    \retval     编码后的数据长度，0表示失败
*/
uint16_t com_encode_ieee_c37118(const phasor_result_t* phasor_data, 
                               uint8_t* buffer, uint16_t buffer_size);

/*!
    \brief      自定义协议编码
    \param[in]  phasor_data: 相量数据
    \param[out] buffer: 编码后的数据缓冲区
    \param[in]  buffer_size: 缓冲区大小
    \retval     编码后的数据长度，0表示失败
*/
uint16_t com_encode_custom(const phasor_result_t* phasor_data, 
                          uint8_t* buffer, uint16_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* COM_DRIVER_H */

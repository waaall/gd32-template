/**
  ******************************************************************************
  * @file    serial_bridge.h
  * @brief   Modular Serial Bridge for forwarding data between UARTs
  ******************************************************************************
  */

#ifndef __SERIAL_BRIDGE_H
#define __SERIAL_BRIDGE_H

#include "usart.h"

/* Config --------------------------------------------------------------------*/
#define SB_BUFFER_SIZE  256   // Size of each RX buffer (Double buffering)
#define SB_FRAME_BUFFER_SIZE  256  // 单帧缓存大小（需覆盖完整帧长度）
#define SB_TX_QUEUE_DEPTH     4    // 发送队列深度（满则丢帧）

/* Types ---------------------------------------------------------------------*/

typedef struct {
    UART_HandleTypeDef *huart_src;      // Source UART (RX)
    UART_HandleTypeDef *huart_dst;      // Destination UART (TX)
    
    // Double buffering for RX
    uint8_t rx_buf[2][SB_BUFFER_SIZE];
    volatile uint8_t rx_idx;            // Current buffer index (0 or 1)

    // 帧缓存与解析状态
    uint8_t frame_buf[SB_FRAME_BUFFER_SIZE];
    uint16_t frame_len;
    uint8_t frame_in_progress;
    uint8_t begin_match;
    uint8_t end_match;

    // 发送队列（帧完整后再发送）
    struct {
        uint8_t data[SB_FRAME_BUFFER_SIZE];
        uint16_t len;
    } tx_queue[SB_TX_QUEUE_DEPTH];
    uint8_t tx_head;
    uint8_t tx_tail;
    uint8_t tx_count;
    uint8_t tx_active;
    
    // Status
    uint8_t initialized;
} SerialBridge_t;

/* Public Functions ----------------------------------------------------------*/

/**
 * @brief  Initialize a serial bridge channel
 * @param  bridge: Pointer to bridge structure
 * @param  src: Source UART handle (where data comes in)
 * @param  dst: Destination UART handle (where data goes out)
 * @retval 0 on success
 */
int SB_Init(SerialBridge_t *bridge, UART_HandleTypeDef *src, UART_HandleTypeDef *dst);

/**
 * @brief  Handle RX Event (Call this from HAL_UARTEx_RxEventCallback)
 * @param  bridge: Pointer to bridge structure
 * @param  huart: Handle of the UART that triggered the event
 * @param  size: Number of bytes received
 * @retval 1 if handled, 0 if not relevant to this bridge
 */
int SB_HandleRxEvent(SerialBridge_t *bridge, UART_HandleTypeDef *huart, uint16_t size);

/**
 * @brief  Handle Error (Call this from HAL_UART_ErrorCallback)
 * @param  bridge: Pointer to bridge structure
 * @param  huart: Handle of the UART that triggered the error
 */
void SB_HandleError(SerialBridge_t *bridge, UART_HandleTypeDef *huart);

/**
 * @brief  Handle TX Complete (Call this from HAL_UART_TxCpltCallback)
 * @param  bridge: Pointer to bridge structure
 * @param  huart: Handle of the UART that triggered the event
 */
void SB_HandleTxCplt(SerialBridge_t *bridge, UART_HandleTypeDef *huart);

#endif /* __SERIAL_BRIDGE_H */

/**
 ******************************************************************************
 * @file    serial_bridge.h
 * @brief   Transparent bidirectional UART bridge
 ******************************************************************************
 */

#ifndef __SERIAL_BRIDGE_H
#define __SERIAL_BRIDGE_H

#include "usart.h"

/* Config --------------------------------------------------------------------*/
#define SB_BUFFER_SIZE 256U
#define SB_TX_QUEUE_DEPTH 8U
#define SB_DIRECTION_COUNT 2U

/* Types ---------------------------------------------------------------------*/

typedef struct {
  UART_HandleTypeDef *huart_rx;
  UART_HandleTypeDef *huart_tx;

  uint8_t rx_buf[2][SB_BUFFER_SIZE];
  volatile uint8_t rx_idx;

  struct {
    uint8_t data[SB_BUFFER_SIZE];
    uint16_t len;
  } tx_queue[SB_TX_QUEUE_DEPTH];

  volatile uint8_t tx_head;
  volatile uint8_t tx_tail;
  volatile uint8_t tx_count;
  volatile uint8_t tx_active;

  volatile uint16_t rx_restart_errors;
  volatile uint16_t tx_drop_count;
  volatile uint16_t tx_error_count;
} SerialBridgeDirection_t;

typedef struct {
  SerialBridgeDirection_t dir[SB_DIRECTION_COUNT];
  uint8_t initialized;
} SerialBridge_t;

/* Public Functions ----------------------------------------------------------*/

/**
 * @brief  Initialize a transparent bidirectional bridge between two UARTs.
 * @param  bridge: Pointer to bridge structure.
 * @param  uart_a: First UART endpoint.
 * @param  uart_b: Second UART endpoint.
 * @retval 0 on success.
 */
int SB_Init(SerialBridge_t *bridge, UART_HandleTypeDef *uart_a,
            UART_HandleTypeDef *uart_b);

/**
 * @brief  Handle RX idle/DMA event.
 * @param  bridge: Pointer to bridge structure.
 * @param  huart: UART that received bytes.
 * @param  size: Number of bytes received in this chunk.
 * @retval 1 if handled, 0 if not relevant to this bridge.
 */
int SB_HandleRxEvent(SerialBridge_t *bridge, UART_HandleTypeDef *huart,
                     uint16_t size);

/**
 * @brief  Handle UART error and re-arm the affected receive side.
 * @param  bridge: Pointer to bridge structure.
 * @param  huart: UART that triggered the error.
 */
void SB_HandleError(SerialBridge_t *bridge, UART_HandleTypeDef *huart);

/**
 * @brief  Handle TX complete and send the next queued chunk.
 * @param  bridge: Pointer to bridge structure.
 * @param  huart: UART that completed transmission.
 */
void SB_HandleTxCplt(SerialBridge_t *bridge, UART_HandleTypeDef *huart);

#endif /* __SERIAL_BRIDGE_H */

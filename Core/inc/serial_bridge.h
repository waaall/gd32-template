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

/* Types ---------------------------------------------------------------------*/

typedef struct {
    UART_HandleTypeDef *huart_src;      // Source UART (RX)
    UART_HandleTypeDef *huart_dst;      // Destination UART (TX)
    
    // Double buffering for RX
    uint8_t rx_buf[2][SB_BUFFER_SIZE];
    volatile uint8_t rx_idx;            // Current buffer index (0 or 1)
    
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

#endif /* __SERIAL_BRIDGE_H */

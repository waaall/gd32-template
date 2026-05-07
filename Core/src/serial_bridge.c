/**
 ******************************************************************************
 * @file    serial_bridge.c
 * @brief   Transparent bidirectional UART bridge implementation
 *
 * 设计说明：
 *  - 不解析协议、不过滤帧，收到的字节原样转发到对端串口。
 *  - 两个方向各自使用 ReceiveToIdle + DMA 接收和 DMA 发送队列。
 *  - RX 使用双缓冲，收到 chunk 后立即复制到 TX 队列并重新 arm 接收。
 *  - USART3 发送前切换 CTRL485 为发送态，发送完成或错误后恢复接收态。
 ******************************************************************************
 */

#include "serial_bridge.h"
#include "main.h"

#include <string.h>

/* Private Functions ---------------------------------------------------------*/
static void _init_direction(SerialBridgeDirection_t *dir,
                            UART_HandleTypeDef *rx, UART_HandleTypeDef *tx);
static int _direction_index_by_rx(SerialBridge_t *bridge,
                                  UART_HandleTypeDef *huart);
static int _direction_index_by_tx(SerialBridge_t *bridge,
                                  UART_HandleTypeDef *huart);
static void _restart_rx(SerialBridgeDirection_t *dir);
static void _enqueue_chunk(SerialBridgeDirection_t *dir, const uint8_t *data,
                           uint16_t len);
static void _kick_tx(SerialBridgeDirection_t *dir);
static void _pop_tx_head(SerialBridgeDirection_t *dir);
static void _set_rs485_tx_enable(UART_HandleTypeDef *huart, uint8_t enable);
static uint32_t _enter_critical(void);
static void _exit_critical(uint32_t primask);

/* Implementation ------------------------------------------------------------*/

int SB_Init(SerialBridge_t *bridge, UART_HandleTypeDef *uart_a,
            UART_HandleTypeDef *uart_b) {
  if (bridge == NULL || uart_a == NULL || uart_b == NULL) {
    return -1;
  }

  memset(bridge, 0, sizeof(*bridge));
  _init_direction(&bridge->dir[0], uart_a, uart_b);
  _init_direction(&bridge->dir[1], uart_b, uart_a);
  bridge->initialized = 1U;

  _set_rs485_tx_enable(uart_a, 0U);
  _set_rs485_tx_enable(uart_b, 0U);
  _restart_rx(&bridge->dir[0]);
  _restart_rx(&bridge->dir[1]);

  return 0;
}

int SB_HandleRxEvent(SerialBridge_t *bridge, UART_HandleTypeDef *huart,
                     uint16_t size) {
  if (bridge == NULL || bridge->initialized == 0U) {
    return 0;
  }

  int idx = _direction_index_by_rx(bridge, huart);
  if (idx < 0) {
    return 0;
  }

  SerialBridgeDirection_t *dir = &bridge->dir[idx];
  if (size > SB_BUFFER_SIZE) {
    size = SB_BUFFER_SIZE;
    dir->tx_drop_count++;
  }

  if (size > 0U) {
    _enqueue_chunk(dir, dir->rx_buf[dir->rx_idx], size);
  }

  dir->rx_idx = (uint8_t)(dir->rx_idx ^ 1U);
  _restart_rx(dir);

  return 1;
}

void SB_HandleError(SerialBridge_t *bridge, UART_HandleTypeDef *huart) {
  if (bridge == NULL || bridge->initialized == 0U) {
    return;
  }

  int rx_idx = _direction_index_by_rx(bridge, huart);
  if (rx_idx >= 0) {
    HAL_UART_AbortReceive(huart);
    _restart_rx(&bridge->dir[rx_idx]);
  }

  int tx_idx = _direction_index_by_tx(bridge, huart);
  if (tx_idx >= 0) {
    SerialBridgeDirection_t *dir = &bridge->dir[tx_idx];

    HAL_UART_AbortTransmit(huart);
    _set_rs485_tx_enable(huart, 0U);
    dir->tx_error_count++;

    uint32_t primask = _enter_critical();
    if (dir->tx_active != 0U) {
      dir->tx_active = 0U;
      _pop_tx_head(dir);
    }
    _exit_critical(primask);

    _kick_tx(dir);
  }
}

void SB_HandleTxCplt(SerialBridge_t *bridge, UART_HandleTypeDef *huart) {
  if (bridge == NULL || bridge->initialized == 0U) {
    return;
  }

  int idx = _direction_index_by_tx(bridge, huart);
  if (idx < 0) {
    return;
  }

  SerialBridgeDirection_t *dir = &bridge->dir[idx];

  uint32_t primask = _enter_critical();
  if (dir->tx_active != 0U) {
    dir->tx_active = 0U;
    _pop_tx_head(dir);
  }
  _exit_critical(primask);

  _set_rs485_tx_enable(huart, 0U);
  _kick_tx(dir);
}

static void _init_direction(SerialBridgeDirection_t *dir,
                            UART_HandleTypeDef *rx, UART_HandleTypeDef *tx) {
  memset(dir, 0, sizeof(*dir));
  dir->huart_rx = rx;
  dir->huart_tx = tx;
}

static int _direction_index_by_rx(SerialBridge_t *bridge,
                                  UART_HandleTypeDef *huart) {
  for (uint8_t i = 0U; i < SB_DIRECTION_COUNT; i++) {
    if (bridge->dir[i].huart_rx == huart) {
      return (int)i;
    }
  }
  return -1;
}

static int _direction_index_by_tx(SerialBridge_t *bridge,
                                  UART_HandleTypeDef *huart) {
  for (uint8_t i = 0U; i < SB_DIRECTION_COUNT; i++) {
    if (bridge->dir[i].huart_tx == huart) {
      return (int)i;
    }
  }
  return -1;
}

static void _restart_rx(SerialBridgeDirection_t *dir) {
  HAL_StatusTypeDef status = HAL_UARTEx_ReceiveToIdle_DMA(
      dir->huart_rx, dir->rx_buf[dir->rx_idx], SB_BUFFER_SIZE);
  if (status == HAL_OK) {
    if (dir->huart_rx->hdmarx != NULL) {
      __HAL_DMA_DISABLE_IT(dir->huart_rx->hdmarx, DMA_IT_HT);
    }
  } else {
    dir->rx_restart_errors++;
  }
}

static void _enqueue_chunk(SerialBridgeDirection_t *dir, const uint8_t *data,
                           uint16_t len) {
  if (data == NULL || len == 0U) {
    return;
  }

  uint32_t primask = _enter_critical();
  if (dir->tx_count >= SB_TX_QUEUE_DEPTH) {
    dir->tx_drop_count++;
    _exit_critical(primask);
    return;
  }

  uint8_t slot = dir->tx_tail;
  memcpy(dir->tx_queue[slot].data, data, len);
  dir->tx_queue[slot].len = len;
  dir->tx_tail = (uint8_t)((dir->tx_tail + 1U) % SB_TX_QUEUE_DEPTH);
  dir->tx_count++;
  _exit_critical(primask);

  _kick_tx(dir);
}

static void _kick_tx(SerialBridgeDirection_t *dir) {
  uint8_t tx_head;
  uint16_t tx_len;

  uint32_t primask = _enter_critical();
  if (dir->tx_active != 0U || dir->tx_count == 0U) {
    _exit_critical(primask);
    return;
  }

  tx_head = dir->tx_head;
  tx_len = dir->tx_queue[tx_head].len;
  dir->tx_active = 1U;
  _exit_critical(primask);

  _set_rs485_tx_enable(dir->huart_tx, 1U);

  HAL_StatusTypeDef status =
      HAL_UART_Transmit_DMA(dir->huart_tx, dir->tx_queue[tx_head].data, tx_len);
  if (status != HAL_OK) {
    _set_rs485_tx_enable(dir->huart_tx, 0U);

    primask = _enter_critical();
    dir->tx_active = 0U;
    dir->tx_error_count++;
    if (status != HAL_BUSY) {
      _pop_tx_head(dir);
    }
    _exit_critical(primask);
  }
}

static void _pop_tx_head(SerialBridgeDirection_t *dir) {
  if (dir->tx_count == 0U) {
    return;
  }

  dir->tx_head = (uint8_t)((dir->tx_head + 1U) % SB_TX_QUEUE_DEPTH);
  dir->tx_count--;
}

static void _set_rs485_tx_enable(UART_HandleTypeDef *huart, uint8_t enable) {
  if (huart != NULL && huart->Instance == USART3) {
    HAL_GPIO_WritePin(CTRL485_GPIO_Port, CTRL485_Pin,
                      (enable != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  }
}

static uint32_t _enter_critical(void) {
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  return primask;
}

static void _exit_critical(uint32_t primask) { __set_PRIMASK(primask); }

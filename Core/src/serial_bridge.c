/**
  ******************************************************************************
  * @file    serial_bridge.c
  * @brief   Modular Serial Bridge implementation
  *
  * 使用说明：
  *  - 调用 SB_Init() 绑定“源串口(RX)”与“目标串口(TX)”
  *  - 在 HAL_UARTEx_RxEventCallback() 中调用 SB_HandleRxEvent()
  *  - 在 HAL_UART_ErrorCallback() 中调用 SB_HandleError()
  *
  * 设计说明：
  *  - 仅做单向转发：源串口收到的数据转发到目标串口
  *  - 采用“接收空闲中断 + DMA”方式，双缓冲切换避免覆盖
  *  - 错误发生时自动重启接收，保证链路恢复
  *
  * 数据流与调用关系：
  *  - 数据流：Src(UARTx RX) → DMA写入rx_buf → RxEvent回调 → DMA发送 → Dst(UARTy TX)
  *  - 调用链：
  *      main/baisc_init: SB_Init()
  *      HAL_UARTEx_RxEventCallback: SB_HandleRxEvent()
  *      HAL_UART_ErrorCallback: SB_HandleError()
  ******************************************************************************
  */

#include "serial_bridge.h"

/* Private Functions ---------------------------------------------------------*/
static void _restart_rx(SerialBridge_t *bridge);

/* Implementation ------------------------------------------------------------*/

int SB_Init(SerialBridge_t *bridge, UART_HandleTypeDef *src, UART_HandleTypeDef *dst)
{
    if (!bridge || !src || !dst) {
        return -1;
    }

    // 保存源/目标串口句柄
    bridge->huart_src = src;
    bridge->huart_dst = dst;
    bridge->rx_idx = 0;
    bridge->initialized = 1;

    // 启动源串口的接收（DMA + 空闲中断）
    // Start RX on the first buffer
    _restart_rx(bridge);

    return 0;
}

int SB_HandleRxEvent(SerialBridge_t *bridge, UART_HandleTypeDef *huart, uint16_t size)
{
    if (!bridge || !bridge->initialized) {
        return 0;
    }

    // 判断是否为“源串口”的接收事件
    if (huart == bridge->huart_src) {
        // 将源串口收到的数据转发到目标串口
        // 使用DMA发送，降低CPU占用
        if (size > 0) {
            HAL_UART_Transmit_DMA(bridge->huart_dst, 
                                  bridge->rx_buf[bridge->rx_idx], 
                                  size);
        }

        // 切换双缓冲索引
        bridge->rx_idx = !bridge->rx_idx;

        // 重新启动接收，使用新的缓冲区
        _restart_rx(bridge);
        
        return 1; // Handled
    }

    return 0; // Not for us
}

void SB_HandleError(SerialBridge_t *bridge, UART_HandleTypeDef *huart)
{
    if (!bridge || !bridge->initialized) {
        return;
    }

    if (huart == bridge->huart_src) {
        // 接收错误：中止后重新启动接收
        HAL_UART_AbortReceive(bridge->huart_src);
        _restart_rx(bridge);
    }
}

static void _restart_rx(SerialBridge_t *bridge)
{
    // 启动“接收空闲中断 + DMA”模式
    HAL_UARTEx_ReceiveToIdle_DMA(bridge->huart_src, 
                                 bridge->rx_buf[bridge->rx_idx], 
                                 SB_BUFFER_SIZE);
}

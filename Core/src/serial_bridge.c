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
  *  - 先缓存并按帧解析：仅当检测到完整帧后才转发
  *
  * 数据流与调用关系：
  *  - 数据流：Src(UARTx RX) → DMA写入rx_buf → RxEvent回调 → 帧缓存解析 → 发送队列 → DMA发送 → Dst(UARTy TX)
  *  - 调用链：
  *      main/basic_init: SB_Init()
  *      HAL_UARTEx_RxEventCallback: SB_HandleRxEvent()
  *      HAL_UART_ErrorCallback: SB_HandleError()
  *      HAL_UART_TxCpltCallback: SB_HandleTxCplt()
  ******************************************************************************
  */

#include "serial_bridge.h"
#include <string.h>

/* Private Functions ---------------------------------------------------------*/
static void _restart_rx(SerialBridge_t *bridge);
static void _process_rx_chunk(SerialBridge_t *bridge, const uint8_t *data, uint16_t len);
static void _handle_sync_byte(SerialBridge_t *bridge, uint8_t byte);
static void _handle_frame_byte(SerialBridge_t *bridge, uint8_t byte);
static void _reset_frame_state(SerialBridge_t *bridge);
static void _enqueue_frame(SerialBridge_t *bridge, const uint8_t *data, uint16_t len);
static void _kick_tx(SerialBridge_t *bridge);

#define SB_BEGIN_MARKER "BEGIN:"  // 帧格式：BEGIN:{freq:.3f},{power:.3f}END\n
#define SB_END_MARKER   "END\n"
#define SB_BEGIN_LEN    (sizeof(SB_BEGIN_MARKER) - 1U)
#define SB_END_LEN      (sizeof(SB_END_MARKER) - 1U)

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
    bridge->frame_len = 0;
    bridge->frame_in_progress = 0;
    bridge->begin_match = 0;
    bridge->end_match = 0;
    bridge->tx_head = 0;
    bridge->tx_tail = 0;
    bridge->tx_count = 0;
    bridge->tx_active = 0;
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
        // 先缓存并解析帧，帧完整后再转发
        if (size > 0) {
            _process_rx_chunk(bridge, bridge->rx_buf[bridge->rx_idx], size);
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

void SB_HandleTxCplt(SerialBridge_t *bridge, UART_HandleTypeDef *huart)
{
    if (!bridge || !bridge->initialized) {
        return;
    }

    if (huart != bridge->huart_dst) {
        return;
    }

    if (bridge->tx_active != 0U) {
        if (bridge->tx_count > 0U) {
            bridge->tx_head = (uint8_t)((bridge->tx_head + 1U) % SB_TX_QUEUE_DEPTH);
            bridge->tx_count--;
        }
        bridge->tx_active = 0U;
    }

    _kick_tx(bridge);
}

static void _restart_rx(SerialBridge_t *bridge)
{
    // 启动“接收空闲中断 + DMA”模式
    HAL_UARTEx_ReceiveToIdle_DMA(bridge->huart_src, 
                                 bridge->rx_buf[bridge->rx_idx], 
                                 SB_BUFFER_SIZE);
}

static void _process_rx_chunk(SerialBridge_t *bridge, const uint8_t *data, uint16_t len)
{
    // 将本次DMA收到的连续字节逐个喂给状态机
    // 注意：RX事件可能是“半帧”或“多帧”，因此必须逐字节解析
    for (uint16_t i = 0; i < len; i++) {
        if (bridge->frame_in_progress != 0U) {
            // 已检测到BEGIN:，当前在帧内解析
            _handle_frame_byte(bridge, data[i]);
        } else {
            // 尚未检测到BEGIN:，处于同步阶段
            _handle_sync_byte(bridge, data[i]);
        }
    }
}

static void _handle_sync_byte(SerialBridge_t *bridge, uint8_t byte)
{
    // 同步状态：只寻找帧头 "BEGIN:"
    // begin_match 记录当前已匹配的帧头长度（0..SB_BEGIN_LEN）
    uint8_t idx = bridge->begin_match;

    if (byte == (uint8_t)SB_BEGIN_MARKER[idx]) {
        idx++;
        if (idx == (uint8_t)SB_BEGIN_LEN) {
            // 帧头完全匹配：进入帧内状态
            memcpy(bridge->frame_buf, SB_BEGIN_MARKER, SB_BEGIN_LEN);
            bridge->frame_len = (uint16_t)SB_BEGIN_LEN;
            bridge->frame_in_progress = 1U;
            bridge->end_match = 0;
            idx = 0;
        }
    } else {
        // 匹配失败：若当前字节刚好等于帧头首字节，则保留为1，否则清零
        idx = (byte == (uint8_t)SB_BEGIN_MARKER[0]) ? 1U : 0U;
    }

    bridge->begin_match = idx;
}

static void _handle_frame_byte(SerialBridge_t *bridge, uint8_t byte)
{
    // 帧内状态：持续收集字节并判断是否遇到帧尾 "END\n"
    if (bridge->frame_len >= SB_FRAME_BUFFER_SIZE) {
        // 缓冲区溢出，丢弃当前帧并重新同步
        _reset_frame_state(bridge);
        // 该字节可能是下一帧的帧头首字节，继续同步判断
        _handle_sync_byte(bridge, byte);
        return;
    }

    // 保存当前字节到帧缓冲区
    bridge->frame_buf[bridge->frame_len++] = byte;

    // end_match 记录当前已匹配的帧尾长度（0..SB_END_LEN）
    uint8_t idx = bridge->end_match;
    if (byte == (uint8_t)SB_END_MARKER[idx]) {
        idx++;
        if (idx == (uint8_t)SB_END_LEN) {
            // 帧尾完全匹配：判定帧完整，入队等待发送
            _enqueue_frame(bridge, bridge->frame_buf, bridge->frame_len);
            _reset_frame_state(bridge);
            return;
        }
    } else {
        // 匹配失败：若当前字节刚好等于帧尾首字节，则保留为1，否则清零
        idx = (byte == (uint8_t)SB_END_MARKER[0]) ? 1U : 0U;
    }

    bridge->end_match = idx;
}

static void _reset_frame_state(SerialBridge_t *bridge)
{
    // 清空帧解析状态（不清除帧缓冲内容，避免多余开销）
    bridge->frame_len = 0;
    bridge->frame_in_progress = 0U;
    bridge->begin_match = 0U;
    bridge->end_match = 0U;
}

static void _enqueue_frame(SerialBridge_t *bridge, const uint8_t *data, uint16_t len)
{
    // 发送队列入队：仅完整帧才会进入此流程
    if (len == 0U || len > SB_FRAME_BUFFER_SIZE) {
        return;
    }

    if (bridge->tx_count >= SB_TX_QUEUE_DEPTH) {
        // 发送队列满：丢弃该帧
        return;
    }

    // 拷贝帧数据到队列尾部
    memcpy(bridge->tx_queue[bridge->tx_tail].data, data, len);
    bridge->tx_queue[bridge->tx_tail].len = len;
    bridge->tx_tail = (uint8_t)((bridge->tx_tail + 1U) % SB_TX_QUEUE_DEPTH);
    bridge->tx_count++;

    // 若当前无发送进行中，则触发DMA发送
    _kick_tx(bridge);
}

static void _kick_tx(SerialBridge_t *bridge)
{
    // 仅当未在发送且队列非空时启动DMA发送
    if (bridge->tx_active != 0U || bridge->tx_count == 0U) {
        return;
    }

    if (HAL_UART_Transmit_DMA(bridge->huart_dst,
                              bridge->tx_queue[bridge->tx_head].data,
                              bridge->tx_queue[bridge->tx_head].len) == HAL_OK) {
        bridge->tx_active = 1U;
    }
}

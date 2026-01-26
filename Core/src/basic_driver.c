/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : basic_driver.c
  * @brief          : basic_driver
  * @author         : zhengxu
  ******************************************************************************
  * @details        ： 时间相关参数读取&修改
  ******************************************************************************
  */
/* USER CODE END Header */

#include "adc.h"
#include "tim.h"
#include "gpio.h"
#include "main.h"
#include "usart.h"

#include <math.h>
#include <stdlib.h>

#include "basic_driver.h"
#include "zero_crossing.h"  // 过零检测模块
#include "adc_service.h"    // ADC采样服务模块
#include "serial_bridge.h"  // 串口转发模块


extern SerialBridge_t bridge_u3_to_u1;

// 局部函数 ---------------------------------------------------*/

#define TIMER_CLOCK_FREQ 64     // 定义定时器时钟频率

// 更新psc_arr(内部函数)
uint8_t _set_timer_psc_arr(TIM_HandleTypeDef *, uint16_t);

// 定时器参数实时更新函数
uint8_t _update_timer_interval(TIM_HandleTypeDef*, uint16_t);
void _reset_and_start_timer(TIM_HandleTypeDef *htim);
/**
  ******************************************************************************
  *
  * 逻辑相关封装
  *
  ******************************************************************************
*/

void basic_init(void)
{
    // 配置 TIM7 为 周期 (用于测试输出)
    // _update_timer_interval(&htim7, 500);

    // 过零检测模块初始化
    if (ZC_Init() == 0) {
        // 启动过零检测
        ZC_Start();
    }

    // ADC服务模块初始化
    ADC_SRV_Config_t adc_config = {
        .calc_periods = ADC_SRV_DEFAULT_PERIODS,      // 计算10个工频周期
        .calc_interval_ms = ADC_SRV_CALC_INTERVAL_MS, // 每**ms计算一次
        .voltage_scale_ua = ADC_SRV_BASE_VOLT_SCALE,  // 基础电压缩放系数
        .voltage_scale_ub = ADC_SRV_BASE_VOLT_SCALE,
        .voltage_scale_uc = ADC_SRV_BASE_VOLT_SCALE,
        .calib_coef = {                               // 默认校准系数
            .coef_ua = ADC_SRV_COEF_DEFAULT,          // 10000 = 1.0倍
            .coef_ub = ADC_SRV_COEF_DEFAULT,
            .coef_uc = ADC_SRV_COEF_DEFAULT
        }
    };
    if (ADC_SRV_Init(&adc_config) == 0) {
        ADC_SRV_Start();
    }
    // 启动 ADC DMA 转换
    ADC_Start_DMA_Conversion();

    // 配置 TIM10 用于ADC计算任务标志; 需要略大于周期*calc_periods,否则数据不全无法计算
    _update_timer_interval(&htim10, ADC_SRV_CALC_INTERVAL_MS);

    // 串口转发初始化: USART3 (RX=PD9) -> USART1 (TX=PA9)
    (void)SB_Init(&bridge_u3_to_u1, &huart3, &huart1);
}

/**
  ******************************************************************************
  *
  * 定时器底层接口封装
  *
  ******************************************************************************
*/

/* 修改定时器参数 ---------------------------------------------------------*/

uint8_t _update_timer_interval(TIM_HandleTypeDef *htim_addr, uint16_t period_ms)
{
    uint8_t status;

    // 调用函数计算并设置 PSC 和 ARR
    status = _set_timer_psc_arr(htim_addr, period_ms);
    if (status != 0)
    {
        // 错误处理：如果设置 PSC 和 ARR 失败
        return status;
    }

    // 重新启动定时器，使新参数生效
    HAL_TIM_Base_Stop_IT(htim_addr);
    HAL_TIM_Base_Start_IT(htim_addr);

    return 0;  // 返回 0 表示成功
}

uint8_t _set_timer_psc_arr(TIM_HandleTypeDef *htim_addr, uint16_t period_ms)
{
    uint32_t psc = 0;
    uint32_t arr = 0;

    // 根据输入的周期计算 PSC 和 ARR
    if (period_ms >= 100 && period_ms <= 2000)
    {
        psc = TIMER_CLOCK_FREQ * 100 - 1;  // 计算预分频器
    }
    else if (period_ms < 100)
    {
        psc = TIMER_CLOCK_FREQ - 1;  // 计算预分频器
    }
    else {return 1;} // 错误处理：如果周期超出范围

    // 计算自动重装载值 ARR
    arr = (uint32_t)(period_ms * (TIMER_CLOCK_FREQ * 1000 / (psc + 1))) - 1;

    // 设置定时器的 PSC 和 ARR
    __HAL_TIM_SET_PRESCALER(htim_addr, psc);     // 更新预分频器
    __HAL_TIM_SET_AUTORELOAD(htim_addr, arr);    // 更新自动重装载值

    return 0;  // 返回 0 表示成功
}

// 定时器重置
void _reset_and_start_timer(TIM_HandleTypeDef *htim)
{
    // 清空中断标志位
    __HAL_TIM_CLEAR_FLAG(htim, TIM_FLAG_UPDATE);

    // 检查定时器运行状态
    if ((htim->Instance->CR1 & TIM_CR1_CEN) != 0)
    {
        // 如果在运行，仅重置计数器
        __HAL_TIM_SET_COUNTER(htim, 0);
    }
    else
    {
        // 如果不在运行，重置计数器并开启定时器
        __HAL_TIM_SET_COUNTER(htim, 0);
        HAL_TIM_Base_Start_IT(htim);
    }
}

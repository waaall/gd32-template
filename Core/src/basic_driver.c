/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : basic_driver.c
  * @brief          : basic_driver
  * @author         : zhengxu
  ******************************************************************************
  * @details        ： 所有与通信无关且与硬件相关的函数在此文件中定义，主要分为四部分：
  *                         1. 逻辑相关函数接口封装
  *                         2. 时间相关参数读取&修改
  *                         3. 起搏感知(时间无关)参数读取&修改
  *                         4. 硬件相关封装（逻辑无关）
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


// 全局变量
volatile uint16_t adc_v_int = 0;

// ADC转换完成标志位
static volatile Status_Bool_Type ADC_CPLT_STATUS = isFalse; 

// 感知到的电压值mv
static volatile int16_t sensed_adc_v = 0;

// 局部函数 ---------------------------------------------------*/

#define TIMER_CLOCK_FREQ 64     // 定义定时器时钟频率

#define ADC_REF_V 2048          // 定义ADC参考电压mv
#define ADC_BIAS 2046           // 定义ADC偏置value
#define ADC_AMP_SCALER 10       // 定义ADC放大倍数

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
    // 配置 TIM7 为 1 秒周期
    _update_timer_interval(&htim7, 1000);

    // 开启 TIM7 定时器中断
    HAL_TIM_Base_Start_IT(&htim7);
}

// 感知到的ADC value转换为电压值----------------------------------------------*/
void sensed_value_handler(void)
{
    // 没有轮询检查是否转换完成是因为上次结果也是ok的
    uint16_t adc_value = HAL_ADC_GetValue(&hadc1);

    // 参考电压ADC_REF_V，偏置ADC_BIAS, 放大倍数ADC_AMP_SCALER, 得到电压 votage/mv
    sensed_adc_v = abs((adc_value-ADC_BIAS) * ADC_REF_V * 10 / (ADC_AMP_SCALER*4095.0f));
    ADC_CPLT_STATUS = isTrue;

    // adc_v_int = abs((adc_value-ADC_BIAS) * ADC_REF_V / (ADC_AMP_SCALER*4095));
    adc_v_int = adc_value; // for test ADC_BIAS
}

// 外部获取的ADC状态和ADC value (only used by Sensed_Module_Handler)-------*/
int16_t get_last_sensed_value(void)
{
    if (ADC_CPLT_STATUS ==isTrue)
    {
        ADC_CPLT_STATUS = isFalse;
        return sensed_adc_v;
    }
    else {return -1;} // 返回0的数就是ADC没有更新
}

Status_Bool_Type get_adc_status(void) {return ADC_CPLT_STATUS;}

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

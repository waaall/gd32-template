/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : basic_test.c
  * @brief          : basic_test
  * @author         : zhengxu
  ******************************************************************************
  * @details        ： 所有与测试相关的函数在此文件中定义
  *                                                          
  ******************************************************************************
  */

/* USER CODE END Header */
#include "main.h"
#include "usart.h"
#include "adc.h"

#include "basic_driver.h"
#include "basic_test.h"

#include <stdio.h>
#include <string.h>



/**
  ******************************************************************************
  *
  * ADC VREFINT 数据处理和打印
  *
  ******************************************************************************
*/

/**
  * @brief  打印第5通道（VREFINT）的ADC数据到串口
  * @note   VREFINT 是内部参考电压，通常为1.2V
  *         可以用来计算实际的 VDDA 电压
  * @retval None
  */
void print_vrefint_data(void)
{
    char uart_buffer[100];
    uint16_t vrefint_value;
    uint32_t vdda_mv;  // 使用毫伏单位的整数
    
    // 读取第5个通道的数据（索引为4，因为从0开始）
    vrefint_value = adc_dma_buffer[4];
    
    // 计算 VDDA 电压（单位：毫伏）
    // VREFINT_CAL 是校准值，通常存储在地址 0x1FFF7A2A (STM32F4系列)
    // VREFINT 典型值为 1.21V (在 3.3V VDDA 下，ADC值约为 1520)
    // VDDA = 1.21V * 4095 / vrefint_value
    // 转换为毫伏：VDDA_mV = 1210mV * 4095 / vrefint_value
    vdda_mv = (1210UL * 4095UL) / vrefint_value;
    
    // 格式化数据（使用整数，精度到毫伏）
    sprintf(uart_buffer, "VREFINT ADC Value: %u, VDDA: %u.%03uV\r\n", 
            vrefint_value, vdda_mv / 1000, vdda_mv % 1000);
    
    // 通过串口发送
    HAL_UART_Transmit(&huart1, (uint8_t*)uart_buffer, strlen(uart_buffer), HAL_MAX_DELAY);
}
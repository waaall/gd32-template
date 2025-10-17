/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : pace_test.h
  * @brief          : Header for pace_test.c file.
  *                   This file contains the common defines of the application.
  * @author         : zhengxu
  ******************************************************************************
*/
/* USER CODE END Header */

#ifndef BASIC_TEST_H
#define BASIC_TEST_H

#include "main.h"
#include "basic_driver.h"

// 测试func ---------------------------------------------------------

// ADC VREFINT 数据处理和打印
void print_vrefint_data(void);

// 异常处理宏（根据实际需求调整）--------------------------------------------
#define CHECK_HAL_STATUS(status)     \
    do {                             \
        if ((status) != HAL_OK) {    \
            handle_hal_error();      \
        }                            \
    } while(0)

// 异常处理函数声明
void handle_hal_error(void);

#endif // BASIC_TEST_H
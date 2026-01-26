/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : basic_test.h
  * @brief          : Header for basic_test.c file.
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
void print_test_data(void);

// 测试 4-20mA 输出
void test_ma_out(void);

#endif // BASIC_TEST_H
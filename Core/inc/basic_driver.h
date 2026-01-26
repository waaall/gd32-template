/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : basic_driver.h
  * @brief          : Header for basic_driver.c file.
  *                   This file contains the common defines of the application.
  * @author         : zhengxu
  ******************************************************************************
*/
/* USER CODE END Header */

#ifndef BASIC_DRIVER_H
#define BASIC_DRIVER_H

#include "main.h"

typedef enum {
  isTrue = 0,
  isFalse
}Status_Bool_Type;

// 初始化
void basic_init(void);

#endif // BASIC_DRIVER_H
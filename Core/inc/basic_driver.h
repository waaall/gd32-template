/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : pace_driver.h
  * @brief          : Header for pace_driver.c file.
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

// adc
extern volatile uint16_t adc_v_int;

void sensed_value_handler(void);
int16_t get_last_sensed_value(void);
Status_Bool_Type get_adc_status(void);

// 其他tool接口封装---------------------------------------------------------

// 异常处理宏（根据实际需求调整）-----------------------------------------------
#define CHECK_HAL_STATUS(status)     \
    do {                             \
        if ((status) != HAL_OK) {    \
            handle_hal_error();      \
        }                            \
    } while(0)

// 异常处理函数声明
void handle_hal_error(void);

#endif // BASIC_DRIVER_H
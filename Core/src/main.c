/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "crc.h"
#include "dma.h"
#include "i2c.h"
#include "iwdg.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "basic_driver.h"
#include "basic_test.h"
#include "zero_crossing.h"  // 过零检测模块
#include "adc_service.h"    // ADC采样服务模块（用于定时器回调）
#include "serial_bridge.h"  // 串口转发模块
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static volatile uint8_t adc_srv_calc_pending = 0;
static volatile uint8_t print_test_pending = 0;

/* Serial Bridge Instance */
// Config: Forward USART3 (RX=PD9) -> USART1 (TX=PA9)
// Note: CTRL485 (PD10) is initialized to Low in gpio.c, enabling RS485 Reception on USART3.
SerialBridge_t bridge_u3_to_u1;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/**
  ******************************************************************************
  *
  * 中断回调函数：表明各模块的连接/跳转逻辑
  *             1. TIMER
  *             2. ADC
  *             3. GPIO
  *             4. 通信
  *
  ******************************************************************************
*/

/* 1. TIMER的中断回调函数 -------------------------------------------------------*/
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM7)
  {
    // 每秒置位打印标志，主循环中再执行实际串口输出
    print_test_pending = 1;
  }
  else if (htim->Instance == TIM10)
  {
    // 标记ADC服务需要在主循环中执行计算任务（每**ms触发一次）
    adc_srv_calc_pending = 1;
  }
  else if (htim->Instance == TIM13)
  {
    //
  }
  else if (htim->Instance == TIM14)
  {
    //
  }
}

/**
 * @brief  TIM输入捕获回调函数（HAL库标准回调）
 * @note   HAL_TIM_IRQHandler会自动调用此函数
 */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM2)
  {
    // 调用过零检测模块的处理函数
    ZC_TIM_CaptureCallback(htim);
  }
}

/* 2. ADC的中断回调函数 --------------------------------------------------------*/

/**
 * @brief  ADC转换完成回调（HAL库自动调用）
 * @param  hadc: ADC句柄指针
 * @note   此回调在DMA中断上下文中执行
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
  if (hadc->Instance == ADC1) {
    // 调用ADC服务模块处理数据
    extern volatile uint16_t adc_dma_buffer[];
    ADC_SRV_DMA_ConvCpltCallback(hadc, adc_dma_buffer);
  }
}

/* 3. GPIO的中断回调函数 -------------------------------------------------------*/

// 重定义GPIO中断回调函数
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  // if (GPIO_Pin == MAG_Pin)
  // {
  //   //
  // }
}

/* 4. 通信的中断回调函数 -------------------------------------------------------*/

// 串口发送完成的中断回调函数
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
      // 处理USART1发送完成后的操作
  }
}

// 串口接收完成的中断回调函数
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        // 处理USART1接收到的数据
    }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
  // Handle Serial Bridge Events
  // USART3 (RX=PD9) -> USART1 (TX=PA9)
  SB_HandleRxEvent(&bridge_u3_to_u1, huart, size);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  SB_HandleError(&bridge_u3_to_u1, huart);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_CRC_Init();
  MX_TIM7_Init();
  MX_TIM10_Init();
  MX_TIM11_Init();
  MX_TIM13_Init();
  MX_TIM14_Init();
  MX_USART1_UART_Init();
  MX_ADC1_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_TIM2_Init();
  MX_USART3_UART_Init();
  MX_IWDG_Init();
  MX_TIM9_Init();
  MX_TIM4_Init();
  /* USER CODE BEGIN 2 */

  // 项目初始化（包含ADC服务、过零检测等所有模块）
  basic_init();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    HAL_IWDG_Refresh(&hiwdg);

    if (adc_srv_calc_pending != 0U)
    {
      adc_srv_calc_pending = 0U;
      ADC_SRV_CalculateTask();
    }

    if (print_test_pending != 0U)
    {
      print_test_pending = 0U;
      print_test_data();
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 6;
  RCC_OscInitStruct.PLL.PLLN = 64;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enables the Clock Security System
  */
  HAL_RCC_EnableCSS();
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

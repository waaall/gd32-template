/***************************************************************************//**
  file: main.c
  author: zhengxu
  version: V1.0.0
  date: 20250820

*******************************************************************************/
//gd32
#include "gd32f4xx.h"
#include "myboard.h"
#include "systick.h"

//freeRTOS
#include "FreeRTOS.h"
#include "task.h"


//任务优先级
#define START_TASK_PRIO		1
//任务堆栈大小	
#define START_STK_SIZE 		128  
//任务句柄
TaskHandle_t StartTask_Handler;
//任务函数
void start_task(void *pvParameters);

//任务优先级
#define LED1_TASK_PRIO		3
//任务堆栈大小	
#define LED1_STK_SIZE 		50  
//任务句柄
TaskHandle_t LED1Task_Handler;
//任务函数
void LED_Thread1(void *pvParameters);

//任务优先级
#define LED2_TASK_PRIO		4
//任务堆栈大小	
#define LED2_STK_SIZE 		50  
//任务句柄
TaskHandle_t LED2Task_Handler;
//任务函数
void LED_Thread2(void *pvParameters);

/*!
    \brief      main function
    \param[in]  none
    \param[out] none
    \retval     none
*/
int main(void)
{ 
	systick_config();//配置系统主频168M,外部8M晶振,配置在#define __SYSTEM_CLOCK_168M_PLL_8M_HXTAL        (uint32_t)(168000000)	
	
	// 初始化LED（使用myboard.h中定义的LED）
	gd_eval_led_init(LED1);
	
	//创建开始任务
	xTaskCreate((TaskFunction_t )start_task,            //任务函数
							(const char*    )"start_task",          //任务名称
							(uint16_t       )START_STK_SIZE,        //任务堆栈大小
							(void*          )NULL,                  //传递给任务函数的参数
							(UBaseType_t    )START_TASK_PRIO,       //任务优先级
							(TaskHandle_t*  )&StartTask_Handler);   //任务句柄   
							
	vTaskStartScheduler();          //开启任务调度
}

//开始任务任务函数
void start_task(void *pvParameters)
{
    taskENTER_CRITICAL();           //进入临界区
    //创建LED1任务
    xTaskCreate((TaskFunction_t )LED_Thread1,     	
                (const char*    )"led1_task",   	
                (uint16_t       )LED1_STK_SIZE, 
                (void*          )NULL,				
                (UBaseType_t    )LED1_TASK_PRIO,	
                (TaskHandle_t*  )&LED1Task_Handler);   


	    xTaskCreate((TaskFunction_t )LED_Thread2,     	
                (const char*    )"led2_task",   	
                (uint16_t       )LED2_STK_SIZE, 
                (void*          )NULL,				
                (UBaseType_t    )LED2_TASK_PRIO,	
                (TaskHandle_t*  )&LED2Task_Handler);   
													
							
    vTaskDelete(StartTask_Handler); //删除开始任务
    taskEXIT_CRITICAL();            //退出临界区

}

//LED1任务函数 
void LED_Thread1(void *pvParameters)
{
    while(1)
    {
      gd_eval_led_on(LED1);  // 点亮LED1
      vTaskDelay(100);
    }
} 

//LED2任务函数 
void LED_Thread2(void *pvParameters)
{
    while(1)
    {
			gd_eval_led_off(LED1);  // 关闭LED1
      vTaskDelay(250);
    }
}  



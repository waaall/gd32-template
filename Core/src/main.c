/*!
    \file    pmu_main.c
    \brief   Main application for PMU (Phasor Measurement Unit)
    
    \version 2025-08-29, V1.0.0, PMU project for GD32F4xx
*/

#include "gd32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "adc_driver.h"
#include "fft_phasor_task.h"
#include "com_driver.h"

#include <stdio.h>
#include <stdbool.h>

/* =================== 系统配置 =================== */
#define MAIN_TASK_STACK_SIZE    1024
#define MAIN_TASK_PRIORITY      (tskIDLE_PRIORITY + 1)

#define DMA_QUEUE_SIZE          8
#define PHASOR_QUEUE_SIZE       4

/* =================== 全局变量 =================== */
static QueueHandle_t g_dma_queue = NULL;
static QueueHandle_t g_phasor_queue = NULL;
static TaskHandle_t g_main_task_handle = NULL;

/* =================== 私有函数声明 =================== */
static void system_clock_config(void);
static void led_gpio_config(void);
static void main_task_function(void *pvParameters);
static void print_system_info(void);
static void print_phasor_statistics(void);

/* =================== Main函数 =================== */
int main(void)
{
    /* 配置系统时钟 */
    system_clock_config();
    
    /* 配置LED GPIO */
    led_gpio_config();
    
    /* 创建队列 */
    g_dma_queue = xQueueCreate(DMA_QUEUE_SIZE, sizeof(uint8_t));
    g_phasor_queue = xQueueCreate(PHASOR_QUEUE_SIZE, sizeof(phasor_result_t));
    
    if (g_dma_queue == NULL || g_phasor_queue == NULL) {
        /* 队列创建失败，系统错误 */
        while (1) {
            /* 错误指示 */
        }
    }
    
    /* 初始化ADC驱动 */
    if (adc_driver_init(g_dma_queue) != pdPASS) {
        /* ADC初始化失败 */
        while (1) {
            /* 错误指示 */
        }
    }
    
    /* 初始化FFT相量计算任务 */
    phasor_config_t phasor_config;
    fft_phasor_get_default_config(&phasor_config);
    
    if (fft_phasor_task_init(g_dma_queue, g_phasor_queue, &phasor_config) != pdPASS) {
        /* FFT任务初始化失败 */
        while (1) {
            /* 错误指示 */
        }
    }
    
    /* 初始化通信驱动 */
    com_config_t com_config;
    com_get_default_config(&com_config);
    
    if (com_driver_init(g_phasor_queue, &com_config) != pdPASS) {
        /* 通信驱动初始化失败 */
        while (1) {
            /* 错误指示 */
        }
    }
    
    /* 创建主任务 */
    if (xTaskCreate(main_task_function, "MainTask", MAIN_TASK_STACK_SIZE/4, 
                   NULL, MAIN_TASK_PRIORITY, &g_main_task_handle) != pdPASS) {
        /* 主任务创建失败 */
        while (1) {
            /* 错误指示 */
        }
    }
    
    /* 打印系统信息 */
    print_system_info();
    
    /* 启动FreeRTOS调度器 */
    vTaskStartScheduler();
    
    /* 程序不应该执行到这里 */
    while (1) {
        /* 系统错误 */
    }
}

/* =================== 私有函数实现 =================== */

static void system_clock_config(void)
{
    /* 配置系统时钟为180MHz */
    /* 这里使用默认配置，实际项目中可能需要调整 */
    
    /* 使能外部高速晶振 */
    rcu_osci_on(RCU_HXTAL);
    rcu_osci_stab_wait(RCU_HXTAL);
    
    /* 配置PLL - 简化版本，实际参数需要根据具体需求调整 */
    // rcu_pll_config(RCU_PLLSRC_HXTAL, 25, 360, 2); // 根据实际API调整参数
    
    /* 配置AHB和APB预分频器 */
    rcu_ahb_clock_config(RCU_AHB_CKSYS_DIV1);
    rcu_apb1_clock_config(RCU_APB1_CKAHB_DIV4);
    rcu_apb2_clock_config(RCU_APB2_CKAHB_DIV2);
    
    /* 选择系统时钟源 - 使用内部RC振荡器 */
    // rcu_system_clock_source_config(RCU_CKSYSSRC_IRC16M);
    
    /* 更新系统时钟频率变量 */
    SystemCoreClockUpdate();
}

static void led_gpio_config(void)
{
    /* 配置LED GPIO (假设使用PC13) */
    rcu_periph_clock_enable(RCU_GPIOC);
    
    gpio_mode_set(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_13);
    gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_13);
    
    /* 初始状态：LED关闭 */
    gpio_bit_set(GPIOC, GPIO_PIN_13);
}

static void main_task_function(void *pvParameters)
{
    TickType_t last_wake_time;
    const TickType_t task_period = pdMS_TO_TICKS(1000); // 1秒周期
    
    last_wake_time = xTaskGetTickCount();
    
    /* 启动各个驱动 */
    fft_phasor_task_start();
    com_driver_start();
    adc_start_sampling();
    
    /* 发送系统启动状态 */
    com_send_status(0x0001, "PMU_SYSTEM_STARTED");
    
    for (;;) {
        /* 周期性任务 */
        vTaskDelayUntil(&last_wake_time, task_period);
        
        /* LED心跳指示 */
        static bool led_state = false;
        if (led_state) {
            gpio_bit_set(GPIOC, GPIO_PIN_13);    // LED关闭
        } else {
            gpio_bit_reset(GPIOC, GPIO_PIN_13);  // LED开启
        }
        led_state = !led_state;
        
        /* 打印统计信息 */
        print_phasor_statistics();
        
        /* 检查系统状态 */
        if (!adc_is_sampling()) {
            com_send_status(0x0002, "ADC_SAMPLING_STOPPED");
        }
        
        if (!com_is_link_active()) {
            com_send_status(0x0003, "COM_LINK_INACTIVE");
        }
        
        /* 监控堆内存使用情况 */
        size_t free_heap = xPortGetFreeHeapSize();
        if (free_heap < 1024) { // 堆空间不足1KB时报警
            com_send_status(0x0005, "LOW_HEAP_SPACE");
        }
    }
}

static void print_system_info(void)
{
    printf("\n========================================\n");
    printf("PMU System Information\n");
    printf("========================================\n");
    printf("MCU: GD32F4xx\n");
    printf("System Clock: %lu MHz\n", SystemCoreClock / 1000000);
    printf("FreeRTOS Version: %s\n", tskKERNEL_VERSION_NUMBER);
    printf("ADC Sample Rate: %.1f kHz\n", ADC_SAMPLE_RATE_HZ / 1000.0f);
    printf("FFT Size: %d\n", FFT_SIZE);
    printf("Channels: %d\n", ADC_CHANNELS_NUM);
    printf("Frame Size: %d samples\n", ADC_FRAME_SIZE);
    printf("========================================\n\n");
}

static void print_phasor_statistics(void)
{
    static uint32_t print_counter = 0;
    
    /* 每10秒打印一次统计信息 */
    if (++print_counter >= 10) {
        print_counter = 0;
        
        uint32_t frames_processed, avg_time, max_time;
        fft_phasor_get_statistics(&frames_processed, &avg_time, &max_time);
        
        com_statistics_t com_stats;
        com_get_statistics(&com_stats);
        
        printf("=== PMU Statistics ===\n");
        printf("Frames Processed: %lu\n", frames_processed);
        printf("Avg Process Time: %lu us\n", avg_time);
        printf("Max Process Time: %lu us\n", max_time);
        printf("TX Packets: %lu\n", com_stats.tx_packets);
        printf("RX Packets: %lu\n", com_stats.rx_packets);
        printf("TX Errors: %lu\n", com_stats.tx_errors);
        printf("RX Errors: %lu\n", com_stats.rx_errors);
        printf("Free Heap: %u bytes\n", (unsigned int)xPortGetFreeHeapSize());
        printf("======================\n");
    }
}

/* =================== FreeRTOS钩子函数 =================== */

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    /* 栈溢出处理 */
    com_send_status(0x00FF, "STACK_OVERFLOW");
    taskDISABLE_INTERRUPTS();
    for (;;);
}

void vApplicationMallocFailedHook(void)
{
    /* 内存分配失败处理 */
    com_send_status(0x00FE, "MALLOC_FAILED");
    taskDISABLE_INTERRUPTS();
    for (;;);
}

void vApplicationIdleHook(void)
{
    /* 空闲任务钩子 - 可以在这里实现低功耗模式 */
}

void vApplicationTickHook(void)
{
    /* 系统滴答钩子 - 可以在这里实现高精度定时功能 */
}

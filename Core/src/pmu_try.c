#include "gd32f4xx.h"
#include "gd32f4xx_adc.h"
#include "gd32f4xx_dma.h"
#include "gd32f4xx_timer.h"
#include "gd32f4xx_rcu.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

// 配置参数
#define ADC_BUF_LEN   200     // 每通道点数（20ms窗口）
#define ADC_CH_NUM    6       // 三相电压+电流

static uint16_t adc_buffer[ADC_BUF_LEN * ADC_CH_NUM] __attribute__((aligned(4)));

// FreeRTOS 队列，通知处理任务
static QueueHandle_t adcQueue;

// ================= ADC + DMA 配置 =================
void adc_dma_config(void)
{
    rcu_periph_clock_enable(RCU_DMA1);
    rcu_periph_clock_enable(RCU_ADC0);
    rcu_periph_clock_enable(RCU_ADC1);
    rcu_periph_clock_enable(RCU_ADC2);

    // ADC时钟分频
    ADC_SYNCCTL |= ADC_SYNCCTL_ADCCK; // 具体分频配置看手册

    // 配置DMA
    dma_channel_disable(DMA1, DMA_CH0);
    dma_deinit(DMA1, DMA_CH0);
    dma_parameter_struct dma_init_struct;
    dma_struct_para_init(&dma_init_struct);
    dma_init_struct.direction = DMA_PERIPHERAL_TO_MEMORY;
    dma_init_struct.memory0_addr = (uint32_t)adc_buffer;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.memory_width = DMA_MEMORY_WIDTH_16BIT;
    dma_init_struct.number = ADC_BUF_LEN * ADC_CH_NUM;
    dma_init_struct.periph_addr = (uint32_t)&ADC_RDATA(ADC0);
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.periph_width = DMA_PERIPHERAL_WIDTH_16BIT;
    dma_init_struct.priority = DMA_PRIORITY_HIGH;
    dma_init(DMA1, DMA_CH0, &dma_init_struct);

    dma_circulation_enable(DMA1, DMA_CH0);   // 环形缓冲
    dma_channel_enable(DMA1, DMA_CH0);

    // 配置ADC 同步模式
    ADC_SYNCCTL = ADC_ALL_ROUTINE_PARALLEL;   // ADC0/1/2 并行
    ADC_CTL1(ADC0) |= ADC_CTL1_DMA;

    // 配置通道序列 (例子：6 路)
    ADC_RSQ0(ADC0) = RSQ0_RL(ADC_CH_NUM - 1);
    ADC_RSQ2(ADC0) = 0 | (1<<5) | (2<<10) | (3<<15) | (4<<20) | (5<<25); // 通道0~5

    // 开启ADC
    ADC_CTL1(ADC0) |= ADC_CTL1_ADCON;
    ADC_CTL1(ADC1) |= ADC_CTL1_ADCON;
    ADC_CTL1(ADC2) |= ADC_CTL1_ADCON;
}

// ================= DMA 完成中断 =================
void DMA1_Channel0_IRQHandler(void)
{
    if(dma_interrupt_flag_get(DMA1, DMA_CH0, DMA_INT_FLAG_FTF)) {
        dma_interrupt_flag_clear(DMA1, DMA_CH0, DMA_INT_FLAG_FTF);

        // 通知处理任务
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(adcQueue, adc_buffer, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

// ================= 处理任务 =================
void vPhasorTask(void *pvParameters)
{
    uint16_t samples[ADC_BUF_LEN * ADC_CH_NUM];

    while(1) {
        if (xQueueReceive(adcQueue, samples, portMAX_DELAY) == pdPASS) {
            // Hann窗
            // IpDFT计算
            // 频率/相位/幅值
            // 打包发送PDC
        }
    }
}

// ================= 主函数 =================
int main(void)
{
    adcQueue = xQueueCreate(2, sizeof(adc_buffer));

    adc_dma_config();

    // FreeRTOS 任务
    xTaskCreate(vPhasorTask, "Phasor", 512, NULL, tskIDLE_PRIORITY + 2, NULL);

    vTaskStartScheduler();

    while(1);
}

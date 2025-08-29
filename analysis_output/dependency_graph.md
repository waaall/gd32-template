# 项目依赖关系分析报告

## 总体统计信息
- 总文件数: 771 个
- 被调用文件数: 110 个
- 未被调用文件数: 661 个
- 缺少include文件数: 54 个
- 依赖关系层数: 5 层

### 各层文件统计:
- 第 0 层: 1 个文件
- 第 1 层: 13 个文件
- 第 2 层: 32 个文件
- 第 3 层: 84 个文件
- 第 4 层: 10 个文件
### 未找到的include及其依赖

#### Drivers/CMSIS/DSP/Include/arm_math_types.h
3 个未找到的include
- arm64_neon.h
- arm_mve.h
- arm_neon.h

#### Drivers/CMSIS/cmsis_compiler.h
6 个未找到的include
- cmsis_armcc.h
- cmsis_armclang.h
- cmsis_armclang_ltm.h
- cmsis_ccs.h
- cmsis_csm.h
- cmsis_iccarm.h

#### Drivers/FreeRTOS/include/FreeRTOS.h
1 个未找到的include
- reent.h

#### Drivers/FreeRTOS/include/deprecated_definitions.h
43 个未找到的include
- ../../../Source/portable/GCC/ColdFire_V2/portmacro.h
- ../../Source/portable/CodeWarrior/ColdFire_V2/portmacro.h
- ../../Source/portable/CodeWarrior/HCS12/portmacro.h
- ../../Source/portable/GCC/ARM7_AT91FR40008/portmacro.h
- ../../Source/portable/GCC/ARM7_AT91SAM7S/portmacro.h
- ../../Source/portable/GCC/ARM7_LPC2000/portmacro.h
- ../../Source/portable/GCC/ARM7_LPC23xx/portmacro.h
- ../../Source/portable/GCC/ARM_CM3/portmacro.h
- ../../Source/portable/GCC/H8S2329/portmacro.h
- ../../Source/portable/GCC/HCS12/portmacro.h
- ../../Source/portable/GCC/MCF5235/portmacro.h
- ../../Source/portable/GCC/MSP430F449/portmacro.h
- ../../Source/portable/GCC/MicroBlaze/portmacro.h
- ../../Source/portable/GCC/PPC405_Xilinx/portmacro.h
- ../../Source/portable/GCC/PPC440_Xilinx/portmacro.h
- ../../Source/portable/IAR/78K0R/portmacro.h
- ../../Source/portable/IAR/ARM_CM3/portmacro.h
- ../../Source/portable/IAR/V850ES/portmacro.h
- ../../Source/portable/MPLAB/PIC18F/portmacro.h
- ../../Source/portable/MPLAB/PIC24_dsPIC/portmacro.h
- ../../Source/portable/MPLAB/PIC32MX/portmacro.h
- ../../Source/portable/RVDS/ARM_CM3/portmacro.h
- ../../Source/portable/Rowley/MSP430F449/portmacro.h
- ../../Source/portable/SDCC/Cygnal/portmacro.h
- ../portable/GCC/ATMega323/portmacro.h
- ../portable/IAR/ATMega323/portmacro.h
- ..\..\Source\portable\GCC\STR75x\portmacro.h
- ..\..\Source\portable\IAR\AtmelSAM7S64\portmacro.h
- ..\..\Source\portable\IAR\AtmelSAM9XE\portmacro.h
- ..\..\Source\portable\IAR\LPC2000\portmacro.h
- ..\..\Source\portable\IAR\MSP430\portmacro.h
- ..\..\Source\portable\IAR\STR71x\portmacro.h
- ..\..\Source\portable\IAR\STR75x\portmacro.h
- ..\..\Source\portable\IAR\STR91x\portmacro.h
- ..\..\Source\portable\Paradigm\Tern_EE\small\portmacro.h
- ..\..\Source\portable\RVDS\ARM7_LPC21xx\portmacro.h
- ..\..\Source\portable\Softune\MB96340\portmacro.h
- ..\..\Source\portable\owatcom\16bitdos\flsh186\portmacro.h
- ..\..\Source\portable\owatcom\16bitdos\pc\portmacro.h
- ..\portable\BCC\16BitDOS\PC\prtmacro.h
- ..\portable\BCC\16BitDOS\flsh186\prtmacro.h
- frconfig.h
- libFreeRTOS/Include/portmacro.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_enet.h
1 个未找到的include
- phy.h


## 分层依赖关系

### 第 0 层 (1 个文件)

#### Core/src/main.c

**依赖关系:**
- Core/inc/adc_driver.h
- Core/inc/com_driver.h
- Core/inc/fft_phasor_task.h
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h
- Drivers/FreeRTOS/include/FreeRTOS.h
- Drivers/FreeRTOS/include/queue.h
- Drivers/FreeRTOS/include/semphr.h
- Drivers/FreeRTOS/include/task.h

### 第 1 层 (13 个文件)

#### Core/inc/adc_driver.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h
- Drivers/FreeRTOS/include/FreeRTOS.h
- Drivers/FreeRTOS/include/queue.h

#### Core/inc/com_driver.h

**依赖关系:**
- Core/inc/fft_phasor_task.h
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h
- Drivers/FreeRTOS/include/FreeRTOS.h
- Drivers/FreeRTOS/include/queue.h
- Drivers/FreeRTOS/include/task.h

#### Core/inc/fft_phasor_task.h

**依赖关系:**
- Core/inc/adc_driver.h
- Drivers/CMSIS/DSP/Include/arm_math.h
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h
- Drivers/FreeRTOS/include/FreeRTOS.h
- Drivers/FreeRTOS/include/queue.h
- Drivers/FreeRTOS/include/task.h

#### Core/inc/main.h

  无依赖关系

#### Core/src/adc_driver.c

**依赖关系:**
- Core/inc/adc_driver.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_adc.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_dma.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_gpio.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_rcu.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_timer.h

#### Core/src/com_driver.c

**依赖关系:**
- Core/inc/com_driver.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_gpio.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_rcu.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_usart.h

#### Core/src/fft_phasor_task.c

**依赖关系:**
- Core/inc/fft_phasor_task.h

#### Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

**依赖关系:**
- Core/inc/gd32f4xx_libopt.h
- Drivers/CMSIS/GD/GD32F4xx/Include/system_gd32f4xx.h
- Drivers/CMSIS/core_cm4.h

#### Drivers/FreeRTOS/include/FreeRTOS.h

**依赖关系:**
- Drivers/FreeRTOS/include/FreeRTOSConfig.h
- Drivers/FreeRTOS/include/portable.h
- Drivers/FreeRTOS/include/projdefs.h

  未找到依赖:
    - reent.h

#### Drivers/FreeRTOS/include/queue.h

**依赖关系:**
- Drivers/FreeRTOS/include/task.h

#### Drivers/FreeRTOS/include/semphr.h

**依赖关系:**
- Drivers/FreeRTOS/include/queue.h

#### Drivers/FreeRTOS/include/task.h

**依赖关系:**
- Drivers/FreeRTOS/include/list.h

#### Drivers/FreeRTOS/source/queue.c

**依赖关系:**
- Drivers/FreeRTOS/include/FreeRTOS.h
- Drivers/FreeRTOS/include/croutine.h
- Drivers/FreeRTOS/include/queue.h
- Drivers/FreeRTOS/include/task.h

### 第 2 层 (32 个文件)

#### Core/inc/fft_phasor_task.h

**依赖关系:**
- Core/inc/adc_driver.h
- Drivers/CMSIS/DSP/Include/arm_math.h
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h
- Drivers/FreeRTOS/include/FreeRTOS.h
- Drivers/FreeRTOS/include/queue.h
- Drivers/FreeRTOS/include/task.h

#### Core/inc/gd32f4xx_libopt.h

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_adc.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_can.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_crc.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_ctc.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_dac.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_dbg.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_dci.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_dma.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_enet.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_exmc.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_exti.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_fmc.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_fwdgt.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_gpio.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_i2c.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_ipa.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_iref.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_misc.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_pmu.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_rcu.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_rtc.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_sdio.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_spi.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_syscfg.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_timer.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_tli.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_trng.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_usart.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_wwdgt.h

#### Core/src/adc_driver.c

**依赖关系:**
- Core/inc/adc_driver.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_adc.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_dma.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_gpio.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_rcu.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_timer.h

#### Core/src/fft_phasor_task.c

**依赖关系:**
- Core/inc/fft_phasor_task.h

#### Drivers/CMSIS/DSP/Include/arm_math.h

**依赖关系:**
- Drivers/CMSIS/DSP/Include/arm_math_memory.h
- Drivers/CMSIS/DSP/Include/arm_math_types.h
- Drivers/CMSIS/DSP/Include/dsp/basic_math_functions.h
- Drivers/CMSIS/DSP/Include/dsp/bayes_functions.h
- Drivers/CMSIS/DSP/Include/dsp/complex_math_functions.h
- Drivers/CMSIS/DSP/Include/dsp/controller_functions.h
- Drivers/CMSIS/DSP/Include/dsp/distance_functions.h
- Drivers/CMSIS/DSP/Include/dsp/fast_math_functions.h
- Drivers/CMSIS/DSP/Include/dsp/filtering_functions.h
- Drivers/CMSIS/DSP/Include/dsp/interpolation_functions.h
- Drivers/CMSIS/DSP/Include/dsp/matrix_functions.h
- Drivers/CMSIS/DSP/Include/dsp/none.h
- Drivers/CMSIS/DSP/Include/dsp/quaternion_math_functions.h
- Drivers/CMSIS/DSP/Include/dsp/statistics_functions.h
- Drivers/CMSIS/DSP/Include/dsp/support_functions.h
- Drivers/CMSIS/DSP/Include/dsp/svm_functions.h
- Drivers/CMSIS/DSP/Include/dsp/transform_functions.h
- Drivers/CMSIS/DSP/Include/dsp/utils.h

#### Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

**依赖关系:**
- Core/inc/gd32f4xx_libopt.h
- Drivers/CMSIS/GD/GD32F4xx/Include/system_gd32f4xx.h
- Drivers/CMSIS/core_cm4.h

#### Drivers/CMSIS/GD/GD32F4xx/Include/system_gd32f4xx.h

  无依赖关系

#### Drivers/CMSIS/GD/GD32F4xx/Source/system_gd32f4xx.c

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/CMSIS/core_cm4.h

**依赖关系:**
- Drivers/CMSIS/cmsis_compiler.h
- Drivers/CMSIS/cmsis_version.h
- Drivers/CMSIS/mpu_armv7.h

#### Drivers/FreeRTOS/include/FreeRTOS.h

**依赖关系:**
- Drivers/FreeRTOS/include/FreeRTOSConfig.h
- Drivers/FreeRTOS/include/portable.h
- Drivers/FreeRTOS/include/projdefs.h

  未找到依赖:
    - reent.h

#### Drivers/FreeRTOS/include/FreeRTOSConfig.h

  无依赖关系

#### Drivers/FreeRTOS/include/croutine.h

**依赖关系:**
- Drivers/FreeRTOS/include/list.h

#### Drivers/FreeRTOS/include/list.h

  无依赖关系

#### Drivers/FreeRTOS/include/portable.h

**依赖关系:**
- Drivers/FreeRTOS/include/deprecated_definitions.h
- Drivers/FreeRTOS/include/mpu_wrappers.h
- Drivers/FreeRTOS/portable/RVDS/ARM_CM4F/portmacro.h

#### Drivers/FreeRTOS/include/projdefs.h

  无依赖关系

#### Drivers/FreeRTOS/include/queue.h

**依赖关系:**
- Drivers/FreeRTOS/include/task.h

#### Drivers/FreeRTOS/include/task.h

**依赖关系:**
- Drivers/FreeRTOS/include/list.h

#### Drivers/FreeRTOS/source/croutine.c

**依赖关系:**
- Drivers/FreeRTOS/include/FreeRTOS.h
- Drivers/FreeRTOS/include/croutine.h
- Drivers/FreeRTOS/include/task.h

#### Drivers/FreeRTOS/source/list.c

**依赖关系:**
- Drivers/FreeRTOS/include/FreeRTOS.h
- Drivers/FreeRTOS/include/list.h

#### Drivers/FreeRTOS/source/queue.c

**依赖关系:**
- Drivers/FreeRTOS/include/FreeRTOS.h
- Drivers/FreeRTOS/include/croutine.h
- Drivers/FreeRTOS/include/queue.h
- Drivers/FreeRTOS/include/task.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_adc.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_dma.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_gpio.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_rcu.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_timer.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_usart.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_adc.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_adc.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_dma.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_dma.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_gpio.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_gpio.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_rcu.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_rcu.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_timer.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_timer.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_usart.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_usart.h

### 第 3 层 (84 个文件)

#### Drivers/CMSIS/DSP/Include/arm_math_memory.h

**依赖关系:**
- Drivers/CMSIS/DSP/Include/arm_math_types.h

#### Drivers/CMSIS/DSP/Include/arm_math_types.h

**依赖关系:**
- Drivers/CMSIS/cmsis_compiler.h

  未找到依赖:
    - arm64_neon.h
    - arm_mve.h
    - arm_neon.h

#### Drivers/CMSIS/DSP/Include/dsp/basic_math_functions.h

**依赖关系:**
- Drivers/CMSIS/DSP/Include/arm_math_memory.h
- Drivers/CMSIS/DSP/Include/arm_math_types.h
- Drivers/CMSIS/DSP/Include/dsp/none.h
- Drivers/CMSIS/DSP/Include/dsp/utils.h

#### Drivers/CMSIS/DSP/Include/dsp/bayes_functions.h

**依赖关系:**
- Drivers/CMSIS/DSP/Include/arm_math_memory.h
- Drivers/CMSIS/DSP/Include/arm_math_types.h
- Drivers/CMSIS/DSP/Include/dsp/none.h
- Drivers/CMSIS/DSP/Include/dsp/statistics_functions.h
- Drivers/CMSIS/DSP/Include/dsp/utils.h

#### Drivers/CMSIS/DSP/Include/dsp/complex_math_functions.h

**依赖关系:**
- Drivers/CMSIS/DSP/Include/arm_math_memory.h
- Drivers/CMSIS/DSP/Include/arm_math_types.h
- Drivers/CMSIS/DSP/Include/dsp/fast_math_functions.h
- Drivers/CMSIS/DSP/Include/dsp/none.h
- Drivers/CMSIS/DSP/Include/dsp/utils.h

#### Drivers/CMSIS/DSP/Include/dsp/controller_functions.h

**依赖关系:**
- Drivers/CMSIS/DSP/Include/arm_math_memory.h
- Drivers/CMSIS/DSP/Include/arm_math_types.h
- Drivers/CMSIS/DSP/Include/dsp/none.h
- Drivers/CMSIS/DSP/Include/dsp/utils.h

#### Drivers/CMSIS/DSP/Include/dsp/distance_functions.h

**依赖关系:**
- Drivers/CMSIS/DSP/Include/arm_math_memory.h
- Drivers/CMSIS/DSP/Include/arm_math_types.h
- Drivers/CMSIS/DSP/Include/dsp/basic_math_functions.h
- Drivers/CMSIS/DSP/Include/dsp/fast_math_functions.h
- Drivers/CMSIS/DSP/Include/dsp/none.h
- Drivers/CMSIS/DSP/Include/dsp/statistics_functions.h
- Drivers/CMSIS/DSP/Include/dsp/utils.h

#### Drivers/CMSIS/DSP/Include/dsp/fast_math_functions.h

**依赖关系:**
- Drivers/CMSIS/DSP/Include/arm_math_memory.h
- Drivers/CMSIS/DSP/Include/arm_math_types.h
- Drivers/CMSIS/DSP/Include/dsp/basic_math_functions.h
- Drivers/CMSIS/DSP/Include/dsp/none.h
- Drivers/CMSIS/DSP/Include/dsp/utils.h

#### Drivers/CMSIS/DSP/Include/dsp/filtering_functions.h

**依赖关系:**
- Drivers/CMSIS/DSP/Include/arm_math_memory.h
- Drivers/CMSIS/DSP/Include/arm_math_types.h
- Drivers/CMSIS/DSP/Include/dsp/fast_math_functions.h
- Drivers/CMSIS/DSP/Include/dsp/none.h
- Drivers/CMSIS/DSP/Include/dsp/support_functions.h
- Drivers/CMSIS/DSP/Include/dsp/utils.h

#### Drivers/CMSIS/DSP/Include/dsp/interpolation_functions.h

**依赖关系:**
- Drivers/CMSIS/DSP/Include/arm_math_memory.h
- Drivers/CMSIS/DSP/Include/arm_math_types.h
- Drivers/CMSIS/DSP/Include/dsp/none.h
- Drivers/CMSIS/DSP/Include/dsp/utils.h

#### Drivers/CMSIS/DSP/Include/dsp/matrix_functions.h

**依赖关系:**
- Drivers/CMSIS/DSP/Include/arm_math_memory.h
- Drivers/CMSIS/DSP/Include/arm_math_types.h
- Drivers/CMSIS/DSP/Include/dsp/none.h
- Drivers/CMSIS/DSP/Include/dsp/utils.h

#### Drivers/CMSIS/DSP/Include/dsp/none.h

**依赖关系:**
- Drivers/CMSIS/DSP/Include/arm_math_types.h

#### Drivers/CMSIS/DSP/Include/dsp/quaternion_math_functions.h

**依赖关系:**
- Drivers/CMSIS/DSP/Include/arm_math_memory.h
- Drivers/CMSIS/DSP/Include/arm_math_types.h
- Drivers/CMSIS/DSP/Include/dsp/none.h
- Drivers/CMSIS/DSP/Include/dsp/utils.h

#### Drivers/CMSIS/DSP/Include/dsp/statistics_functions.h

**依赖关系:**
- Drivers/CMSIS/DSP/Include/arm_math_memory.h
- Drivers/CMSIS/DSP/Include/arm_math_types.h
- Drivers/CMSIS/DSP/Include/dsp/basic_math_functions.h
- Drivers/CMSIS/DSP/Include/dsp/fast_math_functions.h
- Drivers/CMSIS/DSP/Include/dsp/none.h
- Drivers/CMSIS/DSP/Include/dsp/utils.h

#### Drivers/CMSIS/DSP/Include/dsp/support_functions.h

**依赖关系:**
- Drivers/CMSIS/DSP/Include/arm_math_memory.h
- Drivers/CMSIS/DSP/Include/arm_math_types.h
- Drivers/CMSIS/DSP/Include/dsp/none.h
- Drivers/CMSIS/DSP/Include/dsp/utils.h

#### Drivers/CMSIS/DSP/Include/dsp/svm_functions.h

**依赖关系:**
- Drivers/CMSIS/DSP/Include/arm_math_memory.h
- Drivers/CMSIS/DSP/Include/arm_math_types.h
- Drivers/CMSIS/DSP/Include/dsp/none.h
- Drivers/CMSIS/DSP/Include/dsp/svm_defines.h
- Drivers/CMSIS/DSP/Include/dsp/utils.h

#### Drivers/CMSIS/DSP/Include/dsp/transform_functions.h

**依赖关系:**
- Drivers/CMSIS/DSP/Include/arm_math_memory.h
- Drivers/CMSIS/DSP/Include/arm_math_types.h
- Drivers/CMSIS/DSP/Include/dsp/basic_math_functions.h
- Drivers/CMSIS/DSP/Include/dsp/complex_math_functions.h
- Drivers/CMSIS/DSP/Include/dsp/none.h
- Drivers/CMSIS/DSP/Include/dsp/utils.h

#### Drivers/CMSIS/DSP/Include/dsp/utils.h

**依赖关系:**
- Drivers/CMSIS/DSP/Include/arm_math_types.h

#### Drivers/CMSIS/cmsis_compiler.h

**依赖关系:**
- Drivers/CMSIS/cmsis_gcc.h

  未找到依赖:
    - cmsis_armcc.h
    - cmsis_armclang.h
    - cmsis_armclang_ltm.h
    - cmsis_ccs.h
    - cmsis_csm.h
    - cmsis_iccarm.h

#### Drivers/CMSIS/cmsis_version.h

  无依赖关系

#### Drivers/CMSIS/mpu_armv7.h

  无依赖关系

#### Drivers/FreeRTOS/include/deprecated_definitions.h

**依赖关系:**
- Drivers/FreeRTOS/portable/RVDS/ARM_CM4F/portmacro.h

  未找到依赖:
    - ../../../Source/portable/GCC/ColdFire_V2/portmacro.h
    - ../../Source/portable/CodeWarrior/ColdFire_V2/portmacro.h
    - ../../Source/portable/CodeWarrior/HCS12/portmacro.h
    - ../../Source/portable/GCC/ARM7_AT91FR40008/portmacro.h
    - ../../Source/portable/GCC/ARM7_AT91SAM7S/portmacro.h
    - ../../Source/portable/GCC/ARM7_LPC2000/portmacro.h
    - ../../Source/portable/GCC/ARM7_LPC23xx/portmacro.h
    - ../../Source/portable/GCC/ARM_CM3/portmacro.h
    - ../../Source/portable/GCC/H8S2329/portmacro.h
    - ../../Source/portable/GCC/HCS12/portmacro.h
    - ../../Source/portable/GCC/MCF5235/portmacro.h
    - ../../Source/portable/GCC/MSP430F449/portmacro.h
    - ../../Source/portable/GCC/MicroBlaze/portmacro.h
    - ../../Source/portable/GCC/PPC405_Xilinx/portmacro.h
    - ../../Source/portable/GCC/PPC440_Xilinx/portmacro.h
    - ../../Source/portable/IAR/78K0R/portmacro.h
    - ../../Source/portable/IAR/ARM_CM3/portmacro.h
    - ../../Source/portable/IAR/V850ES/portmacro.h
    - ../../Source/portable/MPLAB/PIC18F/portmacro.h
    - ../../Source/portable/MPLAB/PIC24_dsPIC/portmacro.h
    - ../../Source/portable/MPLAB/PIC32MX/portmacro.h
    - ../../Source/portable/RVDS/ARM_CM3/portmacro.h
    - ../../Source/portable/Rowley/MSP430F449/portmacro.h
    - ../../Source/portable/SDCC/Cygnal/portmacro.h
    - ../portable/GCC/ATMega323/portmacro.h
    - ../portable/IAR/ATMega323/portmacro.h
    - ..\..\Source\portable\GCC\STR75x\portmacro.h
    - ..\..\Source\portable\IAR\AtmelSAM7S64\portmacro.h
    - ..\..\Source\portable\IAR\AtmelSAM9XE\portmacro.h
    - ..\..\Source\portable\IAR\LPC2000\portmacro.h
    - ..\..\Source\portable\IAR\MSP430\portmacro.h
    - ..\..\Source\portable\IAR\STR71x\portmacro.h
    - ..\..\Source\portable\IAR\STR75x\portmacro.h
    - ..\..\Source\portable\IAR\STR91x\portmacro.h
    - ..\..\Source\portable\Paradigm\Tern_EE\small\portmacro.h
    - ..\..\Source\portable\RVDS\ARM7_LPC21xx\portmacro.h
    - ..\..\Source\portable\Softune\MB96340\portmacro.h
    - ..\..\Source\portable\owatcom\16bitdos\flsh186\portmacro.h
    - ..\..\Source\portable\owatcom\16bitdos\pc\portmacro.h
    - ..\portable\BCC\16BitDOS\PC\prtmacro.h
    - ..\portable\BCC\16BitDOS\flsh186\prtmacro.h
    - frconfig.h
    - libFreeRTOS/Include/portmacro.h

#### Drivers/FreeRTOS/include/list.h

  无依赖关系

#### Drivers/FreeRTOS/include/mpu_wrappers.h

  无依赖关系

#### Drivers/FreeRTOS/portable/RVDS/ARM_CM4F/portmacro.h

  无依赖关系

#### Drivers/FreeRTOS/source/list.c

**依赖关系:**
- Drivers/FreeRTOS/include/FreeRTOS.h
- Drivers/FreeRTOS/include/list.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_adc.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_can.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_crc.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_ctc.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_dac.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_dbg.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_dci.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_dma.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_enet.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

  未找到依赖:
    - phy.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_exmc.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_exti.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_fmc.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_fwdgt.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_gpio.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_i2c.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_ipa.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_iref.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_misc.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_pmu.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_rcu.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_rtc.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_sdio.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_spi.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_syscfg.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_timer.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_tli.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_trng.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_usart.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_wwdgt.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_adc.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_adc.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_can.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_can.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_crc.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_crc.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_ctc.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_ctc.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_dac.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_dac.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_dbg.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_dbg.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_dci.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_dci.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_dma.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_dma.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_enet.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_enet.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_exmc.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_exmc.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_exti.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_exti.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_fmc.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_fmc.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_fwdgt.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_fwdgt.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_gpio.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_gpio.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_i2c.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_i2c.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_ipa.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_ipa.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_iref.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_iref.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_misc.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_misc.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_pmu.c

**依赖关系:**
- Drivers/CMSIS/core_cm4.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_pmu.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_rcu.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_rcu.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_rtc.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_rtc.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_sdio.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_sdio.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_spi.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_rcu.h
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_spi.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_syscfg.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_syscfg.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_timer.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_timer.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_tli.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_tli.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_trng.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_trng.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_usart.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_usart.h

#### Drivers/GD32F4xx_standard_peripheral/Source/gd32f4xx_wwdgt.c

**依赖关系:**
- Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_wwdgt.h

### 第 4 层 (10 个文件)

#### Drivers/CMSIS/DSP/Include/arm_math_types.h

**依赖关系:**
- Drivers/CMSIS/cmsis_compiler.h

  未找到依赖:
    - arm64_neon.h
    - arm_mve.h
    - arm_neon.h

#### Drivers/CMSIS/DSP/Include/dsp/fast_math_functions.h

**依赖关系:**
- Drivers/CMSIS/DSP/Include/arm_math_memory.h
- Drivers/CMSIS/DSP/Include/arm_math_types.h
- Drivers/CMSIS/DSP/Include/dsp/basic_math_functions.h
- Drivers/CMSIS/DSP/Include/dsp/none.h
- Drivers/CMSIS/DSP/Include/dsp/utils.h

#### Drivers/CMSIS/DSP/Include/dsp/none.h

**依赖关系:**
- Drivers/CMSIS/DSP/Include/arm_math_types.h

#### Drivers/CMSIS/DSP/Include/dsp/statistics_functions.h

**依赖关系:**
- Drivers/CMSIS/DSP/Include/arm_math_memory.h
- Drivers/CMSIS/DSP/Include/arm_math_types.h
- Drivers/CMSIS/DSP/Include/dsp/basic_math_functions.h
- Drivers/CMSIS/DSP/Include/dsp/fast_math_functions.h
- Drivers/CMSIS/DSP/Include/dsp/none.h
- Drivers/CMSIS/DSP/Include/dsp/utils.h

#### Drivers/CMSIS/DSP/Include/dsp/support_functions.h

**依赖关系:**
- Drivers/CMSIS/DSP/Include/arm_math_memory.h
- Drivers/CMSIS/DSP/Include/arm_math_types.h
- Drivers/CMSIS/DSP/Include/dsp/none.h
- Drivers/CMSIS/DSP/Include/dsp/utils.h

#### Drivers/CMSIS/DSP/Include/dsp/svm_defines.h

  无依赖关系

#### Drivers/CMSIS/DSP/Include/dsp/utils.h

**依赖关系:**
- Drivers/CMSIS/DSP/Include/arm_math_types.h

#### Drivers/CMSIS/cmsis_compiler.h

**依赖关系:**
- Drivers/CMSIS/cmsis_gcc.h

  未找到依赖:
    - cmsis_armcc.h
    - cmsis_armclang.h
    - cmsis_armclang_ltm.h
    - cmsis_ccs.h
    - cmsis_csm.h
    - cmsis_iccarm.h

#### Drivers/CMSIS/cmsis_gcc.h

  无依赖关系

#### Drivers/FreeRTOS/portable/RVDS/ARM_CM4F/portmacro.h

  无依赖关系

# 项目依赖关系分析报告

## 总体统计信息
- 总文件数: 75 个
- 被调用文件数: 72 个
- 未被调用文件数: 3 个
- 缺少include文件数: 8 个
- 依赖关系层数: 4 层

### 各层文件统计:
- 第 0 层: 1 个文件
- 第 1 层: 6 个文件
- 第 2 层: 5 个文件
- 第 3 层: 61 个文件
### 未找到的include及其依赖

#### Drivers/CMSIS/core_cm4_simd.h
3 个未找到的include
- cmsis_ccs.h
- cmsis_csm.h
- cmsis_iar.h

#### Drivers/CMSIS/core_cmFunc.h
2 个未找到的include
- cmsis_ccs.h
- cmsis_iar.h

#### Drivers/CMSIS/core_cmInstr.h
2 个未找到的include
- cmsis_ccs.h
- cmsis_iar.h

#### Drivers/GD32F4xx_standard_peripheral/Include/gd32f4xx_enet.h
1 个未找到的include
- phy.h


## 分层依赖关系

### 第 0 层 (1 个文件)

#### Core/src/main.c

**依赖关系:**
- Core/inc/gd32f450i_eval.h
- Core/inc/main.h
- Core/inc/systick.h
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

### 第 1 层 (6 个文件)

#### Core/inc/gd32f450i_eval.h

**依赖关系:**
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Core/inc/main.h

  无依赖关系

#### Core/inc/systick.h

  无依赖关系

#### Core/src/gd32f450i_eval.c

**依赖关系:**
- Core/inc/gd32f450i_eval.h

#### Core/src/systick.c

**依赖关系:**
- Core/inc/systick.h
- Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

#### Drivers/CMSIS/GD/GD32F4xx/Include/gd32f4xx.h

**依赖关系:**
- Core/inc/gd32f4xx_libopt.h
- Drivers/CMSIS/GD/GD32F4xx/Include/system_gd32f4xx.h
- Drivers/CMSIS/core_cm4.h

### 第 2 层 (5 个文件)

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
- Drivers/CMSIS/core_cm4_simd.h
- Drivers/CMSIS/core_cmFunc.h
- Drivers/CMSIS/core_cmInstr.h

### 第 3 层 (61 个文件)

#### Drivers/CMSIS/core_cm4_simd.h

  未找到依赖:
    - cmsis_ccs.h
    - cmsis_csm.h
    - cmsis_iar.h

#### Drivers/CMSIS/core_cmFunc.h

  未找到依赖:
    - cmsis_ccs.h
    - cmsis_iar.h

#### Drivers/CMSIS/core_cmInstr.h

  未找到依赖:
    - cmsis_ccs.h
    - cmsis_iar.h

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

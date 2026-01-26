# GD32F470 PMU 项目

基于 GD32F470VIT6 微控制器的电能质量监测单元（PMU - Power Measurement Unit），采用 STM32CubeMX 工作流进行开发，实现三相交流电压测量、过零检测、频谱分析等功能。

## 项目特性

- **硬件平台**: GD32F470VIT6 (兼容 STM32F429VIT6)
- **开发工具链**: ARM GCC + STM32 HAL + Makefile
- **核心功能**:
  - 三相电压实时采样与 FFT 频谱分析（5.88kHz 有效采样率）
  - 过零检测
  - RMS 电压、基波电压、相位角测量
  - 谐波失真（THD）计算
  - 电网频率监测（38-65Hz 可配，滑动平均算法）
  - 循环缓冲区管理与降采样

## 目录

- [快速开始](#快速开始)
- [项目架构](#项目架构)
- [核心模块](#核心模块)
- [编译与烧录](#编译与烧录)
- [STM32-to-GD32 移植说明](#stm32-to-gd32-移植说明)
- [开发指南](#开发指南)
- [常见问题](#常见问题)

## 快速开始

### 环境配置

**必需工具**:

- `arm-none-eabi-gcc` - ARM GCC 编译器
- `make` - GNU Make 构建工具
- `pyocd` 或 `openocd` - 调试/烧录工具

**工具安装**:

```bash
# macOS (使用 Homebrew)
brew install --cask gcc-arm-embedded
brew install make pyocd

# Linux (Ubuntu/Debian)
sudo apt install gcc-arm-none-eabi make
pip install pyocd

# pyocd 设备包安装
pyocd pack update
pyocd pack install GD32F470VI
```

### 克隆与编译

```bash
# 克隆项目
git clone <repository-url>
cd PMU-code

# 编译项目
make

# 清理编译输出
make clean
```

### 烧录程序

**使用 pyocd (推荐)**:

```bash
pyocd flash --erase chip --target GD32F470VI build/cubemx_gd32.elf
```

**使用 OpenOCD**:

```bash
openocd -f cmsis-dap.cfg -f stm32f4xx.cfg \
  -c init \
  -c "reset halt" \
  -c "wait_halt" \
  -c "flash write_image erase build/cubemx_gd32.elf" \
  -c reset \
  -c shutdown
```

## 项目架构

```
PMU-code/
├── Core/                          # 核心应用代码
│   ├── inc/                       # 头文件
│   │   ├── adc_service.h          # ADC 采样服务模块
│   │   ├── zero_crossing.h        # 过零检测模块
│   │   ├── basic_driver.h         # 基础驱动接口
│   │   ├── basic_test.h           # 测试工具
│   │   ├── adc.h / dma.h / tim.h  # HAL 外设配置
│   │   └── ...
│   └── src/                       # 源文件
│       ├── adc_service.c          # ADC 服务实现
│       ├── zero_crossing.c        # 过零检测实现
│       ├── basic_driver.c         # 驱动初始化
│       ├── basic_test.c           # 测试函数
│       ├── main.c                 # 主程序与中断回调
│       └── ...
├── Drivers/                       # HAL 驱动库
│   ├── STM32F4xx_HAL_Driver/      # STM32 HAL（用于 GD32）
│   └── CMSIS/                     # ARM CMSIS 标准
├── Docs/                          # 文档
│   ├── ZERO_CROSSING_README.md    # 过零检测详细文档
│   └── ...
├── build/                         # 编译输出（.gitignore）
├── startup_gd32f450_470.S         # GD32 启动文件
├── gd32f4xx_flash.ld              # GD32 链接脚本
├── Makefile                       # 构建脚本
├── cubemx_gd32.ioc                # STM32CubeMX 项目文件
└── README.md 
```

## 核心模块

### 1. ADC 采样服务模块

**功能**: 三相电压循环采样、FFT 分析、RMS/THD 计算

**硬件电路参数**:

```c
#define ADC_SRV_HW_RUI       390.0f     // 输入电压采样电阻 (Ω)
#define ADC_SRV_HW_UIBL      240390.0f  // 电压互感器倍率
#define ADC_SRV_BASE_VOLT_SCALE  (ADC_SRV_HW_UIBL / ADC_SRV_HW_RUI)  // ≈616.1
```

**主要 API**:

```c
// 初始化与控制
int8_t ADC_SRV_Init(const ADC_SRV_Config_t *config);
int8_t ADC_SRV_Start(void);
int8_t ADC_SRV_Stop(void);

// 数据获取
int8_t ADC_SRV_GetThreePhaseResult(ADC_SRV_ThreePhaseResult_t *result);
int8_t ADC_SRV_GetVoltageResult(uint8_t channel, ADC_SRV_VoltageResult_t *result);

// 定时计算任务（在主循环中调用）
int8_t ADC_SRV_CalculateTask(void);
```

### 2. 过零检测模块

**功能**: 交流电过零检测与频率测量

**主要 API**:

```c
// 初始化与控制
int8_t ZC_Init(void);
int8_t ZC_Start(void);
int8_t ZC_Stop(void);

// 数据获取
int8_t ZC_GetData(ZC_Channel_t channel, ZC_Data_t *data);
float  ZC_GetAvgFrequency(void);  // 双通道平均频率

// 参数配置
int8_t ZC_SetConfig(const ZC_Config_t *config);
int8_t ZC_SetFakePeriod(float ms);              // 毛刺抑制
int8_t ZC_SetFreqRange(float min_hz, float max_hz);  // 频率范围
int8_t ZC_SetWindowSize(uint8_t n);             // 滑动窗口
```

**使用示例**:

```c
// 初始化（已在 basic_driver.c 中完成）
ZC_Init();
ZC_Start();

// 获取频率
ZC_Data_t data;
if (ZC_GetData(ZC_CHANNEL_A, &data) == 0 && data.is_valid) {
    printf("频率: %.3f Hz, 周期: %lu 计数\n",
           data.frequency_avg,
           data.period_avg);
}

// 动态调整参数
ZC_SetFreqRange(49.5f, 50.5f);  // 更严格的频率范围
ZC_SetWindowSize(20);           // 更平滑的输出
```

**详细文档**: 参见 [`Docs/ZERO_CROSSING_README.md`](Docs/ZERO_CROSSING_README.md)

### 3. 基础驱动模块 (basic_driver)

**功能**: 统一初始化各模块，封装定时器参数更新接口

**核心函数**:

```c
void basic_init(void);  // 初始化所有模块（在 main 中调用）
```

**初始化流程**:

1. 配置 TIM7 (500ms 周期，用于测试输出)
2. 初始化过零检测模块 (`ZC_Init()` + `ZC_Start()`)
3. 初始化 ADC 服务模块 (`ADC_SRV_Init()` + `ADC_SRV_Start()`)
4. 启动 ADC DMA 转换
5. 配置 TIM10 (240ms 周期，用于 ADC 计算任务标志)

### 4. 中断回调架构

**设计原则**: 所有 HAL 中断回调函数在 `main.c` 中重写，保持模块解耦。

**TIM 中断回调** (`HAL_TIM_PeriodElapsedCallback`):

```c
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM7) {
        // LED 闪烁 + 测试数据打印
        print_test_data();
    } else if (htim->Instance == TIM10) {
        // 标记 ADC 计算任务
        adc_srv_calc_pending = 1;
    }
}
```

**TIM 输入捕获回调** (`HAL_TIM_IC_CaptureCallback`):

```c
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM2) {
        // 过零检测模块处理
        ZC_TIM_CaptureCallback(htim);
    }
}
```

**ADC 转换完成回调** (`HAL_ADC_ConvCpltCallback`):

```c
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC1) {
        // ADC 服务模块数据处理
        ADC_SRV_DMA_ConvCpltCallback(hadc, adc_buffer);
    }
}
```

## 编译与烧录

### 编译命令

```bash
# 完整编译
make

# 查看编译详情
make V=1

# 清理
make clean

# 查看内存使用
arm-none-eabi-size build/cubemx_gd32.elf
```

**典型编译输出**:

```
   text    data     bss     dec     hex filename
   5932     108    3444    9484    250c build/cubemx_gd32.elf
```

- `text`: 代码段 (Flash)
- `data`: 初始化数据 (Flash → RAM)
- `bss`: 未初始化数据 (RAM)

### 烧录方法

#### 方法 1: pyocd (推荐)

```bash
# 首次设置
pyocd pack update
pyocd pack install GD32F470VI

# 烧录
pyocd flash --erase chip --target GD32F470VI build/cubemx_gd32.elf

# 调试
pyocd gdb --target GD32F470VI
```

**pyocd pack 位置**:

- Linux: `$HOME/.local/share/cmsis-pack-manager`
- macOS: `$HOME/Library/Application Support/cmsis-pack-manager`
- Windows: `C:\Users\$USER\AppData\cmsis-pack-manager`

#### 方法 2: OpenOCD

```bash
openocd -f cmsis-dap.cfg -f stm32f4xx.cfg \
  -c "program build/cubemx_gd32.elf verify reset exit"
```

**注意**: 使用 `stm32f4xx.cfg` 而非 `gd32f4xx.cfg` 以避免兼容性问题。

## STM32-to-GD32 移植说明

本项目使用 **STM32 HAL 驱动库**开发 GD32 硬件，通过以下方式实现兼容性：

### 核心原理

GD32F4xx 是 STM32F4xx 的寄存器级兼容芯片，STM32 HAL 驱动可以直接在 GD32 上运行。

### 移植步骤

#### 1. 选择对应芯片

| GD32 型号    | 对应 STM32 型号 | 说明                   |
| ------------ | --------------- | ---------------------- |
| GD32F470VIT6 | STM32F429VIT6   | 外设资源、封装完全一致 |

#### 2. 使用 STM32CubeMX 生成代码

1. 打开 `cubemx_gd32.ioc`，选择 **STM32F429VIT6**
2. 配置外设（时钟、GPIO、TIM、ADC 等）
3. 选择 **Makefile** 工程类型生成代码

#### 3. 替换启动文件和链接脚本

**关键 Makefile 修改**:

```makefile
# 使用 GD32 启动文件（非 STM32 生成的）
ASM_SOURCES =
ASMM_SOURCES = startup_gd32f450_470.S  # 而非 startup_stm32f429xx.s

# 使用 GD32 链接脚本（非 STM32 生成的）
LDSCRIPT = gd32f4xx_flash.ld           # 而非 STM32F429XX_FLASH.ld
```

#### 4. 修改中断向量表

编辑 `startup_gd32f450_470.S`，将 GD32 的中断名称映射到 STM32 HAL 期望的名称：

**原始 GD32 命名**:

```asm
.word DMA0_Channel0_IRQHandler
.word DMA0_Channel1_IRQHandler
```

**修改为 STM32 命名**:

```asm
.word DMA1_Stream0_IRQHandler
.word DMA1_Stream1_IRQHandler
```

**保留不存在的外设** (如 SAI):

```asm
.word 0     /* Vector Number 103, Reserved (STM32 的 SAI，GD32 无此外设) */
```

#### 5. 验证配置

```bash
# 检查向量表符号
arm-none-eabi-nm build/cubemx_gd32.elf | grep -E "_sidata|_sdata|_edata|_sbss|_ebss"

# 检查向量表位置
arm-none-eabi-objdump -h build/cubemx_gd32.elf | grep -E "vectors|isr"

# 反汇编前 120 行（检查 Reset_Handler）
arm-none-eabi-objdump -D build/cubemx_gd32.elf | sed -n '1,120p'
```

### 兼容性注意事项

1. **时钟配置**: 必须读取实际 PLL 配置（`SystemClock_Config()`），不要假设最大时钟
2. **外设差异**: 部分 STM32 外设（SAI、高级定时器）在 GD32 上可能不存在或有差异
3. **OpenOCD 配置**: 使用 `stm32f4xx.cfg` 而非 `gd32f4xx.cfg`（除非使用 Windows GD32EmbeddedBuilder）

## 开发指南

### 添加新源文件

1. 将 `.c` 文件放入 `Core/src/`
2. 将 `.h` 文件放入 `Core/inc/`
3. **更新 Makefile** 的 `C_SOURCES` 变量（约第 38-78 行）
4. 运行 `make` 编译

### 命名约定

- **外部函数**: 大写模块名开头（如 `ZC_Init`, `ADC_SRV_Start`）
- **内部函数**: 下划线开头（如 `_process_capture`, `_calculate_fft`）
- **宏定义**: 全大写（如 `ADC_SRV_BUFFER_SIZE`）
- **结构体**: 首字母大写 + `_t` 后缀（如 `ZC_Data_t`）

### 模块开发模式

**初始化位置**:

- HAL 外设配置: `adc.c`, `tim.c`, `gpio.c` (CubeMX 生成)
- 自定义模块初始化: `basic_driver.c` 中的 `basic_init()`
- 中断回调函数: `main.c` (保持解耦)

**示例: 添加新模块**

```c
// 1. 在 Core/inc/my_module.h 中定义接口
#ifndef __MY_MODULE_H
#define __MY_MODULE_H
int8_t MY_Init(void);
int8_t MY_Start(void);
#endif

// 2. 在 Core/src/my_module.c 中实现
#include "my_module.h"
int8_t MY_Init(void) { /* ... */ }
int8_t MY_Start(void) { /* ... */ }

// 3. 在 basic_driver.c 的 basic_init() 中调用
#include "my_module.h"
void basic_init(void) {
    // ... 其他初始化
    MY_Init();
    MY_Start();
}

// 4. 更新 Makefile 的 C_SOURCES
C_SOURCES += Core/src/my_module.c

// 5. 编译
make
```

### 调试技巧

```bash
# 终端 1: 启动 pyocd GDB 服务器
pyocd gdb --target GD32F470VI

# 终端 2: 连接 GDB
arm-none-eabi-gdb build/cubemx_gd32.elf
(gdb) target remote :3333
(gdb) load
(gdb) break main
(gdb) continue
```

## 常见问题

### 1. pyocd XML 错误

**现象**: `pyocd flash` 报 XML 解析错误

**原因**: GD32 SVD 文件头有多余空格

**解决方法**:

```bash
# 1. 解压 pack 文件
cd ~/.local/share/cmsis-pack-manager/packs/GigaDevice/GD32F4xx_DFP/3.0.3/
mv GigaDevice.GD32F4xx_DFP.3.0.3.pack GD32F4xx.zip
unzip GD32F4xx.zip -d extracted/

# 2. 编辑 SVD 文件，删除 XML 声明前的空格
vim extracted/SVD/GD32F4xx.svd
# 删除第一行的前导空格

# 3. 重新打包
cd extracted && zip -r ../GigaDevice.GD32F4xx_DFP.3.0.3.pack *
```

### 2. 时钟计算错误

**错误示例**: 假设 PCLK2 = 84MHz → ADC_CLK = 10.5MHz

**正确方法**: 查看 `main.c` 中的 `SystemClock_Config()`

```c
RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;  // PCLK2 = 32MHz
// ADC_CLOCK_SYNC_PCLK_DIV8 → ADC_CLK = 4MHz
```

### 3. 首次烧录失败

**原因**: 芯片保护或引脚配置错误

**解决方法**:

```bash
# 完全擦除芯片
pyocd erase --chip --target GD32F470VI

# 或使用 OpenOCD
openocd -f cmsis-dap.cfg -f stm32f4xx.cfg \
  -c init \
  -c "reset halt" \
  -c "stm32f4x mass_erase 0" \
  -c shutdown
```

### 4. 数据异常

**问题**: ADC 或过零检测数据异常

**排查步骤**:

1. 检查数据有效标志: `data.is_valid`
2. 查看统计信息: `data.error_count / data.valid_count`
3. 调整参数范围（频率、毛刺抑制）
4. 验证硬件信号质量（示波器）

### 5. 内存不足

**现象**: 编译报 `.bss will not fit in region 'RAM'`

**解决方法**:

1. 查看 `.map` 文件定位大数组
2. 减小缓冲区大小（如 `ADC_SRV_BUFFER_SIZE`）
3. 使用 CCM (0x10000000) 存储临时数组

## 相关文档

- [过零检测模块详细文档](Docs/ZERO_CROSSING_README.md)
- [CLAUDE.md](CLAUDE.md) - AI 开发辅助说明
- [AGENTS.md](AGENTS.md) - 项目开发规范

## 更新日志

- **2025-11-12**: 完善 README，新增 ADC 服务模块说明
- **2025-10-11**: 添加过零检测模块
- **2025-08-10**: 初始项目结构

# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a GD32F470VIT6 microcontroller project that uses STM32CubeMX workflow for development. The project uses GD32 (a cost-effective STM32 clone) but leverages STM32 HAL drivers and development tools. The main application is a PMU (Power Measurement Unit) with zero-crossing detection capabilities for AC power monitoring.

**Key Architecture Point**: This project uses STM32 HAL drivers for GD32 hardware by carefully mapping interrupt vectors and peripheral definitions between the two platforms.

## Build Commands

### Compilation
```bash
# Build the project
make

# Clean build artifacts
make clean
```

Build outputs are in `build/`:
- `cubemx_gd32.elf` - Main executable
- `cubemx_gd32.hex` - Intel HEX format
- `cubemx_gd32.bin` - Binary format
- `cubemx_gd32.map` - Memory map

### Flashing/Programming

**Using pyocd (recommended for Mac/Linux)**:
```bash
# One-time setup
pyocd pack update
pyocd pack install GD32F470VI

# Flash the device
pyocd flash --erase chip --target GD32F470VI build/cubemx_gd32.elf
```

**Using OpenOCD**:
```bash
# Use stm32f4xx.cfg as compatibility layer (gd32f4xx.cfg renamed)
openocd -f cmsis-dap.cfg -f stm32f4xx.cfg -c init -c "reset halt" -c "wait_halt" -c "flash write_image erase build/cubemx_gd32.elf" -c reset -c shutdown
```

## Code Architecture

### STM32-to-GD32 Compatibility Layer

The project achieves GD32 compatibility through:

1. **Startup File**: `startup_gd32f450_470.S` with interrupt vectors mapped to STM32 naming conventions
2. **Linker Script**: `gd32f4xx_flash.ld` configured for GD32F470VIT (3MB Flash, 512KB SRAM, 64KB TCM)
3. **HAL Drivers**: Uses STM32F4xx HAL drivers which are register-compatible with GD32

**IMPORTANT**: When modifying interrupt handlers, ensure names match both:
- The startup file's vector table entries
- The STM32 HAL driver expectations
- Some peripherals (e.g., SAI) exist in STM32 but not GD32 - these remain as `0` (Reserved) in the vector table

### Module Structure

The codebase follows a modular peripheral driver pattern:

```
Core/
├── inc/            # Header files for each peripheral module
│   ├── adc.h       # ADC configuration and control
│   ├── tim.h       # Timer configurations
│   ├── gpio.h      # GPIO setup
│   ├── usart.h     # Serial communication
│   ├── dma.h       # DMA configuration
│   ├── i2c.h       # I2C bus
│   ├── spi.h       # SPI bus
│   ├── zero_crossing.h  # Zero-crossing detection module
│   ├── basic_driver.h   # Low-level driver utilities
│   └── basic_test.h     # Testing framework
└── src/            # Corresponding implementation files
```

**Callback Architecture**: The project uses HAL callback functions for interrupt handling. Key callbacks are defined in `main.c` (lines 78-100):
- `HAL_TIM_PeriodElapsedCallback()` - Timer period elapsed events
- `HAL_TIM_IC_CaptureCallback()` - Timer input capture (used by zero-crossing)
- `HAL_ADC_ConvCpltCallback()` - ADC conversion complete

### Zero-Crossing Detection Module

The zero-crossing detection is a major feature for AC power monitoring:

- **Hardware**: TIM2 input capture on PB10 (CH3/A-phase) and PB11 (CH4/C-phase)
- **Timer Config**: 16MHz clock (APB1 64MHz ÷ 4 prescaler), 62.5ns resolution
- **Algorithm**:
  - Glitch suppression (default 2ms)
  - Frequency range validation (38-65Hz default)
  - Sliding window averaging (10 periods default)
  - Automatic overflow handling for 32-bit counter

**Module Files**:
- `Core/inc/zero_crossing.h` - API definitions
- `Core/src/zero_crossing.c` - Implementation
- `Docs/ZERO_CROSSING_README.md` - Detailed documentation with 11 usage examples

**Initialization Pattern**: See `main.c` - modules are initialized after HAL peripheral setup:
```c
// After MX_TIM2_Init()
ZC_Init();   // Initialize zero-crossing module
ZC_Start();  // Start detection
```

### STM32CubeMX Integration

This project uses STM32CubeMX for initial code generation:
- `.ioc` file: `cubemx_gd32.ioc`
- HAL configuration: `Core/inc/stm32f4xx_hal_conf.h`
- System initialization: `Core/src/system_stm32f4xx.c`

**USER CODE blocks**: When regenerating from CubeMX, code within `/* USER CODE BEGIN */` and `/* USER CODE END */` markers is preserved.

## Hardware Target

**Microcontroller**: GD32F470VIT6
- Core: ARM Cortex-M4F @ 200MHz
- Flash: 3MB (0x08000000)
- SRAM: 512KB (0x20000000)
- TCM RAM: 64KB (0x10000000)
- Compatible with: STM32F429VIT6

## Toolchain

**Required Tools**:
- `arm-none-eabi-gcc` - ARM GCC compiler (uses hardware FPU: `-mfloat-abi=hard -mfpu=fpv4-sp-d16`)
- `make` - Build system
- `pyocd` or `openocd` - Debug/flash tool

**Compiler Flags** (from Makefile):
- CPU: Cortex-M4 with FPU
- Optimization: `-Og` (debug builds)
- Defines: `USE_HAL_DRIVER`, `STM32F429xx` (note: uses STM32 define for GD32)

## Development Workflow

### Adding New Source Files

1. Place `.c` files in appropriate directory (usually `Core/src/`)
2. Place `.h` files in `Core/inc/`
3. **Add to Makefile**: Update `C_SOURCES` variable (lines 38-78)
4. Run `make`

### Modifying Peripheral Configuration

If using STM32CubeMX to regenerate:
1. Edit `cubemx_gd32.ioc` in CubeMX
2. Generate code for STM32F429VIT6 with Makefile option
3. Verify `startup_gd32f450_470.S` is still referenced (not regenerated startup_stm32f429xx.s)
4. Verify `gd32f4xx_flash.ld` is still referenced (not regenerated STM32F429XX_FLASH.ld)

**Critical Makefile Lines** (do not let CubeMX overwrite):
```makefile
ASMM_SOURCES = startup_gd32f450_470.S  # Not startup_stm32f429xx.s
LDSCRIPT = gd32f4xx_flash.ld           # Not STM32F429XX_FLASH.ld
```

### Debugging

Memory usage is displayed after build:
```
   text    data     bss     dec     hex filename
   5932     108    3444    9484    250c build/cubemx_gd32.elf
```

To verify symbols and vector table:
```bash
arm-none-eabi-nm build/cubemx_gd32.elf | grep -E "_sidata|_sdata|_edata|_sbss|_ebss"
arm-none-eabi-objdump -h build/cubemx_gd32.elf | grep -E "vectors|isr"
```

## Common Pitfalls

1. **pyocd XML Error**: GD32 SVD files may have leading spaces. Fix by:
   - Extract `.pack` file → `.zip`
   - Edit `SVD/GD32F4xx.svd` - remove 2 leading spaces from XML header
   - Re-compress as `.pack`

2. **OpenOCD Compatibility**: Use `stm32f4xx.cfg` with GD32, not `gd32f4xx.cfg`, unless using Windows GD32EmbeddedBuilder's OpenOCD

3. **Interrupt Vector Mismatch**: If adding new interrupt handlers, ensure the function name matches the startup file's vector table entry

4. **Clock Configuration**: System assumes APB1 = 64MHz. If changing clocks, recalculate all timer prescalers and zero-crossing module parameters (see ZERO_CROSSING_README.md formulas)

5. **HAL Peripheral Incompatibilities**: Some STM32 peripherals (SAI, advanced timers) may not exist or differ on GD32 - verify in GD32F4xx datasheet

6. **ADC Clock Calculation Error**:
   - **WRONG**: Assuming PCLK2 = 84MHz → ADC_CLK = 84MHz/8 = 10.5MHz
   - **CORRECT**: Must read actual PLL configuration from `SystemClock_Config()`:
     - HSE = 12MHz, PLLM=6, PLLN=64, PLLP=DIV2 → SYSCLK = 64MHz
     - APB2CLKDivider = DIV2 → PCLK2 = 32MHz
     - ADC_CLOCK_SYNC_PCLK_DIV8 → **ADC_CLK = 4MHz**
   - **Key lesson**: Always verify actual clock tree from main.c, not assumptions from comments or datasheet maximum values

## personal habit

- 中断回调函数倾向于在main.c中重写
- 个人代码的初始化工作都在 @Core/src/basic_driver.c 进行而不是在main.c; 个人代码不包括stm32cubeMX自动生成的
- 非外部调用函数以_开头而不是以大写模块名开头，举例：外部ZC_Init, 内部_process_capture；外部ADC_SRV_Start, 内部_calculate_fft
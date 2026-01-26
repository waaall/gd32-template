# PMU-code Project Instructions

This is a **Power Monitoring Unit (PMU)** project running on GD32F450VIT6 microcontroller (ARM Cortex-M4), designed for three-phase AC power grid analysis. The project uses STM32 HAL compatibility layers to run on GD32 hardware.

## Project Architecture

### Hardware Platform
- **Target MCU**: GD32F450VIT6 (LQFP-100, Cortex-M4, 200MHz max)
- **Compatibility**: Uses STM32F4xx HAL drivers (GD32 is STM32F4-compatible)
- **Key Hardware**: RN8209C power measurement IC, MCP6542 comparators, TIM2 input capture for zero-crossing detection

### Critical Cross-Platform Pattern
This project demonstrates the **GD32-STM32 compatibility approach**:
```c
// Uses STM32F4xx headers but runs on GD32F450 hardware
#include "stm32f4xx_hal.h"  // Actually runs on GD32
```

Key files for GD32-STM32 compatibility:
- `startup_gd32f450_470.S` - GD32 startup file (not STM32)
- `gd32f4xx_flash.ld` - GD32 linker script
- OpenOCD configs: Use `stm32f4xx.cfg` instead of `gd32f4xx.cfg` for compatibility

## Core Application Domain

### Primary Function: Zero-Crossing Detection & Frequency Measurement
This is a **precision grid frequency analyzer** with sub-Hz accuracy:

```c
// Core module: zero_crossing.c
// TIM2 CH3 (PB10): FA signal - A-phase zero crossing
// TIM2 CH4 (PB11): FC signal - C-phase zero crossing
// Timer precision: 13.89ns (72MHz, Prescaler=0)
// Frequency accuracy: ±0.00007% for 50Hz grid
```

**Key signal path**: AC Grid → ZMPT107 voltage transformer → MCP6542 comparator → TIM2 input capture → MCU frequency calculation

### Secondary Functions
- **Power measurement**: Via RN8209C IC communication (UART)
- **ADC sampling**: AHALF buffered AC voltage sampling for waveform analysis
- **Flash storage**: External SPI flash for data logging

## Build System & Workflow

### Build Commands
```bash
# Primary build (uses existing Makefile)
make

# Clean build
make clean

# Flash to target
make download  # Uses OpenOCD task
```

### VS Code Integration
Available tasks in `.vscode/tasks.json`:
- **build**: Runs `make` 
- **download**: Uses OpenOCD to flash via CMSIS-DAP

### Debugging Setup
```bash
# OpenOCD with GD32 (using STM32 config for compatibility)
openocd -f cmsis-dap.cfg -f stm32f4xx.cfg

# Alternative: pyocd (recommended for GD32)
pyocd flash --target GD32F470VI build/cubemx_gd32.elf
```

## Project Structure Patterns

### Core Application Code
```
Core/
├── inc/                     # Application headers
│   ├── main.h              # Pin definitions, GPIO mappings
│   ├── zero_crossing.h     # Zero-crossing detection API
│   └── basic_driver.h      # Hardware abstraction layer
└── src/                     # Application source
    ├── main.c              # Main loop, initialization
    ├── zero_crossing.c     # TIM2 input capture, frequency calc
    └── basic_driver.c      # ADC, GPIO, peripheral drivers
```

### Hardware Configuration
```
hardware/硬件电路文档.md     # Complete hardware design docs (Chinese)
cubemx_gd32.ioc           # STM32CubeMX project file (for GD32)
gd32f4xx_flash.ld         # Memory layout: 3MB Flash, 512KB RAM
```

## Key Technical Patterns

### Timer Configuration for Maximum Precision
```c
// TIM2 setup for zero-crossing capture
Prescaler: 0              // Maximum resolution (13.89ns)
Counter Period: 0xFFFFFFFF // 32-bit timer, 59.6s range
Input Filter: 0           // No digital filtering (hardware already filtered)
```

### Memory Configuration (GD32F470VIT)
```
Flash:   0x08000000 - 3072KB (3MB)
SRAM:    0x20000000 - 512KB  
TCM RAM: 0x10000000 - 64KB
```

### HAL Error Handling Pattern
```c
#define CHECK_HAL_STATUS(status)     \
    do {                             \
        if ((status) != HAL_OK) {    \
            handle_hal_error();      \
        }                            \
    } while(0)
```

## Development Conventions

### File Naming
- Use STM32-style naming even for GD32-specific files
- Hardware docs in Chinese: `硬件电路文档.md`
- Zero-crossing module uses `ZC_` prefix consistently

### Frequency Measurement Constants
```c
#define ZC_FREQ_CALC_CONST 16000000U  // 16MHz timer clock
// Usage: frequency_hz = ZC_FREQ_CALC_CONST / period_counts
```

### ADC Channel Mapping
Based on `cubemx_gd32.ioc`:
- PA0-PA7, PB0-PB1: ADC123_IN0-9
- PC0-PC3: ADC123_IN10-13  
- AHALF signal → ADC sampling for waveform analysis

## Testing & Debugging

### Zero-Crossing Module Testing
```c
ZC_Data_t data;
if (ZC_GetData(ZC_CHANNEL_A, &data) == 0 && data.is_valid) {
    printf("Frequency: %.3f Hz\n", data.frequency_avg);
    printf("Period: %lu counts\n", data.period_avg);
}
```

### Hardware Verification Commands
```bash
# Check vector table and symbols
arm-none-eabi-objdump -h build/cubemx_gd32.elf | grep -E "vectors|isr"
arm-none-eabi-nm build/cubemx_gd32.elf | grep -E "_sidata|_sdata|_edata"
```

## Common Issues & Solutions

### GD32-STM32 Compatibility Issues
1. **Interrupt vector naming**: GD32 uses different DMA interrupt names
2. **OpenOCD config**: Use `stm32f4xx.cfg`, not `gd32f4xx.cfg` 
3. **SVD file issues**: GD32 pack files may have XML whitespace errors

### Precision Requirements
This is a **power grid analyzer** requiring exceptional timing accuracy:
- Grid frequency tolerance: ±0.2% (national standard)
- This implementation achieves: ±0.00007% (3000x better)
- Critical for power quality analysis and protection systems

When working with this codebase, prioritize timing precision, understand the dual measurement architecture (analog + digital), and maintain the GD32-STM32 compatibility patterns.
# Repository Guidelines

## Project Structure & Module Organization
- `Core/src` holds CubeMX-generated application code plus custom modules such as `basic_driver.c` and `zero_crossing.c`; headers mirror in `Core/inc`.
- `Drivers/STM32F4xx_HAL_Driver` and `Drivers/CMSIS` bring in vendor libraries—treat them as third-party and avoid manual edits.
- `build/` is produced by `make`; wipe with `make clean` and keep it out of version control, while `Docs/` and `hardware/` collect reference assets.

## Build, Test, and Development Commands
- `make` (optionally `GCC_PATH=/path/to/toolchain`) builds `build/cubemx_gd32.elf` and reports section sizes.
- `make clean` clears `build/` before a rebuild or when switching branches.
- `openocd -f cmsis-dap.cfg -f gd32f4xx.cfg -c "program build/cubemx_gd32.elf verify reset exit"` flashes through OpenOCD; use `pyocd flash --target GD32F470VI build/cubemx_gd32.elf` when on a CMSIS-DAP probe.

## Coding Style & Naming Conventions
- Keep the CubeMX layout: two-space indentation at block starts, braces on their own lines, and nested bodies indented consistently.
- Custom functions stay snake_case (`print_test_data`), callbacks retain HAL naming, and macros remain uppercase.
- Preserve CubeMX doc blocks (`@brief`, `@note`); add short English comments only where behavior diverges from generated defaults.
- Run `clang-format -style=LLVM` on modified C files before committing sizeable edits.

## Testing Guidelines
- Treat builds as the first test: `make` must pass cleanly, then verify critical interrupts (`TIM`, `ADC`, `GPIO`) on hardware.
- Document new bench procedures inside `Docs/`, noting trigger sources and expected UART output from helpers like `print_test_data()`.
- Place experimental diagnostics in `*_test.c` files and guard them with compile-time flags so release images stay lean.

## Commit & Pull Request Guidelines
- Model commit messages on existing history—concise, imperative statements such as `add more debug code`; squash noisy WIP locally.
- PRs should link issues, list hardware validation (probe, board revision, exercised modules), and attach logs or captures when diagnosing timing.
- Rebase onto `main`, confirm `make` is warning-free, and keep generated artifacts in sync with `cubemx_gd32.ioc` before asking for review.

## Flashing & Debugging Notes
- Confirm `gd32f4xx_flash.ld` matches your board’s memory layout before programming; stage linker edits in a dedicated commit.
- Refresh CMSIS device packs with `pyocd pack update` whenever HAL sources change to keep device definitions current.

## Clock Calculation Error:
   - **CORRECT**: Must read actual PLL configuration from `SystemClock_Config()`:
     - HSE = 12MHz, PLLM=6, PLLN=64, PLLP=DIV2 → SYSCLK = 64MHz
     - APB2CLKDivider = DIV2 → PCLK2 = 32MHz
     - ADC_CLOCK_SYNC_PCLK_DIV8 → **ADC_CLK = 4MHz**
   - **Key lesson**: Always verify actual clock tree from main.c, not assumptions from comments or datasheet maximum values

## personal habit

- 中断回调函数倾向于在main.c中重写
- 个人代码的初始化工作都在 @Core/src/basic_driver.c 进行而不是在main.c; 个人代码不包括stm32cubeMX自动生成的
- 非外部调用函数以_开头而不是以大写模块名开头，举例：外部ZC_Init, 内部_process_capture；外部ADC_SRV_Start, 内部_calculate_fft
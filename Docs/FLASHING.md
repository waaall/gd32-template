# STM32 / GD32 编译与烧录说明

## 1. 先分清固件、下载器和目标配置

本工程只有一套 Makefile，生成同一份应用固件的三种格式：

- `build/cubemx_gd32.elf`：包含段地址和调试信息，推荐用于下载、校验和调试。
- `build/cubemx_gd32.hex`：包含地址信息。
- `build/cubemx_gd32.bin`：裸二进制，需要指定下载地址。

`cubemx_gd32` 是当前工程的输出名称，不能只凭名字判断是否适用于某块板。
OpenOCD 的 `.cfg` 决定怎样连接下载器和操作目标 Flash，不会重新编译或转换应用固件。
**同一份已构建固件，只更换下载器 / 目标 `.cfg`，文件内容不会变化；更换晶振配置并重新编译则会改变固件。**
两种芯片能否共用固件，还取决于实际用到的中断、外设、时钟及存储地址，不是更换 `.cfg` 就能保证。

### 配置选择表

| 目标 / 用途 | 下载器配置 | 目标配置 | 验证情况及限制 |
| --- | --- | --- | --- |
| STM32F429VIT6 + ST-LINK | `stlink.cfg` | `stm32f4x.cfg` | 本机已实测连接、识别和 Flash 探测通过 |
| GD32F470 + CMSIS-DAP，原生 GD32 驱动 | `cmsis-dap.cfg` | `gd32f4xx.cfg` | 需要包含 `gd32f4xx` Flash 驱动的 OpenOCD；本次未接 GD32 实测 |
| GD32 历史兼容配置 | `cmsis-dap.cfg` | `stm32f4xx.cfg` | 实际使用 `stm32f2x` Flash 驱动；仅配置解析通过，不能代替 GD32 实机验证 |
| GD-Link，且枚举为 CMSIS-DAP | 已包含在文件中 | `openocd_gdlink.cfg` | 合并下载器与 GD32 目标配置；同样需要 `gd32f4xx` Flash 驱动 |

**特别注意：`stm32f4x.cfg` 与 `stm32f4xx.cfg` 是两个不同文件。**
前者是这次 STM32 + ST-LINK 测试使用的配置；后者是仓库保留的精简兼容配置，文件头虽写 GD32，但 Flash 驱动仍是 `stm32f2x`。
不要根据文件名或注释混用。

## 2. 接线与烧录前检查

1. 确认目标芯片型号、板卡版本和板上高速晶振频率。
2. 连接 SWDIO、SWCLK、GND；按下载器要求将 VTref 接目标板的 3.3V 参考电源。需要硬件复位时再接 NRST。
3. 核实目标板实际 VDD。不要将 5V 接到芯片 VDD 或 3.3V 参考线上，也不要把下载器输出电源误当电压检测输入。
4. 关闭占用下载器的其他 OpenOCD / IDE 会话。
5. 将继电器、RS485 总线等置于允许固件启动的安全状态；本固件不只是串口桥，还会初始化 ADC、过零检测和看门狗。
6. Flash 校验只能证明写入内容匹配，不能证明串口桥及采样功能已经通过硬件测试。

本次 ST-LINK 曾报告约 4.4～4.5V。该读数不等同于万用表测得的芯片 VDD，需核实 VTref 接线和实际供电，未确认前不继续擦写。

以下命令均在工程根目录执行。多个下载器同时连接时，在目标配置之后、`init` 或 `program` 之前添加：

```bash
-c "adapter serial 实际下载器序列号"
```

## 3. 晶振只通过现有脚本切换

不要单独手工修改 `main.c` 或 HAL 头文件；使用：

```bash
# 当前 STM32 开发板：8MHz
python3 Scripts/switch_hse_freq.py 8

# 板上晶振确为 12MHz 时使用；不要只根据芯片品牌选择
python3 Scripts/switch_hse_freq.py 12

# reset 的含义是恢复 12MHz 配置，不是复位芯片
python3 Scripts/switch_hse_freq.py reset
```

脚本同时修改：

| 文件 | 修改项 |
| --- | --- |
| `Core/inc/stm32f4xx_hal_conf.h` | `HSE_VALUE` |
| `Core/src/main.c` | `RCC_OscInitStruct.PLL.PLLM` |
| 根目录唯一的 `.ioc` 文件 | `RCC.HSE_VALUE`、`RCC.PLLM` |

当前 PLLN=64、PLLP=DIV2：

- 8MHz：PLLM=4，SYSCLK = 8 / 4 × 64 / 2 = **64MHz**。
- 12MHz：PLLM=6，SYSCLK = 12 / 6 × 64 / 2 = **64MHz**。
- 两种配置下 APB1 / APB2 均为 32MHz；当前 ADC 使用 PCLK2 / 8，ADC 时钟为 4MHz。

脚本仅修改文件并输出验证信息，**不会自动执行 make 或烧录**。
切换失败时检查输出，不要继续编译下载。

## 4. 允许编译后再生成固件

```bash
make clean && make
arm-none-eabi-size build/cubemx_gd32.elf
arm-none-eabi-objdump -s -j .vectors build/cubemx_gd32.elf
```

切换晶振后，旧 `build/` 固件仍是旧配置，不能直接拿来下载。
日志和原固件备份请放在 `logs/flashing/`，不要放在会被 `make clean` 删除的 `build/` 内。

### 本工程当前链接布局的边界

Makefile 仍引用 `startup_gd32f450_470.S` 和 `gd32f4xx_flash.ld`。
链接脚本声明 Flash 3072KB、普通 RAM 512KB 和 TCMRAM 64KB，不能当成 STM32F429 的实际容量。
每次构建都应检查实际装载段、初始栈和使用的中断是否落在目标支持的范围内。

当前启动文件的向量表首项引用 **`_sp`，不是 `_estack`**。
2026-10-23 的 12MHz 构建中，`_sp=0x2000F158`，并不是 `0x20080000`；
因此不能仅凭 `_estack` 数值断言当前固件的初始栈已越界。
但 `sysmem.c` 的 `_sbrk()` 会使用 `_estack` 作为堆边界，这仍是跨芯片复用时需要审查的限制，
也不应使用 GD32 的总容量去判断 STM32 固件是否安全。
本次文档工作没有重写启动文件、链接脚本或堆分配逻辑。

## 5. STM32F429VIT6 + ST-LINK

### 5.1 先测试连接和 Flash 探测，不烧录

```bash
openocd -f ./stlink.cfg -c "transport select hla_swd" \
  -f ./stm32f4x.cfg \
  -c "gdb_port disabled" -c "tcl_port disabled" -c "telnet_port disabled" \
  -c init -c targets \
  -c "mdw 0xE0042000 1" \
  -c "flash probe 0" -c "flash info 0" \
  -c shutdown
```

这不会执行擦除、下载或复位。目标配置的默认连接事件会设置调试相关寄存器；
需要严格只读检查时，可在 `init` 前加 `-c "stm32f4x.cpu configure -event examine-end {}"` 禁用该事件。

本机已读取到：

```text
STLINK V2J45M31
clock speed 2000 kHz
stm32f4x.cpu ... running
device id = 0x20036419
flash size = 2048 KiB
Dual Bank 2048 kiB STM32F42x/43x/469/479 found
flash 'stm32f2x' found at 0x08000000
```

`stm32f2x` 是该 OpenOCD 配置的 Flash 驱动名称，不表示芯片被识别成 STM32F2。
本次探测列出 24 个扇区，均显示 `not protected`。

### 5.2 备份板内原固件

下面的 **2MB 长度只适用于已探测确认的这块 STM32**。
该命令读取主 Flash，不包含 Option Bytes、OTP 或外部存储器，也不是运行中数据的一致性快照。
不要在设备正在自行写 Flash 时备份。

```bash
mkdir -p logs/flashing
BACKUP="logs/flashing/stm32-before-$(date +%Y%m%d-%H%M%S).bin"
openocd -f ./stlink.cfg -c "transport select hla_swd" \
  -f ./stm32f4x.cfg \
  -c "gdb_port disabled" -c "tcl_port disabled" -c "telnet_port disabled" \
  -c "stm32f4x.cpu configure -event examine-end {}" \
  -c init -c "dump_image $BACKUP 0x08000000 0x00200000" \
  -c shutdown && shasum -a 256 "$BACKUP"
```

确认备份成功及文件长度为 2097152 字节后再继续。不要执行整片擦除来测试连接。

### 5.3 下载、校验并复位运行

确认晶振、供电、最新构建和备份后，执行：

```bash
openocd -f ./stlink.cfg -c "transport select hla_swd" \
  -f ./stm32f4x.cfg \
  -c "program build/cubemx_gd32.elf verify reset exit"
```

`program` 会复位 / 暂停目标、按映像需要擦除 Flash 扇区并写入；
`verify` 校验写入内容；`reset` 使目标重新启动；`exit` 退出 OpenOCD。
被擦除扇区内、但不属于本映像的数据也可能丢失，因此先备份。
不要把“下载成功”“校验成功”“应用功能正常”混为一谈。

如果必须使用 BIN：

```bash
openocd -f ./stlink.cfg -c "transport select hla_swd" \
  -f ./stm32f4x.cfg \
  -c "program build/cubemx_gd32.bin 0x08000000 verify reset exit"
```

ELF / HEX 自带地址，通常不额外指定 `0x08000000`。

## 6. GD32F470 + CMSIS-DAP / GD-Link

本次未连接 GD32，以下步骤不代表 GD32 实机下载验证完成。
先确认下载器是否枚举为 CMSIS-DAP，并确认 OpenOCD 支持目标 Flash 驱动。

### 6.1 包含原生 GD32 驱动的 OpenOCD

```bash
# 只检查配置能否解析，不连接硬件
openocd -f ./cmsis-dap.cfg -f ./gd32f4xx.cfg -c shutdown

# 连接并探测，核实芯片及 Flash 实际容量
openocd -f ./cmsis-dap.cfg -f ./gd32f4xx.cfg \
  -c init -c targets -c "flash probe 0" -c "flash info 0" -c shutdown

# 确认晶振、构建及备份后才能执行下载
openocd -f ./cmsis-dap.cfg -f ./gd32f4xx.cfg \
  -c "program build/cubemx_gd32.elf verify reset exit"
```

`openocd_gdlink.cfg` 已包含 CMSIS-DAP 下载器配置，上述下载命令可写为：

```bash
openocd -f ./openocd_gdlink.cfg \
  -c "program build/cubemx_gd32.elf verify reset exit"
```

本机 Homebrew OpenOCD 0.12.0 解析 `gd32f4xx.cfg` 时实际报错：

```text
Error: flash driver 'gd32f4xx' not found
```

这是当前 OpenOCD 未包含该驱动，不是接线错误。
应安装 / 选择支持该 GD32 驱动的工具版本，或使用经过实机验证的替代方案。
不要靠修改芯片 ID 来绕过。

### 6.2 仓库保留的历史兼容配置

```bash
# 连接探测，不下载
openocd -f ./cmsis-dap.cfg -f ./stm32f4xx.cfg \
  -c init -c targets -c "flash probe 0" -c "flash info 0" -c shutdown
```

这条路线使用 `stm32f2x` 驱动。只有在具体 GD32 板上确认容量、扇区布局和擦写均正确后，
才可继续使用下列历史下载命令；不能仅凭配置能解析就认为可用，尤其不要假设它正确支持全部 3MB Flash。

```bash
openocd -f ./cmsis-dap.cfg -f ./stm32f4xx.cfg \
  -c "program build/cubemx_gd32.elf verify reset exit"
```

### 6.3 pyOCD 备选路线

先通过 `pyocd list` 确认下载器可见，再安装 / 确认对应 CMSIS Device Family Pack。
目标名称以 `pyocd list --targets` 的实际输出为准，不能只照抄芯片丝印。

```bash
pyocd list
pyocd list --targets | grep -i gd32
```

只有输出确实包含 `GD32F470VI` 时，才使用仓库原有命令中的该目标名：

```bash
pyocd flash --erase sector --target GD32F470VI build/cubemx_gd32.elf
```

优先按扇区擦除，不默认使用 `--erase chip`。
本机先前 `pyocd list` 未发现下载器，但 OpenOCD 可识别当前 ST-LINK；
因此 STM32 当前采用 OpenOCD，不把 pyOCD 未发现探针直接解释成板子未连接。

## 7. 下载后的硬件验收

1. 保存 OpenOCD 日志，确认写入、校验和复位各阶段均成功。
2. 检查程序能进入主循环，SysTick 正常递增，且没有持续 HardFault 或看门狗重启。
3. 串口桥验收：USART1 PA9/TX、PA10/RX；USART3 PD8/TX、PD9/RX；两端均为 **115200、8N1、无流控**。
4. USART3 对应 RS485，PD10 控制收发方向。TTL 串口应接 MCU 侧，RS485 A/B 应接收发器侧，电气接口不要混接。
5. 分别向两端注入可识别数据，检查对端字节完全一致、无重复、无丢失，并检查 PD10 发送后能恢复接收态。
6. ADC、TIM 输入捕获、GPIO 中断需在实际板上验证；记录板卡版本、输入源、频率 / 幅值、预期与实际输出。
   未实际接线测试的项目必须标为未验证。

## 8. 本次操作记录（2026-10-23）

### 8.1 前期连接与配置检查

- STM32 + ST-LINK + 仓库 `stm32f4x.cfg`：连接及 Flash 探测通过；此阶段未擦写、未复位。
- 在确认晶振前曾完成 12MHz 配置的干净构建：text=159116、data=1156、bss=60624 字节；
  `basic_test.c` 有两个既有的未使用静态函数告警，不是无告警构建。
- 随后用户确认当前板晶振为 8MHz，并要求先不编译：已执行 `python3 Scripts/switch_hse_freq.py 8`，
  核对仅上述三份配置文件中的四项值发生变化，切换后未再次编译。
- 当时 `build/` 仍是旧的 12MHz 产物，没有直接下载到 8MHz 板；之后按下节重新构建。
- 首次完整 Flash 备份请求曾因 USB 访问审批服务失败而未执行；后续重试成功，见下节。
- GD32 原生配置：本机解析失败，缺少 `gd32f4xx` Flash 驱动；历史兼容配置仅解析通过，无 GD32 实机验证。

### 8.2 用户要求继续后的实际烧录

- 再次确认 HSE=8MHz、PLLM=4，并执行 `make clean && make -j4`：构建成功。
  text=159164、data=1156、bss=60624 字节；仍有上述两个既有告警。
- 下载器：ST-LINK V2J45M31，序列号 `004100633000000A5630434B`。
- 使用仓库 `stlink.cfg + stm32f4x.cfg`；探测到设备 ID `0x20036419`、2MB 双 Bank Flash。
- 烧录前成功读取完整主 Flash：
  `logs/flashing/stm32-before-8mhz-20261023-134626.bin`，长度 2097152 字节。
  SHA-256：`7be87412aeb46ebb392ddbabc37947922a239f28d42c8809b97e2f6f08f6cd3e`。
- 下载映像：`build/cubemx_gd32.elf`；ELF SHA-256：
  `bd8c5847e49bd8dad2c487980c1117fc5ef33e13a45ad229e09b11ea206bd674`。
  对应 BIN 长度 160324 字节，SHA-256：
  `8437b64bb44091bf2905f9cb459d143ec6bdb4f43a04e911b9cfdaf0214b5f58`。
- 执行 `program build/cubemx_gd32.elf verify reset exit`，实际输出：

  ```text
  ** Programming Finished **
  ** Verified OK **
  ** Resetting Target **
  ```

- 复位后只读检查：CPU 为 running；`SystemCoreClock=64000000`；
  SysTick 三次采样为 `0x7934`、`0x7D1E`、`0x8108`，两次间隔各约 1000ms，计数持续递增。
- 串口桥 `initialized=1`；两个方向的接收重启错误、队列丢弃、发送错误计数均为 0；
  USART1 / USART3 的 HAL ErrorCode 均为 0；CFSR / HFSR 均为 0。
  这些检查不等同于串口双向数据或所有中断已经通过硬件验收。
- 本次未注入串口测试数据，未完成 ADC / TIM 捕获 / GPIO 的硬件验收；GD32 仍未实机烧录。
- ST-LINK 在备份及烧录期间仍报告约 4.37～4.39V，尚无万用表实测值，不将其记录为已确认的芯片 VDD。

日志和备份位于被 Git 忽略的 `logs/flashing/`，请另行保留：

| 文件 | 内容 |
| --- | --- |
| `build-stm32-8mhz-20261023.log` | 8MHz 配置构建日志 |
| `backup-stm32-8mhz-20261023.log` | 原固件备份日志 |
| `program-stm32-8mhz-20261023.log` | 下载、校验、复位日志 |
| `runtime-stm32-8mhz-20261023.log` | 复位后只读运行检查 |
| `stm32-8mhz-20261023.json` | 固件与备份路径、长度和校验值 |

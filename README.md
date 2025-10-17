# GD32F4xx开源开发环境

一个基于GD32F4xx系列微控制器的开源开发环境，支持自动化Makefile生成和编译。

## 项目概述

本项目为GD32F470VIT微控制器提供了完整的开发环境，包括：

- 自动化的Makefile生成系统
- 完整的GD32F4xx标准外设库
- CMSIS库支持
- 开源工具链支持（ARM GNU Toolchain）
- OpenOCD调试支持

本分支是用stm32-vscode的开发流程来开发gd32, 具体方法如下：

## 移植具体操作


### 1. 寻找对应芯片

对于gd32，基本上是对stm32的一比一复刻，理解了上述知识后只需要一点点工作就可以了。

比如对于GD32F470VIT6，对照他们的外设资源，就可以确定最接近的就是STM32F429VIT6，具体字母代表什么，比如接口个数、封装等信息我就不展开说了，随便查一查就知道了。


### 2. 初始化代码/库文件

#### 2.1 STM32CubeMX初始化

这个就当作是在开发stm32就可以，初始化时钟、外设、中断等，然后生成MAKEFILE项目文件。

#### 2.2 下载gd32芯片相关文件

`GigaDevice.GD32F4xx_DFP.3.0.3.pack/Device/F4XX`中Source和Include文件夹中就有上文提到的那几个文件。

### 3. 核对定义

用vscode或者任何文本编辑器对比也可以，用下面指令也可以，我建议前者。

查看向量表位置与内容：
  - arm-none-eabi-objdump -h your.elf | grep -E "vectors|isr"
  - arm-none-eabi-objdump -D your.elf | sed -n '1,120p'  # 检查起始几项是否为 _estack/Reset_Handler
  
确认符号是否导出：
  - arm-none-eabi-nm your.elf | grep -E "_sidata|_sdata|_edata|_sbss|_ebss|_estack|__gVectors|g_pfnVectors"

运行期核对时钟：
  - 在 main 里调用 SystemCoreClockUpdate 后打印/断点检查 SystemCoreClock

核对芯片手册尤其是外设接口、寄存器定义是否一致。（不一致需要修改对应的库文件）

### 4. 修改中断向量表

也就是在`startup_gd32f4xx.s`中定义的中断向量表和中断向量处理函数名，要和stm32中一致，比如在gd32中DMA中断处理函数是这样命名的：

```
                    .word DMA0_Channel0_IRQHandler
                    .word DMA0_Channel1_IRQHandler
                    .word DMA0_Channel2_IRQHandler
                    .word DMA0_Channel3_IRQHandler
```

而在stm32中DMA中断处理函数是这样命名的：

```
  .word     DMA1_Stream0_IRQHandler
  .word     DMA1_Stream1_IRQHandler
  .word     DMA1_Stream2_IRQHandler
  .word     DMA1_Stream3_IRQHandler
```

绝大多数只是名称不一样，不一样的替换就可以，但要注意有些是没有的比如stm的SAI：

```
  .word     SAI1_IRQHandler     /* SAI1
```

gd32中没有定义，这个就不要改：

```
                    .word 0     /* Vector Number 103,Reserved */
```

### 5. 修改MAKEFILE

#### ASM项
```makefile
# ASM sources
ASM_SOURCES = \
# startup_stm32f429xx.s

# ASM sources
ASMM_SOURCES = \
startup_gd32f450_470.S
```
#### LD项
```makefile
# LDSCRIPT = STM32F429XX_FLASH.ld
LDSCRIPT = gd32f4xx_flash.ld
```

别的都不用改，就可以像make + arm-gcc + openocd/pyocd 一样开发了。

#### openocd 
对于openocd最好是用“假的”`stm32f4xx.cfg`，否则可能有兼容性问题，当然如果用`GD32EmbeddedBuilder`中的openocd也可以，但只有windows版。

### 6. 总结

总之对于GD32F470VIT6只需要三步：

1. 修改`startup_gd32f4xx.s`文件中的中断处理函数；
2. 修改makefile中的ASM项和LD项。
3. openocd可能要改一下`stm.ctg`的文件。

## 项目结构

```bash
this_project/
├── Core/                         # 核心应用代码
│   ├── inc/                      # 头文件目录
│   │   ├── adc.h
│   │   ├── basic_driver.h
│   │   ├── basic_test.h
│   │   ├── crc.h
│   │   ├── dma.h
│   │   ├── gpio.h
│   │   ├── main.h
│   │   ├── stm32f4xx_hal_conf.h
│   │   ├── stm32f4xx_it.h
│   │   ├── systick.h
│   │   ├── tim.h
│   │   └── usart.h
│   └── src/                      # 源文件目录
│       ├── adc.c
│       ├── basic_driver.c
│       ├── basic_test.c
│       ├── crc.c
│       ├── dma.c
│       ├── gpio.c
│       ├── main.c
│       ├── stm32f4xx_hal_msp.c
│       ├── stm32f4xx_it.c
│       ├── syscalls.c
│       ├── sysmem.c
│       ├── system_stm32f4xx.c
│       ├── tim.c
│       └── usart.c
├── Drivers/                       # 驱动库
│   ├── CMSIS/                     # ARM CMSIS标准
│   └── STM32F4xx_HAL_Driver/      # stm32库
│       ├── Inc/                   # 外设库头文件
│       └── Src/                   # 外设库源文件
├── scripts/                       # 构建脚本
│   ├── analyze_c_project.py       # 分析c项目的脚本
│   ├── create_makefile.py         # Makefile自动生成脚本
│   └── convert_to_utf8.sh         # utf8转码sh脚本
├── gd32f4xx_flash.ld              # 链接脚本 (GD32F470VIT)
├── startup_gd32f450_470.S         # 启动文件
├── Makefile                       # 生成的Makefile
├── *.cfg                          # OpenOCD配置文件
└── README.md                      # 项目说明文档
```

## 环境要求

### 必需工具

- **ARM GNU Toolchain**: arm-none-eabi-gcc工具链
- **Make**: GNU Make构建工具
- **Python 3**: 用于运行Makefile生成脚本
- **OpenOCD** (可选pyocd): 用于调试和烧录

#### 注：

1. windows 用软件GD32EmbeddedBuilder的openocd

```bash
\\GD32EmbeddedBuilder_***\\Tools\\OpenOCD\\
```

2. mac 和 linux 建议用pyocd : `pip install pyocd`;也可以用openocd,但需要一些trick: 改名字（gd32f4xx.ctg改为stm32f4xx.ctg）里面_CHIPNAME和_TARGETNAME改了就可以了:

```bash
openocd -f cmsis-dap.cfg -f stm32f4xx.cfg -c init -c "reset halt" -c "wait_halt" -c "flash write_image erase build/gd32f4xx_project.elf" -c reset -c shutdown
```

具体依赖安装步骤参见我的笔记：

```text
https://github.com/waaall/EmbeddedTech/blob/main/stm32-develop.md
```


## 开始

### 1. 克隆项目

```bash
git clone <your-repo-url>
ls
cd ***
```

### 2. 生成Makefile

```bash
# 运行自动生成脚本
python3 scripts/create_makefile.py
```

### 3. 编译项目

```bash
# 编译项目
make

# 清理编译输出
make clean
```

### 4. 编译输出

编译成功后，在 `build/`目录下会生成：

- `gd32f4xx_project.elf` - ELF可执行文件
- `gd32f4xx_project.hex` - Intel HEX格式文件
- `gd32f4xx_project.bin` - 二进制文件
- `gd32f4xx_project.map` - 内存映射文件

## 自动化构建系统

### Makefile生成流程

本项目使用Python脚本自动生成Makefile，流程如下：

1. **扫描项目目录**: 递归扫描所有子目录
2. **收集源文件**: 自动发现所有 `.c`源文件
3. **收集汇编文件**: 自动发现 `.s`和 `.S`汇编文件
4. **收集头文件目录**: 自动发现所有包含 `.h`文件的目录
5. **收集链接脚本**: 自动发现 `.ld`链接脚本
6. **生成Makefile**: 基于模板替换生成最终Makefile

### 关键脚本说明

#### `scripts/create_makefile.py`

- **功能**: 自动扫描项目并生成Makefile
- **输入**: `scripts/template.makefile` (模板)
- **输出**: `./Makefile` (最终Makefile)
- **使用**: `python3 scripts/create_makefile.py`

#### `scripts/template.makefile`

- **功能**: Makefile模板文件，包含占位符
- **占位符**:
  - `@@C_SOURCES@@` - C源文件列表
  - `@@ASM_SOURCES@@` - .s汇编文件列表
  - `@@ASMM_SOURCES@@` - .S汇编文件列表
  - `@@C_INCLUDES@@` - 头文件目录列表
  - `@@LDSCRIPT@@` - 链接脚本路径

## 内存配置

### GD32F470VIT内存映射

```
Flash:   0x08000000 - 3072KB (3MB)
SRAM:    0x20000000 - 512KB  
TCM RAM: 0x10000000 - 64KB
```

### 内存使用情况

编译后会显示内存使用统计：

```
   text    data     bss     dec     hex filename
   5932     108    3444    9484    250c build/gd32f4xx_project.elf
```

- **text**: 程序代码大小
- **data**: 初始化数据大小
- **bss**: 未初始化数据大小

## 调试和烧录

### 使用pyocd

```bash
pyocd pack update
pyocd pack find gd32f4
pyocd pack install GD32F470VI
pyocd flash --erase chip --target GD32F470VI build/gd32f4xx_project.elf
```

#### pyocd pack install location

- Linux: $HOME/.local/share/cmsis-pack-manager
- Mac: $HOME/Library/Application Support/cmsis-pack-manager
- Windows: 'C:\\Users\\$USER\\AppData\\cmsis-pack-manager'


### 使用OpenOCD

```bash
openocd -f cmsis-dap.cfg -f gd32f4xx.cfg -c init -c "reset halt" -c "wait_halt" -c "flash write_image erase build/gd32f4xx_project.elf" -c reset -c shutdown
```

### 配置文件说明

- `cmsis-dap.cfg`: CMSIS-DAP调试器配置
- `gd32f4xx.cfg`: GD32F4xx目标配置
- `openocd_gdlink.cfg`: GDLink调试器配置

## 开发流程

### 添加新文件

1. 将 `.c`文件放入适当目录
2. 将 `.h`文件放入对应的 `inc`目录
3. 重新运行 `python3 scripts/create_makefile.py`
4. 运行 `make`编译

### 修改代码后更新

```bash
# 重新生成Makefile (如果添加了新文件)
python3 scripts/create_makefile.py

# 编译
make

# 或者使用VS Code任务
# Ctrl+Shift+P -> Tasks: Run Task -> build
```

### 清理和重新编译

```bash
# 清理编译输出
make clean

# 重新编译
make
```

## VS Code集成

项目包含VS Code任务配置：

### 可用任务

- **build**: 编译项目 (`make`)
- **download**: 烧录到目标板 (使用OpenOCD)

### 使用方法

1. 打开VS Code
2. `Ctrl+Shift+P` -> `Tasks: Run Task`
3. 选择相应任务

## 常见问题

### 烧录问题

#### pyocd xml error

问题：pyocd报错xml，因为svd文件头多了两个空格
```bash
➜  3.0.3-new ls
3.0.3.pack
➜  3.0.3-new mv 3.0.3.pack 3.0.3.zip  
➜  3.0.3-new ls
3.0.3  3.0.3.zip
➜  3.0.3-new cd 3.0.3    
➜  3.0.3 ls
Device  Flash  GigaDevice.GD32F4xx_DFP.pdsc  SVD
➜  3.0.3 cd SVD    
➜  SVD ls
GD32F403.SFR  GD32F403.svd  GD32F4xx.SFR  GD32F4xx.svd
➜  SVD sudo vim GD32F4xx.svd

# vim进入insert模式删了开头两个空格后 Esc :wq! 保存，再压缩成zip，重命名.pack
```


### 编译错误

1. **工具链未找到**: 确保ARM工具链在PATH中
2. **Python脚本错误**: 确保使用Python 3
3. **链接错误**: 检查链接脚本内存配置

### 内存不足

- 检查 `.map`文件了解内存使用情况
- 优化代码或调整链接脚本

### 调试问题

- 确保OpenOCD正确配置
- 检查调试器连接
- 验证目标板电源

## 许可证

基于GD32F4xx官方示例代码修改，遵循GD32相关许可协议。

## 贡献

欢迎提交Issue和Pull Request来改进这个项目！

---

**注意**: 本项目基于GD32F470VIT开发板配置，如使用其他GD32F4xx型号，请相应调整内存配置和启动文件。

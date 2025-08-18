# GD32F4xx开源开发环境

一个基于GD32F4xx系列微控制器的开源开发环境，支持自动化Makefile生成和编译。

## 项目概述

本项目为GD32F470VIT微控制器提供了完整的开发环境，包括：

- 自动化的Makefile生成系统
- 完整的GD32F4xx标准外设库
- CMSIS库支持
- 开源工具链支持（ARM GNU Toolchain）
- OpenOCD调试支持

## 项目结构

```
try-gd32-mac/
├── Core/                           # 核心应用代码
│   ├── inc/                        # 头文件目录
│   │   ├── main.h
│   │   ├── gd32f4xx_it.h
│   │   ├── gd32f4xx_libopt.h
│   │   ├── gd32f450i_eval.h
│   │   └── systick.h
│   └── src/                        # 源文件目录
│       ├── main.c                  # 主程序入口
│       ├── gd32f4xx_it.c          # 中断处理函数
│       ├── gd32f450i_eval.c       # 评估板相关代码
│       └── systick.c               # 系统滴答定时器
├── Drivers/                        # 驱动库
│   ├── CMSIS/                      # ARM CMSIS标准
│   │   ├── core_cm4.h             # Cortex-M4核心头文件
│   │   └── GD/GD32F4xx/           # GD32F4xx特定文件
│   │       ├── Include/           # 头文件
│   │       └── Source/            # 源文件
│   └── GD32F4xx_standard_peripheral/  # GD32标准外设库
│       ├── Include/               # 外设库头文件
│       └── Source/                # 外设库源文件
├── scripts/                        # 构建脚本
│   ├── create_makefile.py         # Makefile自动生成脚本
│   └── template.makefile          # Makefile模板
├── gd32f4xx_flash.ld              # 链接脚本 (GD32F470VIT)
├── startup_gd32f450_470.S         # 启动文件
├── Makefile                        # 生成的Makefile
├── *.cfg                          # OpenOCD配置文件
└── README.md                       # 项目说明文档
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

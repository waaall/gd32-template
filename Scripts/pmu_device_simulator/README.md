# PMU 下位机协议模拟器

`Scripts/pmu_device_simulator` 用于在没有真实下位机时，模拟 PMU 固件侧的上位机通信协议。它会读取离线 CSV 中的频率和功率数据，按照下位机当前 `Modules/host_com` 的二进制协议，对外表现成一台会响应指令、支持参数读写、可按流控周期发送遥测数据的设备。

这个工具适合用来测试上位机软件、通信解析逻辑、参数配置界面和一次调频数据展示流程。它不是旧版 `BEGIN:{freq},{power}END` 文本发送器，而是协议级模拟器。

## 使用方法

从仓库根目录运行：

```sh
python Scripts/pmu_device_simulator/main.py
```

也可以用模块方式运行：

```sh
python -m Scripts.pmu_device_simulator
```

运行参数不通过命令行传入，统一在 [config.json](config.json) 中配置。JSON 内的相对路径都相对于 `Scripts/pmu_device_simulator` 目录解析。

常用配置项：

```json
{
  "serial": {
    "port": "/dev/tty.usbserial-pmu",
    "dry_run": true,
    "baudrate": 115200
  },
  "csv": {
    "path": "../../Tests/output-test-data/2020.05.07.00.00.00-2020.05.07.12.00.00.csv",
    "power_unit": "MW",
    "source_interval_sec": 1.0,
    "eof_behavior": "stop"
  },
  "stream": {
    "auto_start": false,
    "default_period_ms": 500
  },
  "logging": {
    "log_level": "INFO",
    "log_file": "",
    "log_hex_frames": false
  }
}
```

`serial.dry_run=true` 时不打开串口，发送帧会以十六进制打印到标准输出，便于先检查协议内容。接真实串口测试时，把 `dry_run` 改为 `false`，并把 `serial.port` 改成实际串口设备名。

默认 `stream.auto_start=false`，模拟器启动后不会主动发送遥测，必须等上位机发送 `STREAM_CTRL_REQ START`。如果需要单独观察 CSV 回放输出，可以改成 `true`，启动后会直接按 `stream.default_period_ms` 周期发送。

参数状态保存在 [params_state.json](params_state.json)。上位机发送 `SET_PARAM_REQ` 且模拟器判断成功后，会更新这个 JSON 文件并递增 `config_version`。

## 设计思路

模拟器的目标是尽量复刻真实下位机的通信边界，而不是只把 CSV 行格式化后写串口。因此它做了三件事：

1. 协议层完全使用真实固件的帧结构：`A5 5A` 帧头、协议版本、消息类型、序号、payload 长度和 CRC16-CCITT-FALSE。
2. 控制面支持真实指令：参数 GET/SET、遥测流 START/STOP/SET_PERIOD/GET_STATE、状态查询和错误响应。
3. 数据面把离线 CSV 伪装成实时遥测：按通信周期发送 `PF_BASIC` telemetry，让上位机看到的就是下位机当前协议版本的数据流。

离线数据和通信周期是分开的。`csv.source_interval_sec` 表示 CSV 原始数据周期，`stream.default_period_ms` 或上位机 `STREAM_CTRL_REQ` 中的周期表示通信发送周期。例如 CSV 1 秒一行、通信周期 500 ms 时，同一行 CSV 会连续发送两次；通信周期 200 ms 时，同一行会发送五次。

`csv.power_unit` 表示 CSV 中 `Power` 列的单位，支持 `W`、`kW`、`MW`（大小写不敏感）。模拟器会在读取 CSV 时转换为 W，再写入协议字段 `active_power_w_i32`。当前示例 CSV 的 `Power=185.028` 表示 `185.028 MW`，因此默认配置写为 `"power_unit": "MW"`。

CSV 播放结束后的行为由 `csv.eof_behavior` 决定：

- `stop`：停止遥测流。
- `hold`：持续发送最后一行。
- `loop`：回到第一行循环播放。

## 代码框架

主要模块如下：

| 文件 | 职责 |
| --- | --- |
| [main.py](main.py) | 程序入口，读取配置、初始化日志、CSV、参数表、串口和模拟器。 |
| [config_model.py](config_model.py) | 解析 `config.json`，把 JSON 转换成结构化配置对象。 |
| [logging_setup.py](logging_setup.py) | 配置控制台日志和可选文件日志。 |
| [protocol.py](protocol.py) | 协议常量、帧编码、帧解析、CRC16 和小端读写工具。 |
| [serial_endpoint.py](serial_endpoint.py) | 串口读写封装，以及 `dry_run` 十六进制输出。 |
| [csv_source.py](csv_source.py) | 读取 CSV，按模拟时间轴返回当前应发送的频率/功率样本。 |
| [telemetry.py](telemetry.py) | 构造 `PF_BASIC` 27 字节遥测 payload。 |
| [params.py](params.py) | 模拟参数对象表，处理 GET/SET、类型校验、范围约束和 JSON 写回。 |
| [simulator.py](simulator.py) | 主状态机，处理 RX 指令、生成响应、调度遥测发送。 |

配置文件和状态文件：

| 文件 | 用途 |
| --- | --- |
| [config.json](config.json) | 运行配置：串口、CSV、流控、参数文件路径、日志设置。 |
| [params_state.json](params_state.json) | 模拟下位机运行时参数状态，会被成功的 `SET_PARAM_REQ` 修改。 |

## 主流程

启动流程：

1. `main.py` 固定读取同目录 `config.json`。
2. 初始化日志，日志等级来自 `logging.log_level`。
3. `CsvPlaybackSource` 加载 CSV，并解析 `Timestamp`、`Power`、`Freq` 列。
4. `ParamStore` 加载或创建 `params_state.json`。
5. `SerialEndpoint` 根据配置进入真实串口模式或 `dry_run` 模式。
6. `PmuDeviceSimulator` 进入循环，开始接收上位机帧并按需发送响应和遥测。

运行循环：

1. 从串口读取已有字节。
2. 逐字节喂给 `FrameParser`。
3. 解析出完整帧后，根据 `type` 分发到对应处理函数。
4. 高优先级控制指令立即生成响应帧。
5. 如果遥测流已启动，并且到达下一次发送时间，构造并发送 `PF_BASIC` telemetry。
6. 循环等待下一轮串口输入或发送周期。

流控流程：

1. 上位机发送 `STREAM_CTRL_REQ START`。
2. 模拟器返回 `STREAM_CTRL_RESP(status=OK, stream_enabled=1, period_ms=当前周期)`。
3. 模拟器立即安排第一帧 telemetry，随后按周期发送。
4. 上位机发送 `STOP` 后，模拟器停止发送 telemetry，但仍继续响应参数和状态指令。

## 关键实现细节

### 帧格式

所有收发都使用真实固件协议：

```text
+------+---------+------+-------+-----+--------+---------+-------+
| SOF  | version | type | flags | seq | length | payload | crc16 |
+------+---------+------+-------+-----+--------+---------+-------+
| 2B   | 1B      | 1B   | 1B    | 2B  | 2B     | N B     | 2B    |
+------+---------+------+-------+-----+--------+---------+-------+
```

- `SOF` 固定为 `A5 5A`。
- `version` 固定为 `0x01`。
- `seq`、`length`、`crc16` 都是 little-endian。
- CRC 使用 CRC16-CCITT-FALSE，计算范围是从 `version` 到 payload 末尾。
- CRC 错误帧只计数，不响应，贴近固件行为。

### 支持的指令

当前支持：

| type | 名称 | 行为 |
| --- | --- | --- |
| `0x01` | `GET_PARAM_REQ` | 读取参数对象，返回 `GET_PARAM_RESP`。 |
| `0x03` | `SET_PARAM_REQ` | 修改参数对象，成功后写回 JSON。 |
| `0x20` | `STREAM_CTRL_REQ` | 获取流状态、启动、停止、修改周期。 |
| `0x30` | `STATUS_REQ` | 返回当前状态、错误计数和 `config_version`。 |

未知消息类型会返回 `ERROR(status=UNSUPPORTED)`。长度错误返回 `BAD_LENGTH`。

### 参数模拟

参数对象按真实固件的 `host_param_table` 实现：

| object_id | 参数 | 类型 |
| --- | --- | --- |
| `4097` | 毛刺抑制时间 `fake_period_ms` | `float32_le` |
| `4098` | 滑动平均窗口 `window_periods` | `u8` |
| `4099` | 最小有效频率 `min_freq_hz` | `float32_le` |
| `4100` | 最大有效频率 `max_freq_hz` | `float32_le` |
| `4101` | 失活超时 `inactivity_timeout_ms` | `u32_le` |
| `8193..8207` | 5 路 4-20mA 校准参数 | `i16_le` 或 `u16_le` |

4-20mA 校准对象按每通道 3 个参数排列：

- `8193 + ch * 3 + 0`：`zero_offset`，`i16_le`
- `8193 + ch * 3 + 1`：`span_offset`，`i16_le`
- `8193 + ch * 3 + 2`：`gain_10k`，`u16_le`

模拟器会做和固件一致的主要约束：

- `fake_period_ms` 范围为 `0..10`。
- `window_periods` 范围为 `1..30`。
- `min_freq_hz` 必须小于 `max_freq_hz`。
- `inactivity_timeout_ms` 范围为 `50..60000`。
- `zero_offset/span_offset` 限幅到 `-1310..1310`。
- `gain_10k` 限幅到 `5000..15000`。

成功 SET 后：

1. 更新内存状态。
2. `config_version++`。
3. 写回 `params_state.json`。
4. 下一帧 telemetry 置 `CONFIG_CHANGED` 标志。

### 遥测数据映射

CSV 必须至少包含三列：

```csv
Timestamp,Power,Freq
2020-05-07 00:00:00,185.028,50.0
```

映射到 `PF_BASIC`：

| CSV 字段 | 遥测字段 | 规则 |
| --- | --- | --- |
| `Freq` | `freq_a_millihz` | `round(freq_hz * 1000)` |
| `Freq` | `freq_b_millihz` | 与 A 相相同 |
| `Freq` | `freq_selected_millihz` | 与 A/B 相同 |
| `Power` | `active_power_w_i32` | 按 `csv.power_unit` 转换成 W 后，四舍五入到整数 W |

默认置位的质量标志包括：

- `FREQ_A_VALID`
- `FREQ_B_VALID`
- `FREQ_SELECTED_VALID`
- `POWER_VALID`
- `ADC_VALID`
- `MA_OUTPUT_READY`，由 `stream.ma_output_ready` 控制
- `CONFIG_CHANGED`，仅在成功 SET 后的下一帧置位

### 日志

日志由 `logging` 配置段控制：

- `log_level`：控制日志等级，例如 `DEBUG`、`INFO`、`WARNING`。
- `log_file`：为空时只输出到控制台；非空时同时写文件。
- `log_hex_frames`：为 `true` 时记录 RX/TX 原始十六进制帧，建议配合 `log_level=DEBUG` 使用。

### 串口与 dry-run

真实串口模式需要安装 `pyserial`。`dry_run` 模式不依赖串口，会把发送帧打印为十六进制，适合做协议编码检查，但不会接收上位机输入。

如果要测试完整上位机交互，建议使用一对虚拟串口或 USB 串口交叉连接：上位机连接一端，模拟器连接另一端。

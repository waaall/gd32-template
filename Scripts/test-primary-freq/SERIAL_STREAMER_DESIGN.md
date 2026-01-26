# PMU 串口发送器设计文档

## 目标
从 `convert_pmu_csv.py` 生成的 CSV 中读取 `Power` 和 `Freq`，按固定周期（默认 1 秒）格式化为帧并发送到 STM32 串口。

默认帧格式：

```
BEGIN:{freq:.3f},{power:.3f}END\n
```

## 适用数据格式
CSV 头部包含三列：

- `Timestamp`
- `Power`
- `Freq`

示例：

```
Timestamp,Power,Freq
2020-05-07 00:00:00,185.028,50.0
2020-05-07 00:00:01,185.119,50.0
```

## 架构与模块

### 1) CsvSource
负责读取 CSV、解析列、输出 `RowData`。

- 通过列名或列序号定位 `Power/Freq/Timestamp`
- 支持 `start_row` 跳过前 N 行
- 支持 `max_rows` 限制总发送条数
- 无效行（空行或非数值）会被跳过

### 2) FrameFormatter
负责将 `RowData` 格式化为帧字符串。

- 默认使用 `BEGIN:{freq:.3f},{power:.3f}END\n`
- 支持 `frame_template` 自定义
- 模板可用占位符：`{freq}` `{power}` `{timestamp}`

### 3) SerialWriter
负责串口写入或干运行输出。

- `dry_run=true` 时写到 stdout
- 未安装 pyserial 时提示使用 dry-run 或安装依赖

### 4) PmuSerialStreamer
控制发送节奏与循环逻辑。

- `interval_sec` 为发送间隔
- `loop=true` 时 CSV 发送完成后重新从头开始
- 使用 `time.monotonic()` 降低漂移

## 配置参数（serial_streamer_config.json）

- CSV
  - `csv_path`: CSV 文件路径
  - `csv_encoding`: CSV 编码
  - `power_col`, `freq_col`, `timestamp_col`: 列名或列序号
  - `col_base`: 列序号基准（0 或 1）
  - `start_row`: 跳过前 N 行数据
  - `max_rows`: 最大发送条数（0 表示不限制）

- 串口
  - `serial_port`: 串口设备名，例如 `/dev/ttyUSB0`
  - `baudrate`, `bytesize`, `parity`, `stopbits`
  - `timeout`, `write_timeout`
  - `serial_encoding`

- 发送控制
  - `interval_sec`: 发送间隔（秒）
  - `loop`: 是否循环
  - `dry_run`: 不写串口，仅打印
  - `verbose`: 额外日志

- 帧格式
  - `power_precision`, `freq_precision`
  - `frame_template`

## 运行方式

### 1) 使用配置文件

```
python pmu_serial_streamer.py --config serial_streamer_config.json
```

### 2) 命令行覆盖配置

```
python pmu_serial_streamer.py \
  --csv output-test-data/example.csv \
  --port /dev/ttyUSB0 \
  --interval 1.0
```

### 3) 列出串口

```
python pmu_serial_streamer.py --list-ports
```

## 依赖

- Python 3.10+
- `pyserial`（真实写串口时需要）


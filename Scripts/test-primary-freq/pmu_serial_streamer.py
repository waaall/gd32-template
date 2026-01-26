#!/usr/bin/env python3
"""
PMU CSV 串口发送器（按固定周期发送 Power/Freq 到 STM32）。

用法:
  python pmu_serial_streamer.py --config serial_streamer_config.json
  python pmu_serial_streamer.py --csv output-test-data/example.csv --port /dev/ttyUSB0
  python pmu_serial_streamer.py --list-ports

设计要点:
  - CsvSource: 读取 CSV/解析列/筛选行
  - FrameFormatter: 按模板组帧（支持 {freq} {power} {timestamp}）
  - SerialWriter: 串口写入或 dry-run 输出
  - PmuSerialStreamer: 定时发送/可循环
"""
from __future__ import annotations

import argparse
import csv
import dataclasses
import json
import pathlib
import sys
import time
from typing import Iterator, Optional

try:
    import serial
    from serial.tools import list_ports
except ImportError:  # pragma: no cover - optional dependency
    serial = None
    list_ports = None


DEFAULT_FRAME_TEMPLATE = "BEGIN:{freq:.3f},{power:.3f}END\n"


@dataclasses.dataclass
class SerialConfig:
    port: str
    baudrate: int = 115200
    bytesize: int = 8
    parity: str = "N"
    stopbits: float = 1
    timeout: float = 1.0
    write_timeout: float = 1.0
    encoding: str = "utf-8"


@dataclasses.dataclass
class CsvConfig:
    path: str
    power_col: str = "Power"
    freq_col: str = "Freq"
    timestamp_col: str = "Timestamp"
    col_base: int = 0
    encoding: str = "utf-8"
    start_row: int = 0
    max_rows: Optional[int] = None


@dataclasses.dataclass
class StreamConfig:
    interval_sec: float = 1.0
    loop: bool = False
    dry_run: bool = False
    verbose: bool = False


@dataclasses.dataclass
class FormatConfig:
    frame_template: str = ""
    power_precision: int = 3
    freq_precision: int = 3

    def resolved_template(self) -> str:
        if self.frame_template:
            return self.frame_template
        return (
            f"BEGIN:{{freq:.{self.freq_precision}f}},"
            f"{{power:.{self.power_precision}f}}END\n"
        )


@dataclasses.dataclass
class RowData:
    timestamp: str
    power: float
    freq: float


class CsvSource:
    def __init__(self, config: CsvConfig, template: str) -> None:
        self._config = config
        self._template = template

    def iter_rows(self) -> Iterator[RowData]:
        path = pathlib.Path(self._config.path)
        if not path.exists():
            raise SystemExit(f"CSV not found: {path}")

        with path.open("r", encoding=self._config.encoding, newline="") as f:
            reader = csv.reader(f)
            header = next(reader, None)
            if not header:
                raise SystemExit("CSV file has no header")

            # 解析列索引（支持列名或列序号）
            header_norm = [normalize_header(h) for h in header]
            power_idx = resolve_col(header_norm, self._config.power_col, self._config.col_base)
            freq_idx = resolve_col(header_norm, self._config.freq_col, self._config.col_base)
            ts_idx = resolve_col(header_norm, self._config.timestamp_col, self._config.col_base)

            # 模板包含 timestamp 时必须提供对应列
            needs_ts = "{timestamp" in self._template
            if power_idx is None or power_idx < 0:
                raise SystemExit(f"Power column not found: {self._config.power_col}")
            if freq_idx is None or freq_idx < 0:
                raise SystemExit(f"Freq column not found: {self._config.freq_col}")
            if needs_ts and (ts_idx is None or ts_idx < 0):
                raise SystemExit(
                    f"Timestamp column not found: {self._config.timestamp_col}"
                )

            row_index = 0
            yielded = 0
            for row in reader:
                # 跳过空行或列数不足
                if not row or all(not str(c).strip() for c in row):
                    continue
                if max(power_idx, freq_idx, ts_idx or 0) >= len(row):
                    continue

                # 起始行与最大行数控制
                if row_index < self._config.start_row:
                    row_index += 1
                    continue
                if self._config.max_rows is not None and yielded >= self._config.max_rows:
                    break

                # 解析数值，失败则跳过
                power_val = parse_float(row[power_idx])
                freq_val = parse_float(row[freq_idx])
                if power_val is None or freq_val is None:
                    row_index += 1
                    continue

                timestamp = str(row[ts_idx]).strip() if ts_idx is not None else ""
                yield RowData(timestamp=timestamp, power=power_val, freq=freq_val)
                yielded += 1
                row_index += 1


class FrameFormatter:
    def __init__(self, config: FormatConfig) -> None:
        self._template = config.resolved_template()

    def format(self, row: RowData) -> str:
        return self._template.format(
            power=row.power,
            freq=row.freq,
            timestamp=row.timestamp,
        )

    @property
    def template(self) -> str:
        return self._template


class SerialWriter:
    def __init__(self, config: SerialConfig, dry_run: bool) -> None:
        self._config = config
        self._dry_run = dry_run
        self._serial = None

        # dry-run: 不打开串口，直接输出
        if self._dry_run:
            return
        if serial is None:
            raise SystemExit("pyserial is not installed. Use --dry-run or install pyserial.")

        self._serial = serial.Serial(
            port=self._config.port,
            baudrate=self._config.baudrate,
            bytesize=self._config.bytesize,
            parity=self._config.parity,
            stopbits=self._config.stopbits,
            timeout=self._config.timeout,
            write_timeout=self._config.write_timeout,
        )

    def write(self, frame: str) -> None:
        if self._dry_run:
            sys.stdout.write(frame)
            sys.stdout.flush()
            return
        if not self._serial:
            raise RuntimeError("Serial port not initialized")
        data = frame.encode(self._config.encoding)
        self._serial.write(data)

    def close(self) -> None:
        if self._serial:
            self._serial.close()


class PmuSerialStreamer:
    def __init__(
        self,
        csv_source: CsvSource,
        formatter: FrameFormatter,
        writer: SerialWriter,
        stream_config: StreamConfig,
    ) -> None:
        self._csv_source = csv_source
        self._formatter = formatter
        self._writer = writer
        self._stream_config = stream_config

    def run(self) -> int:
        # 使用单调时钟计算节拍，降低累计漂移
        interval = max(self._stream_config.interval_sec, 0.0)
        next_time = time.monotonic()

        try:
            while True:
                sent_any = False
                for row in self._csv_source.iter_rows():
                    now = time.monotonic()
                    if interval > 0 and now < next_time:
                        time.sleep(next_time - now)
                    frame = self._formatter.format(row)
                    self._writer.write(frame)
                    if self._stream_config.verbose:
                        sys.stderr.write(f"Sent: {frame}")
                    sent_any = True
                    next_time += interval

                # 非循环模式：本轮 CSV 发送完就退出
                if not self._stream_config.loop:
                    return 0 if sent_any else 2
        finally:
            self._writer.close()


def normalize_header(cell: str) -> str:
    # 统一表头空白与 BOM
    return " ".join(str(cell).strip().lstrip("\ufeff").split())


def resolve_col(header: list[str], name: str, col_base: int) -> Optional[int]:
    # 支持列名或列序号
    if name is None:
        return None
    spec = str(name).strip()
    if spec.isdigit():
        return int(spec) - col_base
    target = normalize_header(spec)
    if target in header:
        return header.index(target)
    lower_header = [h.lower() for h in header]
    if target.lower() in lower_header:
        return lower_header.index(target.lower())
    return None


def parse_float(value: str) -> Optional[float]:
    try:
        return float(str(value).strip())
    except ValueError:
        return None


def coerce_bool(value) -> bool:
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        return bool(value)
    return str(value).strip().lower() in ("1", "true", "yes", "y", "on")


def load_config(path: str) -> dict:
    # 读取 JSON 配置
    if not path:
        return {}
    config_path = pathlib.Path(path)
    if not config_path.exists():
        raise SystemExit(f"Config not found: {config_path}")
    with config_path.open("r", encoding="utf-8") as f:
        data = json.load(f)
    if not isinstance(data, dict):
        raise SystemExit("Config must be a JSON object")
    return data


def list_serial_ports() -> int:
    # 枚举串口设备
    if list_ports is None:
        print("pyserial is not installed.")
        return 1
    ports = list(list_ports.comports())
    if not ports:
        print("No serial ports found.")
        return 1
    for port in ports:
        print(f"{port.device} - {port.description}")
    return 0


def build_config(args: argparse.Namespace) -> tuple[SerialConfig, CsvConfig, StreamConfig, FormatConfig]:
    # 将命令行参数组装成配置对象
    serial_config = SerialConfig(
        port=args.port,
        baudrate=args.baudrate,
        bytesize=args.bytesize,
        parity=args.parity,
        stopbits=args.stopbits,
        timeout=args.timeout,
        write_timeout=args.write_timeout,
        encoding=args.serial_encoding,
    )
    csv_config = CsvConfig(
        path=args.csv,
        power_col=args.power_col,
        freq_col=args.freq_col,
        timestamp_col=args.timestamp_col,
        col_base=args.col_base,
        encoding=args.csv_encoding,
        start_row=args.start_row,
        max_rows=None if args.max_rows <= 0 else args.max_rows,
    )
    stream_config = StreamConfig(
        interval_sec=args.interval,
        loop=args.loop,
        dry_run=args.dry_run,
        verbose=args.verbose,
    )
    format_config = FormatConfig(
        frame_template=args.frame_template,
        power_precision=args.power_precision,
        freq_precision=args.freq_precision,
    )
    return serial_config, csv_config, stream_config, format_config


def parse_args() -> argparse.Namespace:
    # 先读取配置文件，再解析命令行覆盖
    pre = argparse.ArgumentParser(add_help=False)
    pre.add_argument("--config", default="", help="JSON config file")
    pre_args, remaining = pre.parse_known_args()

    config = load_config(pre_args.config) if pre_args.config else {}
    config = {k: v for k, v in config.items() if v is not None}

    parser = argparse.ArgumentParser(description="Stream PMU CSV to STM32 over serial")
    parser.add_argument("--config", default=pre_args.config, help="JSON config file")
    parser.add_argument("--list-ports", action="store_true", help="List serial ports")

    parser.add_argument("--csv", default=config.get("csv_path", ""), help="CSV path")
    parser.add_argument("--csv-encoding", default=config.get("csv_encoding", "utf-8"))
    parser.add_argument("--power-col", default=config.get("power_col", "Power"))
    parser.add_argument("--freq-col", default=config.get("freq_col", "Freq"))
    parser.add_argument("--timestamp-col", default=config.get("timestamp_col", "Timestamp"))
    parser.add_argument("--col-base", type=int, choices=[0, 1], default=config.get("col_base", 0))
    parser.add_argument("--start-row", type=int, default=config.get("start_row", 0))
    parser.add_argument("--max-rows", type=int, default=config.get("max_rows") or 0)

    parser.add_argument("--port", default=config.get("serial_port", ""), help="Serial port")
    parser.add_argument("--baudrate", type=int, default=config.get("baudrate", 115200))
    parser.add_argument("--bytesize", type=int, default=config.get("bytesize", 8))
    parser.add_argument("--parity", default=config.get("parity", "N"))
    parser.add_argument("--stopbits", type=float, default=config.get("stopbits", 1))
    parser.add_argument("--timeout", type=float, default=config.get("timeout", 1.0))
    parser.add_argument("--write-timeout", type=float, default=config.get("write_timeout", 1.0))
    parser.add_argument("--serial-encoding", default=config.get("serial_encoding", "utf-8"))

    parser.add_argument("--interval", type=float, default=config.get("interval_sec", 1.0))
    parser.add_argument(
        "--loop",
        action=argparse.BooleanOptionalAction,
        default=coerce_bool(config.get("loop", False)),
    )
    parser.add_argument(
        "--dry-run",
        action=argparse.BooleanOptionalAction,
        default=coerce_bool(config.get("dry_run", False)),
    )
    parser.add_argument(
        "--verbose",
        action=argparse.BooleanOptionalAction,
        default=coerce_bool(config.get("verbose", False)),
    )

    parser.add_argument(
        "--frame-template",
        default=config.get("frame_template", ""),
        help="Format string with {freq}, {power}, {timestamp}",
    )
    parser.add_argument("--power-precision", type=int, default=config.get("power_precision", 3))
    parser.add_argument("--freq-precision", type=int, default=config.get("freq_precision", 3))

    args = parser.parse_args(remaining)
    if args.list_ports:
        return args

    # 必要参数校验
    if not args.csv:
        parser.error("--csv is required (or provide csv_path in config)")
    if not args.port and not args.dry_run:
        parser.error("--port is required unless --dry-run is set")

    return args


def main() -> int:
    args = parse_args()
    if args.list_ports:
        return list_serial_ports()

    # 组装各模块并启动发送
    serial_config, csv_config, stream_config, format_config = build_config(args)
    formatter = FrameFormatter(format_config)
    csv_source = CsvSource(csv_config, formatter.template)
    writer = SerialWriter(serial_config, stream_config.dry_run)
    streamer = PmuSerialStreamer(csv_source, formatter, writer, stream_config)
    return streamer.run()


if __name__ == "__main__":
    raise SystemExit(main())

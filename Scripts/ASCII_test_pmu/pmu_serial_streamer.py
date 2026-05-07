#!/usr/bin/env python3
"""
PMU CSV 串口发送器（按固定周期发送 Power/Freq 到 STM32）。

用法:
  python pmu_serial_streamer.py --config serial_streamer_config.json
  python pmu_serial_streamer.py --csv ../../Tests/output-test-data/example.csv --port /dev/ttyUSB0
  python pmu_serial_streamer.py --start-time "2020-05-07-05:30" --dry-run  # 从指定时间开始
  python pmu_serial_streamer.py --list-ports

开始时间格式（--start-time）:
  2020-05-07-05:30    → 从 05:30:00 开始
  2020-05-07-05:30:10 → 从 05:30:10 开始
  2020-05-07-05       → 从 05:00:00 开始

设计要点:
  - CsvSource: 读取 CSV/解析列/筛选行/时间筛选
  - FrameFormatter: 按模板组帧（支持 {freq} {power} {timestamp}）
  - SerialWriter: 串口写入或 dry-run 输出
  - PmuSerialStreamer: 定时发送/可循环
"""
from __future__ import annotations

import argparse
import csv
import dataclasses
import json
import logging
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

# 配置日志格式
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
)
logger = logging.getLogger(__name__)

DEFAULT_FRAME_TEMPLATE = "BEGIN:{freq:.3f},{power:.3f}END\n"
SCRIPT_DIR = pathlib.Path(__file__).resolve().parent


def resolve_path(path: str) -> pathlib.Path:
    if not path:
        return pathlib.Path(path)
    p = pathlib.Path(path)
    if p.is_absolute():
        return p
    return SCRIPT_DIR / p


# ============================================================================
# 配置数据类
# ============================================================================

@dataclasses.dataclass
class SerialConfig:
    """串口配置参数"""
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
    """CSV文件配置参数"""
    path: str
    power_col: str = "Power"
    freq_col: str = "Freq"
    timestamp_col: str = "Timestamp"
    col_base: int = 0           # 列索引起始值(0或1)
    encoding: str = "utf-8"
    start_row: int = 0          # 跳过前N行数据
    max_rows: Optional[int] = None  # 最大读取行数
    start_time: Optional[str] = None  # 开始时间前缀


@dataclasses.dataclass
class StreamConfig:
    """数据流发送配置"""
    interval_sec: float = 1.0   # 发送间隔(秒)
    loop: bool = False          # 是否循环发送
    dry_run: bool = False       # 仅输出不实际发送
    verbose: bool = False       # 详细输出模式


@dataclasses.dataclass
class FormatConfig:
    """帧格式配置"""
    frame_template: str = ""
    power_precision: int = 3    # 功率小数位数
    freq_precision: int = 3     # 频率小数位数

    def resolved_template(self) -> str:
        """返回最终使用的帧模板"""
        if self.frame_template:
            return self.frame_template
        return (
            f"BEGIN:{{freq:.{self.freq_precision}f}},"
            f"{{power:.{self.power_precision}f}}END\n"
        )


@dataclasses.dataclass
class RowData:
    """单行CSV数据"""
    timestamp: str
    power: float
    freq: float


# ============================================================================
# CSV数据源类
# ============================================================================

class CsvSource:
    """CSV文件数据源，负责读取和解析CSV文件"""

    def __init__(self, config: CsvConfig, template: str) -> None:
        self._config = config
        self._template = template

    def iter_rows(self) -> Iterator[RowData]:
        """迭代返回CSV中的每一行数据"""
        path = pathlib.Path(self._config.path)
        if not path.exists():
            logger.error(f"CSV文件不存在: {path}")
            raise SystemExit(f"CSV not found: {path}")

        logger.info(f"打开CSV文件: {path}")

        with path.open("r", encoding=self._config.encoding, newline="") as f:
            reader = csv.reader(f)
            header = next(reader, None)
            if not header:
                logger.error("CSV文件无表头")
                raise SystemExit("CSV file has no header")

            # 解析列索引（支持列名或列序号）
            header_norm = [normalize_header(h) for h in header]
            power_idx = resolve_col(header_norm, self._config.power_col, self._config.col_base)
            freq_idx = resolve_col(header_norm, self._config.freq_col, self._config.col_base)
            ts_idx = resolve_col(header_norm, self._config.timestamp_col, self._config.col_base)

            logger.info(f"列索引 - Power: {power_idx}, Freq: {freq_idx}, Timestamp: {ts_idx}")

            # 模板包含 timestamp 时必须提供对应列
            needs_ts = "{timestamp" in self._template
            if power_idx is None or power_idx < 0:
                logger.error(f"未找到Power列: {self._config.power_col}")
                raise SystemExit(f"Power column not found: {self._config.power_col}")
            if freq_idx is None or freq_idx < 0:
                logger.error(f"未找到Freq列: {self._config.freq_col}")
                raise SystemExit(f"Freq column not found: {self._config.freq_col}")
            if needs_ts and (ts_idx is None or ts_idx < 0):
                logger.error(f"未找到Timestamp列: {self._config.timestamp_col}")
                raise SystemExit(
                    f"Timestamp column not found: {self._config.timestamp_col}"
                )

            # 开始时间筛选初始化
            start_time_prefix = None
            if self._config.start_time:
                start_time_prefix = _normalize_time_input(self._config.start_time)
                logger.info(f"指定开始时间前缀: {start_time_prefix}")

            found_start = (start_time_prefix is None)  # 无指定则默认已找到
            warned_approx = False
            last_timestamp = None  # 记录最后时间戳用于边界检查

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
                    logger.info(f"已达到最大行数限制: {self._config.max_rows}")
                    break

                # 获取当前行时间戳
                timestamp = str(row[ts_idx]).strip() if ts_idx is not None else ""
                last_timestamp = timestamp

                # 开始时间筛选逻辑
                if not found_start and start_time_prefix:
                    if timestamp.startswith(start_time_prefix):
                        # 精确前缀匹配
                        found_start = True
                        logger.info(f"找到开始时间: {timestamp}")
                    elif timestamp >= start_time_prefix:
                        # 近似匹配：实际时间 > 指定时间
                        found_start = True
                        if not warned_approx:
                            logger.warning(
                                f"未找到精确匹配 '{start_time_prefix}'，"
                                f"从最接近的时间开始: {timestamp}"
                            )
                            warned_approx = True
                    else:
                        row_index += 1
                        continue  # 跳过早于指定时间的行

                # 解析数值，失败则跳过
                power_val = parse_float(row[power_idx])
                freq_val = parse_float(row[freq_idx])
                if power_val is None or freq_val is None:
                    row_index += 1
                    continue

                yield RowData(timestamp=timestamp, power=power_val, freq=freq_val)
                yielded += 1
                row_index += 1

            # 循环结束后检查：若指定了start_time但从未找到匹配
            if start_time_prefix and not found_start:
                logger.error(
                    f"指定时间 '{start_time_prefix}' 超出CSV数据范围 "
                    f"(最后时间戳: {last_timestamp})"
                )
                raise SystemExit(
                    f"Start time '{start_time_prefix}' is beyond CSV data range"
                )

            logger.info(f"CSV读取完成，共处理 {yielded} 行数据")


# ============================================================================
# 帧格式化类
# ============================================================================

class FrameFormatter:
    """数据帧格式化器，将RowData转换为发送帧"""

    def __init__(self, config: FormatConfig) -> None:
        self._template = config.resolved_template()
        logger.info(f"帧模板: {repr(self._template)}")

    def format(self, row: RowData) -> str:
        """格式化单行数据为发送帧"""
        return self._template.format(
            power=row.power,
            freq=row.freq,
            timestamp=row.timestamp,
        )

    @property
    def template(self) -> str:
        return self._template


# ============================================================================
# 串口写入类
# ============================================================================

class SerialWriter:
    """串口写入器，负责数据的实际发送"""

    def __init__(self, config: SerialConfig, dry_run: bool) -> None:
        self._config = config
        self._dry_run = dry_run
        self._serial = None
        self._bytes_sent = 0  # 统计发送字节数

        # dry-run模式: 不打开串口，直接输出到stdout
        if self._dry_run:
            logger.info("Dry-run模式: 数据将输出到stdout而非串口")
            return

        if serial is None:
            logger.error("pyserial未安装，无法使用串口")
            raise SystemExit("pyserial is not installed. Use --dry-run or install pyserial.")

        # 打开串口连接
        logger.info(f"正在连接串口 {self._config.port}...")
        try:
            self._serial = serial.Serial(
                port=self._config.port,
                baudrate=self._config.baudrate,
                bytesize=self._config.bytesize,
                parity=self._config.parity,
                stopbits=self._config.stopbits,
                timeout=self._config.timeout,
                write_timeout=self._config.write_timeout,
            )
            logger.info(
                f"串口连接成功: {self._config.port} "
                f"(波特率={self._config.baudrate}, 数据位={self._config.bytesize}, "
                f"校验={self._config.parity}, 停止位={self._config.stopbits})"
            )
        except serial.SerialException as e:
            logger.error(f"串口连接失败: {e}")
            raise SystemExit(f"Failed to open serial port: {e}")

    def write(self, frame: str) -> None:
        """写入一帧数据"""
        if self._dry_run:
            sys.stdout.write(frame)
            sys.stdout.flush()
            return
        if not self._serial:
            raise RuntimeError("Serial port not initialized")
        data = frame.encode(self._config.encoding)
        self._serial.write(data)
        self._bytes_sent += len(data)

    def close(self) -> None:
        """关闭串口连接"""
        if self._serial:
            logger.info(f"关闭串口连接，共发送 {self._bytes_sent} 字节")
            self._serial.close()


# ============================================================================
# PMU串口流发送器
# ============================================================================

class PmuSerialStreamer:
    """PMU数据流发送主控类，协调各模块完成定时发送"""

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
        """启动发送循环，返回退出码"""
        # 使用单调时钟计算节拍，降低累计漂移
        interval = max(self._stream_config.interval_sec, 0.0)
        next_time = time.monotonic()

        logger.info(
            f"开始发送数据 (间隔={interval}s, 循环={self._stream_config.loop})"
        )
        sent_count = 0

        try:
            while True:
                sent_any = False
                for row in self._csv_source.iter_rows():
                    now = time.monotonic()
                    if interval > 0 and now < next_time:
                        time.sleep(next_time - now)

                    frame = self._formatter.format(row)
                    self._writer.write(frame)
                    sent_count += 1

                    # 记录发送的数据
                    if self._stream_config.verbose:
                        sys.stderr.write(f"Sent: {frame}")
                    logger.debug(
                        f"发送#{sent_count}: freq={row.freq:.3f}, power={row.power:.3f}"
                    )

                    sent_any = True
                    next_time += interval

                # 非循环模式：本轮CSV发送完就退出
                if not self._stream_config.loop:
                    logger.info(f"发送完成，共发送 {sent_count} 帧")
                    return 0 if sent_any else 2

                logger.info(f"本轮发送完成({sent_count}帧)，开始下一轮循环")
        except KeyboardInterrupt:
            logger.info(f"用户中断，共发送 {sent_count} 帧")
            return 0
        finally:
            self._writer.close()


# ============================================================================
# 辅助函数
# ============================================================================

def normalize_header(cell: str) -> str:
    """统一表头空白与BOM字符"""
    return " ".join(str(cell).strip().lstrip("\ufeff").split())


def resolve_col(header: list[str], name: str, col_base: int) -> Optional[int]:
    """解析列索引，支持列名或列序号"""
    if name is None:
        return None
    spec = str(name).strip()
    # 数字直接作为索引
    if spec.isdigit():
        return int(spec) - col_base
    # 按列名查找(精确匹配)
    target = normalize_header(spec)
    if target in header:
        return header.index(target)
    # 按列名查找(忽略大小写)
    lower_header = [h.lower() for h in header]
    if target.lower() in lower_header:
        return lower_header.index(target.lower())
    return None


def parse_float(value: str) -> Optional[float]:
    """安全解析浮点数，失败返回None"""
    try:
        return float(str(value).strip())
    except ValueError:
        return None


def _normalize_time_input(user_input: str) -> str:
    """
    规范化用户时间输入，转换为CSV时间戳前缀格式。

    支持多种分隔符(- . : 空格)，输出格式: YYYY-MM-DD HH:MM:SS（按输入精度截断）

    示例:
      '2020-05-07-05:50'    → '2020-05-07 05:50'
      '2020.05.07.05.50.10' → '2020-05-07 05:50:10'
      '2020-05-07-05'       → '2020-05-07 05'
    """
    import re
    # 统一分隔符：将所有非数字字符替换为单个分隔符
    parts = re.split(r'[^\d]+', user_input.strip())
    parts = [p for p in parts if p]  # 移除空字符串

    if len(parts) < 3:
        return user_input  # 格式不足，原样返回

    # 构建规范化时间字符串
    # 日期部分: YYYY-MM-DD
    year = parts[0].zfill(4)
    month = parts[1].zfill(2)
    day = parts[2].zfill(2)
    result = f"{year}-{month}-{day}"

    # 时间部分: HH:MM:SS（可选）
    if len(parts) >= 4:
        hour = parts[3].zfill(2)
        result += f" {hour}"
        if len(parts) >= 5:
            minute = parts[4].zfill(2)
            result += f":{minute}"
            if len(parts) >= 6:
                second = parts[5].zfill(2)
                result += f":{second}"

    return result


def coerce_bool(value) -> bool:
    """将各种类型转换为布尔值"""
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        return bool(value)
    return str(value).strip().lower() in ("1", "true", "yes", "y", "on")


def load_config(path: str) -> dict:
    """读取JSON配置文件"""
    if not path:
        return {}
    config_path = pathlib.Path(path)
    if not config_path.exists():
        logger.error(f"配置文件不存在: {config_path}")
        raise SystemExit(f"Config not found: {config_path}")
    logger.info(f"加载配置文件: {config_path}")
    with config_path.open("r", encoding="utf-8") as f:
        data = json.load(f)
    if not isinstance(data, dict):
        raise SystemExit("Config must be a JSON object")
    return data


def list_serial_ports() -> int:
    """枚举并打印所有可用串口"""
    if list_ports is None:
        logger.error("pyserial未安装，无法列出串口")
        print("pyserial is not installed.")
        return 1
    ports = list(list_ports.comports())
    if not ports:
        logger.info("未发现任何串口设备")
        print("No serial ports found.")
        return 1
    logger.info(f"发现 {len(ports)} 个串口设备")
    for port in ports:
        print(f"{port.device} - {port.description}")
    return 0


def build_config(args: argparse.Namespace) -> tuple[SerialConfig, CsvConfig, StreamConfig, FormatConfig]:
    """将命令行参数组装成配置对象"""
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
        start_time=args.start_time if args.start_time else None,
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

    # 记录配置信息
    logger.debug(f"串口配置: port={serial_config.port}, baudrate={serial_config.baudrate}")
    logger.debug(f"CSV配置: path={csv_config.path}, start_row={csv_config.start_row}")
    logger.debug(f"发送配置: interval={stream_config.interval_sec}s, loop={stream_config.loop}")

    return serial_config, csv_config, stream_config, format_config


def parse_args() -> argparse.Namespace:
    # 先读取配置文件，再解析命令行覆盖
    pre = argparse.ArgumentParser(add_help=False)
    pre.add_argument("--config", default="", help="JSON config file")
    pre_args, remaining = pre.parse_known_args()

    config_path = resolve_path(pre_args.config) if pre_args.config else None
    config = load_config(str(config_path)) if config_path else {}
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
    parser.add_argument(
        "--start-time",
        default=config.get("start_time", ""),
        help="开始时间(如 2020-05-07-05:50 或 2020-05-07-05)",
    )

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
        help="详细输出模式(显示每帧发送)",
    )
    parser.add_argument(
        "--debug",
        action=argparse.BooleanOptionalAction,
        default=coerce_bool(config.get("debug", False)),
        help="调试模式(显示DEBUG级别日志)",
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

    if args.csv:
        args.csv = str(resolve_path(args.csv))

    # 必要参数校验
    if not args.csv:
        parser.error("--csv is required (or provide csv_path in config)")
    if not args.port and not args.dry_run:
        parser.error("--port is required unless --dry-run is set")

    return args


def main() -> int:
    """程序入口"""
    args = parse_args()

    # 设置日志级别(--debug 显示DEBUG级别)
    if hasattr(args, 'debug') and args.debug:
        logger.setLevel(logging.DEBUG)
        logging.getLogger().setLevel(logging.DEBUG)

    if args.list_ports:
        return list_serial_ports()

    logger.info("=" * 50)
    logger.info("PMU串口发送器启动")
    logger.info("=" * 50)

    # 组装各模块并启动发送
    serial_config, csv_config, stream_config, format_config = build_config(args)
    formatter = FrameFormatter(format_config)
    csv_source = CsvSource(csv_config, formatter.template)
    writer = SerialWriter(serial_config, stream_config.dry_run)
    streamer = PmuSerialStreamer(csv_source, formatter, writer, stream_config)

    result = streamer.run()
    logger.info("PMU串口发送器退出")
    return result


if __name__ == "__main__":
    raise SystemExit(main())

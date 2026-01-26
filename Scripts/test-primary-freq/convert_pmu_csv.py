#!/usr/bin/env python3
"""
离线处理一次调频CSV：提取 Timestamp/Power/Freq 三列并导出新文件。

用法示例：
  python convert_pmu_csv.py -i "xxx.csv" --timestamp "时间段" --power "20AGC01XQ01.AV" --freq "AMSPEED.AV" --freq-is-speed
  python convert_pmu_csv.py --config config.json

配置说明：
  - JSON：键名与命令行参数一致，如 {"input":"../../Tests/test-data","timestamp":"时间段",...}
  - INI：使用 [main] 段，键名同上

设计要点：
- 自动识别常见编码(utf-8-sig/gbk/gb2312)，并规范化表头空白
- 时间戳支持多格式/epoch，输出三列并按起止时间命名文件
- 支持输入文件夹批处理（默认 ../../Tests/test-data），输出到目录（默认 ../../Tests/output-test-data）
- Freq 可按转速 rpm 转成 Hz（/60）
"""
import argparse
import configparser
import csv
import datetime as dt
import json
import pathlib
import sys


SCRIPT_DIR = pathlib.Path(__file__).resolve().parent
DEFAULT_INPUT = "../../Tests/test-data"
DEFAULT_OUTPUT_DIR = "../../Tests/output-test-data"


def resolve_path(path: str) -> pathlib.Path:
    if not path:
        return pathlib.Path(path)
    p = pathlib.Path(path)
    if p.is_absolute():
        return p
    return SCRIPT_DIR / p


TS_FORMATS = [
    "%y/%m/%d %H:%M:%S",
    "%Y/%m/%d %H:%M:%S",
    "%Y-%m-%d %H:%M:%S",
    "%Y.%m.%d %H.%M.%S",
    "%Y.%m.%d %H:%M:%S",
    "%y/%m/%d %H:%M",
    "%Y/%m/%d %H:%M",
    "%Y-%m-%d %H:%M",
    "%y/%m/%d %H:%M:%S.%f",
    "%Y/%m/%d %H:%M:%S.%f",
    "%Y-%m-%d %H:%M:%S.%f",
    "%Y%m%d%H%M%S",
    "%Y%m%d",
]

BOOL_KEYS = {"freq_is_speed", "recursive"}
INT_KEYS = {"col_base"}
CONFIG_KEYS = {
    "input",
    "timestamp",
    "power",
    "freq",
    "freq_is_speed",
    "col_base",
    "encoding",
    "output",
    "output_dir",
    "out_ts_format",
    "fname_ts_format",
    "pattern",
    "recursive",
}


class ColumnResolveError(ValueError):
    def __init__(self, message: str, header_norm: list[str]) -> None:
        super().__init__(message)
        self.header_norm = header_norm


def parse_timestamp(value: str) -> dt.datetime | None:
    """Parse timestamps in multiple common formats or numeric epoch."""
    # 支持多种字符串格式与 epoch(秒/毫秒/微秒)
    s = str(value).strip()
    if not s:
        return None

    # Numeric epoch seconds / milliseconds
    if s.isdigit():
        if len(s) in (8, 14):
            for fmt in ("%Y%m%d", "%Y%m%d%H%M%S"):
                try:
                    return dt.datetime.strptime(s, fmt)
                except ValueError:
                    pass
        try:
            n = int(s)
            if n > 10**12:  # microseconds
                return dt.datetime.fromtimestamp(n / 1_000_000)
            if n > 10**10:  # milliseconds
                return dt.datetime.fromtimestamp(n / 1_000)
            return dt.datetime.fromtimestamp(n)
        except ValueError:
            return None

    # ISO-like strings
    if "T" in s:
        try:
            s_iso = s.replace("Z", "+00:00")
            return dt.datetime.fromisoformat(s_iso)
        except ValueError:
            pass

    for fmt in TS_FORMATS:
        try:
            return dt.datetime.strptime(s, fmt)
        except ValueError:
            continue
    return None


def normalize_header(cell: str) -> str:
    return " ".join(cell.strip().lstrip("\ufeff").split())


def resolve_col(header: list[str], name: str) -> int | None:
    if name is None:
        return None
    target = normalize_header(name)
    norm_header = [normalize_header(h) for h in header]
    if target in norm_header:
        return norm_header.index(target)
    lower_header = [h.lower() for h in norm_header]
    if target.lower() in lower_header:
        return lower_header.index(target.lower())
    return None


def open_csv_with_encoding(path: pathlib.Path, encoding: str | None):
    # 按常见编码尝试读取CSV并取表头
    encodings = [encoding] if encoding else ["utf-8-sig", "gbk", "gb2312"]
    last_err = None
    for enc in encodings:
        try:
            f = path.open("r", encoding=enc, newline="")
            reader = csv.reader(f, delimiter=",")
            header = next(reader)
            return f, reader, header, enc
        except UnicodeDecodeError as err:
            last_err = err
        except StopIteration:
            raise SystemExit("空文件，无法读取表头。")
    raise SystemExit(f"无法读取文件编码：{last_err}")


def parse_float(value: str) -> float | None:
    s = str(value).strip()
    if not s:
        return None
    try:
        return float(s)
    except ValueError:
        return None


def coerce_config_value(key: str, value):
    if value is None:
        return None
    if key in BOOL_KEYS:
        if isinstance(value, bool):
            return value
        if isinstance(value, (int, float)):
            return bool(value)
        return str(value).strip().lower() in ("1", "true", "yes", "y", "on")
    if key in INT_KEYS:
        if isinstance(value, int):
            return value
        if isinstance(value, float) and value.is_integer():
            return int(value)
        return int(str(value).strip())
    if isinstance(value, str):
        return value
    return str(value)


def load_config(path: str) -> dict:
    # 读取配置文件（json 或 ini），返回与命令行参数同名的键
    if not path:
        return {}
    config_path = pathlib.Path(path)
    if not config_path.exists():
        raise SystemExit(f"找不到配置文件: {config_path}")

    if config_path.suffix.lower() == ".json":
        with config_path.open("r", encoding="utf-8") as f:
            data = json.load(f)
        if not isinstance(data, dict):
            raise SystemExit("配置文件必须是 JSON 对象。")
        raw = data
    else:
        parser = configparser.ConfigParser()
        parser.read(config_path, encoding="utf-8")
        section = parser["main"] if "main" in parser else parser["DEFAULT"]
        raw = dict(section.items())

    config: dict = {}
    for key, value in raw.items():
        if key not in CONFIG_KEYS:
            continue
        try:
            config[key] = coerce_config_value(key, value)
        except ValueError as exc:
            raise SystemExit(f"配置项 {key} 解析失败: {exc}") from exc
    return config


def resolve_column_indices(
    header: list[str],
    timestamp_spec: str,
    power_name: str,
    freq_name: str,
    col_base: int,
) -> tuple[list[str], int, int, int]:
    # 规范化表头并解析列位置（支持列名或列序号）
    header_norm = [normalize_header(h) for h in header]

    ts_spec = timestamp_spec.strip()
    if ts_spec.isdigit():
        ts_idx = int(ts_spec) - col_base
    else:
        ts_idx = resolve_col(header_norm, ts_spec)

    power_idx = resolve_col(header_norm, power_name)
    freq_idx = resolve_col(header_norm, freq_name)

    if ts_idx is None or ts_idx < 0 or ts_idx >= len(header_norm):
        raise ColumnResolveError("未找到时间戳列，请检查列名或列序号。", header_norm)
    if power_idx is None:
        raise ColumnResolveError("未找到 Power 列，请检查列名。", header_norm)
    if freq_idx is None:
        raise ColumnResolveError("未找到 Freq 列，请检查列名。", header_norm)

    return header_norm, ts_idx, power_idx, freq_idx


def extract_rows(
    reader: csv.reader,
    ts_idx: int,
    power_idx: int,
    freq_idx: int,
    out_ts_format: str,
    freq_is_speed: bool,
) -> tuple[list[list[str]], dt.datetime | None, dt.datetime | None, int, int]:
    # 遍历数据行，解析时间戳、功率、频率并汇总输出
    out_rows: list[list[str]] = []
    min_ts: dt.datetime | None = None
    max_ts: dt.datetime | None = None
    rows_written = 0
    rows_skipped = 0
    max_idx = max(ts_idx, power_idx, freq_idx)

    for row in reader:
        if not row or all(not str(c).strip() for c in row):
            continue
        if max_idx >= len(row):
            rows_skipped += 1
            continue

        ts_raw = str(row[ts_idx]).strip()
        ts_val = parse_timestamp(ts_raw)
        if ts_val is None:
            rows_skipped += 1
            continue

        power_val = str(row[power_idx]).strip()
        if not power_val:
            rows_skipped += 1
            continue

        freq_raw = row[freq_idx]
        freq_val = parse_float(freq_raw)
        if freq_val is None:
            rows_skipped += 1
            continue
        if freq_is_speed:
            freq_val = freq_val / 60.0

        if min_ts is None or ts_val < min_ts:
            min_ts = ts_val
        if max_ts is None or ts_val > max_ts:
            max_ts = ts_val

        out_rows.append(
            [
                ts_val.strftime(out_ts_format),
                power_val,
                f"{freq_val}",
            ]
        )
        rows_written += 1

    return out_rows, min_ts, max_ts, rows_written, rows_skipped


def build_output_path(
    input_path: pathlib.Path,
    output_arg: str,
    output_dir: str,
    min_ts: dt.datetime | None,
    max_ts: dt.datetime | None,
    fname_ts_format: str,
) -> pathlib.Path:
    # 根据起止时间生成默认文件名，或使用手动指定文件名/目录
    output_tmp = output_arg.strip()
    if output_tmp:
        return pathlib.Path(output_tmp)
    if min_ts is None or max_ts is None:
        raise ValueError("无法确定起止时间，需手动指定 --output。")
    start_str = min_ts.strftime(fname_ts_format)
    end_str = max_ts.strftime(fname_ts_format)
    base_dir = pathlib.Path(output_dir) if output_dir else input_path.parent
    return base_dir / f"{start_str}-{end_str}.csv"


def collect_input_files(
    input_path: pathlib.Path, pattern: str, recursive: bool
) -> list[pathlib.Path]:
    # 支持输入文件或文件夹，文件夹模式按 pattern 过滤
    if input_path.is_file():
        return [input_path]
    if not input_path.is_dir():
        raise SystemExit(f"找不到输入路径: {input_path}")
    if recursive:
        files = sorted(input_path.rglob(pattern))
    else:
        files = sorted(input_path.glob(pattern))
    return [p for p in files if p.is_file()]


def process_single_file(
    input_path: pathlib.Path,
    args: argparse.Namespace,
    output_dir: str,
) -> tuple[pathlib.Path | None, int, int, str]:
    # 处理单个CSV文件，返回输出路径与统计信息
    f, reader, header, used_enc = open_csv_with_encoding(
        input_path, args.encoding or None
    )
    with f:
        try:
            _header_norm, ts_idx, power_idx, freq_idx = resolve_column_indices(
                header, args.timestamp, args.power, args.freq, args.col_base
            )
        except ColumnResolveError as exc:
            print(f"[{input_path.name}] {exc}", file=sys.stderr)
            print("可用列名:", ", ".join(exc.header_norm), file=sys.stderr)
            return None, 0, 0, used_enc

        out_rows, min_ts, max_ts, rows_written, rows_skipped = extract_rows(
            reader,
            ts_idx,
            power_idx,
            freq_idx,
            args.out_ts_format,
            args.freq_is_speed,
        )

    if not out_rows:
        print(f"[{input_path.name}] 没有有效数据输出。", file=sys.stderr)
        return None, rows_written, rows_skipped, used_enc

    try:
        output_path = build_output_path(
            input_path, args.output, output_dir, min_ts, max_ts, args.fname_ts_format
        )
    except ValueError as exc:
        print(f"[{input_path.name}] {exc}", file=sys.stderr)
        return None, rows_written, rows_skipped, used_enc

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", encoding="utf-8", newline="") as out_f:
        writer = csv.writer(out_f)
        writer.writerow(["Timestamp", "Power", "Freq"])
        writer.writerows(out_rows)

    return output_path, rows_written, rows_skipped, used_enc


def main() -> int:
    # 先读取配置文件（若存在），再解析命令行参数
    pre_parser = argparse.ArgumentParser(add_help=False)
    pre_parser.add_argument("--config", default="", help="配置文件(json/ini)")
    pre_args, remaining = pre_parser.parse_known_args()
    config_path = resolve_path(pre_args.config) if pre_args.config else None
    config = load_config(str(config_path)) if config_path else {}
    config = {k: v for k, v in config.items() if v is not None}

    parser = argparse.ArgumentParser(
        description="提取 Timestamp / Power / Freq 三列并输出新CSV"
    )
    parser.add_argument("--config", default=pre_args.config, help="配置文件(json/ini)")
    parser.add_argument(
        "-i",
        "--input",
        default=DEFAULT_INPUT,
        help=f"输入CSV文件或目录(默认 {DEFAULT_INPUT})",
    )
    parser.add_argument("--timestamp", help="时间戳列名或列序号")
    parser.add_argument("--power", help="Power列名")
    parser.add_argument("--freq", help="Freq列名")
    parser.add_argument("--freq-is-speed", action="store_true", help="Freq列为转速(rpm)，需/60")
    parser.add_argument(
        "--col-base",
        type=int,
        choices=[0, 1],
        default=0,
        help="列序号基准(默认0基)",
    )
    parser.add_argument("--encoding", default="", help="强制编码(如 gbk)")
    parser.add_argument("--output", default="", help="输出文件名(单文件)或输出目录(目录模式)")
    parser.add_argument(
        "--output-dir",
        default="",
        help=f"输出目录(目录模式，默认 {DEFAULT_OUTPUT_DIR})",
    )
    parser.add_argument("--pattern", default="*.csv", help="目录模式下文件匹配(默认 *.csv)")
    parser.add_argument("--recursive", action="store_true", help="目录模式递归查找CSV")
    parser.add_argument(
        "--out-ts-format",
        default="%Y-%m-%d %H:%M:%S",
        help="输出时间戳格式",
    )
    parser.add_argument(
        "--fname-ts-format",
        default="%Y.%m.%d.%H.%M.%S",
        help="文件名时间戳格式",
    )
    parser.set_defaults(**config)
    args = parser.parse_args(remaining)

    # 校验必填字段（可从配置文件提供）
    missing = [name for name in ("timestamp", "power", "freq") if not getattr(args, name)]
    if missing:
        print(f"缺少必要参数: {', '.join(missing)}", file=sys.stderr)
        print("请通过命令行或配置文件提供。", file=sys.stderr)
        return 2

    if args.output:
        args.output = str(resolve_path(args.output))
    if args.output_dir:
        args.output_dir = str(resolve_path(args.output_dir))

    input_path = resolve_path(args.input)
    try:
        input_files = collect_input_files(input_path, args.pattern, args.recursive)
    except SystemExit as exc:
        print(str(exc), file=sys.stderr)
        return 2

    if not input_files:
        print("未找到任何 CSV 文件。", file=sys.stderr)
        return 2

    dir_mode = input_path.is_dir()
    output_dir_arg = args.output_dir.strip()
    if dir_mode:
        # 目录模式下，--output 视为输出目录
        if not output_dir_arg:
            output_dir_arg = args.output.strip()
        if not output_dir_arg:
            output_dir_arg = DEFAULT_OUTPUT_DIR
        args.output = ""

    if output_dir_arg:
        output_dir_arg = str(resolve_path(output_dir_arg))
        pathlib.Path(output_dir_arg).mkdir(parents=True, exist_ok=True)

    total_written = 0
    total_skipped = 0
    success_count = 0

    for input_file in input_files:
        output_path, rows_written, rows_skipped, used_enc = process_single_file(
            input_file, args, output_dir_arg
        )
        total_written += rows_written
        total_skipped += rows_skipped
        if output_path:
            success_count += 1
            if len(input_files) == 1:
                print(
                    f"完成: {output_path} | 读取编码 {used_enc} | 输出 {rows_written} 行 | 跳过 {rows_skipped} 行"
                )
            else:
                print(
                    f"完成: {input_file.name} -> {output_path.name} | 输出 {rows_written} 行 | 跳过 {rows_skipped} 行"
                )

    if len(input_files) > 1:
        print(
            f"批量完成: 成功 {success_count}/{len(input_files)} | 输出 {total_written} 行 | 跳过 {total_skipped} 行"
        )

    return 0 if success_count > 0 else 2


if __name__ == "__main__":
    raise SystemExit(main())

from __future__ import annotations

import dataclasses
import json
import pathlib
from typing import Any

from . import protocol


SCRIPT_DIR = pathlib.Path(__file__).resolve().parent
DEFAULT_CONFIG_PATH = SCRIPT_DIR / "config.json"


@dataclasses.dataclass
class SerialSettings:
    port: str
    dry_run: bool = False
    baudrate: int = 115200
    bytesize: int = 8
    parity: str = "N"
    stopbits: float = 1.0
    timeout: float = 0.0
    write_timeout: float = 1.0


@dataclasses.dataclass
class CsvSettings:
    path: pathlib.Path
    encoding: str = "utf-8"
    timestamp_col: str = "Timestamp"
    power_col: str = "Power"
    power_unit: str = "W"
    freq_col: str = "Freq"
    col_base: int = 0
    start_row: int = 0
    max_rows: int = 0
    start_time: str = ""
    source_interval_sec: float = 1.0
    eof_behavior: str = "stop"


@dataclasses.dataclass
class StreamSettings:
    auto_start: bool = False
    default_period_ms: int = protocol.STREAM_PERIOD_DEFAULT_MS
    min_period_ms: int = protocol.STREAM_PERIOD_MIN_MS
    max_period_ms: int = protocol.STREAM_PERIOD_MAX_MS
    ma_output_ready: bool = True
    poll_interval_ms: int = 5
    exit_on_eof_in_dry_run: bool = True


@dataclasses.dataclass
class ParamSettings:
    path: pathlib.Path


@dataclasses.dataclass
class LoggingSettings:
    level: str = "INFO"
    file: pathlib.Path | None = None
    hex_frames: bool = False


@dataclasses.dataclass
class SimulatorSettings:
    serial: SerialSettings
    csv: CsvSettings
    stream: StreamSettings
    params: ParamSettings
    logging: LoggingSettings
    config_path: pathlib.Path


def load_settings(config_path: pathlib.Path = DEFAULT_CONFIG_PATH) -> SimulatorSettings:
    config_path = config_path.resolve()
    if not config_path.exists():
        raise SystemExit(f"Config file not found: {config_path}")

    with config_path.open("r", encoding="utf-8") as f:
        raw = json.load(f)
    if not isinstance(raw, dict):
        raise SystemExit("Simulator config must be a JSON object")

    base_dir = config_path.parent
    serial_raw = _section(raw, "serial")
    csv_raw = _section(raw, "csv")
    stream_raw = _section(raw, "stream")
    params_raw = _section(raw, "params")
    logging_raw = _section(raw, "logging")

    csv_path = _resolve_path(base_dir, _str(csv_raw, "path", ""))
    params_path = _resolve_path(base_dir, _str(params_raw, "path", "params_state.json"))
    log_file_str = _str_any(logging_raw, ("log_file", "file"), "")

    settings = SimulatorSettings(
        serial=SerialSettings(
            port=_str(serial_raw, "port", ""),
            dry_run=_bool(serial_raw, "dry_run", False),
            baudrate=_int(serial_raw, "baudrate", 115200),
            bytesize=_int(serial_raw, "bytesize", 8),
            parity=_str(serial_raw, "parity", "N"),
            stopbits=_float(serial_raw, "stopbits", 1.0),
            timeout=_float(serial_raw, "timeout", 0.0),
            write_timeout=_float(serial_raw, "write_timeout", 1.0),
        ),
        csv=CsvSettings(
            path=csv_path,
            encoding=_str(csv_raw, "encoding", "utf-8"),
            timestamp_col=_str(csv_raw, "timestamp_col", "Timestamp"),
            power_col=_str(csv_raw, "power_col", "Power"),
            power_unit=_str(csv_raw, "power_unit", "W"),
            freq_col=_str(csv_raw, "freq_col", "Freq"),
            col_base=_int(csv_raw, "col_base", 0),
            start_row=_int(csv_raw, "start_row", 0),
            max_rows=_int(csv_raw, "max_rows", 0),
            start_time=_str(csv_raw, "start_time", ""),
            source_interval_sec=_float(csv_raw, "source_interval_sec", 1.0),
            eof_behavior=_str(csv_raw, "eof_behavior", "stop"),
        ),
        stream=StreamSettings(
            auto_start=_bool(stream_raw, "auto_start", False),
            default_period_ms=_int(
                stream_raw, "default_period_ms", protocol.STREAM_PERIOD_DEFAULT_MS
            ),
            min_period_ms=_int(stream_raw, "min_period_ms", protocol.STREAM_PERIOD_MIN_MS),
            max_period_ms=_int(stream_raw, "max_period_ms", protocol.STREAM_PERIOD_MAX_MS),
            ma_output_ready=_bool(stream_raw, "ma_output_ready", True),
            poll_interval_ms=_int(stream_raw, "poll_interval_ms", 5),
            exit_on_eof_in_dry_run=_bool(stream_raw, "exit_on_eof_in_dry_run", True),
        ),
        params=ParamSettings(path=params_path),
        logging=LoggingSettings(
            level=_str_any(logging_raw, ("log_level", "level"), "INFO"),
            file=_resolve_path(base_dir, log_file_str) if log_file_str else None,
            hex_frames=_bool_any(logging_raw, ("log_hex_frames", "hex_frames"), False),
        ),
        config_path=config_path,
    )
    _validate_settings(settings)
    return settings


def _section(raw: dict[str, Any], key: str) -> dict[str, Any]:
    value = raw.get(key, {})
    if not isinstance(value, dict):
        raise SystemExit(f"Config section '{key}' must be an object")
    return value


def _resolve_path(base_dir: pathlib.Path, value: str) -> pathlib.Path:
    path = pathlib.Path(value)
    if path.is_absolute():
        return path
    return (base_dir / path).resolve()


def _str(raw: dict[str, Any], key: str, default: str) -> str:
    value = raw.get(key, default)
    return "" if value is None else str(value)


def _str_any(raw: dict[str, Any], keys: tuple[str, ...], default: str) -> str:
    for key in keys:
        if key in raw:
            return _str(raw, key, default)
    return default


def _int(raw: dict[str, Any], key: str, default: int) -> int:
    value = raw.get(key, default)
    if isinstance(value, bool):
        return int(value)
    return int(value)


def _float(raw: dict[str, Any], key: str, default: float) -> float:
    return float(raw.get(key, default))


def _bool(raw: dict[str, Any], key: str, default: bool) -> bool:
    value = raw.get(key, default)
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        return bool(value)
    return str(value).strip().lower() in ("1", "true", "yes", "y", "on")


def _bool_any(raw: dict[str, Any], keys: tuple[str, ...], default: bool) -> bool:
    for key in keys:
        if key in raw:
            return _bool(raw, key, default)
    return default


def _normalize_power_unit(value: str) -> str:
    normalized = str(value).strip().lower()
    if normalized == "w":
        return "W"
    if normalized == "kw":
        return "kW"
    if normalized == "mw":
        return "MW"
    raise SystemExit("csv.power_unit must be one of: W, kW, MW")


def _validate_settings(settings: SimulatorSettings) -> None:
    if not settings.serial.dry_run and not settings.serial.port:
        raise SystemExit("serial.port is required when serial.dry_run is false")
    settings.csv.power_unit = _normalize_power_unit(settings.csv.power_unit)
    if settings.csv.source_interval_sec <= 0:
        raise SystemExit("csv.source_interval_sec must be > 0")
    if settings.csv.eof_behavior not in ("stop", "hold", "loop"):
        raise SystemExit("csv.eof_behavior must be one of: stop, hold, loop")
    if settings.stream.min_period_ms < protocol.STREAM_PERIOD_MIN_MS:
        raise SystemExit("stream.min_period_ms cannot be below protocol minimum 50")
    if settings.stream.max_period_ms > protocol.STREAM_PERIOD_MAX_MS:
        raise SystemExit("stream.max_period_ms cannot exceed protocol maximum 60000")
    if settings.stream.min_period_ms > settings.stream.max_period_ms:
        raise SystemExit("stream.min_period_ms cannot exceed stream.max_period_ms")
    if not settings.stream.min_period_ms <= settings.stream.default_period_ms <= settings.stream.max_period_ms:
        settings.stream.default_period_ms = min(
            max(protocol.STREAM_PERIOD_DEFAULT_MS, settings.stream.min_period_ms),
            settings.stream.max_period_ms,
        )
    if settings.stream.poll_interval_ms < 1:
        settings.stream.poll_interval_ms = 1

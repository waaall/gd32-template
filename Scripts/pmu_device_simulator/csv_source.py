from __future__ import annotations

import csv
import dataclasses
import logging
import math
import re
from typing import Optional

from .config_model import CsvSettings


logger = logging.getLogger(__name__)


@dataclasses.dataclass(frozen=True)
class PmuSample:
    timestamp: str
    power_w: float
    freq_hz: float


class CsvPlaybackSource:
    def __init__(self, settings: CsvSettings) -> None:
        self._settings = settings
        self._rows = self._load_rows()
        self._last_index = 0

    @property
    def has_rows(self) -> bool:
        return bool(self._rows)

    @property
    def row_count(self) -> int:
        return len(self._rows)

    def sample_at(self, elapsed_sec: float) -> Optional[PmuSample]:
        if not self._rows:
            return None

        index = int(max(elapsed_sec, 0.0) // self._settings.source_interval_sec)
        if index >= len(self._rows):
            if self._settings.eof_behavior == "loop":
                index %= len(self._rows)
            elif self._settings.eof_behavior == "hold":
                index = len(self._rows) - 1
            else:
                return None

        self._last_index = index
        return self._rows[index]

    def latest_sample(self) -> Optional[PmuSample]:
        if not self._rows:
            return None
        return self._rows[min(self._last_index, len(self._rows) - 1)]

    def _load_rows(self) -> list[PmuSample]:
        path = self._settings.path
        if not path.exists():
            raise SystemExit(f"CSV file not found: {path}")

        logger.info("Loading CSV data: %s", path)
        with path.open("r", encoding=self._settings.encoding, newline="") as f:
            reader = csv.reader(f)
            header = next(reader, None)
            if not header:
                raise SystemExit("CSV file has no header")

            header_norm = [_normalize_header(cell) for cell in header]
            ts_idx = _resolve_col(header_norm, self._settings.timestamp_col, self._settings.col_base)
            power_idx = _resolve_col(header_norm, self._settings.power_col, self._settings.col_base)
            freq_idx = _resolve_col(header_norm, self._settings.freq_col, self._settings.col_base)

            if ts_idx is None:
                raise SystemExit(f"Timestamp column not found: {self._settings.timestamp_col}")
            if power_idx is None:
                raise SystemExit(f"Power column not found: {self._settings.power_col}")
            if freq_idx is None:
                raise SystemExit(f"Freq column not found: {self._settings.freq_col}")

            start_time_prefix = (
                _normalize_time_input(self._settings.start_time)
                if self._settings.start_time
                else ""
            )
            found_start = not start_time_prefix
            max_idx = max(ts_idx, power_idx, freq_idx)
            rows: list[PmuSample] = []
            skipped_data_rows = 0

            for row in reader:
                if not row or all(not str(cell).strip() for cell in row):
                    continue
                if max_idx >= len(row):
                    continue

                if skipped_data_rows < self._settings.start_row:
                    skipped_data_rows += 1
                    continue

                timestamp = str(row[ts_idx]).strip()
                if not found_start:
                    if timestamp.startswith(start_time_prefix) or timestamp >= start_time_prefix:
                        found_start = True
                        logger.info("CSV playback starts at timestamp: %s", timestamp)
                    else:
                        continue

                power = _parse_float(row[power_idx])
                freq = _parse_float(row[freq_idx])
                if power is None or freq is None:
                    continue

                rows.append(PmuSample(timestamp=timestamp, power_w=power, freq_hz=freq))
                if self._settings.max_rows > 0 and len(rows) >= self._settings.max_rows:
                    break

        if start_time_prefix and not found_start:
            raise SystemExit(f"Start time is beyond CSV range: {start_time_prefix}")
        if not rows:
            raise SystemExit("CSV source contains no usable rows")

        logger.info("Loaded %d CSV rows", len(rows))
        return rows


def _normalize_header(cell: str) -> str:
    return " ".join(str(cell).strip().lstrip("\ufeff").split())


def _resolve_col(header: list[str], name: str, col_base: int) -> Optional[int]:
    spec = str(name).strip()
    if spec.isdigit():
        index = int(spec) - col_base
        return index if 0 <= index < len(header) else None
    target = _normalize_header(spec)
    if target in header:
        return header.index(target)
    lower_header = [cell.lower() for cell in header]
    if target.lower() in lower_header:
        return lower_header.index(target.lower())
    return None


def _parse_float(value: str) -> Optional[float]:
    try:
        parsed = float(str(value).strip())
    except ValueError:
        return None
    return parsed if math.isfinite(parsed) else None


def _normalize_time_input(user_input: str) -> str:
    parts = [part for part in re.split(r"[^\d]+", user_input.strip()) if part]
    if len(parts) < 3:
        return user_input

    result = f"{parts[0].zfill(4)}-{parts[1].zfill(2)}-{parts[2].zfill(2)}"
    if len(parts) >= 4:
        result += f" {parts[3].zfill(2)}"
    if len(parts) >= 5:
        result += f":{parts[4].zfill(2)}"
    if len(parts) >= 6:
        result += f":{parts[5].zfill(2)}"
    return result


from __future__ import annotations

import dataclasses
import json
import logging
import math
import pathlib
import struct
import tempfile
from typing import Any

from . import protocol


logger = logging.getLogger(__name__)

ZC_FAKE_PERIOD_MS = 4097
ZC_WINDOW_PERIODS = 4098
ZC_MIN_FREQ_HZ = 4099
ZC_MAX_FREQ_HZ = 4100
ZC_INACTIVITY_TIMEOUT_MS = 4101

MA_CAL_BASE = 8193
MA_CAL_LAST = 8207

ZC_FREQ_CALC_CONST = 16_000_000
ZC_MAX_WINDOW_PERIODS = 30
ZC_MIN_INACTIVITY_TIMEOUT_MS = 50
ZC_MAX_INACTIVITY_TIMEOUT_MS = 60000

MA_OFFSET_LIMIT = 1310
MA_GAIN_MIN = 5000
MA_GAIN_MAX = 15000


@dataclasses.dataclass(frozen=True)
class ParamValue:
    object_id: int
    value_type: int
    value: bytes

    @property
    def value_len(self) -> int:
        return len(self.value)


class ParamStore:
    def __init__(self, path: pathlib.Path) -> None:
        self._path = path
        self._state = self._load_or_create()

    @property
    def config_version(self) -> int:
        return int(self._state.get("config_version", 0)) & 0xFFFF

    def get(self, object_id: int) -> tuple[int, ParamValue | None]:
        if ZC_FAKE_PERIOD_MS <= object_id <= ZC_INACTIVITY_TIMEOUT_MS:
            return self._get_zc(object_id)
        if MA_CAL_BASE <= object_id <= MA_CAL_LAST:
            return self._get_ma(object_id)
        return protocol.STATUS_INVALID_OBJECT, None

    def set(self, object_id: int, value_type: int, value: bytes) -> tuple[int, ParamValue | None]:
        old_state = json.loads(json.dumps(self._state))
        if ZC_FAKE_PERIOD_MS <= object_id <= ZC_INACTIVITY_TIMEOUT_MS:
            status = self._set_zc(object_id, value_type, value)
        elif MA_CAL_BASE <= object_id <= MA_CAL_LAST:
            status = self._set_ma(object_id, value_type, value)
        else:
            status = protocol.STATUS_INVALID_OBJECT

        if status != protocol.STATUS_OK:
            self._state = old_state
            return status, None

        self._state["config_version"] = (self.config_version + 1) & 0xFFFF
        self._write()
        logger.info("Parameter updated: object_id=%d config_version=%d", object_id, self.config_version)
        return self.get(object_id)

    def _get_zc(self, object_id: int) -> tuple[int, ParamValue | None]:
        zc = self._state["zero_crossing"]
        if object_id == ZC_FAKE_PERIOD_MS:
            return protocol.STATUS_OK, _float_value(object_id, float(zc["fake_period_ms"]))
        if object_id == ZC_WINDOW_PERIODS:
            return protocol.STATUS_OK, ParamValue(object_id, protocol.VALUE_U8, bytes([int(zc["window_periods"]) & 0xFF]))
        if object_id == ZC_MIN_FREQ_HZ:
            return protocol.STATUS_OK, _float_value(object_id, float(zc["min_freq_hz"]))
        if object_id == ZC_MAX_FREQ_HZ:
            return protocol.STATUS_OK, _float_value(object_id, float(zc["max_freq_hz"]))
        if object_id == ZC_INACTIVITY_TIMEOUT_MS:
            return protocol.STATUS_OK, ParamValue(
                object_id, protocol.VALUE_U32_LE, protocol.u32_le(int(zc["inactivity_timeout_ms"]))
            )
        return protocol.STATUS_INVALID_OBJECT, None

    def _set_zc(self, object_id: int, value_type: int, value: bytes) -> int:
        zc = self._state["zero_crossing"]
        if object_id in (ZC_FAKE_PERIOD_MS, ZC_MIN_FREQ_HZ, ZC_MAX_FREQ_HZ):
            if len(value) != 4:
                return protocol.STATUS_BAD_LENGTH
            if value_type != protocol.VALUE_FLOAT32_LE:
                return protocol.STATUS_INVALID_VALUE
            requested = struct.unpack("<f", value)[0]
            if not math.isfinite(requested):
                return protocol.STATUS_INVALID_VALUE

            if object_id == ZC_FAKE_PERIOD_MS:
                zc["fake_period_ms"] = float(requested)
            elif object_id == ZC_MIN_FREQ_HZ:
                zc["min_freq_hz"] = float(requested)
            else:
                zc["max_freq_hz"] = float(requested)

        elif object_id == ZC_WINDOW_PERIODS:
            if len(value) != 1:
                return protocol.STATUS_BAD_LENGTH
            if value_type != protocol.VALUE_U8:
                return protocol.STATUS_INVALID_VALUE
            zc["window_periods"] = int(value[0])

        elif object_id == ZC_INACTIVITY_TIMEOUT_MS:
            if len(value) != 4:
                return protocol.STATUS_BAD_LENGTH
            if value_type != protocol.VALUE_U32_LE:
                return protocol.STATUS_INVALID_VALUE
            zc["inactivity_timeout_ms"] = protocol.read_u32_le(value)

        else:
            return protocol.STATUS_INVALID_OBJECT

        return protocol.STATUS_OK if _validate_zc(zc) else protocol.STATUS_INVALID_VALUE

    def _get_ma(self, object_id: int) -> tuple[int, ParamValue | None]:
        channel, field = _ma_channel_field(object_id)
        calib = self._state["ma_calibration"][channel]
        if field == 0:
            return protocol.STATUS_OK, ParamValue(
                object_id, protocol.VALUE_I16_LE, protocol.i16_le(int(calib["zero_offset"]))
            )
        if field == 1:
            return protocol.STATUS_OK, ParamValue(
                object_id, protocol.VALUE_I16_LE, protocol.i16_le(int(calib["span_offset"]))
            )
        return protocol.STATUS_OK, ParamValue(
            object_id, protocol.VALUE_U16_LE, protocol.u16_le(int(calib["gain_10k"]))
        )

    def _set_ma(self, object_id: int, value_type: int, value: bytes) -> int:
        channel, field = _ma_channel_field(object_id)
        calib = self._state["ma_calibration"][channel]
        if field in (0, 1):
            if len(value) != 2:
                return protocol.STATUS_BAD_LENGTH
            if value_type != protocol.VALUE_I16_LE:
                return protocol.STATUS_INVALID_VALUE
            requested = protocol.read_i16_le(value)
            if field == 0:
                calib["zero_offset"] = _clamp(requested, -MA_OFFSET_LIMIT, MA_OFFSET_LIMIT)
            else:
                calib["span_offset"] = _clamp(requested, -MA_OFFSET_LIMIT, MA_OFFSET_LIMIT)
            return protocol.STATUS_OK

        if len(value) != 2:
            return protocol.STATUS_BAD_LENGTH
        if value_type != protocol.VALUE_U16_LE:
            return protocol.STATUS_INVALID_VALUE
        requested = protocol.read_u16_le(value)
        calib["gain_10k"] = _clamp(requested, MA_GAIN_MIN, MA_GAIN_MAX)
        return protocol.STATUS_OK

    def _load_or_create(self) -> dict[str, Any]:
        if not self._path.exists():
            self._path.parent.mkdir(parents=True, exist_ok=True)
            state = _default_state()
            self._state = state
            self._write()
            logger.info("Created default parameter state: %s", self._path)
            return state

        with self._path.open("r", encoding="utf-8") as f:
            state = json.load(f)
        original = json.loads(json.dumps(state))
        state = _normalize_state(state)
        if state != original:
            self._state = state
            self._write()
        return state

    def _write(self) -> None:
        self._path.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(
            "w",
            encoding="utf-8",
            dir=str(self._path.parent),
            delete=False,
            prefix=f".{self._path.name}.",
            suffix=".tmp",
        ) as f:
            json.dump(self._state, f, indent=2, ensure_ascii=False)
            f.write("\n")
            tmp_name = f.name
        pathlib.Path(tmp_name).replace(self._path)


def build_param_response_payload(
    status: int,
    object_id: int,
    value: ParamValue | None,
    config_version: int,
) -> bytes:
    payload = bytearray()
    payload.append(status & 0xFF)
    payload.extend(protocol.u16_le(object_id))
    if status == protocol.STATUS_OK and value is not None:
        payload.append(value.value_type & 0xFF)
        payload.append(value.value_len & 0xFF)
        payload.extend(value.value)
    else:
        payload.append(protocol.VALUE_NONE)
        payload.append(0)
    payload.extend(protocol.u16_le(config_version))
    return bytes(payload)


def _default_state() -> dict[str, Any]:
    return {
        "config_version": 0,
        "zero_crossing": {
            "fake_period_ms": 2.0,
            "window_periods": 10,
            "min_freq_hz": 38.0,
            "max_freq_hz": 65.0,
            "inactivity_timeout_ms": 500,
        },
        "ma_calibration": [
            {"zero_offset": 0, "span_offset": 0, "gain_10k": 10000},
            {"zero_offset": 0, "span_offset": 0, "gain_10k": 10000},
            {"zero_offset": 0, "span_offset": 0, "gain_10k": 10225},
            {"zero_offset": 0, "span_offset": 0, "gain_10k": 10000},
            {"zero_offset": 0, "span_offset": 0, "gain_10k": 10000},
        ],
    }


def _normalize_state(state: Any) -> dict[str, Any]:
    if not isinstance(state, dict):
        return _default_state()
    default = _default_state()
    state.setdefault("config_version", default["config_version"])
    state.setdefault("zero_crossing", default["zero_crossing"])
    state.setdefault("ma_calibration", default["ma_calibration"])
    if not isinstance(state["ma_calibration"], list):
        state["ma_calibration"] = default["ma_calibration"]
    while len(state["ma_calibration"]) < 5:
        state["ma_calibration"].append({"zero_offset": 0, "span_offset": 0, "gain_10k": 10000})
    state["ma_calibration"] = state["ma_calibration"][:5]
    for i, item in enumerate(state["ma_calibration"]):
        if not isinstance(item, dict):
            item = {}
            state["ma_calibration"][i] = item
        item["zero_offset"] = _clamp(int(item.get("zero_offset", 0)), -MA_OFFSET_LIMIT, MA_OFFSET_LIMIT)
        item["span_offset"] = _clamp(int(item.get("span_offset", 0)), -MA_OFFSET_LIMIT, MA_OFFSET_LIMIT)
        item["gain_10k"] = _clamp(int(item.get("gain_10k", 10000)), MA_GAIN_MIN, MA_GAIN_MAX)
    if not _validate_zc(state["zero_crossing"]):
        state["zero_crossing"] = default["zero_crossing"]
    state["config_version"] = int(state.get("config_version", 0)) & 0xFFFF
    return state


def _validate_zc(zc: dict[str, Any]) -> bool:
    try:
        fake_period_ms = float(zc["fake_period_ms"])
        window_periods = int(zc["window_periods"])
        min_freq_hz = float(zc["min_freq_hz"])
        max_freq_hz = float(zc["max_freq_hz"])
        inactivity_timeout_ms = int(zc["inactivity_timeout_ms"])
    except (KeyError, TypeError, ValueError):
        return False

    if fake_period_ms < 0.0 or fake_period_ms > 10.0:
        return False
    if window_periods <= 0 or window_periods > ZC_MAX_WINDOW_PERIODS:
        return False
    if not (min_freq_hz > 0.0 and max_freq_hz > 0.0 and min_freq_hz < max_freq_hz):
        return False
    if not ZC_MIN_INACTIVITY_TIMEOUT_MS <= inactivity_timeout_ms <= ZC_MAX_INACTIVITY_TIMEOUT_MS:
        return False

    min_period_count = _freq_hz_to_period_count(max_freq_hz)
    fake_period_count = int(fake_period_ms * 16000.0)
    return fake_period_count < min_period_count


def _freq_hz_to_period_count(freq_hz: float) -> int:
    return int(ZC_FREQ_CALC_CONST / freq_hz + 0.5)


def _float_value(object_id: int, value: float) -> ParamValue:
    return ParamValue(object_id, protocol.VALUE_FLOAT32_LE, struct.pack("<f", float(value)))


def _ma_channel_field(object_id: int) -> tuple[int, int]:
    offset = object_id - MA_CAL_BASE
    return offset // 3, offset % 3


def _clamp(value: int, minimum: int, maximum: int) -> int:
    return max(minimum, min(maximum, int(value)))

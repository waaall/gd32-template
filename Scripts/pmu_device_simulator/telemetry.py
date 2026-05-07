from __future__ import annotations

import math

from . import protocol
from .csv_source import PmuSample


KIND_PF_BASIC = 0x01
PF_BASIC_PAYLOAD_SIZE = 27

FLAG_FREQ_A_VALID = 1 << 0
FLAG_FREQ_B_VALID = 1 << 1
FLAG_FREQ_SELECTED_VALID = 1 << 2
FLAG_POWER_VALID = 1 << 3
FLAG_ADC_VALID = 1 << 4
FLAG_MA_OUTPUT_READY = 1 << 5
FLAG_CONFIG_CHANGED = 1 << 6


def build_pf_basic_payload(
    sample: PmuSample,
    stream_seq: int,
    timestamp_ms: int,
    config_version: int,
    config_changed: bool,
    ma_output_ready: bool,
) -> bytes:
    freq_millihz = _freq_to_i32(sample.freq_hz)
    power_w = _round_power_w(sample.power_w)

    quality_flags = (
        FLAG_FREQ_A_VALID
        | FLAG_FREQ_B_VALID
        | FLAG_FREQ_SELECTED_VALID
        | FLAG_ADC_VALID
        | FLAG_POWER_VALID
    )
    if ma_output_ready:
        quality_flags |= FLAG_MA_OUTPUT_READY
    if config_changed:
        quality_flags |= FLAG_CONFIG_CHANGED

    payload = bytearray()
    payload.append(KIND_PF_BASIC)
    payload.extend(protocol.u16_le(stream_seq))
    payload.extend(protocol.u32_le(timestamp_ms))
    payload.extend(protocol.u16_le(config_version))
    payload.extend(protocol.u16_le(quality_flags))
    payload.extend(protocol.i32_le(freq_millihz))
    payload.extend(protocol.i32_le(freq_millihz))
    payload.extend(protocol.i32_le(freq_millihz))
    payload.extend(protocol.i32_le(power_w))
    if len(payload) != PF_BASIC_PAYLOAD_SIZE:
        raise AssertionError(f"PF_BASIC payload size mismatch: {len(payload)}")
    return bytes(payload)


def _freq_to_i32(freq_hz: float) -> int:
    if not math.isfinite(freq_hz) or freq_hz < 0.0:
        return 0
    value = int(freq_hz * 1000.0 + 0.5)
    return min(value, 0x7FFFFFFF)


def _round_power_w(power_w: float) -> int:
    if not math.isfinite(power_w):
        return 0
    if power_w >= 0x7FFFFFFF:
        return 0x7FFFFFFF
    if power_w <= -0x80000000:
        return -0x80000000
    if power_w >= 0.0:
        return int(power_w + 0.5)
    return int(power_w - 0.5)


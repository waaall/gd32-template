from __future__ import annotations

import dataclasses
from enum import Enum, auto
from typing import Optional


SOF = b"\xA5\x5A"
VERSION = 0x01
MAX_PAYLOAD_SIZE = 128
HEADER_SIZE = 9
CRC_SIZE = 2

BODY_HEADER_SIZE = 7


MSG_GET_PARAM_REQ = 0x01
MSG_GET_PARAM_RESP = 0x02
MSG_SET_PARAM_REQ = 0x03
MSG_SET_PARAM_RESP = 0x04
MSG_TELEMETRY = 0x10
MSG_STREAM_CTRL_REQ = 0x20
MSG_STREAM_CTRL_RESP = 0x21
MSG_STATUS_REQ = 0x30
MSG_STATUS_RESP = 0x31
MSG_ERROR = 0x7F

STATUS_OK = 0x00
STATUS_INVALID_OBJECT = 0x01
STATUS_INVALID_VALUE = 0x02
STATUS_READ_ONLY = 0x03
STATUS_BUSY = 0x04
STATUS_BAD_LENGTH = 0x05
STATUS_UNSUPPORTED = 0x06
STATUS_CRC_ERROR = 0x07
STATUS_INTERNAL_ERROR = 0x08

VALUE_NONE = 0x00
VALUE_BOOL_U8 = 0x01
VALUE_U8 = 0x02
VALUE_U16_LE = 0x03
VALUE_U32_LE = 0x04
VALUE_I8 = 0x05
VALUE_I16_LE = 0x06
VALUE_I32_LE = 0x07
VALUE_FLOAT32_LE = 0x08

STREAM_GET_STATE = 0x00
STREAM_START = 0x01
STREAM_STOP = 0x02
STREAM_SET_PERIOD = 0x03

STREAM_PERIOD_MIN_MS = 50
STREAM_PERIOD_MAX_MS = 60000
STREAM_PERIOD_DEFAULT_MS = 500

STATUS_FLAG_STREAM_ENABLED = 1 << 0
STATUS_FLAG_RX_ARMED = 1 << 1
STATUS_FLAG_TX_BUSY = 1 << 2
STATUS_FLAG_ADC_VALID = 1 << 3
STATUS_FLAG_FREQ_VALID = 1 << 4
STATUS_FLAG_MA_OUTPUT_READY = 1 << 5
STATUS_FLAG_ERROR_LATCHED = 1 << 6


class ParseKind(Enum):
    NONE = auto()
    FRAME = auto()
    CRC_ERROR = auto()
    LENGTH_ERROR = auto()
    VERSION_ERROR = auto()


@dataclasses.dataclass
class Frame:
    msg_type: int
    flags: int
    seq: int
    payload: bytes

    @property
    def length(self) -> int:
        return len(self.payload)


@dataclasses.dataclass
class ParseResult:
    kind: ParseKind
    frame: Optional[Frame] = None


def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def u16_le(value: int) -> bytes:
    return int(value & 0xFFFF).to_bytes(2, "little", signed=False)


def u32_le(value: int) -> bytes:
    return int(value & 0xFFFFFFFF).to_bytes(4, "little", signed=False)


def i16_le(value: int) -> bytes:
    return int(value).to_bytes(2, "little", signed=True)


def i32_le(value: int) -> bytes:
    return int(value).to_bytes(4, "little", signed=True)


def read_u16_le(data: bytes, offset: int = 0) -> int:
    return int.from_bytes(data[offset : offset + 2], "little", signed=False)


def read_u32_le(data: bytes, offset: int = 0) -> int:
    return int.from_bytes(data[offset : offset + 4], "little", signed=False)


def read_i16_le(data: bytes, offset: int = 0) -> int:
    return int.from_bytes(data[offset : offset + 2], "little", signed=True)


def read_i32_le(data: bytes, offset: int = 0) -> int:
    return int.from_bytes(data[offset : offset + 4], "little", signed=True)


def encode_frame(msg_type: int, seq: int, payload: bytes = b"", flags: int = 0) -> bytes:
    if len(payload) > MAX_PAYLOAD_SIZE:
        raise ValueError(f"payload too large: {len(payload)} > {MAX_PAYLOAD_SIZE}")

    body = bytearray()
    body.append(VERSION)
    body.append(msg_type & 0xFF)
    body.append(flags & 0xFF)
    body.extend(u16_le(seq))
    body.extend(u16_le(len(payload)))
    body.extend(payload)
    crc = crc16_ccitt_false(bytes(body))
    return SOF + bytes(body) + u16_le(crc)


def hex_bytes(data: bytes) -> str:
    return " ".join(f"{byte:02X}" for byte in data)


class FrameParser:
    _WAIT_SOF0 = 0
    _WAIT_SOF1 = 1
    _READ_BODY = 2

    def __init__(self, max_payload_size: int = MAX_PAYLOAD_SIZE) -> None:
        self._max_payload_size = min(max_payload_size, MAX_PAYLOAD_SIZE)
        self._state = self._WAIT_SOF0
        self._body = bytearray()
        self._expected = BODY_HEADER_SIZE
        self._payload_length = 0

    def push_byte(self, byte: int) -> ParseResult:
        byte &= 0xFF

        if self._state == self._WAIT_SOF0:
            if byte == SOF[0]:
                self._state = self._WAIT_SOF1
            return ParseResult(ParseKind.NONE)

        if self._state == self._WAIT_SOF1:
            if byte == SOF[1]:
                self._state = self._READ_BODY
                self._body.clear()
                self._expected = BODY_HEADER_SIZE
                self._payload_length = 0
            elif byte != SOF[0]:
                self._state = self._WAIT_SOF0
            return ParseResult(ParseKind.NONE)

        if self._state != self._READ_BODY:
            self._reset()
            return ParseResult(ParseKind.NONE)

        self._body.append(byte)
        if len(self._body) == BODY_HEADER_SIZE:
            if self._body[0] != VERSION:
                self._reset()
                return ParseResult(ParseKind.VERSION_ERROR)

            self._payload_length = read_u16_le(self._body, 5)
            if self._payload_length > self._max_payload_size:
                self._reset()
                return ParseResult(ParseKind.LENGTH_ERROR)

            self._expected = BODY_HEADER_SIZE + self._payload_length + CRC_SIZE

        if len(self._body) < self._expected:
            return ParseResult(ParseKind.NONE)

        body_without_crc = bytes(self._body[: BODY_HEADER_SIZE + self._payload_length])
        expected_crc = crc16_ccitt_false(body_without_crc)
        received_crc = read_u16_le(self._body, BODY_HEADER_SIZE + self._payload_length)
        self._reset()

        if received_crc != expected_crc:
            return ParseResult(ParseKind.CRC_ERROR)

        frame = Frame(
            msg_type=body_without_crc[1],
            flags=body_without_crc[2],
            seq=read_u16_le(body_without_crc, 3),
            payload=body_without_crc[BODY_HEADER_SIZE:],
        )
        return ParseResult(ParseKind.FRAME, frame)

    def _reset(self) -> None:
        self._state = self._WAIT_SOF0
        self._body.clear()
        self._expected = BODY_HEADER_SIZE
        self._payload_length = 0


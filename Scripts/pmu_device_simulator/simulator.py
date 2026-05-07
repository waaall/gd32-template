from __future__ import annotations

import dataclasses
import logging
import time

from . import protocol, telemetry
from .config_model import SimulatorSettings
from .csv_source import CsvPlaybackSource
from .params import ParamStore, build_param_response_payload
from .serial_endpoint import SerialEndpoint


logger = logging.getLogger(__name__)


@dataclasses.dataclass
class DeviceState:
    stream_enabled: bool = False
    stream_period_ms: int = protocol.STREAM_PERIOD_DEFAULT_MS
    next_stream_time: float = 0.0
    playback_start_time: float = 0.0
    boot_time: float = 0.0
    stream_seq: int = 0
    frame_seq: int = 0
    config_changed_pending: bool = False
    rx_error_count: int = 0
    crc_error_count: int = 0
    tx_drop_count: int = 0
    error_latched: bool = False


class PmuDeviceSimulator:
    def __init__(
        self,
        settings: SimulatorSettings,
        source: CsvPlaybackSource,
        params: ParamStore,
        endpoint: SerialEndpoint,
    ) -> None:
        self._settings = settings
        self._source = source
        self._params = params
        self._endpoint = endpoint
        self._parser = protocol.FrameParser()
        self._state = DeviceState(stream_period_ms=settings.stream.default_period_ms)
        self._telemetry_sent = 0

    def run(self) -> int:
        now = time.monotonic()
        self._state.boot_time = now
        if self._settings.stream.auto_start:
            self._start_stream(period_ms=None, now=now)

        logger.info(
            "Simulator started: rows=%d auto_start=%s period_ms=%d",
            self._source.row_count,
            self._settings.stream.auto_start,
            self._state.stream_period_ms,
        )

        try:
            while True:
                self._process_rx()
                eof = self._queue_telemetry_if_due(time.monotonic())
                if eof and self._endpoint.dry_run and self._settings.stream.exit_on_eof_in_dry_run:
                    logger.info("CSV playback reached EOF; exiting dry-run")
                    return 0
                time.sleep(self._settings.stream.poll_interval_ms / 1000.0)
        except KeyboardInterrupt:
            logger.info("Interrupted by user")
            return 0
        finally:
            self._endpoint.close()

    def _process_rx(self) -> None:
        data = self._endpoint.read_available()
        if not data:
            return
        if self._settings.logging.hex_frames:
            logger.debug("RX bytes %s", protocol.hex_bytes(data))

        for byte in data:
            result = self._parser.push_byte(byte)
            if result.kind == protocol.ParseKind.FRAME and result.frame is not None:
                self._handle_frame(result.frame)
            elif result.kind == protocol.ParseKind.CRC_ERROR:
                self._state.crc_error_count = (self._state.crc_error_count + 1) & 0xFFFF
                logger.warning("Dropped frame with CRC error")
            elif result.kind in (protocol.ParseKind.LENGTH_ERROR, protocol.ParseKind.VERSION_ERROR):
                self._state.rx_error_count = (self._state.rx_error_count + 1) & 0xFFFF
                self._state.error_latched = True
                logger.warning("Dropped frame parse error: %s", result.kind.name)

    def _handle_frame(self, frame: protocol.Frame) -> None:
        logger.debug(
            "RX frame type=0x%02X seq=%d length=%d",
            frame.msg_type,
            frame.seq,
            frame.length,
        )
        if frame.msg_type == protocol.MSG_GET_PARAM_REQ:
            self._handle_get_param(frame)
        elif frame.msg_type == protocol.MSG_SET_PARAM_REQ:
            self._handle_set_param(frame)
        elif frame.msg_type == protocol.MSG_STREAM_CTRL_REQ:
            self._handle_stream_ctrl(frame)
        elif frame.msg_type == protocol.MSG_STATUS_REQ:
            self._handle_status_req(frame)
        else:
            self._send_error(frame.msg_type, frame.seq, protocol.STATUS_UNSUPPORTED, 0)

    def _handle_get_param(self, frame: protocol.Frame) -> None:
        object_id = protocol.read_u16_le(frame.payload) if frame.length >= 2 else 0
        if frame.length != 2:
            self._send_param_response(
                protocol.MSG_GET_PARAM_RESP,
                frame.seq,
                protocol.STATUS_BAD_LENGTH,
                object_id,
                None,
            )
            return

        status, value = self._params.get(object_id)
        logger.info("GET_PARAM object_id=%d status=0x%02X", object_id, status)
        self._send_param_response(protocol.MSG_GET_PARAM_RESP, frame.seq, status, object_id, value)

    def _handle_set_param(self, frame: protocol.Frame) -> None:
        object_id = protocol.read_u16_le(frame.payload) if frame.length >= 2 else 0
        if frame.length < 4:
            self._send_param_response(
                protocol.MSG_SET_PARAM_RESP,
                frame.seq,
                protocol.STATUS_BAD_LENGTH,
                object_id,
                None,
            )
            return

        value_type = frame.payload[2]
        value_len = frame.payload[3]
        if frame.length != 4 + value_len:
            self._send_param_response(
                protocol.MSG_SET_PARAM_RESP,
                frame.seq,
                protocol.STATUS_BAD_LENGTH,
                object_id,
                None,
            )
            return

        status, effective = self._params.set(object_id, value_type, frame.payload[4:])
        if status == protocol.STATUS_OK:
            self._state.config_changed_pending = True
        logger.info("SET_PARAM object_id=%d status=0x%02X", object_id, status)
        self._send_param_response(
            protocol.MSG_SET_PARAM_RESP, frame.seq, status, object_id, effective
        )

    def _handle_stream_ctrl(self, frame: protocol.Frame) -> None:
        if frame.length != 5:
            self._send_error(frame.msg_type, frame.seq, protocol.STATUS_BAD_LENGTH, 0)
            return

        command = frame.payload[0]
        arg = protocol.read_u32_le(frame.payload, 1)
        status = protocol.STATUS_OK
        now = time.monotonic()

        if command == protocol.STREAM_GET_STATE:
            pass
        elif command == protocol.STREAM_START:
            if arg != 0 and not self._period_valid(arg):
                status = protocol.STATUS_INVALID_VALUE
            else:
                self._start_stream(arg if arg != 0 else None, now)
        elif command == protocol.STREAM_STOP:
            self._state.stream_enabled = False
            logger.info("Stream stopped")
        elif command == protocol.STREAM_SET_PERIOD:
            if not self._period_valid(arg):
                status = protocol.STATUS_INVALID_VALUE
            else:
                self._state.stream_period_ms = int(arg)
                logger.info("Stream period set: %d ms", self._state.stream_period_ms)
        else:
            status = protocol.STATUS_UNSUPPORTED

        payload = bytes(
            [
                status & 0xFF,
                1 if self._state.stream_enabled else 0,
            ]
        ) + protocol.u16_le(self._state.stream_period_ms)
        self._send_frame(protocol.MSG_STREAM_CTRL_RESP, frame.seq, payload)
        logger.info(
            "STREAM_CTRL command=0x%02X arg=%d status=0x%02X enabled=%s period_ms=%d",
            command,
            arg,
            status,
            self._state.stream_enabled,
            self._state.stream_period_ms,
        )

    def _handle_status_req(self, frame: protocol.Frame) -> None:
        if frame.length != 0:
            self._send_error(frame.msg_type, frame.seq, protocol.STATUS_BAD_LENGTH, 0)
            return

        payload = bytearray()
        payload.extend(protocol.u32_le(self._build_status_flags()))
        payload.extend(protocol.u32_le(self._timestamp_ms()))
        payload.extend(protocol.u16_le(self._params.config_version))
        payload.extend(protocol.u16_le(self._state.rx_error_count))
        payload.extend(protocol.u16_le(self._state.crc_error_count))
        payload.extend(protocol.u16_le(self._state.tx_drop_count))
        self._send_frame(protocol.MSG_STATUS_RESP, frame.seq, bytes(payload))
        logger.info("STATUS_REQ responded")

    def _queue_telemetry_if_due(self, now: float) -> bool:
        if not self._state.stream_enabled:
            return False
        if now < self._state.next_stream_time:
            return False

        elapsed_sec = now - self._state.playback_start_time
        sample = self._source.sample_at(elapsed_sec)
        if sample is None:
            self._state.stream_enabled = False
            logger.info("Stream stopped at CSV EOF")
            return True

        payload = telemetry.build_pf_basic_payload(
            sample=sample,
            stream_seq=self._state.stream_seq,
            timestamp_ms=self._timestamp_ms(),
            config_version=self._params.config_version,
            config_changed=self._state.config_changed_pending,
            ma_output_ready=self._settings.stream.ma_output_ready,
        )
        frame = protocol.encode_frame(
            protocol.MSG_TELEMETRY,
            self._state.frame_seq,
            payload,
        )
        self._endpoint.write(frame)
        if self._settings.logging.hex_frames:
            logger.debug("TX telemetry %s", protocol.hex_bytes(frame))

        self._telemetry_sent += 1
        logger.debug(
            "TELEMETRY #%d stream_seq=%d frame_seq=%d ts=%s freq=%.3f power=%.3f",
            self._telemetry_sent,
            self._state.stream_seq,
            self._state.frame_seq,
            sample.timestamp,
            sample.freq_hz,
            sample.power_w,
        )
        self._state.stream_seq = (self._state.stream_seq + 1) & 0xFFFF
        self._state.frame_seq = (self._state.frame_seq + 1) & 0xFFFF
        self._state.config_changed_pending = False
        self._state.next_stream_time = now + self._state.stream_period_ms / 1000.0
        return False

    def _send_param_response(
        self,
        msg_type: int,
        seq: int,
        status: int,
        object_id: int,
        value,
    ) -> None:
        payload = build_param_response_payload(
            status=status,
            object_id=object_id,
            value=value,
            config_version=self._params.config_version,
        )
        self._send_frame(msg_type, seq, payload)

    def _send_error(self, related_type: int, related_seq: int, status: int, detail_code: int) -> None:
        payload = bytearray()
        payload.append(related_type & 0xFF)
        payload.extend(protocol.u16_le(related_seq))
        payload.append(status & 0xFF)
        payload.extend(protocol.u16_le(detail_code))
        self._state.error_latched = True
        self._send_frame(protocol.MSG_ERROR, related_seq, bytes(payload))
        logger.warning(
            "ERROR related_type=0x%02X seq=%d status=0x%02X detail=%d",
            related_type,
            related_seq,
            status,
            detail_code,
        )

    def _send_frame(self, msg_type: int, seq: int, payload: bytes) -> None:
        frame = protocol.encode_frame(msg_type, seq, payload)
        self._endpoint.write(frame)
        if self._settings.logging.hex_frames:
            logger.debug("TX frame type=0x%02X %s", msg_type, protocol.hex_bytes(frame))

    def _start_stream(self, period_ms: int | None, now: float) -> None:
        if period_ms is not None:
            self._state.stream_period_ms = int(period_ms)
        elif self._state.stream_period_ms == 0:
            self._state.stream_period_ms = self._settings.stream.default_period_ms
        self._state.stream_enabled = True
        self._state.next_stream_time = now
        self._state.playback_start_time = now
        logger.info("Stream started: period_ms=%d", self._state.stream_period_ms)

    def _build_status_flags(self) -> int:
        flags = 0
        latest_sample = self._source.latest_sample()
        if self._state.stream_enabled:
            flags |= protocol.STATUS_FLAG_STREAM_ENABLED
        if self._endpoint.rx_armed:
            flags |= protocol.STATUS_FLAG_RX_ARMED
        if latest_sample is not None:
            flags |= protocol.STATUS_FLAG_ADC_VALID | protocol.STATUS_FLAG_FREQ_VALID
        if self._settings.stream.ma_output_ready:
            flags |= protocol.STATUS_FLAG_MA_OUTPUT_READY
        if self._state.error_latched:
            flags |= protocol.STATUS_FLAG_ERROR_LATCHED
        return flags

    def _timestamp_ms(self) -> int:
        return int((time.monotonic() - self._state.boot_time) * 1000.0) & 0xFFFFFFFF

    def _period_valid(self, period_ms: int) -> bool:
        return self._settings.stream.min_period_ms <= period_ms <= self._settings.stream.max_period_ms


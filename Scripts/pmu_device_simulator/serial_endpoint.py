from __future__ import annotations

import logging
import sys

from . import protocol
from .config_model import SerialSettings


logger = logging.getLogger(__name__)

try:
    import serial
except ImportError:  # pragma: no cover
    serial = None


class SerialEndpoint:
    def __init__(self, settings: SerialSettings, log_hex_frames: bool) -> None:
        self._settings = settings
        self._log_hex_frames = log_hex_frames
        self._serial = None
        self._bytes_written = 0

        if settings.dry_run:
            logger.info("Serial dry-run is enabled; TX frames are printed as hex")
            return

        if serial is None:
            raise SystemExit("pyserial is not installed")

        self._serial = serial.Serial(
            port=settings.port,
            baudrate=settings.baudrate,
            bytesize=settings.bytesize,
            parity=settings.parity,
            stopbits=settings.stopbits,
            timeout=settings.timeout,
            write_timeout=settings.write_timeout,
        )
        logger.info(
            "Opened serial port %s baud=%d bytesize=%d parity=%s stopbits=%s",
            settings.port,
            settings.baudrate,
            settings.bytesize,
            settings.parity,
            settings.stopbits,
        )

    @property
    def rx_armed(self) -> bool:
        return True

    @property
    def dry_run(self) -> bool:
        return self._settings.dry_run

    def read_available(self) -> bytes:
        if self._serial is None:
            return b""
        waiting = self._serial.in_waiting
        if waiting <= 0:
            return b""
        return self._serial.read(waiting)

    def write(self, data: bytes) -> None:
        if self._serial is None:
            print(protocol.hex_bytes(data))
            sys.stdout.flush()
            self._bytes_written += len(data)
            return

        self._serial.write(data)
        self._bytes_written += len(data)
        if self._log_hex_frames:
            logger.debug("TX %s", protocol.hex_bytes(data))

    def close(self) -> None:
        if self._serial is not None:
            logger.info("Closing serial port after writing %d bytes", self._bytes_written)
            self._serial.close()


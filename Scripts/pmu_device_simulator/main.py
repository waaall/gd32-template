from __future__ import annotations

import pathlib
import sys


if __package__ in (None, ""):
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))
    __package__ = "pmu_device_simulator"

from .config_model import DEFAULT_CONFIG_PATH, load_settings
from .csv_source import CsvPlaybackSource
from .logging_setup import setup_logging
from .params import ParamStore
from .serial_endpoint import SerialEndpoint
from .simulator import PmuDeviceSimulator


def main() -> int:
    settings = load_settings(DEFAULT_CONFIG_PATH)
    logger = setup_logging(settings.logging)
    logger.info("Using config: %s", settings.config_path)
    logger.info("Using params: %s", settings.params.path)

    source = CsvPlaybackSource(settings.csv)
    params = ParamStore(settings.params.path)
    endpoint = SerialEndpoint(settings.serial, settings.logging.hex_frames)
    simulator = PmuDeviceSimulator(settings, source, params, endpoint)
    return simulator.run()


if __name__ == "__main__":
    raise SystemExit(main())


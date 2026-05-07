from __future__ import annotations

import logging

from .config_model import LoggingSettings


def setup_logging(settings: LoggingSettings) -> logging.Logger:
    level = getattr(logging, settings.level.upper(), logging.INFO)
    root = logging.getLogger()
    root.handlers.clear()
    root.setLevel(level)

    formatter = logging.Formatter(
        fmt="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S",
    )

    console = logging.StreamHandler()
    console.setLevel(level)
    console.setFormatter(formatter)
    root.addHandler(console)

    if settings.file is not None:
        settings.file.parent.mkdir(parents=True, exist_ok=True)
        file_handler = logging.FileHandler(settings.file, encoding="utf-8")
        file_handler.setLevel(level)
        file_handler.setFormatter(formatter)
        root.addHandler(file_handler)

    return logging.getLogger("pmu_device_simulator")


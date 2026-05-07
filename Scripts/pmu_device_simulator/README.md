# PMU Device Simulator

This folder simulates the PMU firmware side of the binary host protocol. It is
intended for testing host software without a real lower controller.

Run from the repository root:

```sh
python Scripts/test-primary-freq/pmu_device_simulator/main.py
```

All runtime options are configured in `config.json`; there is no CLI parameter
surface. Paths inside the JSON file are resolved relative to this folder.

## Behavior

- Parses and emits the same `A5 5A` binary frame format as `Modules/host_com`.
- Supports `GET_PARAM_REQ`, `SET_PARAM_REQ`, `STREAM_CTRL_REQ`, and
  `STATUS_REQ`.
- Sends `PF_BASIC` telemetry frames (`type=0x10`, 27-byte payload).
- Uses `params_state.json` as the simulated runtime parameter store. Successful
  `SET_PARAM_REQ` updates that JSON file and increments `config_version`.
- Waits for `STREAM_CTRL_REQ START` by default. Set `stream.auto_start` to
  `true` for standalone playback.

## Offline Data Timing

`csv.source_interval_sec` describes the original offline sample period. The
active stream period comes from `stream.default_period_ms` or the host
`STREAM_CTRL_REQ`.

For example, if the CSV source interval is `1.0` second and the stream period is
`500` ms, the simulator sends the same CSV row twice before moving to the next
row.

`csv.eof_behavior` controls what happens at the end of the CSV:

- `stop`: stop the stream.
- `hold`: keep sending the last row.
- `loop`: wrap back to the first loaded row.

## Logging

`logging.log_level` controls console and optional file logging. Set
`logging.log_file` to a relative or absolute path to also write logs to disk.
Set `logging.log_hex_frames` to `true` to include raw protocol frames in DEBUG
logs.

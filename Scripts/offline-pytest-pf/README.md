# 一次调频离线性能计算工具

本工具用于离线批量计算一次调频性能指标，算法移植自 C++ 实时计算模块：
- `app/src/PrimaryFrequency/Service/Realtime/IncrementalCalculator.cpp`
- `app/src/PrimaryFrequency/Service/Realtime/ActionStateMachine.cpp`

详细算法说明参见 `doc/phase2/realtime-calculation-flow.md`

## 依赖

仅需 Python 3.8+，无外部依赖（全部使用标准库）。

## 快速开始

```bash
# 基本用法
python main.py -i input.csv -o output.json

# 使用示例数据
python main.py -i sample_data/example_input.csv -o result.json

# 指定配置文件
python main.py -c config.json -i input.csv

# 不指定配置文件（默认读取脚本目录的 config.json）
python main.py -i input.csv

# 限定时间范围
python main.py -i sample_data/example_input.csv --start-time "2026-01-29 11:02:00" --end-time "2026-01-29 11:05:00"

# 覆盖参数
python main.py -i input.csv --rated-power 350 --dead-zone 0.033 --speed-ratio 0.05
```

说明：除非使用绝对路径，所有路径默认相对于脚本所在目录（`Scripts/offline-pytest-pf/`）解析。

## 命令行参数

### 基本参数
| 参数 | 说明 |
|------|------|
| `-c, --config` | 配置文件路径 (JSON)，默认使用脚本目录下的 `config.json` |
| `-i, --input` | 输入数据文件路径 (CSV)，未指定时使用配置文件中的 `io.inputFile` |
| `-o, --output` | 输出结果文件路径 (JSON) |

### 机组参数
| 参数 | 说明 | 默认值 |
|------|------|--------|
| `--unit-id` | 机组 ID | default |
| `--rated-power` | 额定功率 (MW) | 600.0 |
| `--speed-ratio` | 转速不等率 | 0.05 |
| `--limit-coefficient` | 出力限幅系数 | 0.06 |

### 全局参数
| 参数 | 说明 | 默认值 |
|------|------|--------|
| `--dead-zone` | 死区 (Hz) | 0.033 |
| `--end-hold-sec` | 动作结束保持时间 (秒) | 1 |
| `--windows-sec` | 窗口时长列表，逗号分隔 | 15,30,60 |

### 公式参数
| 参数 | 说明 | 默认值 |
|------|------|--------|
| `--delta-f-mode` | 频率偏差计算口径 (RatedFrequency/DeadZoneExcess) | DeadZoneExcess |
| `--initial-power-window` | 初始功率平均窗口 (秒) | 10 |
| `--qualification-cap` | 合格率上限 | 1.0 |

### CSV 列名
| 参数 | 说明 | 默认值 |
|------|------|--------|
| `--timestamp-col` | 时间戳列名 | Timestamp |
| `--frequency-col` | 频率列名 | Freq |
| `--power-col` | 功率列名 | Power |
| `--quality-col` | 质量码列名 | quality |
| `--timestamp-format` | 时间戳格式 | %Y-%m-%d %H:%M:%S |
 

### 时间范围
| 参数 | 说明 |
|------|------|
| `--start-time` | 起始时间（仅处理该时间及之后的数据） |
| `--end-time` | 结束时间（仅处理该时间及之前的数据） |

时间字符串支持 `--timestamp-format` 指定的格式或 ISO 8601。

### 日志参数
| 参数 | 说明 | 默认值 |
|------|------|--------|
| `--log-level` | 日志级别 (DEBUG/INFO/WARNING/ERROR) | INFO |
| `--log-file` | 日志文件路径 | 无 |

## 输入格式 (CSV)

```csv
Timestamp,Power,Freq,quality
2026-01-29 11:02:21,232.780,49.970,0
2026-01-29 11:02:22,232.742,49.968,0
```

| 列名 | 类型 | 说明 |
|------|------|------|
| Timestamp | string | 时间戳，格式可配置 |
| Freq | float | 频率 (Hz) |
| Power | float | 功率 (MW) |
| quality | int | 质量码：0=good, 1=bad, 2=unknown（可选） |

## 输出格式 (JSON)

```json
{
  "summary": {
    "totalActions": 1,
    "shortActions": 0,
    "summaryTotal": 1,
    "qualifiedActions": 0,
    "qualificationRate": 0.0
  },
  "actions": [
    {
      "unitId": "default",
      "startTime": "2026-01-29T11:02:32",
      "endTime": "2026-01-29T11:02:52",
      "duration": 20,
      "effectiveDuration": 14,
      "finalWindowSec": 14,
      "isShortAction": true,
      "isLargeDisturbance": false,
      "direction": "PowerUp",
      "initialPower": 232.66,
      "maxFreqDeviation": -0.065,
      "windowMetrics": {
        "15": {
          "windowSec": 15,
          "response": 0.31,
          "energyContrib": 0.24,
          "qualifiedRate": 0.28,
          "isCompleted": false
        }
      },
      "latestMetrics": {
        "response": 0.31,
        "energyContrib": 0.24,
        "qualifiedRate": 0.28
      }
    }
  ]
}
```

同时会生成 `.detail.csv` 文件，包含秒级详细数据。

## 核心算法

### 1. 起始出力计算
- 取动作前 N 秒（默认 10 秒）平均功率
- 仅使用 `quality == 0` 的样本
- 样本不足时用可用样本均值
- 无有效样本时回退到触发瞬时功率

### 2. 频率偏差口径

**RatedFrequency 口径**：
```
Δf = frequency - 50.0
```

**DeadZoneExcess 口径**（默认）：
```
if |frequency - 50.0| <= dead_zone:
    Δf = 0.0
else:
    excess = |frequency - 50.0| - dead_zone
    Δf = excess if frequency > 50.0 else -excess
```

### 3. 理论调整量
```
ΔP_theory = -(Δf × Pn) / (fn × R)
限幅: ±(limit_coefficient × Pn)
```

### 4. 同向贡献判定
```python
def directional_magnitude(actual, theoretical):
    if abs(theoretical) < ε or actual × theoretical <= 0:
        return 0.0
    return abs(actual)
```

### 5. 梯形积分
```
sum = (v0 + vN)/2 + Σ(v1..vN-1)
integral = sum × dt
```

### 6. 窗口指标
- **响应指数** = max_actual / max_theoretical
- **电量贡献** = integral_actual / integral_theoretical
- **合格率** = (min(response, cap) + min(energy, cap)) / 2

## 配置文件示例

参见 `config.json`：

```json
{
  "unit": {
    "unitId": "default",
    "ratedPower": 600.0,
    "speedRatio": 0.05,
    "limitCoefficient": null
  },
  "global": {
    "deadZone": 0.033,
    "ratedFrequency": 50.0,
    "endHoldSec": 1,
    "windowsSec": [15, 30, 60]
  },
  "formula": {
    "deltaFMode": "DeadZoneExcess",
    "initialPowerAvgWindowSec": 10,
    "qualificationCap": 1.0,
    "speedRatioFallback": 0.05,
    "largeDisturbanceThreshold": 0.06
  },
  "io": {
    "inputFile": "input.csv",
    "outputFile": "output.json",
    "startTime": null,
    "endTime": null,
    "timestampFormat": "%Y-%m-%d %H:%M:%S",
    "timestampColumn": "Timestamp",
    "frequencyColumn": "Freq",
    "powerColumn": "Power",
    "qualityColumn": "quality"
  },
  "logging": {
    "level": "INFO",
    "logFile": null
  },
  "includeShortActionsInSummary": false
}
```

## 目录结构

```
tests/offline-pytest-pf/
├── README.md               # 使用说明
├── requirements.txt        # Python 依赖
├── config.json             # 默认配置
├── main.py                 # 主入口脚本
├── pf_offline/             # 核心模块包
│   ├── __init__.py
│   ├── config.py           # 配置加载
│   ├── io_handler.py       # 数据输入输出
│   ├── calculator.py       # 核心计算（移植 IncrementalCalculator）
│   ├── action_detector.py  # 动作检测（移植 ActionStateMachine）
│   ├── metrics.py          # 数据结构定义
│   └── logger_setup.py     # 日志配置
└── sample_data/
    └── example_input.csv   # 示例输入
```

## 参考

- `app/src/PrimaryFrequency/Service/Realtime/IncrementalCalculator.cpp` - 计算逻辑
- `app/src/PrimaryFrequency/Service/Realtime/ActionStateMachine.cpp` - 状态机
- `app/src/PrimaryFrequency/Service/Realtime/ActionContext.h` - 数据结构
- `doc/phase2/realtime-calculation-flow.md` - 算法文档

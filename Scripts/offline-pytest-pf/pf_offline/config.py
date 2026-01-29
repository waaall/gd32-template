# -*- coding: utf-8 -*-
"""
配置加载与命令行解析

支持从 JSON 文件加载配置，并通过命令行参数覆盖
"""

import argparse
import json
import logging
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import List, Optional

logger = logging.getLogger(__name__)


class DeltaFMode(Enum):
    """频率偏差计算口径"""
    RATED_FREQUENCY = "RatedFrequency"  # Δf = f - 50.0
    DEAD_ZONE_EXCESS = "DeadZoneExcess" # Δf = sign * (|f - 50| - deadZone)


@dataclass
class UnitConfig:
    """机组配置"""
    unit_id: str = "default"
    rated_power: float = 600.0        # 额定功率 (MW)
    speed_ratio: float = 0.05         # 转速不等率
    limit_coefficient: Optional[float] = None  # 出力限幅系数（None 时使用 0.06）

    def get_limit_coefficient(self) -> float:
        """获取出力限幅系数，按额定功率分档"""
        if self.limit_coefficient is not None:
            return self.limit_coefficient
        if self.rated_power >= 600:
            return 0.06
        if self.rated_power >= 350:
            return 0.08
        if self.rated_power >= 100:
            return 0.10
        return 0.08


@dataclass
class GlobalConfig:
    """全局配置"""
    dead_zone: float = 0.033          # 死区 (Hz)
    rated_frequency: float = 50.0     # 额定频率 (Hz)
    end_hold_sec: int = 1             # 动作结束保持时间 (秒)
    windows_sec: List[int] = field(default_factory=lambda: [15, 30, 60])


@dataclass
class FormulaProfile:
    """公式参数配置"""
    delta_f_mode: DeltaFMode = DeltaFMode.DEAD_ZONE_EXCESS
    initial_power_avg_window_sec: int = 10    # 初始功率平均窗口 (秒)
    qualification_cap: float = 1.0            # 合格率上限
    qualification_threshold_small: float = 0.8 # 小扰动合格率阈值
    qualification_threshold_large: float = 0.8 # 大扰动合格率阈值
    speed_ratio_fallback: float = 0.05        # 转速不等率兜底值
    large_disturbance_threshold: float = 0.06 # 大扰动阈值 (Hz)


@dataclass
class IOConfig:
    """输入输出配置"""
    input_file: str = "input.csv"
    output_file: str = "output.json"
    timestamp_format: str = "%Y-%m-%d %H:%M:%S"
    timestamp_column: str = "Timestamp"
    frequency_column: str = "Freq"
    power_column: str = "Power"
    quality_column: str = "quality"
    start_time: Optional[str] = None
    end_time: Optional[str] = None


@dataclass
class LogConfig:
    """日志配置"""
    level: str = "INFO"
    log_file: Optional[str] = None


@dataclass
class Config:
    """完整配置"""
    unit: UnitConfig = field(default_factory=UnitConfig)
    global_cfg: GlobalConfig = field(default_factory=GlobalConfig)
    formula: FormulaProfile = field(default_factory=FormulaProfile)
    io: IOConfig = field(default_factory=IOConfig)
    logging: LogConfig = field(default_factory=LogConfig)

    # 是否在汇总中包含短动作
    include_short_actions_in_summary: bool = False


def load_config(config_path: Optional[str] = None) -> Config:
    """从 JSON 文件加载配置"""
    config = Config()

    if config_path and Path(config_path).exists():
        with open(config_path, 'r', encoding='utf-8') as f:
            data = json.load(f)

        # 机组配置
        if 'unit' in data:
            u = data['unit']
            config.unit.unit_id = u.get('unitId', config.unit.unit_id)
            config.unit.rated_power = u.get('ratedPower', config.unit.rated_power)
            config.unit.speed_ratio = u.get('speedRatio', config.unit.speed_ratio)
            config.unit.limit_coefficient = u.get('limitCoefficient')

        # 全局配置
        if 'global' in data:
            g = data['global']
            config.global_cfg.dead_zone = g.get('deadZone', config.global_cfg.dead_zone)
            config.global_cfg.rated_frequency = g.get('ratedFrequency', config.global_cfg.rated_frequency)
            config.global_cfg.end_hold_sec = g.get('endHoldSec', config.global_cfg.end_hold_sec)
            config.global_cfg.windows_sec = g.get('windowsSec', config.global_cfg.windows_sec)

        # 公式参数
        if 'formula' in data:
            f = data['formula']
            mode_str = f.get('deltaFMode', 'DeadZoneExcess')
            if mode_str == 'RatedFrequency':
                config.formula.delta_f_mode = DeltaFMode.RATED_FREQUENCY
            else:
                config.formula.delta_f_mode = DeltaFMode.DEAD_ZONE_EXCESS
            config.formula.initial_power_avg_window_sec = f.get(
                'initialPowerAvgWindowSec', config.formula.initial_power_avg_window_sec)
            config.formula.qualification_cap = f.get(
                'qualificationCap', config.formula.qualification_cap)
            config.formula.qualification_threshold_small = f.get(
                'qualificationThresholdSmall', config.formula.qualification_threshold_small)
            config.formula.qualification_threshold_large = f.get(
                'qualificationThresholdLarge', config.formula.qualification_threshold_large)
            config.formula.speed_ratio_fallback = f.get(
                'speedRatioFallback', config.formula.speed_ratio_fallback)
            config.formula.large_disturbance_threshold = f.get(
                'largeDisturbanceThreshold', config.formula.large_disturbance_threshold)

        # IO 配置
        if 'io' in data:
            io = data['io']
            config.io.input_file = io.get('inputFile', config.io.input_file)
            config.io.output_file = io.get('outputFile', config.io.output_file)
            config.io.timestamp_format = io.get('timestampFormat', config.io.timestamp_format)
            config.io.timestamp_column = io.get('timestampColumn', config.io.timestamp_column)
            config.io.frequency_column = io.get('frequencyColumn', config.io.frequency_column)
            config.io.power_column = io.get('powerColumn', config.io.power_column)
            config.io.quality_column = io.get('qualityColumn', config.io.quality_column)
            config.io.start_time = io.get('startTime', config.io.start_time)
            config.io.end_time = io.get('endTime', config.io.end_time)

        # 日志配置
        if 'logging' in data:
            lg = data['logging']
            config.logging.level = lg.get('level', config.logging.level)
            config.logging.log_file = lg.get('logFile')

        # 其他
        config.include_short_actions_in_summary = data.get(
            'includeShortActionsInSummary', config.include_short_actions_in_summary)

        logger.info(f"已加载配置文件: {config_path}")

    return config


def parse_args() -> argparse.Namespace:
    """解析命令行参数"""
    parser = argparse.ArgumentParser(
        description='一次调频离线性能计算工具',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  python main.py -i input.csv -o output.json
  python main.py -c config.json -i input.csv
  python main.py -i input.csv --rated-power 350 --dead-zone 0.033
        """
    )

    # 基本参数
    parser.add_argument('-c', '--config', type=str, default=None,
                        help='配置文件路径 (JSON)')
    parser.add_argument('-i', '--input', type=str, default=None,
                        help='输入数据文件路径 (CSV)')
    parser.add_argument('-o', '--output', type=str, default=None,
                        help='输出结果文件路径 (JSON)')

    # 机组参数覆盖
    parser.add_argument('--unit-id', type=str, default=None,
                        help='机组 ID')
    parser.add_argument('--rated-power', type=float, default=None,
                        help='额定功率 (MW)')
    parser.add_argument('--speed-ratio', type=float, default=None,
                        help='转速不等率')
    parser.add_argument('--limit-coefficient', type=float, default=None,
                        help='出力限幅系数')

    # 全局参数覆盖
    parser.add_argument('--dead-zone', type=float, default=None,
                        help='死区 (Hz)')
    parser.add_argument('--end-hold-sec', type=int, default=None,
                        help='动作结束保持时间 (秒)')
    parser.add_argument('--windows-sec', type=str, default=None,
                        help='窗口时长列表，逗号分隔，如 "15,30,60"')

    # 公式参数覆盖
    parser.add_argument('--delta-f-mode', type=str, choices=['RatedFrequency', 'DeadZoneExcess'],
                        default=None, help='频率偏差计算口径')
    parser.add_argument('--initial-power-window', type=int, default=None,
                        help='初始功率平均窗口 (秒)')
    parser.add_argument('--qualification-cap', type=float, default=None,
                        help='合格率上限')
    parser.add_argument('--qualification-threshold-small', type=float, default=None,
                        help='小扰动合格率阈值')
    parser.add_argument('--qualification-threshold-large', type=float, default=None,
                        help='大扰动合格率阈值')

    # 日志参数
    parser.add_argument('--log-level', type=str, choices=['DEBUG', 'INFO', 'WARNING', 'ERROR'],
                        default=None, help='日志级别')
    parser.add_argument('--log-file', type=str, default=None,
                        help='日志文件路径')

    # CSV 列名
    parser.add_argument('--timestamp-col', type=str, default=None,
                        help='时间戳列名')
    parser.add_argument('--frequency-col', type=str, default=None,
                        help='频率列名')
    parser.add_argument('--power-col', type=str, default=None,
                        help='功率列名')
    parser.add_argument('--quality-col', type=str, default=None,
                        help='质量码列名')
    parser.add_argument('--timestamp-format', type=str, default=None,
                        help='时间戳格式，如 "%%Y-%%m-%%d %%H:%%M:%%S"')
    parser.add_argument('--start-time', type=str, default=None,
                        help='起始时间（仅处理该时间及之后的数据）')
    parser.add_argument('--end-time', type=str, default=None,
                        help='结束时间（仅处理该时间及之前的数据）')

    return parser.parse_args()


def apply_args_to_config(config: Config, args: argparse.Namespace) -> Config:
    """将命令行参数应用到配置"""
    # 机组参数
    if args.unit_id is not None:
        config.unit.unit_id = args.unit_id
    if args.rated_power is not None:
        config.unit.rated_power = args.rated_power
    if args.speed_ratio is not None:
        config.unit.speed_ratio = args.speed_ratio
    if args.limit_coefficient is not None:
        config.unit.limit_coefficient = args.limit_coefficient

    # 全局参数
    if args.dead_zone is not None:
        config.global_cfg.dead_zone = args.dead_zone
    if args.end_hold_sec is not None:
        config.global_cfg.end_hold_sec = args.end_hold_sec
    if args.windows_sec is not None:
        config.global_cfg.windows_sec = [int(x.strip()) for x in args.windows_sec.split(',')]

    # 公式参数
    if args.delta_f_mode is not None:
        if args.delta_f_mode == 'RatedFrequency':
            config.formula.delta_f_mode = DeltaFMode.RATED_FREQUENCY
        else:
            config.formula.delta_f_mode = DeltaFMode.DEAD_ZONE_EXCESS
    if args.initial_power_window is not None:
        config.formula.initial_power_avg_window_sec = args.initial_power_window
    if args.qualification_cap is not None:
        config.formula.qualification_cap = args.qualification_cap
    if args.qualification_threshold_small is not None:
        config.formula.qualification_threshold_small = args.qualification_threshold_small
    if args.qualification_threshold_large is not None:
        config.formula.qualification_threshold_large = args.qualification_threshold_large

    # 日志参数
    if args.log_level is not None:
        config.logging.level = args.log_level
    if args.log_file is not None:
        config.logging.log_file = args.log_file

    # IO 参数
    if args.input is not None:
        config.io.input_file = args.input
    if args.output is not None:
        config.io.output_file = args.output
    if args.timestamp_col is not None:
        config.io.timestamp_column = args.timestamp_col
    if args.frequency_col is not None:
        config.io.frequency_column = args.frequency_col
    if args.power_col is not None:
        config.io.power_column = args.power_col
    if args.quality_col is not None:
        config.io.quality_column = args.quality_col
    if args.timestamp_format is not None:
        config.io.timestamp_format = args.timestamp_format
    if args.start_time is not None:
        config.io.start_time = args.start_time
    if args.end_time is not None:
        config.io.end_time = args.end_time
    return config

# -*- coding: utf-8 -*-
"""
数据结构定义

移植自 ActionContext.h 中的 SecondSample、WindowMetrics、ActionContext 结构
"""

from dataclasses import dataclass, field
from datetime import datetime
from typing import Dict, List, Optional
from enum import Enum


class Direction(Enum):
    """调节方向枚举"""
    POWER_UP = "PowerUp"      # 低频升出力
    POWER_DOWN = "PowerDown"  # 高频降出力


@dataclass
class SecondSample:
    """
    单点采样数据结构
    用于记录每一秒的原始数据及计算出的增量数据
    """
    timestamp: Optional[datetime] = None
    frequency: float = 0.0
    power: float = 0.0
    delta_p_theory: float = 0.0   # 理论调整量
    delta_p_actual: float = 0.0   # 实际调整量
    is_effective: bool = True     # 是否为有效样本（死区内为 False）
    quality: int = 2              # 数据质量码：0=good, 1=bad, 2=unknown


@dataclass
class WindowMetrics:
    """
    窗口指标结构
    用于存储特定时间窗口（如15s, 30s）内的聚合计算结果
    """
    window_sec: int = 0                     # 窗口时长（秒）
    max_actual_adjustment: float = 0.0      # 最大实际调整量
    max_theoretical_adjustment: float = 0.0 # 最大理论调整量
    energy_actual: float = 0.0              # 实际调节电量积分
    energy_theoretical: float = 0.0         # 理论调节电量积分
    sample_count: int = 0                   # 包含的样本数
    last_updated_at: Optional[datetime] = None   # 最后更新时间
    response: float = 0.0                   # 响应指数
    energy_contrib: float = 0.0             # 电量贡献指数
    qualified_rate: float = 0.0             # 合格率
    is_completed: bool = False              # 窗口是否已结束
    completed_at: Optional[datetime] = None # 窗口结束时间

    def to_dict(self) -> dict:
        """转换为字典，便于 JSON 序列化"""
        return {
            'windowSec': self.window_sec,
            'maxActualAdjustment': round(self.max_actual_adjustment, 4),
            'maxTheoreticalAdjustment': round(self.max_theoretical_adjustment, 4),
            'energyActual': round(self.energy_actual, 6),
            'energyTheoretical': round(self.energy_theoretical, 6),
            'sampleCount': self.sample_count,
            'response': round(self.response, 6),
            'energyContrib': round(self.energy_contrib, 6),
            'qualifiedRate': round(self.qualified_rate, 6),
            'isCompleted': self.is_completed,
        }


@dataclass
class ActionContext:
    """
    动作上下文结构
    存储一次完整调频动作的所有相关信息
    """
    unit_id: str = ""
    start_time: Optional[datetime] = None
    end_time: Optional[datetime] = None
    duration: int = 0                       # 持续时长（秒）
    effective_duration: int = 0             # 有效持续时长（秒）
    final_window_sec: int = 0               # 最终归属的考核窗口时长
    is_short_action: bool = False           # 是否为短动作
    is_large_disturbance: bool = False      # 是否为大扰动
    direction: Direction = Direction.POWER_UP   # 调节方向
    initial_power: float = 0.0              # 初始功率
    max_freq_deviation: float = 0.0         # 最大频差
    max_freq_deviation_time: Optional[datetime] = None  # 最大频差时刻
    per_second_series: List[SecondSample] = field(default_factory=list)
    window_metrics: Dict[int, WindowMetrics] = field(default_factory=dict)
    missing_samples: int = 0                # 缺失样本数
    has_latest_metrics: bool = False        # 是否包含最新时刻指标
    latest_metrics: Optional[WindowMetrics] = None  # 最新时刻指标

    def to_dict(self) -> dict:
        """转换为字典，便于 JSON 序列化"""
        result = {
            'unitId': self.unit_id,
            'startTime': self.start_time.isoformat() if self.start_time else None,
            'endTime': self.end_time.isoformat() if self.end_time else None,
            'duration': self.duration,
            'effectiveDuration': self.effective_duration,
            'finalWindowSec': self.final_window_sec,
            'isShortAction': self.is_short_action,
            'isLargeDisturbance': self.is_large_disturbance,
            'direction': self.direction.value,
            'initialPower': round(self.initial_power, 4),
            'maxFreqDeviation': round(self.max_freq_deviation, 6),
            'windowMetrics': {},
        }

        # 窗口指标
        for window_sec, metrics in sorted(self.window_metrics.items()):
            result['windowMetrics'][str(window_sec)] = metrics.to_dict()

        # 短动作的最新指标
        if self.has_latest_metrics and self.latest_metrics:
            result['latestMetrics'] = self.latest_metrics.to_dict()

        return result

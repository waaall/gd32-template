# -*- coding: utf-8 -*-
"""
核心计算逻辑

移植自 IncrementalCalculator.cpp，实现一次调频性能指标的计算
"""

import logging
from datetime import datetime
from typing import Dict, List, Optional, Tuple

from .config import DeltaFMode, FormulaProfile, GlobalConfig, UnitConfig
from .metrics import SecondSample, WindowMetrics

logger = logging.getLogger(__name__)

# 数值稳定性阈值：用于避免除零/符号噪声
EPSILON = 1e-6


def directional_magnitude(actual: float, theoretical: float) -> float:
    """
    计算"同向"实际调整量
    仅当实际调整方向与理论方向一致时，才计入调整量；否则记为 0
    """
    if abs(theoretical) < EPSILON:
        return 0.0
    if actual * theoretical <= 0.0:
        return 0.0
    return abs(actual)


class IncrementalCalculator:
    """
    增量计算器

    核心逻辑：接收实时采样点，按内置公式计算理论值，
    并根据配置的窗口更新积分、响应指数等指标
    """

    def __init__(self):
        self._unit_config: Optional[UnitConfig] = None
        self._profile: Optional[FormulaProfile] = None
        self._windows_sec: List[int] = []
        self._window_metrics: Dict[int, WindowMetrics] = {}
        self._samples: List[SecondSample] = []
        self._initial_power: float = 0.0
        self._speed_ratio: float = 0.05
        self._qualification_cap: float = 1.0
        self._rated_frequency: float = 50.0
        self._dead_zone: float = 0.033
        self._delta_f_mode: DeltaFMode = DeltaFMode.DEAD_ZONE_EXCESS

    def reset(
        self,
        unit_config: UnitConfig,
        profile: FormulaProfile,
        windows_sec: List[int],
        initial_power: float,
        dead_zone: float
    ) -> None:
        """
        重置计算器

        在新动作开始时调用，初始化所有参数、清空样本和窗口指标
        """
        self._unit_config = unit_config
        self._profile = profile
        self._windows_sec = sorted(windows_sec)
        self._window_metrics = {}

        # 初始化各窗口结构
        for window_sec in self._windows_sec:
            metrics = WindowMetrics()
            metrics.window_sec = window_sec
            self._window_metrics[window_sec] = metrics

        self._samples = []
        # 初始功率：来自动作前窗口平均功率（由状态机在 reset 前确定）
        self._initial_power = initial_power

        # 转速不等率：优先使用机组配置，缺失时采用公式配置或默认值
        self._speed_ratio = unit_config.speed_ratio
        if abs(self._speed_ratio) < EPSILON:
            self._speed_ratio = (profile.speed_ratio_fallback
                                if profile.speed_ratio_fallback > 0.0 else 0.05)

        self._qualification_cap = (profile.qualification_cap
                                  if profile.qualification_cap > 0.0 else 1.0)
        self._rated_frequency = 50.0
        self._dead_zone = max(0.0, dead_zone)
        self._delta_f_mode = profile.delta_f_mode

    def add_sample(
        self,
        timestamp: Optional[datetime],
        frequency: float,
        power: float,
        quality: int,
        is_effective: bool
    ) -> SecondSample:
        """
        添加采样点

        核心入口：接收新数据，计算瞬时的理论/实际调整量，并存入样本序列
        """
        # 计算瞬时理论值和实际值
        freq_deviation = self._resolve_frequency_deviation(frequency)
        theoretical_adjustment = self._calc_theoretical_adjustment(freq_deviation)
        actual_adjustment = power - self._initial_power

        sample = SecondSample(
            timestamp=timestamp,
            frequency=frequency,
            power=power,
            delta_p_theory=theoretical_adjustment,
            delta_p_actual=actual_adjustment,
            is_effective=is_effective,
            quality=quality
        )

        self._samples.append(sample)
        return sample

    def update_all_windows(self) -> None:
        """
        更新所有窗口的统计指标

        遍历所有配置的窗口，重新计算截至当前的累积指标
        """
        if not self._samples:
            return

        elapsed_sec = self.elapsed_seconds(effective_only=True)
        last_time = self._samples[-1].timestamp

        for window_sec in self._windows_sec:
            metrics = self._window_metrics.get(window_sec, WindowMetrics())
            # 已完成的窗口不再更新
            if metrics.is_completed:
                continue

            # 确定当前窗口包含的样本范围
            window_end_index = self._resolve_window_end_index(window_sec, effective_only=True)
            computed = self._compute_metrics(window_end_index)
            computed.window_sec = window_sec

            # 统计有效样本数
            effective_count = sum(
                1 for i in range(window_end_index) if self._samples[i].is_effective
            )
            computed.sample_count = effective_count
            computed.last_updated_at = last_time

            # 检查窗口时间是否到达
            if window_sec > 0 and elapsed_sec >= window_sec:
                computed.is_completed = True
                computed.completed_at = last_time

            self._window_metrics[window_sec] = computed

    def get_window_metrics(self, window_sec: int) -> WindowMetrics:
        """获取指定窗口的当前指标"""
        return self._window_metrics.get(window_sec, WindowMetrics())

    def latest_metrics(self) -> WindowMetrics:
        """
        计算"最新一刻"的指标（用于短动作）

        以已运行的秒数作为"动态窗口秒数"
        """
        if not self._samples:
            return WindowMetrics()

        elapsed_sec = self.elapsed_seconds(effective_only=True)
        window_end_index = self._resolve_window_end_index(elapsed_sec, effective_only=True)
        metrics = self._compute_metrics(window_end_index)
        metrics.window_sec = elapsed_sec

        effective_count = sum(
            1 for i in range(window_end_index) if self._samples[i].is_effective
        )
        metrics.sample_count = effective_count
        metrics.last_updated_at = self._samples[-1].timestamp

        return metrics

    @property
    def samples(self) -> List[SecondSample]:
        """返回样本序列"""
        return self._samples

    @property
    def window_metrics(self) -> Dict[int, WindowMetrics]:
        """返回所有窗口指标"""
        return self._window_metrics

    @property
    def initial_power(self) -> float:
        """返回初始功率"""
        return self._initial_power

    @property
    def rated_frequency(self) -> float:
        """返回额定频率"""
        return self._rated_frequency

    def elapsed_seconds(self, effective_only: bool = False) -> int:
        """
        计算当前累计秒数

        优先使用时间戳计算真实持续时长；若不可用则退化为采样点计数
        """
        if not self._samples:
            return 0

        start = self._samples[0].timestamp
        end = self._samples[-1].timestamp
        sample_seconds = max(1, len(self._samples))

        # 非有效限定模式：优先时间戳
        if not effective_only and start and end:
            secs = int((end - start).total_seconds())
            return max(sample_seconds, max(0, secs))

        if not effective_only:
            return sample_seconds

        # 有效限定模式
        max_points = len(self._samples)
        if max_points < 2:
            return 1 if self._samples[0].is_effective else 0

        # 检查时间戳有效性
        timestamps_valid = True
        has_effective_pair = False
        effective_count = sum(1 for s in self._samples if s.is_effective)

        for i in range(1, max_points):
            prev = self._samples[i - 1]
            curr = self._samples[i]
            if not prev.is_effective or not curr.is_effective:
                continue
            has_effective_pair = True

            if (not prev.timestamp or not curr.timestamp or
                (curr.timestamp - prev.timestamp).total_seconds() <= 0):
                timestamps_valid = False
                break

        if timestamps_valid and has_effective_pair:
            effective_elapsed = 0.0
            for i in range(1, max_points):
                prev = self._samples[i - 1]
                curr = self._samples[i]
                if not prev.is_effective or not curr.is_effective:
                    continue
                dt = (curr.timestamp - prev.timestamp).total_seconds()
                effective_elapsed += dt
            return max(0, int(effective_elapsed))

        return max(0, effective_count)

    def _resolve_window_end_index(self, window_sec: int, effective_only: bool = False) -> int:
        """
        内部逻辑：确定指定窗口时长对应的样本结束索引
        """
        max_points = len(self._samples)
        if max_points <= 0:
            return 0

        if window_sec <= 0:
            return max_points

        start = self._samples[0].timestamp

        if not effective_only and not start:
            # 无时间戳时按"秒==点"退化处理
            return min(window_sec, max_points)

        if not effective_only:
            window_ms = window_sec * 1000
            for i in range(max_points):
                ts = self._samples[i].timestamp
                if not ts:
                    return min(window_sec, max_points)
                elapsed = (ts - start).total_seconds() * 1000
                if elapsed < 0:
                    return min(window_sec, max_points)
                # 找到第一个超过窗口末时刻的样本索引
                if elapsed > window_ms:
                    return i
            return max_points

        # effective_only 模式
        timestamps_valid = True
        has_effective_pair = False

        for i in range(1, max_points):
            prev = self._samples[i - 1]
            curr = self._samples[i]
            if not prev.is_effective or not curr.is_effective:
                continue
            has_effective_pair = True

            if (not prev.timestamp or not curr.timestamp or
                (curr.timestamp - prev.timestamp).total_seconds() <= 0):
                timestamps_valid = False
                break

        if not timestamps_valid or not has_effective_pair:
            # 退化为有效样本计数
            effective_count = 0
            for i in range(max_points):
                if not self._samples[i].is_effective:
                    continue
                effective_count += 1
                if effective_count > window_sec:
                    return i
            return max_points

        # 基于时间戳计算有效累计时间
        effective_elapsed = 0.0
        for i in range(1, max_points):
            prev = self._samples[i - 1]
            curr = self._samples[i]
            if not prev.is_effective or not curr.is_effective:
                continue

            dt = (curr.timestamp - prev.timestamp).total_seconds()
            effective_elapsed += dt

            if effective_elapsed > float(window_sec):
                return i

        return max_points

    def _compute_metrics(self, window_end_index: int) -> WindowMetrics:
        """
        核心计算逻辑：计算窗口内的积分和比率
        """
        metrics = WindowMetrics()
        if not self._samples or window_end_index <= 0:
            return metrics

        max_points = min(window_end_index, len(self._samples))
        max_actual = 0.0
        max_theoretical = 0.0

        directional_actual: List[float] = []
        directional_theoretical: List[float] = []

        # 遍历样本：计算极值，并构造"同向贡献"的积分序列
        for i in range(max_points):
            sample = self._samples[i]
            theoretical = sample.delta_p_theory

            if sample.is_effective:
                max_theoretical = max(max_theoretical, abs(theoretical))

            # 实际调整仅统计与理论方向一致的部分
            directional = directional_magnitude(sample.delta_p_actual, theoretical)

            if sample.is_effective:
                max_actual = max(max_actual, directional)

            directional_actual.append(directional if sample.is_effective else 0.0)
            directional_theoretical.append(abs(theoretical) if sample.is_effective else 0.0)

        metrics.max_actual_adjustment = max_actual
        metrics.max_theoretical_adjustment = max_theoretical

        # 计算响应指数：Max(实际同向调整) / Max(理论调整)
        response_raw = (0.0 if max_theoretical < EPSILON
                       else max_actual / max_theoretical)
        metrics.response = max(0.0, response_raw)

        # 计算电量积分：对"同向实际调整"和"理论调整"做时间积分
        integral_actual = self._compute_integral(directional_actual, max_points, effective_only=True)
        integral_theoretical = self._compute_integral(directional_theoretical, max_points, effective_only=True)
        metrics.energy_actual = integral_actual
        metrics.energy_theoretical = integral_theoretical

        # 计算电量贡献指数：Integral(实际同向) / Integral(理论)
        energy_raw = 0.0
        if integral_theoretical > EPSILON:
            energy_raw = integral_actual / integral_theoretical
        metrics.energy_contrib = max(0.0, min(energy_raw, self._qualification_cap))

        # 计算合格率：响应指数与贡献指数的均值（两者均受上限约束）
        response_qualified_rate = min(response_raw, self._qualification_cap)
        energy_qualified_rate = min(energy_raw, self._qualification_cap)
        metrics.qualified_rate = (response_qualified_rate + energy_qualified_rate) / 2.0

        return metrics

    def _compute_integral(
        self,
        values: List[float],
        window_end_index: int,
        effective_only: bool = False
    ) -> float:
        """
        内部逻辑：积分计算（梯形法则）
        """
        point_count = min(window_end_index, len(values))
        if point_count < 2:
            return 0.0

        if not effective_only:
            return self._trapezoidal_integral(values, point_count)

        # effective_only 模式：检查时间戳有效性
        timestamps_valid = True
        for i in range(1, point_count):
            prev = self._samples[i - 1]
            curr = self._samples[i]
            if not prev.is_effective or not curr.is_effective:
                continue
            if (not prev.timestamp or not curr.timestamp or
                (curr.timestamp - prev.timestamp).total_seconds() <= 0):
                timestamps_valid = False
                break

        total = 0.0
        if timestamps_valid:
            for i in range(1, point_count):
                prev = self._samples[i - 1]
                curr = self._samples[i]
                if not prev.is_effective or not curr.is_effective:
                    continue
                dt = (curr.timestamp - prev.timestamp).total_seconds()
                total += (values[i - 1] + values[i]) * 0.5 * dt
            return total

        # 退化为等间隔积分：仅对有效且连续的样本对积分
        dt = 1.0
        for i in range(1, point_count):
            prev = self._samples[i - 1]
            curr = self._samples[i]
            if not prev.is_effective or not curr.is_effective:
                continue
            total += (values[i - 1] + values[i]) * 0.5 * dt
        return total

    def _trapezoidal_integral(self, values: List[float], point_count: int) -> float:
        """
        梯形积分：优先基于真实时间戳；若时间戳无效/不单调，则退化为等间隔
        """
        safe_point_count = min(point_count, len(values), len(self._samples))
        if safe_point_count < 2:
            return 0.0

        # 检查时间戳有效性
        timestamps_valid = self._samples[0].timestamp is not None
        if timestamps_valid:
            for i in range(1, safe_point_count):
                prev = self._samples[i - 1]
                curr = self._samples[i]
                if (not prev.timestamp or not curr.timestamp or
                    (curr.timestamp - prev.timestamp).total_seconds() <= 0):
                    timestamps_valid = False
                    break

        if timestamps_valid:
            total = 0.0
            for i in range(1, safe_point_count):
                prev = self._samples[i - 1]
                curr = self._samples[i]
                dt = (curr.timestamp - prev.timestamp).total_seconds()
                total += (values[i - 1] + values[i]) * 0.5 * dt
            return total

        # 退化为等间隔
        dt = self._estimate_uniform_dt(safe_point_count)
        total = values[0] * 0.5 + values[safe_point_count - 1] * 0.5
        for i in range(1, safe_point_count - 1):
            total += values[i]
        return total * dt

    def _estimate_uniform_dt(self, point_count: int) -> float:
        """
        估算均匀采样间隔：当时间戳不可用或不连续时使用
        """
        if point_count < 2:
            return 0.0

        start = self._samples[0].timestamp
        end = self._samples[point_count - 1].timestamp
        if not start or not end:
            return 1.0

        total_seconds = (end - start).total_seconds()
        if total_seconds <= 0.0:
            return 1.0

        return total_seconds / (point_count - 1)

    def _resolve_frequency_deviation(self, frequency: float) -> float:
        """
        按配置口径计算频率偏差
        """
        raw_deviation = frequency - self._rated_frequency

        if self._delta_f_mode != DeltaFMode.DEAD_ZONE_EXCESS:
            return raw_deviation

        if self._dead_zone <= EPSILON:
            return raw_deviation

        abs_deviation = abs(raw_deviation)
        # 边界值应算作死区内
        if abs_deviation <= self._dead_zone + EPSILON:
            return 0.0

        excess = abs_deviation - self._dead_zone
        return excess if raw_deviation > 0.0 else -excess

    def _calc_theoretical_adjustment(self, freq_deviation: float) -> float:
        """
        计算理论调整量

        公式：ΔP_theory = -(Δf * Pn) / (fn * R)
        """
        speed_ratio = self._speed_ratio
        if abs(speed_ratio) < EPSILON:
            logger.warning("转速不等率接近0，使用默认值5%")
            speed_ratio = 0.05

        rated_power = self._unit_config.rated_power if self._unit_config else 600.0
        fn = self._rated_frequency

        adjustment = -(freq_deviation * rated_power) / (fn * speed_ratio)

        # 机组可调出力限制：±(limitCoefficient * Pn)
        limit_coef = self._unit_config.get_limit_coefficient() if self._unit_config else 0.06
        max_limit = limit_coef * rated_power
        adjustment = max(-max_limit, min(adjustment, max_limit))

        return adjustment

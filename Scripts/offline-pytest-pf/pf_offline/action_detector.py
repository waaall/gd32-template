# -*- coding: utf-8 -*-
"""
动作检测状态机

移植自 ActionStateMachine.cpp，实现一次调频动作的检测与生命周期管理
"""

import logging
from dataclasses import dataclass
from datetime import datetime
from enum import Enum
from typing import Callable, Dict, List, Optional

from .calculator import IncrementalCalculator
from .config import FormulaProfile, GlobalConfig, UnitConfig
from .metrics import ActionContext, Direction, SecondSample, WindowMetrics

logger = logging.getLogger(__name__)

# 死区判定时使用的 epsilon，避免浮点误差
DEAD_ZONE_EPSILON = 1e-6


class State(Enum):
    """状态定义"""
    IDLE = "Idle"         # 空闲状态：频率在死区内
    TRACKING = "Tracking" # 跟踪状态：频率越过死区，正在记录动作
    ENDING = "Ending"     # 结束中状态：频率回到死区，等待结束保持时间耗尽


@dataclass
class PreActionSample:
    """动作前样本"""
    timestamp: Optional[datetime]
    power: float
    quality: int = 2


@dataclass
class InitialPowerResolution:
    """初始功率解析结果"""
    value: float = 0.0
    used_count: int = 0
    window_sec: int = 0
    used_timestamp: bool = False
    has_valid_samples: bool = False


class ActionStateMachine:
    """
    动作状态机

    核心逻辑：接收实时数据点，根据频率偏差判断动作的开始与结束，
    并管理动作过程中的上下文数据
    """

    def __init__(
        self,
        on_action_started: Optional[Callable[[ActionContext], None]] = None,
        on_action_finished: Optional[Callable[[ActionContext], None]] = None,
        on_metrics_updated: Optional[Callable[[ActionContext], None]] = None
    ):
        """
        初始化状态机

        Args:
            on_action_started: 动作开始回调
            on_action_finished: 动作结束回调
            on_metrics_updated: 指标更新回调
        """
        self._on_action_started = on_action_started
        self._on_action_finished = on_action_finished
        self._on_metrics_updated = on_metrics_updated

        self._global_config: Optional[GlobalConfig] = None
        self._unit_config: Optional[UnitConfig] = None
        self._profile: Optional[FormulaProfile] = None
        self._windows_sec: List[int] = []
        self._configured: bool = False

        self._state: State = State.IDLE
        self._ending_start_time: Optional[datetime] = None
        self._calculator: IncrementalCalculator = IncrementalCalculator()
        self._context: ActionContext = ActionContext()
        self._pre_action_samples: List[PreActionSample] = []

    def configure(
        self,
        global_config: GlobalConfig,
        unit_config: UnitConfig,
        profile: FormulaProfile
    ) -> None:
        """
        配置状态机参数

        Args:
            global_config: 全局配置
            unit_config: 机组配置
            profile: 公式参数
        """
        self._global_config = global_config
        self._unit_config = unit_config
        self._profile = profile
        self._windows_sec = list(global_config.windows_sec)

        # 确保窗口时间按升序排列
        if not self._windows_sec:
            self._windows_sec = [15, 30, 60]
        self._windows_sec.sort()

        self._configured = True
        logger.info(f"状态机已配置 | windows_sec: {self._windows_sec} "
                    f"| dead_zone: {global_config.dead_zone}")

    def reset(self) -> None:
        """
        重置内部状态

        清除当前状态和上下文，恢复到初始状态
        """
        self._state = State.IDLE
        self._ending_start_time = None
        self._context = ActionContext()
        self._calculator = IncrementalCalculator()
        self._pre_action_samples.clear()

    def process_point(
        self,
        timestamp: Optional[datetime],
        frequency: float,
        power: float,
        quality: int
    ) -> None:
        """
        处理新的数据点

        核心入口：根据输入的时间、频率、功率更新状态机状态，并触发计算

        Args:
            timestamp: 时间戳
            frequency: 频率 (Hz)
            power: 功率 (MW)
            quality: 数据质量码 (0=good, 1=bad, 2=unknown)
        """
        if not self._configured:
            return

        # 计算当前频差
        freq_deviation = frequency - self._calculator.rated_frequency
        exceeded = self._exceeds_dead_zone(freq_deviation)

        # 状态：空闲
        if self._state == State.IDLE:
            if exceeded:
                # 频差越过死区，触发新动作
                self._start_action(timestamp, frequency, power, quality)
            else:
                self._update_pre_action_samples(timestamp, power, quality)
            return

        # 状态：跟踪或结束中
        sample = self._calculator.add_sample(timestamp, frequency, power, quality, exceeded)
        self._context.per_second_series.append(sample)

        if sample.is_effective:
            self._update_action_context(sample)

        self._calculator.update_all_windows()
        self._context.window_metrics = dict(self._calculator.window_metrics)
        self._context.duration = self._calculator.elapsed_seconds()

        if self._on_metrics_updated:
            self._on_metrics_updated(self._context)

        # 如果频差仍越过死区，保持或重置为跟踪状态
        if exceeded:
            if self._state == State.ENDING:
                logger.debug(f"状态跳转 | Ending -> Tracking | time: {timestamp}")
                self._state = State.TRACKING
                self._ending_start_time = None
            return

        # 如果频差回归死区
        if self._state == State.TRACKING:
            # 进入结束缓冲阶段
            logger.debug(f"状态跳转 | Tracking -> Ending | time: {timestamp}")
            self._state = State.ENDING
            self._ending_start_time = timestamp

        # 检查结束缓冲时间是否耗尽
        if self._state == State.ENDING:
            hold_sec = max(0, self._global_config.end_hold_sec) if self._global_config else 0

            if hold_sec <= 0:
                logger.debug(f"状态跳转 | Ending -> Idle | time: {timestamp} | reason: holdSec<=0")
                self._finalize_action(timestamp)
                return

            if (self._ending_start_time and timestamp and
                (timestamp - self._ending_start_time).total_seconds() >= hold_sec):
                logger.debug(f"状态跳转 | Ending -> Idle | time: {timestamp} | reason: holdSec reached")
                self._finalize_action(timestamp)

    def mark_missing_sample(self) -> None:
        """标记采样缺失"""
        if self._state == State.IDLE:
            return
        self._context.missing_samples += 1

    @property
    def is_tracking(self) -> bool:
        """是否正在跟踪动作"""
        return self._state != State.IDLE

    @property
    def state(self) -> State:
        """当前状态"""
        return self._state

    @property
    def context(self) -> ActionContext:
        """当前动作上下文"""
        return self._context

    def _start_action(
        self,
        timestamp: Optional[datetime],
        frequency: float,
        power: float,
        quality: int
    ) -> None:
        """内部逻辑：启动新动作"""
        self._state = State.TRACKING
        self._ending_start_time = None

        self._context = ActionContext()
        self._context.unit_id = self._unit_config.unit_id if self._unit_config else ""
        self._context.start_time = timestamp
        self._context.missing_samples = 0
        self._context.direction = Direction.POWER_UP  # 默认值，后续更新

        # 解析初始功率
        initial_power_resolution = self._resolve_initial_power(timestamp, power)
        self._context.initial_power = initial_power_resolution.value

        if not initial_power_resolution.has_valid_samples:
            logger.warning(f"动作前初始功率缺少有效样本 | windowSec: {initial_power_resolution.window_sec} "
                          f"| fallback: use trigger power | time: {timestamp}")
        elif initial_power_resolution.used_count < initial_power_resolution.window_sec:
            logger.warning(f"动作前初始功率样本不足 | windowSec: {initial_power_resolution.window_sec} "
                          f"| available: {initial_power_resolution.used_count} "
                          f"| method: {'timestamp' if initial_power_resolution.used_timestamp else 'count'} "
                          f"| time: {timestamp}")

        # 初始化增量计算器
        self._calculator.reset(
            self._unit_config,
            self._profile,
            self._windows_sec,
            initial_power_resolution.value,
            self._global_config.dead_zone if self._global_config else 0.033
        )
        self._pre_action_samples.clear()

        # 处理第一个点
        sample = self._calculator.add_sample(timestamp, frequency, power, quality, True)
        self._context.per_second_series.append(sample)
        self._update_action_context(sample)
        self._calculator.update_all_windows()
        self._context.window_metrics = dict(self._calculator.window_metrics)
        self._context.duration = self._calculator.elapsed_seconds()

        logger.info(f"动作开始 | time: {timestamp} | freq: {frequency:.3f} Hz "
                   f"| power: {power:.2f} MW | initialPower: {self._context.initial_power:.2f} MW")

        if self._on_action_started:
            self._on_action_started(self._context)
        if self._on_metrics_updated:
            self._on_metrics_updated(self._context)

    def _update_action_context(self, sample: SecondSample) -> None:
        """内部逻辑：更新动作上下文（如最大频差）"""
        freq_deviation = sample.frequency - self._calculator.rated_frequency

        # 记录最大频差
        if abs(freq_deviation) > abs(self._context.max_freq_deviation):
            self._context.max_freq_deviation = freq_deviation
            self._context.max_freq_deviation_time = sample.timestamp
            self._update_direction(freq_deviation)

    def _finalize_action(self, timestamp: Optional[datetime]) -> None:
        """内部逻辑：结束动作并生成最终结果"""
        self._state = State.IDLE

        # 设置结束时间
        if self._context.per_second_series:
            self._context.end_time = self._context.per_second_series[-1].timestamp
        else:
            self._context.end_time = timestamp

        duration_sec = max(1, self._calculator.elapsed_seconds())
        effective_duration_sec = max(1, self._calculator.elapsed_seconds(effective_only=True))
        self._context.duration = duration_sec
        self._context.effective_duration = effective_duration_sec
        self._context.initial_power = self._calculator.initial_power

        # 大扰动判定
        threshold = (self._profile.large_disturbance_threshold
                    if self._profile and self._profile.large_disturbance_threshold > 0.0
                    else 0.06)
        self._context.is_large_disturbance = abs(self._context.max_freq_deviation) >= threshold

        min_window = self._windows_sec[0] if self._windows_sec else 0
        max_window = self._windows_sec[-1] if self._windows_sec else 0

        # 判定短动作逻辑
        if effective_duration_sec > 0 and effective_duration_sec < min_window:
            self._context.is_short_action = True
            self._context.final_window_sec = effective_duration_sec
            self._context.has_latest_metrics = True
            self._context.latest_metrics = self._calculator.latest_metrics()
        else:
            self._context.is_short_action = False
            # 确定最终考核窗口时长
            if effective_duration_sec >= max_window and max_window > 0:
                self._context.final_window_sec = max_window
            else:
                self._context.final_window_sec = self._resolve_final_window_sec(effective_duration_sec)

        self._context.window_metrics = dict(self._calculator.window_metrics)

        logger.info(f"动作结束 | startTime: {self._context.start_time} "
                   f"| endTime: {self._context.end_time} "
                   f"| duration: {duration_sec}s | effectiveDuration: {effective_duration_sec}s "
                   f"| isShortAction: {self._context.is_short_action} "
                   f"| finalWindowSec: {self._context.final_window_sec}")

        # 输出最终指标日志
        if self._context.is_short_action and self._context.latest_metrics:
            m = self._context.latest_metrics
            logger.info(f"短动作指标 | response: {m.response:.6f} "
                       f"| energyContrib: {m.energy_contrib:.6f} "
                       f"| qualifiedRate: {m.qualified_rate:.6f}")
        else:
            final_sec = self._context.final_window_sec
            if final_sec in self._context.window_metrics:
                m = self._context.window_metrics[final_sec]
                logger.info(f"窗口{final_sec}s指标 | response: {m.response:.6f} "
                           f"| energyActual: {m.energy_actual:.6f} "
                           f"| energyTheoretical: {m.energy_theoretical:.6f} "
                           f"| energyContrib: {m.energy_contrib:.6f} "
                           f"| qualifiedRate: {m.qualified_rate:.6f}")

        if self._on_action_finished:
            self._on_action_finished(self._context)

        # 清空上下文准备下一次动作
        self._context = ActionContext()

    def _update_direction(self, freq_deviation: float) -> None:
        """内部逻辑：根据频差更新方向"""
        self._context.direction = (Direction.POWER_UP if freq_deviation < 0.0
                                  else Direction.POWER_DOWN)

    def _exceeds_dead_zone(self, freq_deviation: float) -> bool:
        """
        内部逻辑：死区判定

        边界值应算作死区内，使用 epsilon 避免浮点误差
        """
        dead_zone = self._global_config.dead_zone if self._global_config else 0.033
        return abs(freq_deviation) > dead_zone + DEAD_ZONE_EPSILON

    def _resolve_final_window_sec(self, duration_sec: int) -> int:
        """内部逻辑：根据持续时间解析最终窗口时长"""
        if not self._windows_sec:
            return duration_sec

        final_sec = self._windows_sec[0]
        for window_sec in self._windows_sec:
            if window_sec <= duration_sec:
                final_sec = window_sec
        return final_sec

    def _update_pre_action_samples(
        self,
        timestamp: Optional[datetime],
        power: float,
        quality: int
    ) -> None:
        """内部逻辑：维护动作前样本缓存"""
        window_sec = (self._profile.initial_power_avg_window_sec
                     if self._profile and self._profile.initial_power_avg_window_sec > 0
                     else 10)

        # 基于时间戳清理过期样本
        if timestamp:
            window_ms = window_sec * 1000
            from datetime import timedelta
            cutoff = timestamp - timedelta(milliseconds=window_ms)
            kept = [s for s in self._pre_action_samples
                   if s.timestamp and cutoff <= s.timestamp < timestamp]
            self._pre_action_samples = kept

        # 只保存 quality == 0 的有效样本
        if quality != 0:
            return

        sample = PreActionSample(timestamp=timestamp, power=power, quality=quality)
        self._pre_action_samples.append(sample)

        # 无时间戳时按数量限制
        if not timestamp:
            max_count = max(1, window_sec)
            if len(self._pre_action_samples) > max_count:
                remove_count = len(self._pre_action_samples) - max_count
                self._pre_action_samples = self._pre_action_samples[remove_count:]

    def _resolve_initial_power(
        self,
        timestamp: Optional[datetime],
        fallback_power: float
    ) -> InitialPowerResolution:
        """
        内部逻辑：计算动作前初始功率

        取动作前 N 秒平均功率，仅使用 quality == 0 的样本
        """
        result = InitialPowerResolution()
        result.window_sec = (self._profile.initial_power_avg_window_sec
                            if self._profile and self._profile.initial_power_avg_window_sec > 0
                            else 10)

        if result.window_sec <= 0:
            result.value = fallback_power
            return result

        if not self._pre_action_samples:
            result.value = fallback_power
            return result

        # 检查是否可以使用时间戳
        can_use_timestamp = timestamp is not None
        if can_use_timestamp:
            has_valid_timestamp = any(s.timestamp for s in self._pre_action_samples)
            can_use_timestamp = has_valid_timestamp

        total = 0.0
        count = 0

        if can_use_timestamp:
            from datetime import timedelta
            window_ms = result.window_sec * 1000
            cutoff = timestamp - timedelta(milliseconds=window_ms)

            for sample in self._pre_action_samples:
                if not sample.timestamp:
                    continue
                if sample.timestamp < cutoff or sample.timestamp >= timestamp:
                    continue
                total += sample.power
                count += 1
            result.used_timestamp = True
        else:
            # 退化为取最近 N 个样本
            max_count = max(1, result.window_sec)
            sample_count = len(self._pre_action_samples)
            start_index = max(0, sample_count - max_count)

            for i in range(start_index, sample_count):
                total += self._pre_action_samples[i].power
                count += 1

        if count <= 0:
            result.value = fallback_power
            return result

        result.value = total / count
        result.used_count = count
        result.has_valid_samples = True
        return result

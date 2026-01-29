# -*- coding: utf-8 -*-
"""
数据输入输出

支持 CSV 格式的输入数据读取和 JSON 格式的结果输出
"""

import csv
import json
import logging
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import List, Optional

from .config import IOConfig
from .metrics import ActionContext

logger = logging.getLogger(__name__)


@dataclass
class InputSample:
    """输入样本数据"""
    timestamp: Optional[datetime]
    frequency: float
    power: float
    quality: int = 0


def _parse_time_value(value: str, io_config: IOConfig) -> Optional[datetime]:
    """解析时间字符串，支持配置格式与 ISO"""
    value = value.strip()
    if not value:
        return None
    try:
        return datetime.strptime(value, io_config.timestamp_format)
    except ValueError:
        try:
            return datetime.fromisoformat(value.replace('Z', '+00:00'))
        except ValueError:
            return None


def _parse_time_bound(value: Optional[str], io_config: IOConfig, name: str) -> Optional[datetime]:
    """解析起止时间，解析失败时抛错"""
    if value is None:
        return None
    parsed = _parse_time_value(value, io_config)
    if parsed is None:
        raise ValueError(f"{name} 无法解析: {value}")
    return parsed


def _to_compare(dt: Optional[datetime]) -> Optional[datetime]:
    """将时间转换为可比较的 naive UTC 时间"""
    if dt is None:
        return None
    if dt.tzinfo is None:
        return dt
    return dt.astimezone(timezone.utc).replace(tzinfo=None)


def read_csv_data(
    file_path: str,
    io_config: IOConfig
) -> List[InputSample]:
    """
    从 CSV 文件读取输入数据

    Args:
        file_path: 文件路径
        io_config: IO 配置（包含列名和时间戳格式）

    Returns:
        输入样本列表
    """
    samples: List[InputSample] = []
    file_path = Path(file_path)

    if not file_path.exists():
        raise FileNotFoundError(f"输入文件不存在: {file_path}")

    start_dt = _parse_time_bound(io_config.start_time, io_config, "start_time")
    end_dt = _parse_time_bound(io_config.end_time, io_config, "end_time")
    start_cmp = _to_compare(start_dt)
    end_cmp = _to_compare(end_dt)
    if start_cmp and end_cmp and start_cmp > end_cmp:
        raise ValueError("start_time 不能晚于 end_time")
    filter_enabled = start_cmp is not None or end_cmp is not None
    filtered_out = 0
    missing_ts = 0

    with open(file_path, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)

        # 检查必需的列
        fieldnames = reader.fieldnames or []
        required_cols = [io_config.timestamp_column, io_config.frequency_column, io_config.power_column]
        missing_cols = [col for col in required_cols if col not in fieldnames]
        if missing_cols:
            raise ValueError(f"CSV 文件缺少必需的列: {missing_cols}")

        has_quality = io_config.quality_column in fieldnames

        for row_num, row in enumerate(reader, start=2):  # 从第2行开始（第1行是表头）
            try:
                # 解析时间戳
                ts_str = row.get(io_config.timestamp_column, '').strip()
                timestamp = None
                if ts_str:
                    timestamp = _parse_time_value(ts_str, io_config)
                    if timestamp is None:
                        logger.warning(f"第{row_num}行: 无法解析时间戳 '{ts_str}'")

                if filter_enabled:
                    if timestamp is None:
                        missing_ts += 1
                        continue
                    ts_cmp = _to_compare(timestamp)
                    if start_cmp and ts_cmp < start_cmp:
                        filtered_out += 1
                        continue
                    if end_cmp and ts_cmp > end_cmp:
                        filtered_out += 1
                        continue

                # 解析频率
                freq_str = row.get(io_config.frequency_column, '').strip()
                if not freq_str:
                    logger.warning(f"第{row_num}行: 频率为空，跳过")
                    continue
                frequency = float(freq_str)

                # 解析功率
                power_str = row.get(io_config.power_column, '').strip()
                if not power_str:
                    logger.warning(f"第{row_num}行: 功率为空，跳过")
                    continue
                power = float(power_str)

                # 解析质量码
                quality = 0
                if has_quality:
                    q_str = row.get(io_config.quality_column, '').strip()
                    if q_str:
                        try:
                            quality = int(q_str)
                        except ValueError:
                            quality = 2  # unknown

                samples.append(InputSample(
                    timestamp=timestamp,
                    frequency=frequency,
                    power=power,
                    quality=quality
                ))

            except (ValueError, KeyError) as e:
                logger.warning(f"第{row_num}行: 解析错误 - {e}")
                continue

    if filter_enabled:
        logger.info(
            f"时间范围过滤 | start: {start_dt} | end: {end_dt} "
            f"| filtered: {filtered_out} | missingTimestamp: {missing_ts}"
        )
    logger.info(f"已读取 {len(samples)} 条数据 | file: {file_path}")
    return samples


def write_json_output(
    actions: List[ActionContext],
    output_path: str,
    include_short_actions_in_summary: bool = False,
    qualification_threshold_small: float = 0.8,
    qualification_threshold_large: float = 0.8
) -> None:
    """
    将计算结果写入 JSON 文件

    Args:
        actions: 动作上下文列表
        output_path: 输出文件路径
        include_short_actions_in_summary: 是否在汇总中包含短动作
    """
    # 计算汇总信息
    total_actions = len(actions)
    qualified_actions = 0
    short_action_count = 0

    actions_data = []
    for action in actions:
        threshold = (qualification_threshold_large
                     if action.is_large_disturbance
                     else qualification_threshold_small)
        if action.is_short_action:
            short_action_count += 1
            if not include_short_actions_in_summary:
                # 短动作不计入合格统计
                pass
            elif action.latest_metrics and action.latest_metrics.qualified_rate >= threshold:
                qualified_actions += 1
        else:
            # 非短动作：使用最终窗口的合格率
            final_sec = action.final_window_sec
            if final_sec in action.window_metrics:
                if action.window_metrics[final_sec].qualified_rate >= threshold:
                    qualified_actions += 1

        actions_data.append(action.to_dict())

    # 如果不包含短动作，调整 total
    summary_total = total_actions
    if not include_short_actions_in_summary:
        summary_total = total_actions - short_action_count

    output = {
        'summary': {
            'totalActions': total_actions,
            'shortActions': short_action_count,
            'summaryTotal': summary_total,
            'qualifiedActions': qualified_actions,
            'qualificationRate': qualified_actions / summary_total if summary_total > 0 else 0.0,
        },
        'actions': actions_data,
    }

    output_path = Path(output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    with open(output_path, 'w', encoding='utf-8') as f:
        json.dump(output, f, ensure_ascii=False, indent=2, default=str)

    logger.info(f"已输出结果 | file: {output_path} | totalActions: {total_actions} "
               f"| qualifiedActions: {qualified_actions}")


def write_detailed_csv(
    actions: List[ActionContext],
    output_path: str
) -> None:
    """
    将秒级详细数据写入 CSV 文件

    Args:
        actions: 动作上下文列表
        output_path: 输出文件路径
    """
    output_path = Path(output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    with open(output_path, 'w', encoding='utf-8', newline='') as f:
        writer = csv.writer(f)
        writer.writerow([
            'actionIndex',
            'timestamp',
            'frequency',
            'power',
            'deltaP_theory',
            'deltaP_actual',
            'isEffective',
            'quality'
        ])

        for action_idx, action in enumerate(actions, start=1):
            for sample in action.per_second_series:
                writer.writerow([
                    action_idx,
                    sample.timestamp.isoformat() if sample.timestamp else '',
                    f'{sample.frequency:.3f}',
                    f'{sample.power:.3f}',
                    f'{sample.delta_p_theory:.4f}',
                    f'{sample.delta_p_actual:.4f}',
                    1 if sample.is_effective else 0,
                    sample.quality
                ])

    logger.info(f"已输出详细数据 | file: {output_path}")

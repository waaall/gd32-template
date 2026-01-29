#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
一次调频离线性能计算工具

按照 doc/phase2/realtime-calculation-flow.md 中的算法，从 CSV 数据计算一次调频性能指标。

用法:
    python main.py -i input.csv -o output.json
    python main.py -c config.json -i input.csv
    python main.py -i input.csv --rated-power 350 --dead-zone 0.033
"""

import sys
from pathlib import Path
from typing import List, Optional

# 将模块目录加入 path
sys.path.insert(0, str(Path(__file__).parent))

from pf_offline.config import (
    Config, load_config, parse_args, apply_args_to_config,
    GlobalConfig, UnitConfig, FormulaProfile
)
from pf_offline.logger_setup import setup_logging
from pf_offline.io_handler import read_csv_data, write_json_output, write_detailed_csv
from pf_offline.action_detector import ActionStateMachine
from pf_offline.metrics import ActionContext


def _resolve_path(base_dir: Path, value: Optional[str]) -> Optional[str]:
    if value is None:
        return None
    path = Path(value)
    if path.is_absolute():
        return str(path)
    return str((base_dir / path).resolve(strict=False))


def _resolve_config_paths(config: Config, base_dir: Path) -> None:
    config.io.input_file = _resolve_path(base_dir, config.io.input_file) or config.io.input_file
    config.io.output_file = _resolve_path(base_dir, config.io.output_file) or config.io.output_file
    if config.logging.log_file:
        config.logging.log_file = _resolve_path(base_dir, config.logging.log_file)


def process_data(config: Config) -> List[ActionContext]:
    """
    处理输入数据，检测动作并计算指标

    Args:
        config: 配置对象

    Returns:
        检测到的动作上下文列表
    """
    import logging
    logger = logging.getLogger(__name__)

    # 读取输入数据
    samples = read_csv_data(config.io.input_file, config.io)

    if not samples:
        logger.warning("输入数据为空")
        return []

    # 收集检测到的动作
    detected_actions: List[ActionContext] = []

    def on_action_finished(ctx: ActionContext) -> None:
        """动作结束回调"""
        # 复制上下文以避免后续被清空
        from copy import deepcopy
        detected_actions.append(deepcopy(ctx))

    # 创建状态机
    state_machine = ActionStateMachine(
        on_action_finished=on_action_finished
    )

    # 配置状态机
    global_config = GlobalConfig(
        dead_zone=config.global_cfg.dead_zone,
        rated_frequency=config.global_cfg.rated_frequency,
        end_hold_sec=config.global_cfg.end_hold_sec,
        windows_sec=list(config.global_cfg.windows_sec)
    )

    unit_config = UnitConfig(
        unit_id=config.unit.unit_id,
        rated_power=config.unit.rated_power,
        speed_ratio=config.unit.speed_ratio,
        limit_coefficient=config.unit.limit_coefficient
    )

    formula_profile = FormulaProfile(
        delta_f_mode=config.formula.delta_f_mode,
        initial_power_avg_window_sec=config.formula.initial_power_avg_window_sec,
        qualification_cap=config.formula.qualification_cap,
        speed_ratio_fallback=config.formula.speed_ratio_fallback,
        large_disturbance_threshold=config.formula.large_disturbance_threshold
    )

    state_machine.configure(global_config, unit_config, formula_profile)

    logger.info(f"开始处理数据 | samples: {len(samples)} "
               f"| ratedPower: {unit_config.rated_power} MW "
               f"| deadZone: {global_config.dead_zone} Hz")

    # 逐点处理
    for i, sample in enumerate(samples):
        state_machine.process_point(
            timestamp=sample.timestamp,
            frequency=sample.frequency,
            power=sample.power,
            quality=sample.quality
        )

        # 每 100 个点输出进度
        if (i + 1) % 100 == 0:
            logger.debug(f"已处理 {i + 1}/{len(samples)} 个数据点")

    # 如果最后还在跟踪中，强制结束
    if state_machine.is_tracking:
        logger.warning("数据结束时仍有未完成的动作，强制结束")
        # 使用最后一个样本的时间戳来结束
        last_sample = samples[-1] if samples else None
        last_ts = last_sample.timestamp if last_sample else None
        state_machine._finalize_action(last_ts)

    logger.info(f"处理完成 | 检测到 {len(detected_actions)} 个动作")

    return detected_actions


def main() -> int:
    """主入口"""
    base_dir = Path(__file__).resolve().parent
    # 解析命令行参数
    args = parse_args()
    if args.config is None:
        default_config = base_dir / 'config.json'
        if default_config.exists():
            args.config = str(default_config)
    args.config = _resolve_path(base_dir, args.config)
    if args.input is not None:
        args.input = _resolve_path(base_dir, args.input) or args.input
    if args.output is not None:
        args.output = _resolve_path(base_dir, args.output)
    if args.log_file is not None:
        args.log_file = _resolve_path(base_dir, args.log_file)

    # 加载配置
    config = load_config(args.config)

    # 应用命令行参数覆盖
    config = apply_args_to_config(config, args)
    _resolve_config_paths(config, base_dir)

    # 设置日志
    setup_logging(config.logging.level, config.logging.log_file)

    import logging
    logger = logging.getLogger(__name__)

    if not config.io.input_file:
        logger.error("未指定输入文件，请使用 -i/--input 或在 config.json 中设置 io.inputFile")
        return 1

    logger.info("=" * 60)
    logger.info("一次调频离线性能计算工具")
    logger.info("=" * 60)

    # 打印配置
    logger.info(f"配置信息:")
    logger.info(f"  输入文件: {config.io.input_file}")
    logger.info(f"  输出文件: {config.io.output_file}")
    logger.info(f"  起始时间: {config.io.start_time or '-'}")
    logger.info(f"  结束时间: {config.io.end_time or '-'}")
    logger.info(f"  机组ID: {config.unit.unit_id}")
    logger.info(f"  额定功率: {config.unit.rated_power} MW")
    logger.info(f"  转速不等率: {config.unit.speed_ratio}")
    logger.info(f"  死区: {config.global_cfg.dead_zone} Hz")
    logger.info(f"  窗口时长: {config.global_cfg.windows_sec}")
    logger.info(f"  频差口径: {config.formula.delta_f_mode.value}")
    logger.info(f"  初始功率窗口: {config.formula.initial_power_avg_window_sec}s")
    logger.info(f"  合格率上限: {config.formula.qualification_cap}")
    logger.info(f"  小扰动合格阈值: {config.formula.qualification_threshold_small}")
    logger.info(f"  大扰动合格阈值: {config.formula.qualification_threshold_large}")

    try:
        # 处理数据
        actions = process_data(config)

        # 输出结果
        if actions:
            write_json_output(
                actions,
                config.io.output_file,
                config.include_short_actions_in_summary,
                config.formula.qualification_threshold_small,
                config.formula.qualification_threshold_large
            )

            # 可选：输出详细 CSV
            detailed_csv = Path(config.io.output_file).with_suffix('.detail.csv')
            write_detailed_csv(actions, str(detailed_csv))
        else:
            logger.warning("未检测到任何动作")

        logger.info("计算完成")
        return 0

    except FileNotFoundError as e:
        logger.error(f"文件未找到: {e}")
        return 1
    except ValueError as e:
        logger.error(f"数据错误: {e}")
        return 1
    except Exception as e:
        logger.exception(f"处理过程中发生错误: {e}")
        return 1


if __name__ == '__main__':
    sys.exit(main())

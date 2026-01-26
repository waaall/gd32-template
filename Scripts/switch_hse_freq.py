#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
外置晶振频率切换脚本
用途：在开发板(8MHz)和产品板(12MHz)之间切换HSE晶振频率配置

使用方法：
    python switch_hse_freq.py 8      # 切换到8MHz(开发板)
    python switch_hse_freq.py 12     # 切换到12MHz(产品板)
    python switch_hse_freq.py reset  # 重置为默认12MHz
"""

import re
import sys
from pathlib import Path
from typing import Dict, Optional

# 项目根目录(假设脚本在Docs目录下)
PROJECT_ROOT = Path(__file__).parent.parent


class HSEFrequencySwitcher:
    """负责切换 HSE 频率的工具类，方便后续扩展新的文件或频率"""

    HSE_CONFIGS = {
        8: {
            'freq_hz': 8_000_000,
            'pllm': 4,
            'desc': '8MHz'
        },
        12: {
            'freq_hz': 12_000_000,
            'pllm': 6,
            'desc': '12MHz(默认)'
        }
    }

    def __init__(self, project_root: Path, ioc_file: Optional[Path] = None):
        self.project_root = project_root
        self.hal_conf_file = project_root / "Core" / "inc" / "stm32f4xx_hal_conf.h"
        self.main_file = project_root / "Core" / "src" / "main.c"
        self.ioc_file = self._resolve_ioc_file(ioc_file)
        self._backups: Dict[Path, str] = {}

    def switch(self, freq_mhz: int) -> bool:
        if freq_mhz not in self.HSE_CONFIGS:
            print(f"错误: 不支持的频率 {freq_mhz}MHz")
            print(f"支持的频率: {list(self.HSE_CONFIGS.keys())}")
            return False

        config = self.HSE_CONFIGS[freq_mhz]
        print(f"\n开始切换HSE晶振频率到 {config['desc']}")
        print("=" * 50)

        steps = (
            self._modify_hal_conf,
            self._modify_main_c,
            self._modify_ioc_file,
        )

        for step in steps:
            if not step(config):
                self._rollback_files()
                print("=" * 50)
                print("✗ 切换过程中出现错误, 已停止后续修改并回滚已改动文件")
                return False

        self._backups.clear()

        print("=" * 50)
        print(f"成功切换到 {config['desc']}")
        self._verify_pll_config(config)
        print("建议操作：")
        print("  1. 重新编译项目: make clean && make")
        print("  2. 烧录固件到目标板")
        print("  3. 如果使用STM32CubeMX, 请同步.ioc文件")
        return True

    def _modify_hal_conf(self, config: dict) -> bool:
        if not self.hal_conf_file.exists():
            print(f"错误: 文件不存在 {self.hal_conf_file}")
            return False

        content = self.hal_conf_file.read_text(encoding='utf-8')
        # 修改正则: 明确匹配整个数值部分(包括数字和U后缀)
        pattern = re.compile(
            r'(#define\s+HSE_VALUE\s+)(\d+U?)(\s*(?:/[/*].*)?)',
            re.MULTILINE,
        )

        def repl(match: re.Match) -> str:
            # group(1) = 前缀 "#define HSE_VALUE    "
            # group(2) = 旧数值 "12000000U"
            # group(3) = 注释部分 " /*!< ... */"
            return f"{match.group(1)}{config['freq_hz']}U{match.group(3)}"

        new_content, count = pattern.subn(repl, content, count=1)
        if count == 0:
            print("警告: 未找到 HSE_VALUE 定义")
            return False

        self._write_text(self.hal_conf_file, content, new_content)
        print(f"修改 {self.hal_conf_file.name}: HSE_VALUE -> {config['freq_hz']}")
        return True

    def _modify_main_c(self, config: dict) -> bool:
        if not self.main_file.exists():
            print(f"错误: 文件不存在 {self.main_file}")
            return False

        content = self.main_file.read_text(encoding='utf-8')
        pattern = r'(RCC_OscInitStruct\.PLL\.PLLM\s*=\s*)\d+;'
        replacement = rf'\g<1>{config["pllm"]};'
        new_content, count = re.subn(pattern, replacement, content)

        if count == 0:
            print("警告: 未找到 PLLM 配置")
            return False

        self._write_text(self.main_file, content, new_content)
        print(f"修改 {self.main_file.name}: PLLM = {config['pllm']}")
        return True

    def _modify_ioc_file(self, config: dict) -> bool:
        if self.ioc_file is None:
            return False
        content = self.ioc_file.read_text(encoding='utf-8')

        pattern_hse = re.compile(r'^(RCC\.HSE_VALUE=)\d+', re.MULTILINE)
        pattern_pllm = re.compile(r'^(RCC\.PLLM=)\d+', re.MULTILINE)

        new_content, count_hse = pattern_hse.subn(
            rf'\g<1>{config["freq_hz"]}',
            content,
        )
        new_content, count_pllm = pattern_pllm.subn(
            rf'\g<1>{config["pllm"]}',
            new_content,
        )

        if count_hse == 0:
            print("警告: 未找到 RCC.HSE_VALUE 配置")
            return False
        if count_pllm == 0:
            print("警告: 未找到 RCC.PLLM 配置")
            return False

        self._write_text(self.ioc_file, content, new_content)
        print(
            f"修改 {self.ioc_file.name}: "
            f"RCC.HSE_VALUE={config['freq_hz']}, RCC.PLLM={config['pllm']}"
        )
        return True

    def _verify_pll_config(self, config: dict) -> None:
        content = self.main_file.read_text(encoding='utf-8')
        pllm_match = re.search(r'RCC_OscInitStruct\.PLL\.PLLM\s*=\s*(\d+)', content)
        plln_match = re.search(r'RCC_OscInitStruct\.PLL\.PLLN\s*=\s*(\d+)', content)
        pllp_match = re.search(r'RCC_OscInitStruct\.PLL\.PLLP\s*=\s*RCC_PLLP_DIV(\d+)', content)

        if not all((pllm_match, plln_match, pllp_match)):
            print("警告: 无法读取完整的PLL配置")
            return

        pllm = int(pllm_match.group(1))
        plln = int(plln_match.group(1))
        pllp = int(pllp_match.group(1))

        vco_in = config['freq_hz'] / pllm
        vco_out = vco_in * plln
        sysclk = vco_out / pllp

        print("\n--- PLL配置验证 ---")
        print(f"HSE频率:     {config['freq_hz']/1e6:.1f} MHz")
        print(f"PLLM:        {pllm}")
        print(f"PLLN:        {plln}")
        print(f"PLLP:        {pllp}")
        print(f"VCO输入:     {vco_in/1e6:.1f} MHz (应在1-2MHz)")
        print(f"VCO输出:     {vco_out/1e6:.1f} MHz (应在100-432MHz)")
        print(f"系统时钟:    {sysclk/1e6:.1f} MHz")
        if not (1e6 <= vco_in <= 2e6):
            print("⚠️  警告: VCO输入频率不在推荐范围(1-2MHz)")
        if not (100e6 <= vco_out <= 432e6):
            print("⚠️  警告: VCO输出频率不在有效范围(100-432MHz)")
        print("-------------------\n")

    def _resolve_ioc_file(self, ioc_hint: Optional[Path]) -> Optional[Path]:
        if ioc_hint:
            path = ioc_hint if ioc_hint.is_absolute() else self.project_root / ioc_hint
            if not path.exists():
                print(f"错误: 指定的 .ioc 文件不存在 {path}")
                return None
            return path

        ioc_files = sorted(self.project_root.glob("*.ioc"))
        if not ioc_files:
            print("错误: 未找到 .ioc 文件")
            return None
        if len(ioc_files) > 1:
            print("错误: 找到多个 .ioc 文件，请保留一个或扩展脚本以指定目标：")
            for file in ioc_files:
                print(f"  - {file.name}")
            return None
        return ioc_files[0]

    def _write_text(self, path: Path, original: str, new_content: str) -> None:
        if path not in self._backups:
            self._backups[path] = original
        path.write_text(new_content, encoding='utf-8')

    def _rollback_files(self) -> None:
        for path, content in self._backups.items():
            path.write_text(content, encoding='utf-8')
        self._backups.clear()


def main() -> None:
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(1)

    arg = sys.argv[1].lower()
    if arg == 'reset':
        freq_mhz = 12
    else:
        try:
            freq_mhz = int(arg)
        except ValueError:
            print(f"错误: 无效的参数 '{arg}'")
            print(__doc__)
            sys.exit(1)

    switcher = HSEFrequencySwitcher(PROJECT_ROOT)
    success = switcher.switch(freq_mhz)
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()

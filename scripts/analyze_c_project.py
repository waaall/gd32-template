#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
C/C++项目依赖关系分析 (C/C++ Project Dependency Analyzer)

功能描述:
    通用的C/C++项目依赖关系分析工具, 能够自动扫描项目中的所有源文件和头文件,
    分析它们之间的包含关系, 构建完整的依赖关系图, 并生成详细的分析报告。

主要特性:
    - 自动检测项目结构和文件类型(.c/.cpp/.h/.hpp等)
    - 递归解析#include依赖关系, 支持相对路径和绝对路径
    - 智能匹配头文件与对应源文件的映射关系
    - 分层次展示文件依赖结构, 便于理解项目架构
    - 识别未被使用的文件, 帮助项目清理
    - 检测缺失的头文件引用
    - 生成Markdown格式的详细分析报告

基本结构:
    CProjectAnalyzer:              # 主分析器类
    ├── buildSourceIncludeMaps()   # 扫描和建立文件映射关系
    ├── analyzeDependencies()      # 递归分析依赖关系
    └── generateDetailedReport()   # 生成分析报告

输出文件:
    - dependency_graph.md          # 主分析报告(Markdown格式)
    - unused_files_detailed.log    # 未使用文件详细列表

使用范围:
    适用于各种分析一定规模的C/C++项目

使用方法:
    1. 命令行调用(分析当前目录):
       python analyze_c_project.py
       
    2. 程序调用:
       analyzer = CProjectAnalyzer("./project_path")
       analyzer.buildSourceIncludeMaps()
       analyzer.analyzeDependencies()
       analyzer.generateDetailedReport()

    3. 指定入口文件:
       analyzer.analyzeDependencies(entry_files=["main.c", "app.c"])

注意事项:
    - 确保项目目录具有读取权限
    - 生成的报告文件会覆盖同名的现有文件
    - 系统头文件(如stdio.h)会被自动过滤

版本信息:
    版本: v2.1
    作者: zhengxu
    创建日期: 2025-08-09
    最后更新: 2025-08-19
    许可证: MIT License

更新日志:
    v2.1.0 (2025-08-19):
    - 添加完整的类型注解支持
    - 修复遍历顺序不确定性问题, 确保结果可重现
    - 优化路径解析算法
    - 改进错误处理和日志记录
    - 统一函数命名为小驼峰格式
    - 完善代码文档和注释
    
    v2.0.0:
    - 重构核心算法, 提高分析准确性
    - 添加缺失include文件检测
    - 支持多种项目结构
    - 生成更详细的分析报告
"""
import os
import re
from pathlib import Path
from collections import defaultdict
import logging
from typing import List, Optional, Union, Set, Dict, Tuple

# 配置日志
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

class CProjectAnalyzer:
    def __init__(self, project_root: str, extend_lib_paths: Optional[List[str]] = None) -> None:
        self.project_root = Path(project_root).resolve()
        self.extend_lib_paths = [Path(p).resolve() for p in (extend_lib_paths or [])]
        
        # 类型注解的实例变量
        self.all_files: Set[Path] = set()
        self.used_files: Set[Path] = set()
        self.source_map: Dict[Path, Path] = {}            # 头文件 -> 对应的源文件
        self.include_map: Dict[Path, Path] = {}           # 源文件 -> 对应的头文件
        self.scanned_files: Set[Path] = set()             # 已扫描过的文件
        self.dependency_tree: Dict[Path, Set[Path]] = defaultdict(set)  # 完整的依赖树
        self.include_paths: Set[Path] = set()             # 头文件搜索路径
        self.missing_includes: Set[Tuple[Path, str]] = set()   # 未找到的include文件 (文件路径, include名称)
        self.analysis_stats: Dict[str, Union[int, Dict[int, Set[Path]]]] = {
            'max_depth': 0,
            'files_by_depth': defaultdict(set),
        }
        
    def buildSourceIncludeMaps(self) -> None:
        logger.info("正在扫描所有C项目文件并建立映射关系...")

        # 搜索主项目目录
        search_dirs = [self.project_root] + self.extend_lib_paths
        
        # 添加主项目根目录和额外搜索路径到include搜索路径
        self.include_paths.add(self.project_root)
        for path in self.extend_lib_paths:
            self.include_paths.add(path)
        
        for search_dir in search_dirs:
            if not search_dir.exists():
                logger.warning(f"搜索目录不存在: {search_dir}")
                continue
            self._searchStoreMaps(search_dir)

        logger.info(f"找到 {len(self.include_paths)} 个include路径")
        logger.info(f"找到 {len(self.source_map)} 对头文件-源文件映射")
        logger.info(f"找到 {len(self.all_files)} 个源文件")
    
    def _searchStoreMaps(self, search_dir: Path) -> None:
        include_extensions = {'.h', '.hpp', '.hxx'}
        source_extensions = {'.c', '.cpp', '.cc', '.cxx'}
        c_extensions = include_extensions | source_extensions
        for root, dirs, files in os.walk(search_dir):
            root_path = Path(root)
            has_headers = False
            
            # 在单次遍历中处理所有文件
            for file in files:
                file_ext = Path(file).suffix.lower()
                
                # 不是c/c++相关文件, 跳过
                if file_ext not in c_extensions:
                    continue
                full_path = root_path / file
                self.all_files.add(full_path)
                if file_ext in include_extensions:
                    has_headers = True
                    # 寻找对应的源文件
                    base_name = Path(file).stem
                    
                    # 首先在当前目录查找同名源文件
                    found_source = False
                    for other_file in files:
                        other_ext = Path(other_file).suffix.lower()
                        if (other_ext in source_extensions and 
                            Path(other_file).stem == base_name):
                            source_path = root_path / other_file
                            self.source_map[full_path] = source_path
                            self.include_map[source_path] = full_path
                            found_source = True
                            break
                    
                    # 如果当前目录没有, 查找上级目录的可能源文件夹
                    if not found_source:
                        parent_dir = root_path.parent
                        potential_src_dirs = ['src', 'srcs', 'source', 'sources']
                        
                        for src_dir_name in potential_src_dirs:
                            src_dir = parent_dir / src_dir_name
                            if src_dir.exists() and src_dir.is_dir():
                                for src_file in src_dir.iterdir():
                                    if (src_file.is_file() and 
                                        src_file.suffix.lower() in source_extensions and
                                        src_file.stem == base_name):
                                        self.source_map[full_path] = src_file
                                        self.include_map[src_file] = full_path
                                        found_source = True
                                        break
                            if found_source:
                                break
            
            # 在遍历完当前目录的所有文件后, 添加到include搜索路径
            if has_headers:
                self.include_paths.add(root_path)
        
    def parseProject(self) -> None:
        """解析项目文件, 获取项目中包含的文件(可扩展支持不同项目类型)"""
        # 暂时为空, 可以根据需要扩展支持 Makefile、CMakeLists.txt、.vcxproj 等
        logger.info("项目文件解析功能待实现, 当前跳过")
    
    def analyzeDependencies(self, max_depth: int = 20,
                            entry_files: Optional[List[Union[str, Path]]] = None) -> None:

        logger.info("开始递归依赖分析...")        
        # 如果没有提供入口文件, 自动查找
        if entry_files is None:
            logger.warning("未指定入口文件, 自动查找...")
            entry_files = [
                self.project_root / "main.c",
                self.project_root / "src" / "main.c",
                self.project_root / "source" / "main.c",
                self.project_root / "Core" / "src" / "main.c",
                self.project_root / "User" / "main.c",
                self.project_root / "User" / "main1.c",
                # 可以添加更多入口文件模式
            ]

        # 验证入口文件
        valid_start_files: List[Path] = []
        for main_file in entry_files:
            main_file_path = Path(main_file) if not isinstance(main_file, Path) else main_file
            if main_file_path.exists():
                valid_start_files.append(main_file_path)
                logger.info(f"找到入口文件: {main_file_path}")
        
        if not valid_start_files:
            logger.error("未找到main.c")
            return
            
        # 使用BFS进行层次化分析
        current_level = set(valid_start_files)
        depth = 0
        
        while current_level and depth < max_depth:
            logger.info(f"分析第 {depth} 层, 文件数: {len(current_level)}")
            next_level = set()
            
            # 记录当前层的文件
            self.analysis_stats['files_by_depth'][depth].update(current_level)
            
            # 确保遍历顺序的确定性, 避免不同运行时结果不同
            for file_path in sorted(current_level):
                if file_path not in self.used_files:
                    self.used_files.add(file_path)
                    
                    # 扫描当前文件的includes
                    self._scanIncludesUpdateDependencyTree(file_path)
                    
                    # 收集下一层要分析的文件
                    for included_file in self.dependency_tree[file_path]:
                        if included_file not in self.used_files:
                            next_level.add(included_file)
                            
                        # 如果include的是头文件, 也要包含对应的源文件
                        if included_file in self.source_map:
                            src_file = self.source_map[included_file]
                            if src_file not in self.used_files:
                                next_level.add(src_file)
                    
                    # 如果当前文件是源文件, 也要包含对应的头文件
                    if file_path in self.include_map:
                        include_file = self.include_map[file_path]
                        if include_file not in self.used_files:
                            next_level.add(include_file)

            current_level = next_level
            depth += 1

        self.analysis_stats['max_depth'] = depth
        logger.info(f"依赖分析完成, 最大深度: {depth}")

    def _scanIncludesUpdateDependencyTree(self, file_path: Path) -> None:
        """扫描单个文件的#include语句"""
        if file_path in self.scanned_files:
            logger.debug(f"重复查看文件include: {file_path}")
            return
        self.scanned_files.add(file_path)

        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                
            # 移除注释以避免误匹配
            content = re.sub(r'//.*', '', content)
            content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)
                
            # 查找所有#include语句
            include_patterns = [
                r'#include\s*[<"](.*?)[>"]',  # 标准include
                r'#include\s+[<"](.*?)[>"]',  # include后有空格
            ]

            includes = set()
            for pattern in include_patterns:
                matches = re.findall(pattern, content, re.IGNORECASE)
                includes.update(matches)
                
            # 解析每个include
            for include in includes:
                # 先检查是否为系统头文件, 如果是则跳过
                if self._isSystemInclude(include):
                    continue
                    
                # 对非系统头文件尝试解析路径
                resolved_path = self._getNosysIncludePath(include, file_path)
                if resolved_path:
                    self.dependency_tree[file_path].add(resolved_path)
                else:
                    # 记录未找到的非系统头文件
                    self.missing_includes.add((file_path, include))
                    logger.warning(f"在文件 {file_path} 中未找到 include 文件: {include}")
 
        except Exception as e:
            logger.warning(f"扫描文件 {file_path} 时出错: {e}")
    
    def _isSystemInclude(self, include: str) -> bool:
        """检查是否为系统头文件"""
        # 清理include路径
        include = include.strip().replace('\\', '/')
        
        # 如果是系统头文件(如stdio.h), 跳过
        system_includes = {
            'stdio.h', 'stdlib.h', 'string.h', 'math.h', 'time.h',
            'assert.h', 'errno.h', 'setjmp.h', 'signal.h', 'stdarg.h',
            'stddef.h', 'limits.h', 'float.h', 'iso646.h', 'locale.h',
            'wchar.h', 'wctype.h', 'stdbool.h', 'stdint.h', 'inttypes.h'
        }
        return include in system_includes
    
    def _getNosysIncludePath(self, include: str, current_file: Path) -> Optional[Path]:
        """解析include路径为实际文件路径"""
        # 清理include路径
        include = include.strip().replace('\\', '/')
            
        current_dir = current_file.parent
        
        # 首先尝试相对于当前文件的目录
        candidate = current_dir / include
        if candidate.exists():
            return candidate
            
        # 然后在include搜索路径中查找
        for search_path in self.include_paths:
            candidate = search_path / include
            if candidate.exists():
                return candidate

        return None

    def generateDetailedReport(self) -> None:
        logger.info("生成分析报告...")

        # debug information
        logger.debug(f"dependency_tree: {self.dependency_tree}")

        # 计算统计数据
        unused_all_files = self.all_files - self.used_files
        logger.info(f"总文件{len(self.all_files)}个, 分析深度{self.analysis_stats['max_depth']}层")
        logger.info(f"调用{len(self.used_files)}个, 未被调用{len(unused_all_files)}个")

        # 保存详细结果到文件
        logger.debug("保存详细分析结果到文件...")
        output_dir = self.project_root / "analysis_output"
        output_dir.mkdir(exist_ok=True)
        
        # 生成综合的依赖关系图文件, 包含所有信息
        with open(output_dir / "dependency_graph.md", "w", encoding="utf-8") as f:
            # 写入总体统计信息
            f.write("# 项目依赖关系分析报告\n\n## 总体统计信息\n")
            f.write(f"- 总文件数: {len(self.all_files)} 个\n")
            f.write(f"- 被调用文件数: {len(self.used_files)} 个\n")
            f.write(f"- 未被调用文件数: {len(unused_all_files)} 个\n")
            f.write(f"- 缺少include文件数: {len(self.missing_includes)} 个\n")
            f.write(f"- 依赖关系层数: {self.analysis_stats['max_depth']} 层\n")
            
            # 写入各层文件统计
            f.write("\n### 各层文件统计:\n")
            for depth in sorted(self.analysis_stats['files_by_depth'].keys()):
                f.write(f"- 第 {depth} 层: {len(self.analysis_stats['files_by_depth'][depth])} 个文件\n")
            
            # 写入missing includes
            f.write("### 未找到的include及其依赖\n")
            missing_by_file: Dict[str, List[str]] = defaultdict(list)
            for source_file_path, missing_include in self.missing_includes:
                try:
                    rel_source = source_file_path.relative_to(self.project_root)
                    missing_by_file[str(rel_source)].append(missing_include)
                except ValueError:
                    missing_by_file[str(source_file_path)].append(missing_include)
            
            for source_file, missing_list in sorted(missing_by_file.items()):
                f.write(f"\n#### {source_file}\n")
                f.write(f"{len(missing_list)} 个未找到的include\n")
                for missing_include in sorted(missing_list):
                    f.write(f"- {missing_include}\n")

            # 写入按层次分组的文件列表, 每个文件包含依赖关系
            f.write("\n\n## 分层依赖关系\n")
            for depth in sorted(self.analysis_stats['files_by_depth'].keys()):
                f.write(f"\n### 第 {depth} 层 ({len(self.analysis_stats['files_by_depth'][depth])} 个文件)\n")
                
                for file_path in sorted(self.analysis_stats['files_by_depth'][depth]):
                    try:
                        rel_path = file_path.relative_to(self.project_root)
                        f.write(f"\n#### {rel_path}\n")
                        
                        # 写入该文件的依赖关系
                        if file_path in self.dependency_tree and self.dependency_tree[file_path]:
                            f.write("\n**依赖关系:**\n")
                            for dep in sorted(self.dependency_tree[file_path]):
                                if dep in self.used_files:
                                    try:
                                        dep_rel = dep.relative_to(self.project_root)
                                        f.write(f"- {dep_rel}\n")
                                    except ValueError:
                                        f.write(f"- {dep}\n")
                        
                        # 写入该文件缺少的include文件
                        file_missing_includes = [missing_include for source_file_path, missing_include in self.missing_includes 
                                               if source_file_path == file_path]
                        if file_missing_includes:
                            f.write("\n  未找到依赖:\n")
                            for missing_include in sorted(file_missing_includes):
                                f.write(f"    - {missing_include}\n")
                        
                        # 如果既没有依赖关系也没有缺少的include, 显示无依赖关系
                        if (file_path not in self.dependency_tree or not self.dependency_tree[file_path]) and not file_missing_includes:
                            f.write("\n  无依赖关系\n")
                            
                    except ValueError as e:
                        logger.debug(f"相对路径计算错误: {e}")
                        f.write(f"\n**{file_path}**\n")

        # 保存未调用文件信息
        with open(output_dir / "unused_files_detailed.log", "w", encoding="utf-8") as f:
            f.write("# 详细的未调用文件列表\n")
            
            # 按目录分组未调用文件
            dir_unused = defaultdict(list)
            for file_path in unused_all_files:
                try:
                    rel_path = file_path.relative_to(self.project_root)
                    dir_name = rel_path.parent
                    dir_unused[str(dir_name)].append(str(rel_path))
                except ValueError:
                    dir_unused["external"].append(str(file_path))
            
            for dir_name, files in sorted(dir_unused.items()):
                f.write(f"\n## {dir_name}/ ({len(files)} 个未调用文件)\n")
                for file_path in sorted(files):
                    f.write(f"{file_path}\n")

        logger.info(f"分析报告保存到: {output_dir}")


def main() -> None:
    # 当前目录作为项目根目录, 支持相对路径和绝对路径, 也可以传入其他路径如 "../other_project" 等
    project_root = "."  
    extend_lib_paths = [
        # "/path/to/external/library1",
        # "/path/to/external/library2"
    ]
    analyzer = CProjectAnalyzer(project_root, extend_lib_paths)
    analyzer.buildSourceIncludeMaps()
    # analyzer.parseProject()
    entry_files: List[Path] = [
        # 可以在这里添加特定的入口文件
    ]
    analyzer.analyzeDependencies(entry_files=entry_files if entry_files else None)
    analyzer.generateDetailedReport()

if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""
GD32F4xx Makefile自动生成脚本

该脚本会自动扫描项目目录结构，收集C源文件、汇编文件、头文件目录和链接脚本，
然后基于template.makefile模板生成适合当前项目的Makefile。

主要功能：
1. 自动发现项目中的所有.c源文件
2. 自动发现项目中的所有.s和.S汇编文件 
3. 自动发现项目中的所有头文件目录
4. 自动发现项目中的链接脚本(.ld文件)
5. 基于template.makefile生成最终的Makefile

使用方法：
python3 scripts/create_makefile.py

作者：基于网络资源修改，适配GD32F4xx系列项目
"""

import os
import re

def main(tpl_file, dst_file):
    """
    主函数：生成Makefile的核心逻辑
    
    参数:
        tpl_file: 模板Makefile文件路径 (template.makefile)
        dst_file: 输出Makefile文件路径 (Makefile)
    """
    # 初始化文件列表：存储不同类型的源文件和配置
    cFileList = []      # 存储所有.c源文件路径
    hPathList = []      # 存储所有头文件目录路径
    sFileList = []      # 存储所有.s汇编文件路径  
    SFileList = []      # 存储所有.S汇编文件路径（启动文件等）
    ldFileList = []     # 存储所有.ld链接脚本文件路径

    # 步骤1: 解析模板文件，获取GD32库路径配置（如果存在）
    dList = ["./"]  # 默认从当前目录开始扫描
    d_sp = False    # 标准外设库路径标志
    
    # 注释：原始脚本会查找GD32库路径，但在当前项目中库文件已经包含在项目内
    # 所以这部分代码被保留但不使用外部路径
    with open(tpl_file, "r") as lines:
            for line in lines:
                # 搜索CMSIS库路径定义
                searchObj = re.search( r'^\s*(GD32_CMSIS_LIBRARY_DIR.*)\s*=\s*([^\s]+)$', line)
                if searchObj:
                    d_tmp = searchObj.group(2)
                    print("GD32F4xx CMSIS (template): %s" % d_tmp)
                
                # 搜索标准外设库路径定义
                searchObj = re.search( r'^\s*(GD32_SP_LIBRARY_DIR.*)\s*=\s*([^\s]+)$', line)
                if searchObj:
                    d_sp = searchObj.group(2)
                    print("GD32F4xx_standard_peripheral (template): %s" % d_sp)

    # 步骤2: 递归遍历项目目录，收集所有相关文件
    print("开始扫描项目目录...")
    for d in dList:
        for root,dirs,files in os.walk(d):
            for file in files:
                # 收集C源文件(.c)
                if file.endswith(".c"):
                    cFile = os.path.join(root,file)
                    cFileList.append(cFile)
                # 收集头文件目录(.h文件所在目录)
                if file.endswith(".h"):
                    hPath = os.path.join(root)
                    if hPathList.count(hPath) == 0:  # 去重
                        hPathList.append(hPath)
                # 收集小写.s汇编文件
                if file.endswith(".s"):
                    sFile = os.path.join(root,file)
                    sFileList.append(sFile)
                # 收集大写.S汇编文件（通常是启动文件）
                if file.endswith(".S"):
                    SFile = os.path.join(root,file)
                    SFileList.append(SFile)
                # 收集链接脚本文件(.ld)
                if file.endswith(".ld"):
                    ldFile = os.path.join(root,file)
                    ldFileList.append(ldFile)
    # 步骤3: 输出扫描结果，便于调试
    print("扫描完成，发现以下文件：")
    print("C文件：", cFileList)
    print("头目录：", hPathList)
    print("s文件", sFileList)
    print("S文件", SFileList)  
    print("ld文件", ldFileList)

    # 步骤4: 构建Makefile变量字符串
    print("正在生成Makefile变量...")
    
    # 构建C源文件变量 (C_SOURCES)
    cFileStr = "C_SOURCES += "
    # 构建头文件目录变量 (C_INCLUDES) 
    hPathStr = "C_INCLUDES += "
    # 构建.s汇编文件变量 (ASM_SOURCES)
    sFileStr = "ASM_SOURCES += "
    # 构建.S汇编文件变量 (ASMM_SOURCES)
    SFileStr = "ASMM_SOURCES += "
    # 构建链接脚本变量 (LDSCRIPT) - 使用=而不是+=，与STM32版本一致
    ldFileStr = "LDSCRIPT = "
    # 步骤5: 将文件列表转换为Makefile格式的字符串
    # 处理C源文件列表
    for listStr in cFileList:
        cFileStr += " \\\n" + listStr
    
    # 处理头文件目录列表，添加-I前缀
    for listStr in hPathList:
        hPathStr += " \\\n-I" + listStr
    
    # 处理.s汇编文件列表
    for listStr in sFileList:
        sFileStr += " \\\n" + listStr
    
    # 处理.S汇编文件列表（启动文件等）
    for listStr in SFileList:
        SFileStr += " \\\n" + listStr
    
    # 处理链接脚本：只取第一个，避免多个脚本冲突
    if ldFileList:
        ldFileStr += ldFileList[0]
        print(f"使用链接脚本: {ldFileList[0]}")
    else:
        print("警告: 未找到链接脚本文件!")

    # 步骤6: 生成最终的Makefile文件
    print("正在生成Makefile...")
    
    # 注释：不需要替换外部库路径，因为所有文件都在当前项目中
    # cFileStr = cFileStr.replace(d_sp, '$(GD32_SP_LIBRARY_DIR)')

    try:
        # 读取模板文件
        with open(tpl_file, "r") as f:
            fileStr = f.read()
        
        # 按照STM32CubeMX生成Makefile的顺序替换各个占位符
        print("替换模板占位符...")
        fileStr = fileStr.replace("@@C_SOURCES@@", cFileStr)
        fileStr = fileStr.replace("@@C_INCLUDES@@", hPathStr)
        fileStr = fileStr.replace("@@ASM_SOURCES@@", sFileStr)
        fileStr = fileStr.replace("@@ASMM_SOURCES@@", SFileStr)  
        fileStr = fileStr.replace("@@LDSCRIPT@@", ldFileStr)
        
        # 写入目标Makefile文件
        with open(dst_file, "w") as f:
            f.write(fileStr)
        
        print(f"Makefile生成成功: {dst_file}")
        
    except Exception as e:
        print(f"生成Makefile时发生错误: {e}")
        raise


if __name__ == '__main__':
    """
    脚本入口点
    
    默认使用:
    - 模板文件: ./scripts/template.makefile
    - 输出文件: ./Makefile
    """
    print("=" * 60)
    print("GD32F4xx Makefile自动生成工具")
    print("=" * 60)
    
    main("./scripts/template.makefile", "./Makefile")
    
    print("=" * 60)
    print("Makefile生成完成！")
    print("现在你可以运行 'make' 命令来编译项目")
    print("=" * 60)
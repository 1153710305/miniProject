#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
脚本名称: package_mac.py
脚本功能: 自动化编译与打包 macOS 应用程序 (.app)，封装动态链接库，并生成分发包 (.zip, .dmg)
使用方法: python3 package_mac.py [目标名称] (例如: python3 package_mac.py sdl3-demo)
"""

import os
import sys
import shutil
import subprocess
import plistlib

def run_command(command: list, cwd: str = None) -> bool:
    """
    运行系统命令并处理错误。

    参数:
    command (list): 命令行参数列表
    cwd (str): 命令执行的工作目录，默认为当前目录

    返回:
    bool: 命令是否成功执行
    """
    try:
        # 执行命令，打印输出
        result = subprocess.run(command, cwd=cwd, check=True)
        return result.returncode == 0
    except subprocess.CalledProcessError as e:
        print(f"❌ 执行命令失败: {' '.join(command)}")
        print(f"错误详情: {e}")
        sys.exit(1)

def check_command_exists(cmd: str) -> bool:
    """
    检查系统环境是否存在某个命令。

    参数:
    cmd (str): 命令名称

    返回:
    bool: 命令是否存在
    """
    # 使用 shutil.which 查找命令路径
    return shutil.which(cmd) is not None

def main():
    # --- 1. 参数与路径配置 ---
    # TARGET_NAME (str): 默认构建的目标程序名称（可以从命令行参数传入，例如：sdl3-demo 或 Undersea_Kingdom）
    target_name = sys.argv[1] if len(sys.argv) > 1 else "sdl3-demo"

    # project_root (str): 项目根目录的绝对路径
    project_root = os.path.dirname(os.path.abspath(__file__))
    
    # build_preset (str): CMake 预设名称
    build_preset = "macos-release"
    
    # build_dir (str): 构建生成目标路径
    build_dir = os.path.join(project_root, "out", "build", build_preset)
    
    # dist_dir (str): 打包输出分发路径
    dist_dir = os.path.join(project_root, "out", "dist")
    
    # app_name (str): .app 文件夹包名
    app_name = f"{target_name}.app"
    
    # app_path (str): .app 的完整绝对路径
    app_path = os.path.join(dist_dir, app_name)

    print("==========================================")
    print(f"🍎 开始为 macOS 打包项目: {target_name}")
    print(f"项目根目录: {project_root}")
    print(f"构建目录: {build_dir}")
    print(f"打包输出: {app_path}")
    print("==========================================")

    # --- 2. 编译 Release 版本 ---
    print("⚙️ 正在配置 CMake Release 预设...")
    run_command(["cmake", "--preset", build_preset], cwd=project_root)

    print("🛠️ 正在编译 Release 版本...")
    run_command(["cmake", "--build", build_dir, "--config", "Release", "--target", target_name], cwd=project_root)

    # --- 3. 创建 macOS App Bundle 结构 ---
    print("📂 创建 .app 目录结构...")
    if os.path.exists(app_path):
        shutil.rmtree(app_path)

    # macos_dir (str): Contents/MacOS 目录
    macos_dir = os.path.join(app_path, "Contents", "MacOS")
    # resources_dir (str): Contents/Resources 目录
    resources_dir = os.path.join(app_path, "Contents", "Resources")
    # frameworks_dir (str): Contents/Frameworks 目录
    frameworks_dir = os.path.join(app_path, "Contents", "Frameworks")

    os.makedirs(macos_dir, exist_ok=True)
    os.makedirs(resources_dir, exist_ok=True)
    os.makedirs(frameworks_dir, exist_ok=True)

    # 拷贝可执行二进制文件
    # binary_src (str): 编译出来的可执行二进制源文件路径
    binary_src = os.path.join(build_dir, target_name, target_name)
    # binary_dest (str): .app 包内可执行文件目的路径
    binary_dest = os.path.join(macos_dir, target_name)
    print(f"💾 拷贝可执行文件到 {binary_dest}...")
    shutil.copy2(binary_src, binary_dest)

    # 拷贝静态资源文件夹 (data)
    # data_src (str): 项目自带的 data 资源目录
    data_src = os.path.join(project_root, "data")
    # data_dest (str): .app 包内 Resources/data 目录
    data_dest = os.path.join(resources_dir, "data")
    if os.path.isdir(data_src):
        print("🎨 正在拷贝静态资源目录 (data)...")
        shutil.copytree(data_src, data_dest)
    else:
        print("⚠️ 未发现 data/ 资源目录，跳过拷贝。")

    # 创建 Info.plist 元数据文件
    # plist_path (str): plist 输出完整路径
    plist_path = os.path.join(app_path, "Contents", "Info.plist")
    # plist_data (dict): 包含 bundle 配置的字典数据
    plist_data = {
        "CFBundleDevelopmentRegion": "zh_CN",
        "CFBundleExecutable": target_name,
        "CFBundleIdentifier": f"com.miniproject.{target_name}",
        "CFBundleInfoDictionaryVersion": "6.0",
        "CFBundleName": target_name,
        "CFBundlePackageType": "APPL",
        "CFBundleShortVersionString": "1.0.0",
        "CFBundleSignature": "????",
        "CFBundleVersion": "1.0.0",
        "LSMinimumSystemVersion": "11.0",
        "NSHighResolutionCapable": True,
    }
    print("📝 生成 Info.plist...")
    with open(plist_path, "wb") as fp:
        plistlib.dump(plist_data, fp)

    # --- 4. 封装动态库 (实现免安装运行) ---
    if check_command_exists("dylibbundler"):
        print("📦 发现 dylibbundler，开始打包依赖库并修复 rpath...")
        # 组装 dylibbundler 参数
        # dylibbundler_cmd (list): 命令行调用参数列表
        dylibbundler_cmd = [
            "dylibbundler",
            "-od",
            "-b",
            "-x", binary_dest,
            "-d", os.path.join(frameworks_dir, ""),
            "-p", "@executable_path/../Frameworks/"
        ]
        run_command(dylibbundler_cmd)
        print("✅ 依赖动态库打包完成。")
    else:
        print("⚠️ 未安装 dylibbundler。打包生成的 .app 可能只能在已安装 SDL3 依赖的电脑上运行。")
        print("💡 提示: 建议通过 'brew install dylibbundler' 安装此工具以进行完全独立的本地化发行。")

    # --- 5. 生成分发包 (.dmg / .zip) ---
    print("📦 正在打包为分发格式...")
    os.chdir(dist_dir)

    # 打包 zip 文件
    # zip_name (str): 输出 zip 的文件名
    zip_name = f"{target_name}-macOS"
    # shutil.make_archive 自动把 app_name 压缩成 zip_name.zip
    shutil.make_archive(zip_name, "zip", root_dir=dist_dir, base_dir=app_name)
    print(f"✅ 成功生成 Zip 包: {os.path.join(dist_dir, f'{zip_name}.zip')}")

    # 生成 DMG 镜像
    if check_command_exists("create-dmg"):
        print("💿 发现 create-dmg，正在生成 DMG 镜像...")
        # dmg_path (str): 目标 DMG 文件名
        dmg_path = f"{target_name}-macOS.dmg"
        if os.path.exists(dmg_path):
            os.remove(dmg_path)
        # create_dmg_cmd (list): 命令行调用参数列表
        create_dmg_cmd = [
            "create-dmg",
            "--volname", f"{target_name} Installer",
            "--window-size", "600", "400",
            "--icon-size", "100",
            "--icon", app_name, "200", "190",
            "--hide-extension", app_name,
            "--app-drop-link", "400", "190",
            dmg_path,
            app_name
        ]
        run_command(create_dmg_cmd, cwd=dist_dir)
        print(f"✅ 成功生成 DMG 安装包: {os.path.join(dist_dir, dmg_path)}")
    else:
        print("💡 提示: 可选安装 'brew install create-dmg' 以自动生成美观的 DMG 安装包。")

    print("==========================================")
    print("🎉 打包流程顺利完成！")
    print(f"输出文件所在目录: {dist_dir}")
    print("==========================================")

if __name__ == "__main__":
    main()

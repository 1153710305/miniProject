# -*- coding: utf-8 -*-

"""
脚本名称: package_win.py
脚本功能: 自动化编译与打包 Windows 应用程序 (.exe)，打包所有依赖的动态链接库 (.dll) 和静态资源，生成分发包 (.zip)
使用方法: python package_win.py [目标名称] (例如: python package_win.py sdl3-demo)
"""

import os
import sys
import shutil
import subprocess

def run_command(command: list, cwd: str = None) -> bool:
    """
    运行系统命令并处理错误。
    """
    try:
        result = subprocess.run(command, cwd=cwd, check=True)
        return result.returncode == 0
    except subprocess.CalledProcessError as e:
        print(f"❌ 执行命令失败: {' '.join(command)}")
        print(f"错误详情: {e}")
        sys.exit(1)

def main():
    # --- 1. 参数与路径配置 ---
    # target_name (str): 默认构建的目标程序名称（例如：sdl3-demo 或 Undersea_Kingdom）
    target_name = sys.argv[1] if len(sys.argv) > 1 else "sdl3-demo"

    # project_root (str): 项目根目录的绝对路径
    project_root = os.path.dirname(os.path.abspath(__file__))
    
    # build_preset (str): CMake 预设名称
    build_preset = "x64-release"
    
    # build_dir (str): 构建生成目标路径
    build_dir = os.path.join(project_root, "out", "build", build_preset)
    
    # dist_root (str): 打包输出分发根路径
    dist_root = os.path.join(project_root, "out", "dist")
    
    # package_dir_name (str): 最终打包目录名
    package_dir_name = f"{target_name}-windows-x64"
    
    # dist_dir (str): 最终打包输出完整路径
    dist_dir = os.path.join(dist_root, package_dir_name)

    print("==========================================")
    print(f"💻 开始为 Windows 打包项目: {target_name}")
    print(f"项目根目录: {project_root}")
    print(f"构建目录: {build_dir}")
    print(f"打包输出目录: {dist_dir}")
    print("==========================================")

    # --- 2. 编译 Release 版本 ---
    print("⚙️ 正在配置 CMake Release 预设...")
    run_command(["cmake", "--preset", build_preset], cwd=project_root)

    print("🛠️ 正在编译 Release 版本...")
    run_command(["cmake", "--build", build_dir, "--config", "Release", "--target", target_name], cwd=project_root)

    # --- 3. 创建分发包目录结构 ---
    print("📂 创建分发目录结构...")
    if os.path.exists(dist_dir):
        shutil.rmtree(dist_dir)
    os.makedirs(dist_dir, exist_ok=True)

    # 寻找编译出的可执行文件 (处理 Ninja 直接生成和 MSBuild 放在 Release 子目录的两种情况)
    binary_src_ninja = os.path.join(build_dir, target_name, f"{target_name}.exe")
    binary_src_msbuild = os.path.join(build_dir, target_name, "Release", f"{target_name}.exe")
    
    binary_src = None
    if os.path.exists(binary_src_ninja):
        binary_src = binary_src_ninja
    elif os.path.exists(binary_src_msbuild):
        binary_src = binary_src_msbuild
    else:
        # 尝试备用查找路径
        binary_src_root_ninja = os.path.join(build_dir, f"{target_name}.exe")
        binary_src_root_msbuild = os.path.join(build_dir, "Release", f"{target_name}.exe")
        if os.path.exists(binary_src_root_ninja):
            binary_src = binary_src_root_ninja
        elif os.path.exists(binary_src_root_msbuild):
            binary_src = binary_src_root_msbuild

    if not binary_src:
        print(f"❌ 找不到编译生成的可执行文件: {target_name}.exe")
        sys.exit(1)

    print(f"💾 发现可执行文件: {binary_src}")
    shutil.copy2(binary_src, os.path.join(dist_dir, f"{target_name}.exe"))

    # --- 4. 拷贝静态资源文件夹 (data) ---
    data_src = os.path.join(project_root, "data")
    data_dest = os.path.join(dist_dir, "data")
    if os.path.isdir(data_src):
        print("🎨 正在拷贝静态资源目录 (data)...")
        shutil.copytree(data_src, data_dest)
    else:
        print("⚠️ 未发现 data/ 资源目录，跳过拷贝。")

    # --- 5. 拷贝依赖的 Windows 动态链接库 (.dll) ---
    print("📦 正在收集并拷贝 DLL 依赖库...")
    
    # 架构限定为 x64
    arch = "x64"
    
    # DLL 依赖路径配置字典
    dll_paths = [
        # SDL3 核心库
        os.path.join(project_root, "vendored", "SDL3-3.4.14", "lib", arch, "SDL3.dll"),
        
        # SDL3_image 核心库及可选解码库
        os.path.join(project_root, "vendored", "SDL3_image-3.4.4", "lib", arch, "SDL3_image.dll"),
        os.path.join(project_root, "vendored", "SDL3_image-3.4.4", "lib", arch, "optional", "libpng16-16.dll"),
        os.path.join(project_root, "vendored", "SDL3_image-3.4.4", "lib", arch, "optional", "libwebp-7.dll"),
        os.path.join(project_root, "vendored", "SDL3_image-3.4.4", "lib", arch, "optional", "libwebpdemux-2.dll"),
        os.path.join(project_root, "vendored", "SDL3_image-3.4.4", "lib", arch, "optional", "libwebpmux-3.dll"),
        os.path.join(project_root, "vendored", "SDL3_image-3.4.4", "lib", arch, "optional", "libtiff-6.dll"),
        os.path.join(project_root, "vendored", "SDL3_image-3.4.4", "lib", arch, "optional", "libavif-16.dll"),
        
        # SDL3_mixer 核心库及可选音频解码库
        os.path.join(project_root, "vendored", "SDL3_mixer-3.2.4", "lib", arch, "SDL3_mixer.dll"),
        os.path.join(project_root, "vendored", "SDL3_mixer-3.2.4", "lib", arch, "optional", "libogg-0.dll"),
        os.path.join(project_root, "vendored", "SDL3_mixer-3.2.4", "lib", arch, "optional", "libopus-0.dll"),
        os.path.join(project_root, "vendored", "SDL3_mixer-3.2.4", "lib", arch, "optional", "libopusfile-0.dll"),
        os.path.join(project_root, "vendored", "SDL3_mixer-3.2.4", "lib", arch, "optional", "libwavpack-1.dll"),
        os.path.join(project_root, "vendored", "SDL3_mixer-3.2.4", "lib", arch, "optional", "libgme.dll"),
        os.path.join(project_root, "vendored", "SDL3_mixer-3.2.4", "lib", arch, "optional", "libxmp.dll"),
        
        # SDL3_ttf 核心库
        os.path.join(project_root, "vendored", "SDL3_ttf-3.2.2", "lib", arch, "SDL3_ttf.dll"),
    ]

    for dll_path in dll_paths:
        if os.path.exists(dll_path):
            shutil.copy2(dll_path, dist_dir)
            print(f"  + 已拷贝: {os.path.basename(dll_path)}")
        else:
            # 某些可选 DLL 不存在也不影响，但核心 DLL 必须打印提示
            basename = os.path.basename(dll_path)
            if basename in ["SDL3.dll", "SDL3_image.dll", "SDL3_mixer.dll", "SDL3_ttf.dll"]:
                print(f"❌ 找不到关键 DLL 依赖: {dll_path}")
                sys.exit(1)

    # --- 6. 生成 ZIP 压缩包 ---
    print("📦 正在打包为 ZIP 格式...")
    shutil.make_archive(
        base_name=os.path.join(dist_root, package_dir_name),
        format="zip",
        root_dir=dist_root,
        base_dir=package_dir_name
    )
    
    print("==========================================")
    print("🎉 Windows 打包流程顺利完成！")
    print(f"输出 ZIP 压缩包路径: {os.path.join(dist_root, f'{package_dir_name}.zip')}")
    print("==========================================")

if __name__ == "__main__":
    main()

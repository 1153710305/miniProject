# Windows 平台程序打包发行指南

本指南详细介绍了在 Windows 环境下如何编译、收集依赖并打包发布项目（如 `sdl3-demo` 或 `Undersea_Kingdom`）。项目已为您配置了自动化打包脚本，同时也支持使用 **VS Code** 或 **Visual Studio** 进行手动编译打包。

---

## 🛠️ 环境准备与先决条件

在开始打包之前，请确保您的系统中已安装以下工具：
1. **CMake** (3.20 或更高版本，[官方下载](https://cmake.org/download/))
2. **Visual Studio 2022 / 2026** (已安装 “使用 C++ 的桌面开发” 工作负载，包含 `MSVC` 编译器和 `Ninja` 生成器)
3. **Python 3** (用于运行自动化打包脚本)

---

## 🚀 方法一：使用 Python 脚本一键打包（推荐，最快捷）

项目根目录下已为您创建了自动化打包脚本 [`package_win.py`](file:///c:/Users/work/source/repos/miniProject/package_win.py)。该脚本会自动使用 CMake 编译 Release 版本的程序，并收集所需的所有 DLL 依赖和 `data/` 静态资源，最终生成可以直接解压分发的 ZIP 包。

### 执行步骤：
打开 **PowerShell** 或 **VS 开发人员命令提示 (Developer Command Prompt)**，进入项目根目录并运行：

```powershell
# 1. 进入项目根目录
cd c:\Users\work\source\repos\miniProject

# 2. 运行打包脚本（以打包 sdl3-demo 为例，支持自定义目标名称）
python package_win.py sdl3-demo
```

### 打包输出：
打包完成后，将在 `out/dist/` 目录下生成：
* **解包文件夹：** `out/dist/sdl3-demo-windows-x64/` (可直接在该目录下双击 `sdl3-demo.exe` 启动运行)
* **发布压缩包：** [`out/dist/sdl3-demo-windows-x64.zip`](file:///c:/Users/work/source/repos/miniProject/out/dist/sdl3-demo-windows-x64.zip) (可直接发送给用户进行分发)

---

## 💻 方法二：使用 VS Code 手动打包

由于项目中已经配置了完整的 `CMakePresets.json` 预设，使用 VS Code 的 CMake Tools 插件是最方便的手动打包方式。

### 步骤 1：使用 CMake Tools 进行 Release 编译
1. 用 VS Code 打开项目文件夹 `miniProject`。
2. 确保已安装 **CMake Tools** 插件。
3. 点击 VS Code 底部状态栏的 **CMake 配置预设**，选择 **`x64 Release`** (对应 `CMakePresets.json` 中的 `x64-release` 预设)。
4. 状态栏上的编译器选择为 **Visual Studio Enterprise/Community (cl.exe)**。
5. 选择构建目标为 `sdl3-demo`，点击底部的 **Build (生成)** 按钮，开始编译。
6. 编译生成的程序将输出在 `out/build/x64-release/sdl3-demo/sdl3-demo.exe`。

### 步骤 2：手动收集依赖（建立发布包结构）
1. 在资源管理器中新建一个分发目标目录，例如：`out/dist/sdl3-demo-windows-x64`。
2. 将 `out/build/x64-release/sdl3-demo/sdl3-demo.exe` 拷贝到该目录下。
3. 将项目根目录下的 [`data/`](file:///c:/Users/work/source/repos/miniProject/data) 资源文件夹整体拷贝到该目录下。
4. 从 [`vendored/`](file:///c:/Users/work/source/repos/miniProject/vendored) 依赖库中拷贝以下 x64 版本的动态链接库 (.dll) 到程序同级目录下：
   * **SDL3 核心库：** [`vendored/SDL3-3.4.14/lib/x64/SDL3.dll`](file:///c:/Users/work/source/repos/miniProject/vendored/SDL3-3.4.14/lib/x64/SDL3.dll)
   * **SDL3_image 核心与可选图像解码库：** 
     * [`vendored/SDL3_image-3.4.4/lib/x64/SDL3_image.dll`](file:///c:/Users/work/source/repos/miniProject/vendored/SDL3_image-3.4.4/lib/x64/SDL3_image.dll)
     * 拷贝 `libpng16-16.dll`、`libwebp-7.dll`、`libwebpdemux-2.dll`、`libwebpmux-3.dll`、`libtiff-6.dll`、`libavif-16.dll`（位于 `vendored/SDL3_image-3.4.4/lib/x64/optional/` 目录下）
   * **SDL3_mixer 音频解码库：**
     * [`vendored/SDL3_mixer-3.2.4/lib/x64/SDL3_mixer.dll`](file:///c:/Users/work/source/repos/miniProject/vendored/SDL3_mixer-3.2.4/lib/x64/SDL3_mixer.dll)
     * 拷贝 `libogg-0.dll`、`libopus-0.dll`、`libopusfile-0.dll`、`libwavpack-1.dll`、`libgme.dll`、`libxmp.dll`（位于 `vendored/SDL3_mixer-3.2.4/lib/x64/optional/` 目录下）
   * **SDL3_ttf 字体渲染库：**
     * [`vendored/SDL3_ttf-3.2.2/lib/x64/SDL3_ttf.dll`](file:///c:/Users/work/source/repos/miniProject/vendored/SDL3_ttf-3.2.2/lib/x64/SDL3_ttf.dll)
5. 最终生成的文件夹即为一个绿色免安装的完整版应用包，右键将其压缩为 `.zip` 格式即可分发。

---

## 🏢 方法三：使用 Visual Studio 2022 / 2026 手动打包

Visual Studio 的 IDE 环境也内置了极佳的 CMake 支持。

### 步骤 1：在 Visual Studio 中配置与构建
1. 打开 Visual Studio，选择“**打开本地文件夹**”，然后选择项目根目录 `miniProject`。
2. Visual Studio 会自动检测 `CMakeLists.txt` 并加载配置。
3. 在顶部配置下拉菜单中，选择配置预设为 **`x64 Release`**。
4. 在顶部菜单栏中选择 **生成 -> 全部生成** (Build -> Build All)，编译生成 Release 可执行文件。

### 步骤 2：收集与发布
* 重复 **方法二** 中的“手动收集依赖”步骤，将可执行文件、`data/` 目录和对应的 `SDL3` 相关 `.dll` 文件收集到同一个文件夹并进行 ZIP 压缩。

---

## 📦 准确无误的发行版文件清单

为确保程序在任何未安装 SDL3 环境的 Windows 电脑上能够顺利直接启动，您的分发 ZIP 包解压后**必须**包含以下完整文件：

```text
📁 sdl3-demo-windows-x64/
├── 📄 sdl3-demo.exe         # 编译出的 Release 版本主程序
├── 📁 data/                 # 静态资源夹（音效、贴图、关卡数据等）
│   └── (所有的游戏美术、声音等资源)
├── ⚙️ SDL3.dll              # SDL3 核心库
├── ⚙️ SDL3_image.dll        # 图像加载扩展库
├── ⚙️ SDL3_mixer.dll        # 音频混合播放扩展库
├── ⚙️ SDL3_ttf.dll          # 字体渲染扩展库
├── ⚙️ libpng16-16.dll       # PNG 格式解码依赖
├── ⚙️ libwebp-7.dll         # WebP 格式解码依赖
├── ⚙️ libwebpdemux-2.dll
├── ⚙️ libwebpmux-3.dll
├── ⚙️ libtiff-6.dll         # TIFF 解码依赖
├── ⚙️ libavif-16.dll        # AVIF 解码依赖
├── ⚙️ libogg-0.dll          # Ogg 音频解码依赖
├── ⚙️ libopus-0.dll         # Opus 音频解码依赖
├── ⚙️ libopusfile-0.dll
├── ⚙️ libwavpack-1.dll      # WavPack 音频解码依赖
├── ⚙️ libgme.dll            # 游戏音乐模拟器依赖
└── ⚙️ libxmp.dll            # 模块化音乐播放依赖
```

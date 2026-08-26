# 🍎 macOS 开发配置教程

本教程介绍如何在 macOS 系统（支持 Apple Silicon M1/M2/M3/M4 及 Intel 芯片）上配置、编译和运行 **miniProject**。

---

## 📋 环境要求

- **操作系统**：macOS Big Sur (11.0) 或更高版本
- **IDE / 编辑器**：**VS Code** (推荐) 或 **CLion** / **Xcode**
- **构建工具**：CMake 3.20+、Ninja
- **编译器**：Clang (macOS 自带) 或 GCC

---

## 📦 依赖库安装

在 macOS 上，我们推荐使用 **Homebrew** 统一管理依赖库，这比手动下载更方便，且能自动处理路径。

### 1. 安装 Xcode 命令行工具
打开终端，运行以下命令安装编译器和基本开发工具：
```bash
xcode-select --install
```

### 2. 安装 Homebrew
如果您还没有安装 Homebrew，请运行以下命令进行安装（若已安装可跳过）：
```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

### 3. 使用 Homebrew 安装项目依赖
在终端中一键安装 CMake、Ninja、GLM 以及 SDL3 系列库：
```bash
brew install cmake ninja sdl3 sdl3_image sdl3_mixer sdl3_ttf glm pkg-config
```

> 💡 **提示**：
> - Apple Silicon (M系列) 芯片的 Homebrew 默认安装路径为 `/opt/homebrew`。
> - Intel 芯片的 Homebrew 默认安装路径为 `/usr/local`。
> - 本项目 CMake 配置会自动适配这些路径。

---

## 🛠️ 使用 VS Code 编译与调试 (推荐)

### 1. 安装推荐插件
打开 VS Code，安装以下扩展：
- **C/C++** (Microsoft)
- **CMake Tools** (Microsoft)

### 2. 打开项目
1. 启动 VS Code。
2. 选择 **「打开文件夹...」** (Open Folder...)，选中项目根目录 `miniProject`。

### 3. 选择 CMake 配置预设 (Configure Preset)
1. 点击 VS Code 底部状态栏的 **CMake: [No active kit]** 或 **CMake Preset** 按钮。
2. 此时会弹出预设列表，选择 `macos-debug` (此预设已在 `CMakePresets.json` 中配置)。
3. CMake Tools 会自动执行配置，终端输出 `Configuring done` 和 `Generating done` 即代表配置成功。

### 4. 编译与运行
- **编译项目**：点击状态栏底部的 **Build** 按钮，或者按快捷键 `Cmd + Shift + B`。
- **运行/调试**：
  1. 在状态栏底部的启动目标（Launch Target）中选择你想运行的子项目（例如 `sdl3-demo`）。
  2. 点击状态栏的 ⚙️（运行）或 🐞（调试）按钮，或者直接按 `F5` 启动调试。

---

## 💻 命令行编译与运行

如果您更喜欢纯命令行操作，可以按照以下步骤执行：

### 1. 配置项目 (Configure)
使用 `CMakePresets.json` 中定义好的 `macos-debug` 预设：
```bash
# 在项目根目录下执行
cmake --preset macos-debug
```
*注：如果在配置时提示找不到 SDL3 或 GLM，可以显式指定 Homebrew 路径（以 Apple Silicon 为例）：*
```bash
cmake --preset macos-debug -DCMAKE_PREFIX_PATH="/opt/homebrew"
```

### 2. 编译项目 (Build)
```bash
cmake --build out/build/macos-debug
```

### 3. 运行程序 (Run)
编译完成后，可执行文件会被输出到构建目录下。请在**项目根目录**下执行程序，以确保能正确读取 `data/` 中的静态资源：
```bash
./out/build/macos-debug/sdl3-demo/sdl3-demo
```

---

## ⚠️ 常见问题与注意事项

### 1. 找不到资源文件 (Image/Audio/Fonts)
如果程序启动后黑屏、崩溃或提示找不到 `data/` 目录下的资源，请确保你的**工作目录**（Working Directory）设为了项目根目录。
- **命令行运行**：确保在 `miniProject` 根目录下执行 `./out/build/macos-debug/sdl3-demo/sdl3-demo`。
- **VS Code 运行**：CMake Tools 默认会将工作目录设为项目根目录。如果需要手动配置，可以在 `.vscode/settings.json` 中设置：
  ```json
  "cmake.debugConfig": {
      "cwd": "${workspaceFolder}"
  }
  ```

### 2. macOS 上的渲染后端
本项目使用 SDL3 的 GPU API。在 Windows 上它可能使用 Direct3D 12 或 Vulkan，而在 macOS 上，SDL3 默认会通过 Metal（或者 macOS 支持的 Vulkan/MoltenVK）进行高性能硬件加速渲染，无需开发者手动额外配置 Vulkan SDK。

---

## 📦 macOS 打包与发行教程

在 macOS 上，如果直接分发编译出来的可执行二进制文件，用户在其他机器上运行可能会因为找不到 `data/` 资源目录或未安装 Homebrew 的 SDL3 依赖库而报错崩溃。
标准的打包发行方式是构建一个独立的 **`.app` 应用包 (App Bundle)**，并将依赖的动态库 (`.dylib`) 封装到包内。

我们已经在项目根目录下编写了自动化打包脚本 [package_mac.py](file:///Users/sangsang/VSCodeProjects/miniProject/package_mac.py)，以下是详细的打包与发行步骤。

### 1. 准备打包环境（推荐）
为了让打包出来的 `.app` 能够完全脱离 Homebrew 依赖，并在其他未安装开发环境的 macOS 电脑上运行，建议安装 **`dylibbundler`**。
如果需要生成标准的 macOS 双击安装 DMG 镜像，可以额外安装 **`create-dmg`**。

在终端中执行以下命令安装工具：
```bash
brew install dylibbundler create-dmg
```

### 2. 一键编译并打包
在项目根目录下，直接运行打包脚本即可。脚本会自动完成：配置 Release 预设、编译项目、创建 `.app` 结构、拷贝静态资源、打包依赖 `.dylib` 动态库、生成 `.zip` 以及 `.dmg` 分发包。

```bash
# 赋予脚本执行权限（如已赋予可跳过）
chmod +x package_mac.py

# 打包指定的项目目标（默认为 sdl3-demo）
python3 package_mac.py sdl3-demo
```
如果要打包其他目标（例如 `Undersea_Kingdom`），只需将其作为参数传入：
```bash
python3 package_mac.py Undersea_Kingdom
```

### 3. 生成的发布产物
打包完成后，所有的发布产物将被放置在项目根目录下的 `out/dist/` 文件夹中：
- **`sdl3-demo.app`**：标准的 macOS 应用包，可直接双击运行（包含完整的二进制、图片和音频资源，且依赖库已被内载到 `Frameworks` 目录）。
- **`sdl3-demo-macOS.zip`**：`.app` 包的压缩版本，便于分发或上传至下载页。
- **`sdl3-demo-macOS.dmg`**：生成的标准 macOS 安装镜像（需要已安装 `create-dmg`），双击即可挂载，用户只需将 App 拖拽到 "Applications" 目录即可完成安装。

### 4. 详细的手动打包流程说明
如果您想了解底层的打包细节或手动执行，可参考以下明确的命令：

#### A. 编译 Release 版本
```bash
# 配置 Release 预设
cmake --preset macos-release

# 编译目标程序
cmake --build out/build/macos-release --config Release --target sdl3-demo
```

#### B. 组装 `.app` 目录结构
```bash
# 创建目录
mkdir -p out/dist/sdl3-demo.app/Contents/MacOS
mkdir -p out/dist/sdl3-demo.app/Contents/Resources
mkdir -p out/dist/sdl3-demo.app/Contents/Frameworks

# 复制可执行文件
cp out/build/macos-release/sdl3-demo/sdl3-demo out/dist/sdl3-demo.app/Contents/MacOS/

# 复制资源文件夹 data
cp -R data out/dist/sdl3-demo.app/Contents/Resources/
```

#### C. 生成元数据 `Info.plist`
在 `out/dist/sdl3-demo.app/Contents/Info.plist` 下写入以下内容，以便 macOS 能够正确识别它是一个图形界面应用：
```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>sdl3-demo</string>
    <key>CFBundleIdentifier</key>
    <string>com.miniproject.sdl3-demo</string>
    <key>CFBundleName</key>
    <string>sdl3-demo</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0.0</string>
    <key>LSMinimumSystemVersion</key>
    <string>11.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
```

#### D. 手动修复动态库路径 (RPath)
使用 `dylibbundler` 把可执行文件依赖的所有 Homebrew 原生动态链接库拷贝并更新路径：
```bash
dylibbundler -od -b \
    -x out/dist/sdl3-demo.app/Contents/MacOS/sdl3-demo \
    -d out/dist/sdl3-demo.app/Contents/Frameworks/ \
    -p @executable_path/../Frameworks/
```
*(注：这会自动修复 SDL3、SDL3_image 等动态库的引用，使其指向 App 内包的 `@executable_path/../Frameworks/`，从而不再依赖系统 Homebrew 环境。)*


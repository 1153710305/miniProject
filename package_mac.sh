#!/bin/bash

# ==============================================================================
# 脚本名称: package_mac.sh
# 脚本功能: 自动化编译与打包 macOS 应用程序 (.app)，并可选地使用 dylibbundler 封装动态链接库
# 使用方法: ./package_mac.sh [目标名称] (例如: ./package_mac.sh sdl3-demo)
# ==============================================================================

# 确保脚本遇到错误时立即退出
set -e

# --- 1. 参数与路径配置 ---

# 默认构建的目标程序名称（可以从命令行参数传入，例如：sdl3-demo 或 Undersea_Kingdom）
TARGET_NAME=${1:-"sdl3-demo"}

# 获取项目根目录的绝对路径
PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"

# 构建与打包输出路径
BUILD_PRESET="macos-release"
BUILD_DIR="${PROJECT_ROOT}/out/build/${BUILD_PRESET}"
DIST_DIR="${PROJECT_ROOT}/out/dist"
APP_NAME="${TARGET_NAME}.app"
APP_PATH="${DIST_DIR}/${APP_NAME}"

echo "=========================================="
echo "🍎 开始为 macOS 打包项目: ${TARGET_NAME}"
echo "项目根目录: ${PROJECT_ROOT}"
echo "构建目录: ${BUILD_DIR}"
echo "打包输出: ${APP_PATH}"
echo "=========================================="

# --- 2. 编译 Release 版本 ---

# 检查 CMakePresets.json 预设，执行配置
echo "⚙️ 正在配置 CMake Release 预设..."
cmake --preset ${BUILD_PRESET}

# 执行编译
echo "🛠️ 正在编译 Release 版本..."
cmake --build "${BUILD_DIR}" --config Release --target "${TARGET_NAME}"

# --- 3. 创建 macOS App Bundle 结构 ---

echo "📂 创建 .app 目录结构..."
# 清理旧的打包文件
rm -rf "${APP_PATH}"
# 创建 macOS 标准 Application Bundle 目录
# Contents/MacOS: 存放可执行二进制文件
# Contents/Resources: 存放静态资源文件（如 data 目录）
# Contents/Frameworks: 存放打包进来的第三方动态库（如 SDL3.dylib）
mkdir -p "${APP_PATH}/Contents/MacOS"
mkdir -p "${APP_PATH}/Contents/Resources"
mkdir -p "${APP_PATH}/Contents/Frameworks"

# 拷贝编译好的可执行文件到 Contents/MacOS
cp "${BUILD_DIR}/${TARGET_NAME}/${TARGET_NAME}" "${APP_PATH}/Contents/MacOS/"

# 拷贝静态资源文件夹 (data) 到 Resources
if [ -d "${PROJECT_ROOT}/data" ]; then
    echo "🎨 正在拷贝静态资源目录 (data)..."
    cp -R "${PROJECT_ROOT}/data" "${APP_PATH}/Contents/Resources/"
else
    echo "⚠️ 未发现 data/ 资源目录，跳过拷贝。"
fi

# 创建 Info.plist 文件提供 App 基础元数据
cat > "${APP_PATH}/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>zh_CN</string>
    <key>CFBundleExecutable</key>
    <string>${TARGET_NAME}</string>
    <key>CFBundleIdentifier</key>
    <string>com.miniproject.${TARGET_NAME}</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>${TARGET_NAME}</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0.0</string>
    <key>CFBundleSignature</key>
    <string>????</string>
    <key>CFBundleVersion</key>
    <string>1.0.0</string>
    <key>LSMinimumSystemVersion</key>
    <string>11.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
EOF

# --- 4. 封装动态库 (实现免安装运行) ---

# 检测是否安装了 dylibbundler 工具（专门用于 macOS 依赖打包的利器）
if command -v dylibbundler &> /dev/null; then
    echo "📦 发现 dylibbundler，开始打包依赖库并修复 rpath..."
    # -od: 自动搜索并把系统/外部库（如 Homebrew 安装的 SDL3）拷贝到指定的 Frameworks 目录
    # -b: 自动修改可执行文件中的动态库加载路径为相对路径（指向 Bundle 内的 Frameworks）
    # -x: 目标可执行文件的路径
    # -d: 目标动态库拷贝输出目录
    # -p: 库的运行时搜索路径 @executable_path/../Frameworks/
    dylibbundler -od -b \
        -x "${APP_PATH}/Contents/MacOS/${TARGET_NAME}" \
        -d "${APP_PATH}/Contents/Frameworks/" \
        -p "@executable_path/../Frameworks/"
    echo "✅ 依赖动态库打包完成。"
else
    echo "⚠️ 未安装 dylibbundler。打包生成的 .app 可能只能在已通过 Homebrew 安装了 SDL3 依赖的电脑上运行。"
    echo "💡 提示: 建议通过 'brew install dylibbundler' 安装此工具以进行完全独立的本地化发行。"
fi

# --- 5. 生成分发包 (.dmg / .zip) ---

echo "📦 正在打包为分发格式..."
cd "${DIST_DIR}"
# 打包为 zip 文件，方便传输
zip -r -y "${TARGET_NAME}-macOS.zip" "${APP_NAME}" > /dev/null
echo "✅ 成功生成 Zip 包: ${DIST_DIR}/${TARGET_NAME}-macOS.zip"

# 如果系统有 create-dmg 工具，可自动生成 DMG 镜像
if command -v create-dmg &> /dev/null; then
    echo "💿 发现 create-dmg，正在生成 DMG 镜像..."
    rm -f "${TARGET_NAME}-macOS.dmg"
    create-dmg \
      --volname "${TARGET_NAME} Installer" \
      --window-size 600 400 \
      --icon-size 100 \
      --icon "${APP_NAME}" 200 190 \
      --hide-extension "${APP_NAME}" \
      --app-drop-link 400 190 \
      "${TARGET_NAME}-macOS.dmg" \
      "${APP_NAME}"
    echo "✅ 成功生成 DMG 安装包: ${DIST_DIR}/${TARGET_NAME}-macOS.dmg"
else
    echo "💡 提示: 可选安装 'brew install create-dmg' 以自动生成美观的 DMG 安装包。"
fi

echo "=========================================="
echo "🎉 打包流程顺利完成！"
echo "输出文件所在目录: ${DIST_DIR}"
echo "=========================================="

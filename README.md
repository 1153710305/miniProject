# 🚀 miniProject - SDL3 + ImGui 开发项目

基于 **SDL3 + Vulkan + ImGui + GLM** 的 C++ 多应用开发项目，使用 CMake 构建，开箱即用。

---

## 📋 环境要求

- **操作系统**：Windows 10
- **IDE**：**Visual Studio 2026**（内置 CMake / Ninja）
- **工具链**：CMake 3.15+、Ninja（VS 内置）
- **编译器**：MSVC (cl.exe)

---

## 📦 依赖库与目录结构

将以下第三方库**下载并解压**到项目根目录的 `vendored/` 文件夹：

| 库名称 | 版本 | 解压后目录名 | 下载地址 |
|---|---|---|---|
| **SDL3** | 3.4.14 | `vendored/SDL3-3.4.14` | [🔗 SDL Releases](https://github.com/libsdl-org/SDL/releases) |
| **SDL3_image** | 3.4.4 | `vendored/SDL3_image-3.4.4` | [🔗 SDL_image Releases](https://github.com/libsdl-org/SDL_image/releases) |
| **SDL3_mixer** | 3.2.4 | `vendored/SDL3_mixer-3.2.4` | [🔗 SDL_mixer Releases](https://github.com/libsdl-org/SDL_mixer/releases) |
| **SDL3_ttf** | 3.2.2 | `vendored/SDL3_ttf-3.2.2` | [🔗 SDL_ttf Releases](https://github.com/libsdl-org/SDL_ttf/releases) |
| **GLM** | 1.0.3 | `vendored/glm-1.0.3-VC` | [🔗 GLM Releases](https://github.com/g-truc/glm/releases) |
| **ImGui** | docking/main 分支 | `vendored/imgui` | `git clone -b docking https://github.com/ocornut/imgui.git vendored/imgui` |

解压后应得到如下结构：
```
miniProject/
├── CMakeLists.txt
├── .gitignore
├── README.md
├── .vs/launch.vs.json          ✅ VS 启动配置
├── src/                        # 你的项目源码
└── vendored/
    ├── SDL3-3.4.14/
    │   ├── include/SDL3/SDL.h
    │   ├── lib/x64/SDL3.lib
    │   └── lib/cmake/SDL3/SDL3Config.cmake  ✅ 关键！
    ├── SDL3_image-3.4.4/
    ├── SDL3_mixer-3.2.4/
    ├── SDL3_ttf-3.2.2/
    ├── glm-1.0.3-VC/
    └── imgui/                  # docking/main 分支源码
```

> ⚠️ **重要：`.lib` / `.dll` 等二进制文件不要提交到 Git！只需按本文说明下载解压即可。**

---

## 🔨 使用 Visual Studio 2026 编译

### ✅ 方式一：VS 2026 直接打开（推荐）

1. 启动 **Visual Studio 2026** → 选择 **「打开本地文件夹」** → 选中项目根目录 `miniProject/`
2. VS 自动识别 CMake 项目并开始**自动配置**
3. 在 **「输出」- CMake** 面板查看配置结果，无报错即成功
4. 顶部**启动项下拉框**选择要运行的程序 → **按 F5 编译并运行** 🎉

> 💡 `launch.vs.json` 已配置好 SDL 库 DLL 路径，**无需手动复制 DLL 到输出目录**。
```launch.vs.json
{
  "version": "0.2.1",
  "defaults": {},
  "configurations": [
    {
      "type": "default",
      "project": "CMakeLists.txt",
      "projectTarget": "sdl3-demo.exe (sdl3-demo\\sdl3-demo.exe)",
      "name": "sdl3-demo.exe (sdl3-demo\\sdl3-demo.exe)",
      "env": {
        "PATH": "${env.PROJECT_ROOT}\\vendored\\SDL3-3.4.14\\lib\\x64;${env.PROJECT_ROOT}\\vendored\\SDL3_image-3.4.4\\lib\\x64;${env.PROJECT_ROOT}\\vendored\\SDL3_mixer-3.2.4\\lib\\x64;${env.PROJECT_ROOT}\\vendored\\SDL3_ttf-3.2.2\\lib\\x64;%PATH%"
      },
      "currentDir": "${env.PROJECT_ROOT}"
    }
  ]
}
```
```CMakeUserPresets.json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "x64-debug-user",
      "displayName": "x64 Debug (User)",
      "inherits": "x64-debug",
      "cacheVariables": {
        "CMAKE_PREFIX_PATH": "${sourceDir}/vendored" //所有的三方库文件夹都放到这里，后续就不用逐个增加了
      },
      "environment": { //转成了环境变量
        "PROJECT_ROOT": "${sourceDir}" // 👈 把项目根目录传出去
      }

    }
  ]
}

```

### 🛠️ 方式二：命令行编译
命令行编译需要 **CMake 3.15+** 和 **Ninja**，并且在 **Visual Studio 2026 的 x64 Native Tools 命令提示符** 中执行。（待补充）


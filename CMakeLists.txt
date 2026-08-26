# CMakeList.txt: 顶层 CMake 项目文件，在此处执行全局配置
# 并包含子项目。
#
cmake_minimum_required (VERSION 3.10)

# 如果支持，请为 MSVC 编译器启用热重载。
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()
file(GLOB_RECURSE IMGUI_SOURCES
    "imgui/*.cpp"
    "imgui/backends/*.cpp"
)

file(GLOB_RECURSE IMUI_HEADERS
    "imgui/*.h"
    "imgui/backends/*.h"
)
project ("sdl3-demo") #demo imgui和sdl3demo演示
project ("Undersea_Kingdom") #游戏
project ("cucumber") # 游戏引擎
project ("ilearning") #学习数据结构与算法
# 包含子项目。
add_subdirectory ("sdl3-demo")
add_subdirectory ("Undersea_Kingdom")
add_subdirectory ("cucumber")
add_subdirectory ("ilearning")

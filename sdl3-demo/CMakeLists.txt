# CMakeList.txt: sdl3-demo 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
# 让 CMake 自动在系统 / 预设路径中查找 SDL3 的安装包；REQUIRED 表示 “找不到就直接报错终止构建”，属于强制依赖。
find_package(SDL3 REQUIRED)
find_package(SDL3_image REQUIRED)
find_package(SDL3_mixer REQUIRED)
find_package(SDL3_ttf REQUIRED)
find_package(glm REQUIRED)

# 将源代码添加到此项目的可执行文件。
add_executable (sdl3-demo 
    "sdl3-demo.cpp" 
    #"main.cpp" 
    ${IMGUI_SOURCES}
    ${IMUI_HEADERS}  # 可选：只是让 VS 解决方案里显示头文件
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET sdl3-demo PROPERTY CXX_STANDARD 20)
endif()

target_link_libraries(sdl3-demo PRIVATE SDL3::SDL3 SDL3_image::SDL3_image SDL3_mixer::SDL3_mixer SDL3_ttf::SDL3_ttf glm::glm)
target_include_directories(sdl3-demo PRIVATE 
    ../vendored/cJSON 
    ../vendored/stb 
    ../imgui
    ../imgui/backends
)
# TODO: 如有需要，请添加测试并安装目标。

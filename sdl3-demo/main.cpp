// Dear ImGui: standalone example application for SDL3 + SDL_GPU
// （SDL 是一个跨平台通用库，用于处理窗口、输入、OpenGL/Vulkan/Metal 图形上下文创建等）

// 了解 Dear ImGui：
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs（与本地 docs/ 文件夹内容相同）
// - 更多介绍、链接见 imgui.cpp 文件顶部注释

// 给希望将 imgui_impl_sdlgpu3.cpp/.h 集成到自己引擎/应用中的读者的重要提示：
// - 与其他后端不同，用户必须在发起包含 ImGui_ImplSDLGPU_RenderDrawData 的
//   SDL_GPURenderPass 之前，调用 ImGui_ImplSDLGPU_PrepareDrawData() 函数。
//   这个调用是【强制性的】，否则 ImGui 既不会上传顶点缓冲区也不会上传索引缓冲区到 GPU。
//   详见 imgui_impl_sdlgpu3.cpp 中的说明。

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"
#include <stdio.h>          // printf, fprintf
#include <stdlib.h>         // abort
#include <SDL3/SDL.h>

// 本示例暂不支持 Emscripten 编译！尚在等待 SDL3 对 Emscripten 的支持。
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

// ============================================================
// 主函数
// ============================================================
int main(int, char**)
{
    // ---- 初始化 SDL ----
    // [如果使用 SDL_MAIN_USE_CALLBACKS：下面这段直到主循环开始之前的代码，
    //  通常应放入你的 SDL_AppInit() 回调函数中]
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) // 初始化视频子系统和手柄子系统
    {
        printf("Error: SDL_Init(): %s\n", SDL_GetError());
        return 1;
    }

    // ---- 创建 SDL 窗口及图形上下文相关参数 ----
    // 获取主显示器的内容缩放比例（用于高 DPI 屏幕下的界面自适应缩放）
    float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    // 窗口标志：可调整大小、初始隐藏（等待布局/上下文准备好后再显示，避免闪烁）、支持高像素密度（Retina/HiDPI）
    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    // 创建窗口，宽高按 DPI 缩放比例进行调整，保证在高分屏上视觉尺寸一致
    SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL3+SDL_GPU example", (int)(1280 * main_scale), (int)(800 * main_scale), window_flags);
    if (window == nullptr)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return 1;
    }
    // 窗口居中显示
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    // 之前创建时是隐藏状态，这里显式显示窗口
    SDL_ShowWindow(window);

    // ---- 创建 GPU 设备（SDL3 的新 GPU API，抽象了 Vulkan/DirectX/Metal 等底层图形 API）----
    // 同时声明支持的着色器格式：SPIR-V（Vulkan）、DXIL（DirectX12）、MSL/MetalLib（Metal）
    // 第二个参数 true 表示开启调试模式（debug mode）
    SDL_GPUDevice* gpu_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_METALLIB, true, nullptr);
    if (gpu_device == nullptr)
    {
        printf("Error: SDL_CreateGPUDevice(): %s\n", SDL_GetError());
        return 1;
    }

    // ---- 将窗口与 GPU 设备关联（“认领”窗口，使其可以作为 GPU 渲染的呈现目标）----
    if (!SDL_ClaimWindowForGPUDevice(gpu_device, window))
    {
        printf("Error: SDL_ClaimWindowForGPUDevice(): %s\n", SDL_GetError());
        return 1;
    }
    // 设置交换链（swapchain）参数：SDR 色彩合成模式 + 垂直同步呈现模式（避免画面撕裂）
    SDL_SetGPUSwapchainParameters(gpu_device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);

    // ============================================================
    // 初始化 Dear ImGui 上下文
    // ============================================================
    IMGUI_CHECKVERSION(); // 检查头文件与编译库的版本是否一致，防止 ABI 不匹配导致的诡异崩溃
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io; // 获取 IO 结构体引用；(void)io 避免未使用变量警告（虽然下面用到了）
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // 开启键盘导航控制（Tab/方向键在控件间切换焦点）
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // 开启手柄导航控制

    // ---- 设置 Dear ImGui 主题风格 ----
    ImGui::StyleColorsDark();   // 使用暗色主题
    //ImGui::StyleColorsLight(); // （备选）亮色主题

    // ---- 设置界面缩放（适配高 DPI 屏幕）----
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);        // 将所有尺寸（边距、圆角等）按缩放比例整体放大，“烘焙”成固定值
    //（目前若中途改变缩放比例，需要重置 Style 后重新调用本函数）
    style.FontScaleDpi = main_scale;        // 设置初始字体缩放比例
    //（在 docking 分支中：设置 io.ConfigDpiScaleFonts=true 后，
    //  ImGui 会针对每个窗口所在显示器自动覆盖此设置）

// ============================================================
// 初始化平台层/渲染器后端
// ============================================================
    ImGui_ImplSDL3_InitForSDLGPU(window); // 初始化 SDL3 平台后端（处理输入事件、剪贴板、窗口等与 SDL 的对接）

    // 配置 SDL_GPU 渲染后端的初始化信息
    ImGui_ImplSDLGPU3_InitInfo init_info = {};
    init_info.Device = gpu_device;
    init_info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(gpu_device, window); // 颜色目标格式需与交换链纹理格式一致
    init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;                      // 多重采样数（仅在多视口模式下才会用到）
    init_info.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;  // 交换链合成模式（仅在多视口模式下才会用到）
    init_info.PresentMode = SDL_GPU_PRESENTMODE_VSYNC;                  // 呈现模式：垂直同步
    ImGui_ImplSDLGPU3_Init(&init_info);

    // ============================================================
    // 加载字体（此示例中默认全部注释掉，使用 ImGui 内置字体）
    // ============================================================
    // - 若未显式加载字体，Dear ImGui 会自动选用内置字体：
    //   AddFontDefaultVector()（矢量字体）或 AddFontDefaultBitmap()（位图字体）。
    //   具体选用哪种取决于 (style.FontSizeBase * style.FontScaleMain * style.FontScaleDpi) 是否达到某个较小的阈值。
    // - 可以加载多个字体，并通过 ImGui::PushFont()/PopFont() 在使用时切换。
    // - 若文件无法加载，AddFont 系列函数会返回 nullptr，请在代码中妥善处理这种错误
    //   （例如使用断言、提示错误信息后退出等）。
    // - 更多说明和细节请阅读 'docs/FONTS.md'。
    // - 在 imconfig 文件中定义 '#define IMGUI_ENABLE_FREETYPE' 可使用 FreeType 获得更高质量的字体渲染。
    // - 注意：在 C/C++ 字符串字面量中如果想输入反斜杠 \，需要写成双反斜杠 \\ ！
    //style.FontSizeBase = 20.0f;
    //io.Fonts->AddFontDefaultVector();
    //io.Fonts->AddFontDefaultBitmap();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
    //IM_ASSERT(font != nullptr);

    // ---- 应用程序自身的状态变量（用于演示窗口的交互）----
    bool show_demo_window = true;                              // 是否显示 ImGui 自带的“演示窗口”（内含大量控件示例）
    bool show_another_window = false;                          // 是否显示自定义的“另一个窗口”
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);   // 用作清屏颜色（RGBA，浮点数 0..1 范围）

    // ============================================================
    // 主循环
    // ============================================================
    bool done = false;
    while (!done)
    {
        // ---- 轮询并处理事件（输入、窗口大小变化等）----
        // 可通过 io.WantCaptureMouse / io.WantCaptureKeyboard 标志判断 Dear ImGui 是否想要“占用”本次输入。
        // - 当 io.WantCaptureMouse 为 true 时，不要再将鼠标输入分发给你自己的应用逻辑，也不要清空/覆盖你自己保存的鼠标数据。
        // - 当 io.WantCaptureKeyboard 为 true 时，同理不要再将键盘输入分发给你自己的应用逻辑。
        // 通常做法是：始终把所有输入都传给 Dear ImGui，再根据这两个标志决定是否也让你自己的应用处理这些输入。
        // [如果使用 SDL_MAIN_USE_CALLBACKS：应在你的 SDL_AppEvent() 函数中调用 ImGui_ImplSDL3_ProcessEvent()]
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event); // 把原始 SDL 事件转交给 ImGui 的 SDL3 后端处理（更新鼠标/键盘/窗口状态等）
            if (event.type == SDL_EVENT_QUIT)
                done = true; // 收到系统级退出事件（例如用户关闭所有窗口/系统注销等）
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
                done = true; // 用户点击了本窗口的关闭按钮
        }

        // [如果使用 SDL_MAIN_USE_CALLBACKS：下面这部分代码通常应放入你的 SDL_AppIterate() 函数]

        // 若窗口处于最小化状态，跳过本次渲染，短暂休眠后进入下一次循环，节省 CPU/GPU 资源
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            continue;
        }

        // ---- 开始新的一帧 Dear ImGui ----
        ImGui_ImplSDLGPU3_NewFrame(); // 通知 SDL_GPU 后端准备新一帧的渲染资源
        ImGui_ImplSDL3_NewFrame();    // 通知 SDL3 平台后端更新时间/显示器信息等
        ImGui::NewFrame();            // ImGui 核心库开始收集本帧的 UI 绘制命令

        // 1. 显示 ImGui 内置的“大型演示窗口”
        //    （ShowDemoWindow() 的源码中包含了绝大部分示例代码，浏览其源码可以学到很多 Dear ImGui 的用法！）
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 2. 显示一个我们自己创建的简单窗口。使用 Begin/End 配对来创建一个具名窗口。
        {
            static float f = 0.0f;      // static 局部变量：跨帧保留数值，模拟一个“持久化”的 UI 状态
            static int counter = 0;

            ImGui::Begin("Hello, world!");                          // 创建一个名为 "Hello, world!" 的窗口，并将后续控件添加进去

            ImGui::Text("This is some useful text.");               // 显示一段文本（也支持格式化字符串，类似 printf）
            ImGui::Checkbox("Demo Window", &show_demo_window);      // 复选框，编辑我们用来控制窗口显示/隐藏的 bool 变量
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // 滑动条，编辑一个 0.0f 到 1.0f 范围的浮点数
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // 颜色编辑器，编辑代表颜色的 3 个浮点数（RGB）

            if (ImGui::Button("Button"))                            // 按钮：被点击时返回 true（大多数控件在“被编辑/激活”时都会返回 true）
                counter++;
            ImGui::SameLine();                                      // 让下一个控件与上一个控件保持在同一行
            ImGui::Text("counter = %d", counter);

            // 显示应用程序的平均帧时间与帧率（io.Framerate 由 ImGui 内部统计得出）
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        // 3. 显示另一个简单窗口
        if (show_another_window)
        {
            // 传入指向 bool 变量的指针，窗口右上角会出现一个关闭按钮，点击后会自动将该 bool 置为 false
            ImGui::Begin("Another Window", &show_another_window);
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }

        // ============================================================
        // 渲染阶段
        // ============================================================
        ImGui::Render(); // 结束本帧的 UI 记录，生成待绘制的顶点/索引数据（此时尚未真正提交给 GPU）
        ImDrawData* draw_data = ImGui::GetDrawData(); // 获取本帧的绘制数据（顶点、索引、绘制命令列表等）
        // 若显示区域尺寸小于等于 0（例如窗口被最小化导致的极端情况），视为“已最小化”，跳过实际渲染
        const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);

        // 从 GPU 设备获取一个命令缓冲区，后续的 GPU 指令（拷贝、渲染等）都要记录到这个缓冲区中
        SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(gpu_device);

        // 从交换链中获取一张可供渲染的纹理（即“这一帧要画到哪张画布上”）
        SDL_GPUTexture* swapchain_texture;
        SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, window, &swapchain_texture, nullptr, nullptr);

        if (swapchain_texture != nullptr && !is_minimized)
        {
            // 这一步是【强制性的】：必须调用 ImGui_ImplSDLGPU3_PrepareDrawData()
            // 来把顶点/索引缓冲区上传到 GPU，否则后面渲染时将没有数据可画！
            ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, command_buffer);

            // ---- 配置并开始一个渲染通道（Render Pass）----
            SDL_GPUColorTargetInfo target_info = {};
            target_info.texture = swapchain_texture;                                    // 本次渲染通道的颜色输出目标：交换链纹理
            target_info.clear_color = SDL_FColor{ clear_color.x, clear_color.y, clear_color.z, clear_color.w }; // 清屏颜色
            target_info.load_op = SDL_GPU_LOADOP_CLEAR;    // 加载操作：先清空目标纹理内容（而非保留上一帧内容）
            target_info.store_op = SDL_GPU_STOREOP_STORE;  // 存储操作：渲染结果需要被保留下来（用于显示），而非直接丢弃
            target_info.mip_level = 0;                     // 使用第 0 级 mipmap（未使用 mipmap 链的普通渲染目标）
            target_info.layer_or_depth_plane = 0;           // 使用第 0 层（非纹理数组/立体渲染场景）
            target_info.cycle = false;                      // 不进行资源“循环”（cycle）复用，保持简单场景下的默认行为
            SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command_buffer, &target_info, 1, nullptr);

            // 将本帧的 ImGui 绘制数据实际记录到当前渲染通道中（生成绘制指令）
            ImGui_ImplSDLGPU3_RenderDrawData(draw_data, command_buffer, render_pass);

            // 结束本次渲染通道
            SDL_EndGPURenderPass(render_pass);
        }

        // 提交命令缓冲区，交由 GPU 实际执行本帧记录的所有指令（包括呈现到屏幕）
        SDL_SubmitGPUCommandBuffer(command_buffer);
    }

    // ============================================================
    // 清理资源
    // ============================================================
    // [如果使用 SDL_MAIN_USE_CALLBACKS：下面这部分代码通常应放入你的 SDL_AppQuit() 函数]
    SDL_WaitForGPUIdle(gpu_device);   // 等待 GPU 完成所有未处理的工作，确保后续销毁资源时 GPU 不再访问它们
    ImGui_ImplSDL3_Shutdown();        // 关闭 ImGui 的 SDL3 平台后端
    ImGui_ImplSDLGPU3_Shutdown();     // 关闭 ImGui 的 SDL_GPU 渲染后端
    ImGui::DestroyContext();          // 销毁 ImGui 上下文，释放其内部所有资源

    SDL_ReleaseWindowFromGPUDevice(gpu_device, window); // 解除窗口与 GPU 设备的关联
    SDL_DestroyGPUDevice(gpu_device);                    // 销毁 GPU 设备
    SDL_DestroyWindow(window);                            // 销毁窗口
    SDL_Quit();                                            // 退出 SDL，释放所有 SDL 子系统资源

    return 0;
}

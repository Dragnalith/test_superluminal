// TODO
// - foundation, engine, app
// - frame-centric mainloop
// - re-use fiber
// - renderer independent from the swapchain
// - Input independent from Win32

#include <imgui/imgui.h>

#include <fnd/Window.h>
#include <fnd/WindowManager.h>
#include <fnd/JobSystem.h>
#include <fw/RenderDevice.h>
#include <fw/SwapChain.h>
#include <fw/DearImGuiManager.h>
#include <fw/Renderer.h>
#include <fw/FrameData.h>
#include <fw/DefaultFramePipeline.h>
#include <app/Game.h>

#include <Superluminal/PerformanceAPI.h>
#include <iostream>

int main()
{
    engine::JobSystem::Start([] () {
        std::cout << "Hello World\n";
        engine::JobHandle handle;
        for (int i = 0; i < 10; i++) {
            engine::JobSystem::DispatchJob(handle, [i] {
                std::cout << "Some Job: " << i << "\n";
            });
        }
        engine::JobSystem::Wait(handle);
        std::cout << "Good Night World\n";

    });

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    PerformanceAPI_SetCurrentThreadName("Main Thread");
    engine::WindowManager windowManager;
    engine::Window& window = windowManager.Create("Game Engine With Fiber");
    engine::RenderDevice renderDevice;
    engine::SwapChain swapChain(renderDevice, window);
    engine::DearImGuiManager imguiManager;
    engine::Renderer renderer(renderDevice, swapChain, imguiManager);
    app::Game game;
    engine::DefaultFramePipeline pipeline(windowManager, window, imguiManager, renderer, game);

    // Main loop
    auto lastQuitTime = window.GetLastQuitTime();
    engine::Clock::time_point lastFrameTime = engine::Clock::now() - std::chrono::milliseconds(16);
    uint64_t frameIndex = 0;
    while (lastQuitTime >= window.GetLastQuitTime())
    {
        engine::FrameData frameData;
        frameData.frameIndex = frameIndex;
        frameIndex += 1;
        std::string frameName = std::format("Index = {}", frameIndex);
        PERFORMANCEAPI_INSTRUMENT_DATA("Frame", frameName.c_str());
        {
            PERFORMANCEAPI_INSTRUMENT_DATA("Update", frameName.c_str());
            auto now = engine::Clock::now();
            std::chrono::duration<float> deltatime = now - lastFrameTime;
            lastFrameTime = now;
            frameData.deltatime = deltatime.count();

            pipeline.Update(frameData);
        }

        {
            PERFORMANCEAPI_INSTRUMENT_DATA("Render", frameName.c_str());
            pipeline.Render(frameData); 
        }

        {
            PERFORMANCEAPI_INSTRUMENT_DATA("Kick", frameName.c_str());
            pipeline.Kick(frameData);
        }

        {
            PERFORMANCEAPI_INSTRUMENT_DATA("Clean", frameName.c_str());
            pipeline.Clean(frameData);
        }
    }

    return 0;
}
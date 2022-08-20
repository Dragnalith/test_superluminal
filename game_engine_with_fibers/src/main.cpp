// TODO
// - Platform Lib
// - Input independent from Win32
// - precompiled header

#include <imgui/imgui.h>

#include <app/Window.h>
#include <app/RenderDevice.h>
#include <app/SwapChain.h>
#include <app/WindowManager.h>
#include <app/DearImGuiManager.h>
#include <app/Renderer.h>
#include <app/FrameData.h>

#include <Superluminal/PerformanceAPI.h>

struct AppState
{
    bool fullscreen = false;
    bool vsync = true;
    bool show_demo_window = true;
    bool show_another_window = false;
    float f = 0.0f;
    int counter = 0;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
};

void UpdateGUI(AppState& state) {
    // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
    if (state.show_demo_window)
        ImGui::ShowDemoWindow(&state.show_demo_window);

    // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
    {
        ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

        ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
        ImGui::Checkbox("Demo Window", &state.show_demo_window);      // Edit bools storing our window open/close state
        ImGui::Checkbox("Another Window", &state.show_another_window);

        ImGui::SliderFloat("float", &state.f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3("clear color", (float*)&state.clear_color); // Edit 3 floats representing a color

        if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
            state.counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", state.counter);

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::Checkbox("Fullscreen", &state.fullscreen);
        ImGui::Checkbox("Vsync", &state.vsync);

        ImGui::End();
    }

    // 3. Show another simple window.
    if (state.show_another_window)
    {
        ImGui::Begin("Another Window", &state.show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
        ImGui::Text("Hello from another window!");
        if (ImGui::Button("Close Me"))
            state.show_another_window = false;
        ImGui::End();
    }
}

void CopyImDrawData(const ImDrawData* fromData, app::FrameData* frameData) {
    frameData->drawData.Valid = fromData->Valid;
    frameData->drawData.CmdListsCount = fromData->CmdListsCount;
    frameData->drawData.TotalIdxCount = fromData->TotalIdxCount;
    frameData->drawData.TotalVtxCount = fromData->TotalVtxCount;
    frameData->drawData.DisplayPos = fromData->DisplayPos;
    frameData->drawData.DisplaySize = fromData->DisplaySize;
    frameData->drawData.FramebufferScale = fromData->FramebufferScale;

    for (size_t i = 0; i < frameData->drawData.CmdListsCount; ++i) {
        frameData->drawData.DrawLists.emplace_back();
        
        ImDrawList& fromList = *fromData->CmdLists[i];
        app::DrawList& toList = frameData->drawData.DrawLists.back();
        toList.Flags = fromList.Flags;
        toList.CmdBuffer = fromList.CmdBuffer;
        toList.IdxBuffer = fromList.IdxBuffer;
        toList.VtxBuffer = fromList.VtxBuffer;
        ASSERT_MSG(toList.CmdBuffer.size() == fromList.CmdBuffer.size(), "ImGUI cmdbuffer data copy failed");
        ASSERT_MSG(toList.IdxBuffer.size() == fromList.IdxBuffer.size(), "ImGUI idxbuffer data copy failed");
        ASSERT_MSG(toList.VtxBuffer.size() == fromList.VtxBuffer.size(), "ImGUI vtxbuffer data copy failed");
    }
    ASSERT_MSG(frameData->drawData.DrawLists.size() == frameData->drawData.CmdListsCount, "ImGUI data copy failed");
}

int main()
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    PerformanceAPI_SetCurrentThreadName("Main Thread");
    app::WindowManager windowManager;
    app::Window& window = windowManager.Create("Game Engine With Fiber");
    app::RenderDevice renderDevice;
    app::SwapChain swapChain(renderDevice, window);
    app::DearImGuiManager imguiManager;
    app::Renderer renderer(renderDevice, swapChain, imguiManager);

    AppState state;
    // Main loop
    auto lastQuitTime = window.GetLastQuitTime();
    app::Clock::time_point lastFrameTime = app::Clock::now() - std::chrono::milliseconds(16);
    uint64_t frameIndex = 0;
    while (lastQuitTime >= window.GetLastQuitTime())
    {
        app::FrameData frameData;
        frameData.frameIndex = frameIndex;
        frameIndex += 1;
        std::string frameName = std::format("Index = {}", frameIndex);
        PERFORMANCEAPI_INSTRUMENT_DATA("Frame", frameName.c_str());
        {
            PERFORMANCEAPI_INSTRUMENT_DATA("Update", frameName.c_str());
            auto now = app::Clock::now();
            std::chrono::duration<float> deltatime = now - lastFrameTime;
            lastFrameTime = now;


            frameData.deltatime = deltatime.count();
            window.GetSize(&frameData.width, &frameData.height);

            // Update ImGUI
            for (auto& msg : window.PopMsg()) {
                imguiManager.WndProcHandler(msg.hWnd, msg.msg, msg.wParam, msg.lParam);
            }

            // Start the Dear ImGui frame
            imguiManager.Update(windowManager, window, deltatime.count());

            UpdateGUI(state);
            frameData.fullscreen = state.fullscreen;
            frameData.vsync = state.vsync;

            ImGui::Render(); // Prepare ImDrawData for the current frame
            ImDrawData* drawData = ImGui::GetDrawData(); // Valid until DearImGuiManager::Update()
            CopyImDrawData(drawData, &frameData);
        }

        {
            PERFORMANCEAPI_INSTRUMENT_DATA("Render", frameName.c_str());
            renderer.Render(frameData); 
        }

        {
            PERFORMANCEAPI_INSTRUMENT_DATA("Kick", frameName.c_str());
            renderer.Kick(frameData);
        }
    }

    return 0;
}
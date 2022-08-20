// TODO
// - Copy ImDrawData
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

struct AppState
{
    bool fullscreen = false;
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

int main()
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
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
    while (lastQuitTime >= window.GetLastQuitTime())
    {
        renderer.WaitForPresent();
        auto now = app::Clock::now();
        std::chrono::duration<float> deltatime = now - lastFrameTime;
        lastFrameTime = now;

        // Update ImGUI
        for (auto& msg : window.PopMsg()) {
            imguiManager.WndProcHandler(msg.hWnd, msg.msg, msg.wParam, msg.lParam);
        }

        // Start the Dear ImGui frame
        imguiManager.Update(windowManager, window, deltatime.count());

        UpdateGUI(state);

        ImGui::Render(); // Prepare ImDrawData for the current frame
        ImDrawData* drawData = ImGui::GetDrawData(); // Valid until DearImGuiManager::Update()
        int w, h;
        window.GetSize(&w, &h);
        renderer.Render(w, h, state.fullscreen, drawData); 
    }

    return 0;
}
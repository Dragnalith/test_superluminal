#pragma once

#include <fw/FrameData.h>
#include <app/Game.h>
#include <fnd/Window.h>
namespace engine
{
struct FrameData;
}

namespace app
{

Game::Game() {
    m_lastQuitTime = engine::Window::GetMainWindow().GetLastQuitTime();
}
void Game::Update(engine::FrameData& frameData) 
{
    frameData.result.stop = m_lastQuitTime != engine::Window::GetMainWindow().GetLastQuitTime();
    // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
    if (m_show_demo_window)
        ImGui::ShowDemoWindow(&m_show_demo_window);

    // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
    {
        ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

        ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
        ImGui::Checkbox("Demo Window", &m_show_demo_window);      // Edit bools storing our window open/close state
        ImGui::Checkbox("Another Window", &m_show_another_window);

        ImGui::SliderFloat("float", &m_f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3("clear color", (float*)&m_clear_color); // Edit 3 floats representing a color

        if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
            m_counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", m_counter);

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::Checkbox("Fullscreen", &m_fullscreen);
        ImGui::Checkbox("Vsync", &m_vsync);
        int maxFrameLatency = frameData.maxFrameLatency;
        ImGui::SliderInt("MaxFrameLatency", &maxFrameLatency, 1, 3);
        frameData.result.maxFrameLatency = maxFrameLatency;

        ImGui::End();
    }

    // 3. Show another simple window.
    if (m_show_another_window)
    {
        ImGui::Begin("Another Window", &m_show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
        ImGui::Text("Hello from another window!");
        if (ImGui::Button("Close Me"))
            m_show_another_window = false;
        ImGui::End();
    }
    frameData.fullscreen = m_fullscreen;
    frameData.vsync = m_vsync;
}

}
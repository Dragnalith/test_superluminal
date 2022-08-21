// TODO
// - Why there is there latency during resize?
// - Why does it crash if compile with NDEBUG? (timing or synchronization issue?)
// - re-use fiber
// - renderer independent from the swapchain
// - Input independent from Win32
//
// Superluminal Bug:
// - JobName does not appear
// - Where is the "Idle" job?
// - White is not a color available
// - Rare crash
//
// Superluminal Readability
// - Hard to visualize which job/fiber is scheduled when because
// - Hard to understand the begin and the end of "zone"
//
// Superluminal Other note
// - About "find function", wouldn't it be good to see the result of selected function to function list and callgraph?
// - Still hard to visualize on the y-axis, would it be possible to remove some of the leaf function and hide some of the root function
// - Hard to understand "global information"

#include <imgui/imgui.h>

#include <fnd/Window.h>
#include <fnd/AppMain.h>
#include <fw/RenderDevice.h>
#include <fw/SwapChain.h>
#include <fw/DearImGuiManager.h>
#include <fw/Renderer.h>
#include <fw/FrameData.h>
#include <fw/DefaultFramePipeline.h>
#include <fw/FrameManager.h>
#include <app/Game.h>

#include <iostream>

void AppMain()
{
    engine::RenderDevice renderDevice;
    engine::SwapChain swapChain(renderDevice);
    engine::DearImGuiManager imguiManager;
    engine::Renderer renderer(renderDevice, swapChain, imguiManager);
    app::Game game;
    engine::DefaultFramePipeline pipeline(imguiManager, renderer, game);
    engine::FrameManager frameManager(pipeline);

    frameManager.Start();
}
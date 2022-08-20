// TODO
// - Renderer
// - Platform Lib
// - Input independent from Win32
// - precompiled header


// Dear ImGui: standalone example application for DirectX 12
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

// Important: to compile on 32-bit systems, the DirectX12 backend requires code to be compiled with '#define ImTextureID ImU64'.
// This is because we need ImTextureID to carry a 64-bit value and by default ImTextureID is defined as void*.
// This define is set in the example .vcxproj file and need to be replicated in your app or by adding it to your imconfig.h file.

#include <imgui/imgui.h>

#include <d3d12.h>
#include <dxgi1_4.h>
#include <tchar.h>

#include <iostream>

#include <app/Window.h>
#include <app/RenderDevice.h>
#include <app/SwapChain.h>
#include <app/WindowManager.h>
#include <app/DearImGuiManager.h>
#include <app/DearImGuiRenderer.h>

struct FrameContext
{
    ID3D12CommandAllocator* CommandAllocator;
    UINT64                  FenceValue;
};

// Data
static int const                    NUM_FRAMES_IN_FLIGHT = 1;
static FrameContext                 g_frameContext[NUM_FRAMES_IN_FLIGHT] = {};
static UINT                         g_frameIndex = 0;

//static ID3D12DescriptorHeap*        g_pd3dSrvDescHeap = NULL;
static ID3D12GraphicsCommandList*   g_pd3dCommandList = NULL;
static ID3D12Fence*                 g_fence = NULL;
static HANDLE                       g_fenceEvent = NULL;
static UINT64                       g_fenceLastSignaledValue = 0;


// Forward declarations of helper functions
bool CreateRenderer(app::RenderDevice& renderDevice);
void CleanupRenderer();
void WaitForLastSubmittedFrame();
FrameContext* WaitForNextFrameResources(app::SwapChain& swapChain);

// Main code
int main(int, char**)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    app::WindowManager windowManager;
    app::Window& window = windowManager.Create("Game Engine With Fiber");
    // Initialize Direct3D
    app::RenderDevice renderDevice;
    app::SwapChain swapChain(renderDevice, window);




    // Setup Platform/Renderer backends
    app::DearImGuiManager imguiManager;
    app::DearImGuiRenderer imguiRenderer(renderDevice, imguiManager);


    if (!CreateRenderer(renderDevice))
    {
        CleanupRenderer();

        return 1;
}

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    int w, h;
    window.GetSize(&w, &h);
    auto lastQuitTime = window.GetLastQuitTime();

    app::Clock::time_point lastFrameTime = app::Clock::now() - std::chrono::milliseconds(16);
    while (lastQuitTime >= window.GetLastQuitTime())
    {
        auto now = app::Clock::now();
        std::chrono::duration<float> deltatime = now - lastFrameTime;
        lastFrameTime = now;
        int nextW, nextH;
        window.GetSize(&nextW, &nextH);

        // Update ImGUI
        for (auto& msg : window.PopMsg()) {
            imguiManager.WndProcHandler(msg.hWnd, msg.msg, msg.wParam, msg.lParam);
        }
        // Update Swapchain
        if (nextW != w || nextH != h)
        {
            w = nextW;
            h = nextH;
            WaitForLastSubmittedFrame();
            swapChain.Resize(w, h);

        }

        // Start the Dear ImGui frame
        imguiManager.Update(windowManager, window, deltatime.count());

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }



        FrameContext* frameCtx = WaitForNextFrameResources(swapChain);
        frameCtx->CommandAllocator->Reset();

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource   = swapChain.GetCurrentResource();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
        g_pd3dCommandList->Reset(frameCtx->CommandAllocator, NULL);
        g_pd3dCommandList->ResourceBarrier(1, &barrier);

        // Render Dear ImGui graphics
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dCommandList->ClearRenderTargetView(swapChain.GetCurrentRenderTargetDescriptor(), clear_color_with_alpha, 0, NULL);
        g_pd3dCommandList->OMSetRenderTargets(1, &swapChain.GetCurrentRenderTargetDescriptor(), FALSE, NULL);
        // Rendering
        ImGui::Render(); // Prepare ImDrawData for the current frame
        imguiRenderer.Render(g_pd3dCommandList, ImGui::GetDrawData());

        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
        g_pd3dCommandList->ResourceBarrier(1, &barrier);
        g_pd3dCommandList->Close();

        renderDevice.GetCommandQueue()->ExecuteCommandLists(1, (ID3D12CommandList* const*)&g_pd3dCommandList);
        swapChain.Present(1);
        //swapChain->Present(0);

        UINT64 fenceValue = g_fenceLastSignaledValue + 1;
        renderDevice.GetCommandQueue()->Signal(g_fence, fenceValue);
        g_fenceLastSignaledValue = fenceValue;
        frameCtx->FenceValue = fenceValue;
    }

    WaitForLastSubmittedFrame();
   
    CleanupRenderer();

    return 0;
}

// Helper functions





bool CreateRenderer(app::RenderDevice& renderDevice)
{
    for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
        if (renderDevice.GetDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_frameContext[i].CommandAllocator)) != S_OK)
            return false;

    if (renderDevice.GetDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_frameContext[0].CommandAllocator, NULL, IID_PPV_ARGS(&g_pd3dCommandList)) != S_OK ||
        g_pd3dCommandList->Close() != S_OK)
        return false;

    if (renderDevice.GetDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)) != S_OK)
        return false;

    g_fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (g_fenceEvent == NULL)
        return false;

    return true;
}

void CleanupRenderer()
{
    for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
        if (g_frameContext[i].CommandAllocator) { g_frameContext[i].CommandAllocator->Release(); g_frameContext[i].CommandAllocator = NULL; }
    if (g_pd3dCommandList) { g_pd3dCommandList->Release(); g_pd3dCommandList = NULL; }
    if (g_fence) { g_fence->Release(); g_fence = NULL; }
    if (g_fenceEvent) { CloseHandle(g_fenceEvent); g_fenceEvent = NULL; }
}



void WaitForLastSubmittedFrame()
{
    FrameContext* frameCtx = &g_frameContext[g_frameIndex % NUM_FRAMES_IN_FLIGHT];

    UINT64 fenceValue = frameCtx->FenceValue;
    if (fenceValue == 0)
        return; // No fence was signaled

    frameCtx->FenceValue = 0;
    if (g_fence->GetCompletedValue() >= fenceValue)
        return;

    g_fence->SetEventOnCompletion(fenceValue, g_fenceEvent);
    WaitForSingleObject(g_fenceEvent, INFINITE);
}

FrameContext* WaitForNextFrameResources(app::SwapChain& swapChain)
{
    UINT nextFrameIndex = g_frameIndex + 1;
    g_frameIndex = nextFrameIndex;

    HANDLE waitableObjects[] = { swapChain.GetWaitableObject(), NULL};
    DWORD numWaitableObjects = 1;

    FrameContext* frameCtx = &g_frameContext[nextFrameIndex % NUM_FRAMES_IN_FLIGHT];
    UINT64 fenceValue = frameCtx->FenceValue;
    if (fenceValue != 0) // means no fence was signaled
    {
        frameCtx->FenceValue = 0;
        g_fence->SetEventOnCompletion(fenceValue, g_fenceEvent);
        waitableObjects[1] = g_fenceEvent;
        numWaitableObjects = 2;
    }

    WaitForMultipleObjects(numWaitableObjects, waitableObjects, TRUE, INFINITE);

    return frameCtx;
}
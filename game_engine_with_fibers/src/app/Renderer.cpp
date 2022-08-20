#include <app/Renderer.h>
#include <app/RenderDevice.h>
#include <app/DearImGuiRenderer.h>
#include <app/SwapChain.h>

#include <imgui/imgui.h>
#include <app/Util.h>
#include <d3d12.h>

#include <windows.h>

namespace app 
{

struct FrameContext
{
    ID3D12CommandAllocator* CommandAllocator;
};

struct RendererImpl {
    static constexpr int NUM_FRAMES_IN_FLIGHT = 8;

    RendererImpl(RenderDevice& device, SwapChain& sc, DearImGuiManager& manager) : renderDevice(device), swapChain(sc), imguiRenderer(device, manager) {}

    RenderDevice& renderDevice;
    SwapChain& swapChain;
    DearImGuiRenderer imguiRenderer;
    FrameContext frameContext[NUM_FRAMES_IN_FLIGHT] = {};
    uint64_t frameIndex = 0;
    uint64_t lastPresentedFrameIndex = 0;

    ID3D12GraphicsCommandList* commandList = nullptr;
    ID3D12Fence* gpuCompletionfence = nullptr;
    HANDLE gpuCompletionfenceEvent = nullptr;
    uint64_t fenceLastSignaledValue = 0;

    void WaitForPresent() {
        if (lastPresentedFrameIndex == 0) {
            return;
        }

        if (gpuCompletionfence->GetCompletedValue() >= lastPresentedFrameIndex)
            return;

        gpuCompletionfence->SetEventOnCompletion(lastPresentedFrameIndex, gpuCompletionfenceEvent);
        WaitForSingleObject(gpuCompletionfenceEvent, INFINITE);
    }
};

Renderer::Renderer(RenderDevice& device, SwapChain& sc, DearImGuiManager& manager)
    : m_impl(device, sc, manager)
{
    HRESULT result = S_OK;
    for (uint32_t i = 0; i < RendererImpl::NUM_FRAMES_IN_FLIGHT; i++) {
        result = m_impl->renderDevice.GetDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_impl->frameContext[i].CommandAllocator));
        ASSERT_MSG(result == S_OK, "Command Allocator Creation Failed");
    }

    result = m_impl->renderDevice.GetDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_impl->frameContext[0].CommandAllocator, NULL, IID_PPV_ARGS(&m_impl->commandList));
    ASSERT_MSG(result == S_OK, "CommandList creation has failed");
    ASSERT_MSG(m_impl->commandList->Close() == S_OK, "Command List is closed");

    result = m_impl->renderDevice.GetDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_impl->gpuCompletionfence));
    ASSERT_MSG(result == S_OK, "Fence creation has failed");

    m_impl->gpuCompletionfenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
    ASSERT_MSG(m_impl->gpuCompletionfenceEvent != nullptr, "Fence Event creation has failed");
}

Renderer::~Renderer()
{
    m_impl->WaitForPresent();

    for (UINT i = 0; i < RendererImpl::NUM_FRAMES_IN_FLIGHT; i++) {
        if (m_impl->frameContext[i].CommandAllocator) { m_impl->frameContext[i].CommandAllocator->Release(); m_impl->frameContext[i].CommandAllocator = nullptr; }
    }
    if (m_impl->commandList) { m_impl->commandList->Release(); m_impl->commandList = nullptr; }
    if (m_impl->gpuCompletionfence) { m_impl->gpuCompletionfence->Release(); m_impl->gpuCompletionfence = nullptr; }
    if (m_impl->gpuCompletionfenceEvent) { ::CloseHandle(m_impl->gpuCompletionfenceEvent); m_impl->gpuCompletionfenceEvent = nullptr; }
}

void Renderer::Render(int width, int height, bool fullscreen, ImDrawData* drawData) {

    if (m_impl->swapChain.NeedResize(width, height, fullscreen))
    {
        m_impl->WaitForPresent();
        m_impl->swapChain.Resize(width, height, fullscreen);
    }
    m_impl->frameIndex += 1;
    FrameContext* frameCtx = &m_impl->frameContext[m_impl->frameIndex % RendererImpl::NUM_FRAMES_IN_FLIGHT];
    frameCtx->CommandAllocator->Reset();

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource   = m_impl->swapChain.GetCurrentResource();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
    m_impl->commandList->Reset(frameCtx->CommandAllocator, NULL);
    m_impl->commandList->ResourceBarrier(1, &barrier);

    // Render Dear ImGui graphics
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
    m_impl->commandList->ClearRenderTargetView(m_impl->swapChain.GetCurrentRenderTargetDescriptor(), clear_color_with_alpha, 0, NULL);
    m_impl->commandList->OMSetRenderTargets(1, &m_impl->swapChain.GetCurrentRenderTargetDescriptor(), FALSE, NULL);

    m_impl->imguiRenderer.Render(m_impl->commandList, drawData);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
    m_impl->commandList->ResourceBarrier(1, &barrier);
    m_impl->commandList->Close();

    m_impl->WaitForPresent();
    m_impl->renderDevice.GetCommandQueue()->ExecuteCommandLists(1, (ID3D12CommandList* const*)&m_impl->commandList);
    m_impl->swapChain.Present(0);
    //swapChain->Present(0);
    m_impl->lastPresentedFrameIndex = m_impl->frameIndex;
    m_impl->renderDevice.GetCommandQueue()->Signal(m_impl->gpuCompletionfence, m_impl->lastPresentedFrameIndex);
}

void Renderer::WaitForPresent()
{
    m_impl->WaitForPresent();
}

}
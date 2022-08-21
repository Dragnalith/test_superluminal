#include <fnd/Util.h>

#include <fw/Renderer.h>
#include <fw/RenderDevice.h>
#include <private/DearImGuiRenderer.h>
#include <fw/SwapChain.h>
#include <fw/FrameData.h>

#include <imgui/imgui.h>
#include <d3d12.h>

#include <windows.h>

namespace engine 
{

struct RenderContextManaged : public RenderContext
{
    bool isUsed = false; // just to be able to assert if we used a render context currently used
};

struct RendererImpl {
    static constexpr int NUM_FRAMES_IN_FLIGHT = 8;

    RendererImpl(RenderDevice& device, SwapChain& sc, DearImGuiManager& manager) : renderDevice(device), swapChain(sc), imguiRenderer(device, manager) {}

    RenderDevice& renderDevice;
    SwapChain& swapChain;
    DearImGuiRenderer imguiRenderer;
    RenderContextManaged frameContext[NUM_FRAMES_IN_FLIGHT] = {};
    uint64_t lastPresentedFrameIndex = 0;

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

    RenderContext* Allocate(uint64_t frameIndex) {
        RenderContextManaged* frameCtx = &frameContext[frameIndex % RendererImpl::NUM_FRAMES_IN_FLIGHT];
        ASSERT_MSG(frameCtx->isUsed == false, "RenderContext is already used");
        frameCtx->isUsed = true;
        return frameCtx;
    }

    void Free(RenderContext* ctx) {
        for (int i = 0; i < RendererImpl::NUM_FRAMES_IN_FLIGHT; i++) {
            if (ctx == &frameContext[i]) {
                frameContext[i].isUsed = false;
            }
        }
    }
};

Renderer::Renderer(RenderDevice& device, SwapChain& sc, DearImGuiManager& manager)
    : m_impl(device, sc, manager)
{
    HRESULT result = S_OK;
    for (uint32_t i = 0; i < RendererImpl::NUM_FRAMES_IN_FLIGHT; i++) {
        result = m_impl->renderDevice.GetDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_impl->frameContext[i].CommandAllocator));
        ASSERT_MSG(result == S_OK, "Command Allocator Creation Failed");
        result = m_impl->renderDevice.GetDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_impl->frameContext[0].CommandAllocator, NULL, IID_PPV_ARGS(&m_impl->frameContext[i].commandList));
        ASSERT_MSG(result == S_OK, "CommandList creation has failed");
        result = m_impl->frameContext[i].commandList->Close();
        ASSERT_MSG(result == S_OK, "Command List is closed");
    }


    result = m_impl->renderDevice.GetDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_impl->gpuCompletionfence));
    ASSERT_MSG(result == S_OK, "Fence creation has failed");

    m_impl->gpuCompletionfenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
    ASSERT_MSG(m_impl->gpuCompletionfenceEvent != nullptr, "Fence Event creation has failed");
}

Renderer::~Renderer()
{
    m_impl->WaitForPresent();

    for (UINT i = 0; i < RendererImpl::NUM_FRAMES_IN_FLIGHT; i++) {
        if (m_impl->frameContext[i].commandList) { m_impl->frameContext[i].commandList->Release(); m_impl->frameContext[i].commandList = nullptr; }
        if (m_impl->frameContext[i].CommandAllocator) { m_impl->frameContext[i].CommandAllocator->Release(); m_impl->frameContext[i].CommandAllocator = nullptr; }
    }
    if (m_impl->gpuCompletionfence) { m_impl->gpuCompletionfence->Release(); m_impl->gpuCompletionfence = nullptr; }
    if (m_impl->gpuCompletionfenceEvent) { ::CloseHandle(m_impl->gpuCompletionfenceEvent); m_impl->gpuCompletionfenceEvent = nullptr; }
}

void Renderer::Render(FrameData& frameData) {
    if (m_impl->swapChain.NeedResize(frameData.width, frameData.height, frameData.fullscreen))
    {
        m_impl->WaitForPresent();
        m_impl->swapChain.Resize(frameData.width, frameData.height, frameData.fullscreen);
    }
    RenderContext* renderCtx = m_impl->Allocate(frameData.frameIndex);
    renderCtx->CommandAllocator->Reset();
    frameData.renderContext = renderCtx;

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_impl->swapChain.GetCurrentResource();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    renderCtx->commandList->Reset(renderCtx->CommandAllocator, NULL);
    renderCtx->commandList->ResourceBarrier(1, &barrier);

    // Render Dear ImGui graphics
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
    renderCtx->commandList->ClearRenderTargetView(m_impl->swapChain.GetCurrentRenderTargetDescriptor(), clear_color_with_alpha, 0, NULL);
    renderCtx->commandList->OMSetRenderTargets(1, &m_impl->swapChain.GetCurrentRenderTargetDescriptor(), FALSE, NULL);

    m_impl->imguiRenderer.Render(renderCtx->commandList, frameData.drawData);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    renderCtx->commandList->ResourceBarrier(1, &barrier);
    renderCtx->commandList->Close();
}

void Renderer::Kick(const FrameData& frameData)
{
    m_impl->renderDevice.GetCommandQueue()->ExecuteCommandLists(1, (ID3D12CommandList* const*)&frameData.renderContext->commandList);
    m_impl->swapChain.Present(frameData.vsync ? 1 : 0);
    m_impl->lastPresentedFrameIndex = frameData.frameIndex;
    m_impl->renderDevice.GetCommandQueue()->Signal(m_impl->gpuCompletionfence, m_impl->lastPresentedFrameIndex);
    m_impl->WaitForPresent();
    m_impl->Free(frameData.renderContext);
}

void Renderer::WaitForPresent()
{
    m_impl->WaitForPresent();
}

}
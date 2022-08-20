#pragma once

#include <app/Util.h>

#include <windows.h>
#include <stdint.h>

struct IDXGISwapChain3;
struct ID3D12Resource;
struct D3D12_CPU_DESCRIPTOR_HANDLE;

namespace app
{

class RenderDevice;
class Window;

struct SwapChainImpl;

/*
 * Given a window, own swapchain object and related resources
 * 
 * The swapchain has the ability to be resized, but be careful about
 * synchronization. It should not be resized while it is used by the GPU
 * for some rendering
 */
class SwapChain
{
public:
    SwapChain(RenderDevice& device, Window& window);
    ~SwapChain();

    ID3D12Resource* GetCurrentResource() const;
    const D3D12_CPU_DESCRIPTOR_HANDLE& GetCurrentRenderTargetDescriptor() const;
    HANDLE GetWaitableObject() const;

    void Present(uint32_t syncInterval) const;
    bool NeedResize(int width, int height, bool fullscreen) const;

    void Resize(int width, int height, bool fullscreen);
private:
    Pimpl<SwapChainImpl> m_impl;
};

}
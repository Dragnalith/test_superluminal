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
class SwapChain
{
public:
    SwapChain(RenderDevice& device, Window& window);
    ~SwapChain();

    ID3D12Resource* GetCurrentResource() const;
    const D3D12_CPU_DESCRIPTOR_HANDLE& GetCurrentRenderTargetDescriptor() const;
    HANDLE GetWaitableObject() const;

    void Present(uint32_t syncInterval) const;
    bool NeedResize(int width, int height) const;

    void Resize(int width, int height);
private:
    Pimpl<SwapChainImpl> m_impl;
};

}
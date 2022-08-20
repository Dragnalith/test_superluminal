#pragma once

#include <app/Util.h>

struct ImDrawData;

namespace app
{

class RenderDevice;
class SwapChain;
class DearImGuiManager;
struct FrameData;

struct RendererImpl;

class Renderer
{
public:
	Renderer(RenderDevice& renderDevice, SwapChain& swapChain, DearImGuiManager& manager);
	~Renderer();

	void Render(const FrameData& frameData);
	void WaitForPresent();

private:
	Pimpl<RendererImpl> m_impl;
};

}
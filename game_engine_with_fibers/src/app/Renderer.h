#pragma once

#include <app/Util.h>

struct ImDrawData;

namespace app
{

class RenderDevice;
class SwapChain;
class DearImGuiManager;

struct RendererImpl;

class Renderer
{
public:
	Renderer(RenderDevice& renderDevice, SwapChain& swapChain, DearImGuiManager& manager);
	~Renderer();

	void Render(int width, int height, bool fullscreen, ImDrawData* drawData);
	void WaitForPresent();

private:
	Pimpl<RendererImpl> m_impl;
};

}
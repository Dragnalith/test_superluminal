#pragma once

#include <app/Util.h>
#include <app/Window.h>
#include <app/WindowManager.h>
#include <app/FrameData.h>

struct ID3D12GraphicsCommandList;
struct ImDrawData;

namespace app
{

class RenderDevice;
class DearImGuiManager;

struct DearImGuiRendererImpl;

class DearImGuiRenderer
{
public:
	DearImGuiRenderer(RenderDevice& renderDevice, DearImGuiManager& manager);
	~DearImGuiRenderer();

	void Render(ID3D12GraphicsCommandList* commandList, const DrawData& drawData);

private:
	Pimpl<DearImGuiRendererImpl> m_impl;
};

}
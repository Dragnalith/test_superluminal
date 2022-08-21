#pragma once

#include <windows.h>

#include <fnd/Window.h>
#include <fnd/WindowManager.h>
#include <imgui/imgui.h>

namespace engine
{
class DearImGuiManager
{
public:
	DearImGuiManager();
	~DearImGuiManager();

	void Update(WindowManager& windowManager, Window& window, float delattime);
	LRESULT WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
	ImGuiMouseCursor m_lastMouseCursor = ImGuiMouseCursor_COUNT;
};

}
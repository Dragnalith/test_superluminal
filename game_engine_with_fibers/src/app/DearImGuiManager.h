#pragma once

#include <windows.h>

#include <app/Window.h>
#include <app/WindowManager.h>
#include <imgui/imgui.h>

namespace app
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
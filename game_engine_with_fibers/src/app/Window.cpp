#include "Window.h"
#include <imgui/backends/imgui_impl_win32.h>
#include <tchar.h>

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace app
{
Window::Window(const char* windowName)
    : m_w(1280)
    , m_h(800)
{
    m_wc = { sizeof(WNDCLASSEXA), CS_CLASSDC, Window::WndProc, 0L, 0L, GetModuleHandleA(NULL), NULL, NULL, NULL, NULL, "app's Window Class", NULL };
    ::RegisterClassExA(&m_wc);
    m_hwnd = ::CreateWindowExA(0, m_wc.lpszClassName, windowName, WS_OVERLAPPEDWINDOW, 100, 100, m_w, m_h, NULL, NULL, m_wc.hInstance, NULL);
    ::SetWindowLongPtrA(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    // Show the window
    ::ShowWindow(m_hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(m_hwnd);
};

Window::~Window() {
    ::DestroyWindow(m_hwnd);
    ::UnregisterClassA(m_wc.lpszClassName, m_wc.hInstance);
}

LRESULT WINAPI Window::WndProcImpl(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    {
        std::scoped_lock scope(m_lock);
        m_messageStack.emplace_back(hWnd, msg, wParam, lParam);
    }
    switch (msg)
    {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            std::scoped_lock scope(m_lock);
            m_w = (UINT)LOWORD(lParam);
            m_h = (UINT)HIWORD(lParam);
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }

    return 0;
}


// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI Window::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LONG_PTR pLong = ::GetWindowLongPtrA(hWnd, GWLP_USERDATA);
    Window* window = reinterpret_cast<Window*>(pLong);

#if 0
    if (LRESULT r = ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
        return r;
    }
#endif

    if (window!= nullptr) {
        if (LRESULT r = window->WndProcImpl(hWnd, msg, wParam, lParam)) {
            return r;
        }
    }

    return ::DefWindowProcA(hWnd, msg, wParam, lParam);
}

}


#include "Window.h"
#include <imgui/backends/imgui_impl_win32.h>
#include <tchar.h>

namespace app
{

Window::Window(HWND hwnd, int w, int h)
    : m_hwnd(hwnd)
    , m_w(w)
    , m_h(h)
{
};

void Window::GetSize(uint32_t* w, uint32_t*h) const {
    std::scoped_lock scoped(m_lock);
    *w = m_w;
    *h = m_h;
}

std::vector<WindowMsg> Window::PopMsg() {
    std::vector<WindowMsg> result;
    {
        std::scoped_lock scoped(m_lock);
        m_messageStack.swap(result);
    }
    return result;
}

}


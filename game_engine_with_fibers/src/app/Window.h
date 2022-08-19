#pragma once

#include <windows.h>
#include <stdint.h>
#include <mutex>
#include <vector>

#include <app/SpinLock.h>

namespace app
{

struct WindowMsg {
    WindowMsg(HWND h, UINT m, WPARAM w, LPARAM l) : hWnd(h), msg(m), wParam(w), lParam(l) {}
    HWND hWnd;
    UINT msg;
    WPARAM wParam;
    LPARAM lParam;
};

class Window
{
public:
    Window(const char* name);
    ~Window();
    void GetSize(uint32_t* w, uint32_t*h) const {
        std::scoped_lock scoped(m_lock);
        *w = m_w;
        *h = m_h;
    }

    auto GetHandle() const { return m_hwnd; };

    std::vector<WindowMsg> PopMsg() {
        std::vector<WindowMsg> result;
        {
            std::scoped_lock scoped(m_lock);
            m_messageStack.swap(result);
        }
        return result;
    }


private:
    static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT WINAPI WndProcImpl(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    mutable SpinLock m_lock;
    HWND m_hwnd;
    WNDCLASSEXA m_wc;
    uint32_t m_w = 0;
    uint32_t m_h = 0;

    std::vector<WindowMsg> m_messageStack;
};

}
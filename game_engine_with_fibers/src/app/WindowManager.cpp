#include "WindowManager.h"

#include <app/Util.h>
#include <app/Window.h>

#include <windows.h>

#include <vector>

namespace app
{

    struct WindowManagerImpl {
        class WindowWin64 : public Window {
        public:
            WindowWin64(HWND hwnd, int w, int h) : Window(hwnd, w, h) {}

            LRESULT WINAPI WndProcImpl(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
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
                        m_w = static_cast<int>(LOWORD(lParam));
                        m_h = static_cast<int>(HIWORD(lParam));
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


        };

        auto FindWindow(HWND hwnd) {
            for (auto it = windows.begin(); it != windows.end(); it++) {
                if ((*it)->GetHandle() == hwnd) {
                    return it;
                }
            }

            return windows.end();
        }

        static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
        {
            LONG_PTR pLong = ::GetWindowLongPtrA(hWnd, GWLP_USERDATA);
            auto* managerImpl = reinterpret_cast<WindowManagerImpl*>(pLong);

            if (managerImpl != nullptr) {

                auto iter = managerImpl->FindWindow(hWnd);

                if (iter != managerImpl->windows.end()) {
                    if (LRESULT r = (*iter)->WndProcImpl(hWnd, msg, wParam, lParam)) {
                        return r;
                    }
                }
            }

            return ::DefWindowProcA(hWnd, msg, wParam, lParam);
        }

        WNDCLASSEXA wc;
        std::vector<std::unique_ptr<WindowWin64>> windows;
    };


WindowManager::WindowManager()
{
    m_impl->wc = { sizeof(WNDCLASSEXA), CS_CLASSDC, WindowManagerImpl::WndProc, 0L, 0L, GetModuleHandleA(NULL), NULL, NULL, NULL, NULL, "app's Window Class", NULL };
    ::RegisterClassExA(&m_impl->wc);
}


WindowManager::~WindowManager()
{
    for (auto& w : m_impl->windows) {
        Destroy(*w.get());
    }
    ::UnregisterClassA(m_impl->wc.lpszClassName, m_impl->wc.hInstance);
}

Window& WindowManager::Create(const char* windowName) {
    int w = 1280;
    int h = 800;

    HWND hwnd = ::CreateWindowExA(0, m_impl->wc.lpszClassName, windowName, WS_OVERLAPPEDWINDOW, 100, 100, w, h, NULL, NULL, m_impl->wc.hInstance, NULL);
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    ::SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(m_impl.get()));

    return *m_impl->windows.emplace_back(std::make_unique<WindowManagerImpl::WindowWin64>(hwnd, w, h)).get();
}

void WindowManager::Destroy(Window& window) 
{
    auto iter = m_impl->FindWindow(window.GetHandle());

    ASSERT_MSG(iter != m_impl->windows.end(), "The Window has not been found");

    ::DestroyWindow((*iter)->GetHandle());
    m_impl->windows.erase(iter);
}



}

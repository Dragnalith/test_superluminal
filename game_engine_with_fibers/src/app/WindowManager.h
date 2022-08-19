#pragma once

#include <app/Util.h>

namespace app
{

class Window;
struct WindowManagerImpl;

class WindowManager
{
public:
    WindowManager();
    ~WindowManager();
    Window& Create(const char* name);
    void Destroy(Window& window);
    void SetCursor(void* cursor);

private:
    Pimpl<WindowManagerImpl> m_impl;
};

}
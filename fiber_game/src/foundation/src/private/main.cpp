#include <fnd/AppMain.h>
#include <fnd/Util.h>
#include <fnd/Window.h>
#include <private/JobSystem.h>
#include <private/WindowManager.h>

#include <Superluminal/PerformanceAPI.h>
#include <windows.h>


namespace
{
engine::Window* g_mainWindow = nullptr;
engine::WindowManager* g_windowManager = nullptr;
}

namespace engine
{

Window& Window::GetMainWindow()
{
    ASSERT_MSG(g_mainWindow != nullptr, "Error: window manager not initialized");
    return *g_mainWindow;
}
void Window::SetCursor(void* cursor)
{
    ASSERT_MSG(g_windowManager != nullptr, "Error: window manager not initialized");
    g_windowManager->SetCursor(cursor);
}
}

int main() {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    PerformanceAPI_SetCurrentThreadName("Main Thread");

#if 0
    engine::JobSystem::Start([]() {
        std::cout << "Hello World\n";
        engine::JobHandle handle;
        for (int i = 0; i < 10; i++) {
            engine::JobSystem::DispatchJob(handle, [i] {
                std::cout << "Some Job: " << i << "\n";
                });
        }
        engine::JobSystem::Wait(handle);
        std::cout << "Good Night World\n";

        });
#endif

    engine::WindowManager windowManager;
    g_windowManager = &windowManager;
    g_mainWindow = &windowManager.Create("Fiber Game");
    engine::JobSystem::Start([]{
        AppMain();
    });
    return 0;
}
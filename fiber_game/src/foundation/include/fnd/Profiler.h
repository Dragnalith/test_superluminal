#pragma once

namespace fnd
{
	bool IsProfilingEnabled();
	void SetThreadName(const char* name);
}

#if defined(SUPERLUMINAL_PROFILER_ENABLED)

#include <Superluminal/PerformanceAPI.h>

#define PROFILE_SET_THREADNAME(name) PerformanceAPI_SetCurrentThreadName(name)
#define PROFILE_SCOPE(ID) PERFORMANCEAPI_INSTRUMENT(ID)
#define PROFILE_SCOPE_COLOR(ID, r, g, b) PERFORMANCEAPI_INSTRUMENT_COLOR(ID, PERFORMANCEAPI_MAKE_COLOR(r, g, b))
#define PROFILE_SCOPE_DATA_COLOR(ID, data, r, g, b) PERFORMANCEAPI_INSTRUMENT_DATA_COLOR(ID, data, PERFORMANCEAPI_MAKE_COLOR(r, g, b))
#define PROFILE_JOB_NAME(name) PROFILE_SCOPE_COLOR(name, 0, 0, 0)

#define PROFILE_DEFAULT_FRAME
#define PROFILE_FRAME(name)

#define PROFILE_REGISTER_FIBER(Fiber, Name) PerformanceAPI_RegisterFiber((uint64_t)Fiber)
#define PROFILE_UNREGISTER_FIBER(Fiber, Name) PerformanceAPI_UnregisterFiber((uint64_t)Fiber)
#define SWITCH_TO_FIBER(Fiber, Name) PerformanceAPI_BeginFiberSwitch((uint64_t)::GetCurrentFiber(), (uint64_t)(Fiber)); ::SwitchToFiber((Fiber)); PerformanceAPI_EndFiberSwitch((uint64_t)::GetCurrentFiber())

#elif defined(TRACY_PROFILER_ENABLED)

#define TRACY_DELAYED_INIT
#define TRACY_MANUAL_LIFETIME
#define TRACY_ENABLE
#define TRACY_FIBERS

#include <Tracy/Tracy.hpp>
#define TRACY_MAKE_COLOR(R, G, B) ((((uint32_t)(R)) << 16) | (((uint32_t)(G)) << 8) | ((uint32_t)(B)) | (((uint32_t)(0xFF)) << 24))


#define PROFILE_SET_THREADNAME(name) if (fnd::IsProfilingEnabled()) { tracy::SetThreadName(name); } do {} while(0)
#define PROFILE_SCOPE(ID) ZoneNamedN(___tracy_scoped_zone, ID, fnd::IsProfilingEnabled())
#define PROFILE_SCOPE_COLOR(ID, r, g, b) ZoneNamedNC(___tracy_scoped_zone, ID, TRACY_MAKE_COLOR(r, g, b), fnd::IsProfilingEnabled())
#define PROFILE_SCOPE_DATA_COLOR(ID, data, r, g, b) ZoneNamedNC(___tracy_scoped_zone, ID, TRACY_MAKE_COLOR(r, g, b), fnd::IsProfilingEnabled()); ZoneText(data, strlen( data ))
#define PROFILE_JOB_NAME(name) ZoneTransientN(___tracy_scoped_zone, name, fnd::IsProfilingEnabled()); ZoneColorV(___tracy_scoped_zone, TRACY_MAKE_COLOR(0, 0, 1));

#define PROFILE_DEFAULT_FRAME if (fnd::IsProfilingEnabled()) { FrameMark } do {} while(0) 
#define PROFILE_FRAME(name) if (fnd::IsProfilingEnabled()) { FrameMarkNamed(name); } do {} while(0) 

#define PROFILE_REGISTER_FIBER(Fiber, Name) if (fnd::IsProfilingEnabled()) { TracyFiberEnter(Name); } do {} while(0)
#define PROFILE_UNREGISTER_FIBER(Fiber, Name) if (fnd::IsProfilingEnabled()) { TracyFiberLeave; } do {} while(0);
#define SWITCH_TO_FIBER(Fiber, Name) if (fnd::IsProfilingEnabled()) { TracyFiberEnter(Name); } ::SwitchToFiber((Fiber));

#elif defined(PIX_PROFILER_ENABLED)

#include <windows.h>
#include <WinPixEventRuntime/pix3.h>

#define PROFILE_SET_THREADNAME(name) fnd::SetThreadName(name)
#define PROFILE_SCOPE(name) PIXScopedEvent(PIX_COLOR(200, 200, 200), name)
#define PROFILE_SCOPE_COLOR(name, r, g, b) PIXScopedEvent(PIX_COLOR(r, g, b), name)
#define PROFILE_SCOPE_DATA_COLOR(name, data, r, g, b) PIXScopedEvent(PIX_COLOR(r, g, b), name)
#define PROFILE_JOB_NAME(name) PIXScopedEvent(PIX_COLOR(200, 200, 200), name)

#define PROFILE_DEFAULT_FRAME
#define PROFILE_FRAME(name)

#define PROFILE_REGISTER_FIBER(Fiber, Name)
#define PROFILE_UNREGISTER_FIBER(Fiber, Name)
#define SWITCH_TO_FIBER(Fiber, Name) ::SwitchToFiber((Fiber));

#else

#define PROFILE_SET_THREADNAME(name)
#define PROFILE_SCOPE(ID)
#define PROFILE_SCOPE_COLOR(ID, r, g, b)
#define PROFILE_SCOPE_DATA_COLOR(ID, data, r, g, b)
#define PROFILE_JOB_NAME(name)

#define PROFILE_DEFAULT_FRAME
#define PROFILE_FRAME(name)

#define PROFILE_REGISTER_FIBER(Fiber, Name)
#define PROFILE_UNREGISTER_FIBER(Fiber, Name)
#define SWITCH_TO_FIBER(Fiber, Name) ::SwitchToFiber((Fiber));

#endif

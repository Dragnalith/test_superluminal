#pragma once

#define SUPERLUMINAL_PROFILER_ENABLED 0
#define TRACY_PROFILER_ENABLED 1

#if SUPERLUMINAL_PROFILER_ENABLED

#include <Superluminal/PerformanceAPI.h>

#define PROFILE_SET_THREADNAME(name) PerformanceAPI_SetCurrentThreadName(name)
#define PROFILE_SCOPE(ID) PERFORMANCEAPI_INSTRUMENT(ID)
#define PROFILE_SCOPE_COLOR(ID, r, g, b) PERFORMANCEAPI_INSTRUMENT_COLOR(ID, PERFORMANCEAPI_MAKE_COLOR(r, g, b))
#define PROFILE_SCOPE_DATA_COLOR(ID, data, r, g, b) PERFORMANCEAPI_INSTRUMENT_DATA_COLOR(ID, data, PERFORMANCEAPI_MAKE_COLOR(r, g, b))

#define PROFILE_DEFAULT_FRAME FrameMark
#define PROFILE_FRAME(name) FrameMarkNamed(name)

#define PROFILE_REGISTER_FIBER(Fiber, Name) PerformanceAPI_RegisterFiber((uint64_t)Fiber)
#define PROFILE_UNREGISTER_FIBER(Fiber, Name) PerformanceAPI_UnregisterFiber((uint64_t)Fiber)
#define SWITCH_TO_FIBER(Fiber, Name) PerformanceAPI_BeginFiberSwitch((uint64_t)::GetCurrentFiber(), (uint64_t)(Fiber)); ::SwitchToFiber((Fiber)); PerformanceAPI_EndFiberSwitch((uint64_t)::GetCurrentFiber())

#elif TRACY_PROFILER_ENABLED

#define TRACY_ENABLE
#define TRACY_FIBERS

#include <Tracy/Tracy.hpp>
#define PROFILE_SET_THREADNAME(name) tracy::SetThreadName(name)
#define PROFILE_SCOPE(ID) ZoneScopedNS(ID, 32)
#define PROFILE_SCOPE_COLOR(ID, r, g, b) ZoneScopedNS(ID, 32)
#define PROFILE_SCOPE_DATA_COLOR(ID, data, r, g, b) ZoneScopedNS(ID, 32)

#define PROFILE_DEFAULT_FRAME FrameMark
#define PROFILE_FRAME(name) FrameMarkNamed(name)

#define PROFILE_REGISTER_FIBER(Fiber, Name) TracyFiberEnter(Name);
#define PROFILE_UNREGISTER_FIBER(Fiber, Name) TracyFiberLeave;
#define SWITCH_TO_FIBER(Fiber, Name) TracyFiberEnter(Name); ::SwitchToFiber((Fiber));

#else

#define PROFILE_SET_THREADNAME(name)
#define PROFILE_SCOPE(ID)
#define PROFILE_SCOPE_COLOR(ID, r, g, b)
#define PROFILE_SCOPE_DATA_COLOR(ID, data, r, g, b)

#define PROFILE_DEFAULT_FRAME FrameMark
#define PROFILE_FRAME(name) FrameMarkNamed(name)

#define PROFILE_REGISTER_FIBER(Fiber, Name)
#define PROFILE_UNREGISTER_FIBER(Fiber, Name)
#define SWITCH_TO_FIBER(Fiber, Name) ::SwitchToFiber((Fiber));

#endif

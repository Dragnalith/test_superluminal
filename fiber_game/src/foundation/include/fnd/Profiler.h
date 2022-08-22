#pragma once

#define SUPERLUMINAL_ENABLED 1

#if SUPERLUMINAL_ENABLED
#include <Superluminal/PerformanceAPI.h>

#define PROFILE_SET_THREADNAME(name) PerformanceAPI_SetCurrentThreadName(name)
#define PROFILE_SCOPE(ID) PERFORMANCEAPI_INSTRUMENT(ID)
#define PROFILE_SCOPE_COLOR(ID, r, g, b) PERFORMANCEAPI_INSTRUMENT_COLOR(ID, PERFORMANCEAPI_MAKE_COLOR(r, g, b))
#define PROFILE_SCOPE_DATA_COLOR(ID, data, r, g, b) PERFORMANCEAPI_INSTRUMENT_DATA_COLOR(ID, data, PERFORMANCEAPI_MAKE_COLOR(r, g, b))
#define PROFILE_REGISTER_FIBER(Fiber) PerformanceAPI_RegisterFiber((uint64_t)Fiber)
#define PROFILE_UNREGISTER_FIBER(Fiber) PerformanceAPI_UnregisterFiber((uint64_t)Fiber)

#define SWITCH_TO_FIBER(Fiber) PerformanceAPI_BeginFiberSwitch((uint64_t)::GetCurrentFiber(), (uint64_t)(Fiber)); ::SwitchToFiber((Fiber)); PerformanceAPI_EndFiberSwitch((uint64_t)::GetCurrentFiber())

#else

#define PROFILE_SET_THREADNAME(name)
#define PROFILE_SCOPE(ID)
#define PROFILE_SCOPE_COLOR(ID, r, g, b)
#define PROFILE_SCOPE_DATA_COLOR(ID, data, r, g, b)
#define PROFILE_REGISTER_FIBER(Fiber)
#define PROFILE_UNREGISTER_FIBER(Fiber)
#define SWITCH_TO_FIBER(Fiber) ::SwitchToFiber((Fiber));

#endif

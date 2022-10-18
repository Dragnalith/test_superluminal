#include <fnd/Profiler.h>

#if TRACY_PROFILER_ENABLED
#define TRACY_NO_SYSTEM_TRACING
#define TRACY_NO_CALLSTACK
#define TRACY_NO_CALLSTACK_INLINES
#include <Tracy/TracyClient.cpp>
#endif
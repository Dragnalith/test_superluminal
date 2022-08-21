#pragma once

#include <fnd/SpinLock.h>
#include <fnd/Util.h>

#include <atomic>
#include <deque>
#include <functional>
#include <mutex>

namespace engine
{

struct JobSysemImpl;

class JobHandle {
	friend class FiberJob;
protected:
	std::atomic<int64_t> m_counter = 0;
};

class JobSystem
{
public:
	static void DispatchJob(JobHandle& handle, std::function<void()> job);
	static void Wait(const JobHandle& handle);
	static void YieldJob();
	static void Start(std::function<void()> mainJob);
};

}
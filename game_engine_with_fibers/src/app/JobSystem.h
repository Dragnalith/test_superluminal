#pragma once

#include <app/SpinLock.h>
#include <app/Util.h>

#include <atomic>
#include <deque>
#include <functional>
#include <mutex>

namespace app
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
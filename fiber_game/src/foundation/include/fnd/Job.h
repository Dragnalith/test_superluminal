#pragma once

#include <atomic>
#include <functional>

namespace engine
{

class JobHandle {
	friend class FiberJob;
protected:
	std::atomic<int64_t> m_counter = 0;
};

struct Job
{
	static void DispatchJob(JobHandle& handle, std::function<void()> job);
	static void Wait(const JobHandle& handle);
	static void YieldJob();
};

}
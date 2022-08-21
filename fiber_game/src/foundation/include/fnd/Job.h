#pragma once

#include <atomic>
#include <functional>

namespace engine
{

class JobCounter {
	friend class FiberJob;
public:
	void Set(int64_t value) {
		m_counter.store(value);
	}
protected:
	uint64_t m_test = 0xdeadbeef;
	std::atomic<int64_t> m_counter = 0;
};

struct Job
{
	static void DispatchJob(JobCounter& handle, std::function<void()> job);
	static void Wait(const JobCounter& handle, int64_t value = 0);
	static void YieldJob();
};

}
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

class JobSystem
{
public:
	static void DispatchJob(std::function<void()> job);
	static void YieldJob();
	static void Start(std::function<void()> mainJob);
};

}
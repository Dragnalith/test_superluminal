#include <private/JobSystem.h>

#include <fnd/Job.h>
#include <fnd/Util.h>
#include <fnd/SpinLock.h>

#include <Superluminal/PerformanceAPI.h>

#include <windows.h>
#include <format>
#include <optional>
#include <vector>
#include <iostream>
#include <deque>

#define SWITCH_TO_FIBER(Fiber) PerformanceAPI_BeginFiberSwitch((uint64_t)::GetCurrentFiber(), (uint64_t)(Fiber)); ::SwitchToFiber((Fiber)); PerformanceAPI_EndFiberSwitch((uint64_t)::GetCurrentFiber());

namespace engine
{

class JobQueue;
class JobWorker;

namespace {
    thread_local void* g_threadFiber = nullptr;
    thread_local JobWorker* g_threadWorker = nullptr;
    thread_local JobQueue* g_jobQueue = nullptr;
}

class FiberJob {
    friend class JobQueue;
public:
    FiberJob(const FiberJob&) = delete;
    FiberJob& operator=(const FiberJob&) = delete;
    FiberJob& operator=(FiberJob&& other) = delete;
    FiberJob(FiberJob&& other) = delete;

    FiberJob()
    {
        AssertValid();
        m_fiber = ::CreateFiber(64 * 1024, FiberJob::FiberFunc, this);
        ASSERT_MSG(m_fiber != nullptr, "Fiber creation has failed");
    }

    void SetJob(const char* name, JobCounter* handle, const std::function<void()>& func)
    {
        m_name = name;
        m_handle = handle;
        m_delegate = func;
        AssertValid();
        {
            int64_t value = m_handle->m_counter.fetch_add(1);
            ASSERT_MSG(value >= 0, "handle issue when increment");
        }
    }
    ~FiberJob() {
        if (m_fiber != nullptr) {
            ASSERT_MSG(IsDone(), "fiber is destroyed, but func is not complete. something is wrong");
            ::DeleteFiber(m_fiber);
        }
    }
    static void FiberFunc(void* data) {
        PerformanceAPI_RegisterFiber((uint64_t)GetCurrentFiber());
        FiberJob* fiberJob = reinterpret_cast<FiberJob*>(data);
        {
            const char* name = fiberJob->m_name;
            PERFORMANCEAPI_INSTRUMENT_COLOR(name, PERFORMANCEAPI_MAKE_COLOR(254, 254, 254));
            fiberJob->m_delegate();
        }
        JobCounter* handle = fiberJob->m_handle;
        fiberJob->m_name = nullptr;
        fiberJob->m_handle = nullptr;
        fiberJob->m_delegate = nullptr;
        int64_t value = handle->m_counter.fetch_sub(1);
        ASSERT_MSG(value > 0, "handle issue when decrement");

        PerformanceAPI_UnregisterFiber((uint64_t)GetCurrentFiber());
        SWITCH_TO_FIBER(g_threadFiber);
    }

    void SetWaitingHandle(JobCounter& handle, int64_t value, int64_t reset) {
        m_waitingHandle = &handle;
        m_waitedValue = value;
        m_resetValue = reset;
    }

    void* GetFiberHandle() { return m_fiber; }
    void AssertValid() {
        ASSERT_MSG(m_handle == nullptr || m_handle->m_test == 0xdeadbeef, "invalid handle");
    }
    bool IsDone() const { 
        if (m_name == nullptr) {
            ASSERT_MSG(!m_delegate, "delegate is not empty");
            ASSERT_MSG(m_handle == nullptr, "handle is not null");
            return true;
        }
        else {
            ASSERT_MSG(m_delegate, "delegate is empty");
            ASSERT_MSG(m_handle != nullptr, "handle is null");
            return false;
        }
    }

protected:
    const char* m_name = nullptr;
    JobCounter* m_handle = nullptr;
    void* m_fiber = nullptr;
    std::function<void()> m_delegate;
    JobCounter* m_waitingHandle = nullptr;
    int64_t m_waitedValue = -1;
    int64_t m_resetValue = -1;
};


class JobQueue
{
public:
    JobQueue() = default;
    JobQueue(const JobQueue&) = delete;
    JobQueue(JobQueue&&) = delete;
    JobQueue& operator=(JobQueue&&) = delete;
    JobQueue& operator=(const JobQueue&) = delete;

    ~JobQueue() {
        ASSERT_MSG(m_jobTotalNumber.load() == 0, "Problem: job remain after the queue is destroyed");
    }

    size_t Size() const {
        std::scoped_lock<SpinLock> lock(m_stackLock);
        return m_queue.size();
    }
    int64_t RemainingJobCount() const {
        return m_jobTotalNumber.load();
    }

    void Create(const char* name, JobCounter& handle, std::function<void()> mainJob) {
        int64_t value = m_jobTotalNumber.fetch_add(1);
        ASSERT_MSG(value >= 0, "job count issue when increment");
        std::unique_ptr<FiberJob> job = std::make_unique<FiberJob>();
        job->SetJob(name, &handle, mainJob);
        Push(std::move(job));
    }

    void Release(std::unique_ptr<FiberJob>&& job) {
        int64_t value = m_jobTotalNumber.fetch_sub(1);
        ASSERT_MSG(value > 0, "job count issue when decrement");
    }
    std::unique_ptr<FiberJob> Pop() {
        std::scoped_lock<SpinLock> lock(m_stackLock);

        for (auto it = m_queue.begin(); it != m_queue.end(); it++) {
            (*it)->AssertValid();
            if ((*it)->m_waitingHandle == nullptr) {
                auto job = std::move(*it);
                m_queue.erase(it);
                job->AssertValid();
                return std::move(job);
            }
            else {
                JobCounter* waitingHandle = (*it)->m_waitingHandle;
                int64_t expected = (*it)->m_waitedValue;
                int64_t resetValue = (*it)->m_resetValue;
                if (waitingHandle->m_counter.compare_exchange_strong(expected, resetValue)) {
                    auto job = std::move(*it);
                    m_queue.erase(it);
                    job->AssertValid();
                    return std::move(job);
                }
            }
        }

        return nullptr;
    }

    void Push(std::unique_ptr<FiberJob>&& job) {
        job->AssertValid();

        std::scoped_lock<SpinLock> lock(m_stackLock);
        m_queue.push_back(std::move(job));
    }

private:
    std::deque<std::unique_ptr<FiberJob>> m_queue;
    std::vector<FiberJob> m_job;
    mutable SpinLock m_stackLock;
    std::atomic<int64_t> m_jobTotalNumber = 0;
};


class JobWorker
{
public:
    JobWorker(JobQueue& jobQueue, int index)
        : m_jobQueue(jobQueue)
        , m_index(index)
        , m_thread([this]() { this->ThreadFunc(); })
    {
    }

    JobWorker(const JobWorker&) = delete;
    JobWorker& operator=(const JobWorker&) = delete;
    JobWorker(JobWorker&&) = default;
    JobWorker& operator=(JobWorker&&) = default;

    void Join()
    {
        m_thread.join();
    }

    void SetWaitingHandle(JobCounter& handle, int64_t value, int64_t reset) {
        ASSERT_MSG(m_currentFiber != nullptr, "Wait can only be called from a Job");
        m_currentFiber->SetWaitingHandle(handle, value, reset);

    }
private:
    void ThreadFunc() {
        PerformanceAPI_SetCurrentThreadName(std::format("JobWorker - {}", m_index).c_str());

        g_threadFiber = ::ConvertThreadToFiber(nullptr);
        PerformanceAPI_RegisterFiber((uint64_t)GetCurrentFiber());
        g_threadWorker = this;
        g_jobQueue = &m_jobQueue;

        {
            PERFORMANCEAPI_INSTRUMENT_COLOR("Idle", PERFORMANCEAPI_MAKE_COLOR(163, 73, 164));

            while (m_jobQueue.RemainingJobCount() > 0) {
                std::unique_ptr<FiberJob> job = m_jobQueue.Pop();
                if (job) {
                    job->AssertValid();
                    m_currentFiber = job.get();
                    SWITCH_TO_FIBER(job->GetFiberHandle());
                    m_currentFiber = nullptr;
                    if (!job->IsDone()) {
                        m_jobQueue.Push(std::move(job));
                    }
                    else {
                        m_jobQueue.Release(std::move(job));
                    }
                }
            }
        }
        PerformanceAPI_UnregisterFiber((uint64_t)GetCurrentFiber());
        ::ConvertFiberToThread();
    }

    FiberJob* m_currentFiber = nullptr;
    JobQueue& m_jobQueue;
    int m_index;
    std::thread m_thread;
};


void Job::YieldJob() 
{
    ASSERT_MSG(g_threadFiber != nullptr, "Yield can only be called from a job");
    SWITCH_TO_FIBER(g_threadFiber);
}

void Job::Wait(JobCounter& handle) {
    Job::Wait(handle, 0, 0);
}

void Job::Wait(JobCounter& handle, int64_t value) {
    Job::Wait(handle, value, value);
}

void Job::Wait(JobCounter& handle, int64_t value, int64_t reset) {
    ASSERT_MSG(g_threadWorker != nullptr, "Wait can only be called from a worker");
    g_threadWorker->SetWaitingHandle(handle, value, reset);
    YieldJob();
}


void Job::Dispatch(const char* name, JobCounter& handle, std::function<void()> mainJob) {
    ASSERT_MSG(g_jobQueue != nullptr, "DispatchJob can only be called from a worker");
    g_jobQueue->Create(name, handle, mainJob);
}

void JobSystem::Start(std::function<void()> mainJob) {
    JobQueue jobQueue;
    JobCounter handle;
    jobQueue.Create("Main Job", handle, mainJob);

    constexpr int N = 3;
    std::vector<std::unique_ptr<JobWorker>> workers;
    for (int i = 0; i < N; i++) {
        workers.push_back(std::make_unique<JobWorker>(jobQueue, i));
    }
    for (int i = 0; i < N; i++) {
        workers[i]->Join();
    }
}

}
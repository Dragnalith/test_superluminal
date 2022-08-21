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
public:
    FiberJob(const FiberJob&) = delete;
    FiberJob& operator=(const FiberJob&) = delete;
    FiberJob& operator=(FiberJob&& other) = delete;
    FiberJob(FiberJob&& other) = delete;

    FiberJob(JobHandle& handle, std::atomic<int64_t>& jobCounter, const std::function<void()>& func)
        : m_handle(handle)
        , m_delegate(func)
        , m_jobCounter(jobCounter)
    {
        {
            int64_t value = m_handle.m_counter.fetch_add(1);
            ASSERT_MSG(value >= 0, "handle issue when increment");
        }
        {
            int64_t value = m_jobCounter.fetch_add(1);
            ASSERT_MSG(value >= 0, "job count issue when increment");
        }
        m_fiber = ::CreateFiber(16 * 1024, FiberJob::FiberFunc, this);
        ASSERT_MSG(m_fiber != nullptr, "Fiber creation has failed");
    }

    ~FiberJob() {
        if (m_fiber != nullptr) {
            ASSERT_MSG(m_isDone, "fiber is destroyed, but func is not complete. something is wrong");
            ::DeleteFiber(m_fiber);
            int64_t value = m_jobCounter.fetch_sub(1);
            ASSERT_MSG(value > 0, "job count issue when decrement");
        }

    }
    static void FiberFunc(void* data) {
        FiberJob* fiberJob = reinterpret_cast<FiberJob*>(data);
        fiberJob->m_delegate();
        fiberJob->m_isDone = true;
        int64_t value = fiberJob->m_handle.m_counter.fetch_sub(1);
        ASSERT_MSG(value > 0, "handle issue when decrement");

        ::SwitchToFiber(g_threadFiber);
    }

    void SetWaitingHandle(const JobHandle& handle) {
        m_waitingHandle = &handle;
    }

    void* GetFiberHandle() { return m_fiber; }
    bool IsDone() const { return m_isDone; }
    bool IsReady() const { return m_waitingHandle == nullptr || m_waitingHandle->m_counter.load() == 0; }
private:
    JobHandle& m_handle;
    std::atomic<int64_t>& m_jobCounter;
    void* m_fiber = nullptr;
    std::function<void()> m_delegate;
    bool m_isDone = false;
    const JobHandle* m_waitingHandle = nullptr;
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
        ASSERT_MSG(m_jobCounter.load() == 0, "Problem: job remain after the queue is destroyed");
    }

    size_t Size() const {
        std::scoped_lock<SpinLock> lock(m_stackLock);
        return m_queue.size();
    }
    int64_t RemainingJobCount() const {
        return m_jobCounter.load();
    }

    void Create(JobHandle& handle, std::function<void()> mainJob) {
        Push(std::make_unique<FiberJob>(handle, m_jobCounter, mainJob));
    }
    std::unique_ptr<FiberJob> Pop() {
        std::scoped_lock<SpinLock> lock(m_stackLock);

        for (auto it = m_queue.begin(); it != m_queue.end(); it++) {
            if ((*it)->IsReady()) {
                auto job = std::move(*it);
                m_queue.erase(it);
                return std::move(job);
            }
        }

        return nullptr;
    }

    void Push(std::unique_ptr<FiberJob> job) {
        std::scoped_lock<SpinLock> lock(m_stackLock);
        m_queue.push_back(std::move(job));
    }

private:
    std::deque<std::unique_ptr<FiberJob>> m_queue;
    std::vector<FiberJob> m_job;
    mutable SpinLock m_stackLock;
    std::atomic<int64_t> m_jobCounter = 0;
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

    void SetWaitingHandle(const JobHandle& handle) {
        ASSERT_MSG(m_currentFiber != nullptr, "Wait can only be called from a Job");
        m_currentFiber->SetWaitingHandle(handle);

    }
private:
    void ThreadFunc() {
        PerformanceAPI_SetCurrentThreadName(std::format("JobWorker - {}", m_index).c_str());

        g_threadFiber = ::ConvertThreadToFiber(nullptr);
        g_threadWorker = this;
        g_jobQueue = &m_jobQueue;

        while (m_jobQueue.RemainingJobCount() > 0) {
            std::unique_ptr<FiberJob> job = m_jobQueue.Pop();

            if (job) {
                m_currentFiber = job.get();
                ::SwitchToFiber(job->GetFiberHandle());
                m_currentFiber = nullptr;
                if (!job->IsDone()) {
                    m_jobQueue.Push(std::move(job));
                }
            }
        }

        ::ConvertFiberToThread();
    }

    FiberJob* m_currentFiber = nullptr;
    JobQueue& m_jobQueue;
    int m_index;
    std::thread m_thread;
};


void YieldJob() 
{
    ASSERT_MSG(g_threadFiber != nullptr, "Yield can only be called from a job");
    ::SwitchToFiber(g_threadFiber);
}

void Wait(const JobHandle& handle) {
    ASSERT_MSG(g_threadWorker != nullptr, "Wait can only be called from a worker");
    g_threadWorker->SetWaitingHandle(handle);
    YieldJob();
}


void DispatchJob(JobHandle& handle, std::function<void()> mainJob) {
    ASSERT_MSG(g_jobQueue != nullptr, "DispatchJob can only be called from a worker");
    g_jobQueue->Create(handle, mainJob);
}

void JobSystem::Start(std::function<void()> mainJob) {
    JobQueue jobQueue;
    JobHandle handle;
    jobQueue.Create(handle, mainJob);

    constexpr int N = 4;
    std::vector<std::unique_ptr<JobWorker>> workers;
    for (int i = 0; i < N; i++) {
        workers.push_back(std::make_unique<JobWorker>(jobQueue, i));
    }
    for (int i = 0; i < N; i++) {
        workers[i]->Join();
    }
}

}
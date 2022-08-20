#include <app/JobSystem.h>

#include <windows.h>

#include <Superluminal/PerformanceAPI.h>
#include <format>
#include <optional>
#include <vector>
#include <iostream>

namespace app
{

class JobQueue;

namespace {
    thread_local void* g_threadFiber = nullptr;
    thread_local JobQueue* g_jobQueue = nullptr;
}

class FiberJob {
public:
    FiberJob(const FiberJob&) = delete;
    FiberJob& operator=(const FiberJob&) = delete;
    FiberJob& operator=(FiberJob&& other) = delete;
    FiberJob(FiberJob&& other) = delete;

    FiberJob(std::atomic<int64_t>& jobCounter, const std::function<void()>& func)
        : m_delegate(func)
        , m_jobCounter(jobCounter)
    {
        int64_t value = m_jobCounter.fetch_add(1);
        ASSERT_MSG(value >= 0, "job count issue when increment");
        m_fiber = ::CreateFiber(16 * 1024, FiberJob::FiberFunc, this);
        ASSERT_MSG(m_fiber != nullptr, "Fiber creation has failed");
    }

    ~FiberJob() {
        if (m_fiber != nullptr) {
            ::DeleteFiber(m_fiber);
            int64_t value = m_jobCounter.fetch_sub(1);
            ASSERT_MSG(value > 0, "job count issue when decrement");
        }

    }
    static void FiberFunc(void* data) {
        FiberJob* fiberJob = reinterpret_cast<FiberJob*>(data);
        fiberJob->m_delegate();
        fiberJob->m_isDone = true;

        ::SwitchToFiber(g_threadFiber);
    }


    void* GetFiberHandle() { return m_fiber; }
    bool IsDone() const { return m_isDone; }
private:
    std::atomic<int64_t>& m_jobCounter;
    void* m_fiber = nullptr;
    std::function<void()> m_delegate;
    bool m_isDone = false;
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

    void Create(std::function<void()> mainJob) {
        Push(std::make_unique<FiberJob>(m_jobCounter, mainJob));
    }
    std::unique_ptr<FiberJob> Pop() {
        std::scoped_lock<SpinLock> lock(m_stackLock);
        if (m_queue.size() > 0) {
            auto job = std::move(m_queue.front());
            m_queue.pop_front();
            return std::move(job);
        }
        else {
            return nullptr;
        }
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
private:
    void ThreadFunc() {
        PerformanceAPI_SetCurrentThreadName(std::format("JobWorker - {}", m_index).c_str());

        g_threadFiber = ::ConvertThreadToFiber(nullptr);
        g_jobQueue = &m_jobQueue;

        while (m_jobQueue.RemainingJobCount() > 0) {
            std::unique_ptr<FiberJob> job = m_jobQueue.Pop();

            if (job) {
                ::SwitchToFiber(job->GetFiberHandle());

                if (!job->IsDone()) {
                    m_jobQueue.Push(std::move(job));
                }
            }
        }

        ::ConvertFiberToThread();
    }

    JobQueue& m_jobQueue;
    int m_index;
    std::thread m_thread;
};


void JobSystem::YieldJob() 
{
    ASSERT_MSG(g_threadFiber != nullptr, "Yield can only be called from a worker");
    ::SwitchToFiber(g_threadFiber);
}

void JobSystem::DispatchJob(std::function<void()> mainJob) {
    ASSERT_MSG(g_jobQueue != nullptr, "DispatchJob can only be called from a worker");
    g_jobQueue->Create(mainJob);
}

void JobSystem::Start(std::function<void()> mainJob) {
    JobQueue jobQueue;
    jobQueue.Create(mainJob);

    constexpr int N = 2;
    std::vector<std::unique_ptr<JobWorker>> workers;
    for (int i = 0; i < N; i++) {
        workers.push_back(std::make_unique<JobWorker>(jobQueue, i));
    }
    for (int i = 0; i < N; i++) {
        workers[i]->Join();
    }
}

}
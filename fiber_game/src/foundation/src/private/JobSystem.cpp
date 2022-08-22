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

struct JobDesc {
    JobDesc() = default;
    JobDesc(const char* name, JobCounter* handle, const std::function<void()>& func) {
        m_name = name;
        m_handle = handle;
        m_delegate = func;
    }
    const char* m_name = nullptr;
    JobCounter* m_handle = nullptr;
    std::function<void()> m_delegate;
};

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

    void SetJob(const JobDesc& job)
    {
        ASSERT_MSG(IsDone(), "Job is not done");
        m_job = job;
        AssertValid();
        {
            int64_t value = m_job.m_handle->m_counter.fetch_add(1);
            ASSERT_MSG(value >= 0, "handle issue when increment");
        }
    }
    ~FiberJob() {
        if (m_fiber != nullptr) {
            ASSERT_MSG(IsDone(), "fiber is destroyed, but func is not complete. something is wrong");
            ::DeleteFiber(m_fiber);
        }
    }

    void Invoke() {
        m_useCount += 1;
        if (m_useCount > 1) {
            //__debugbreak();
        }
        {
            const char* name = m_job.m_name;
            PERFORMANCEAPI_INSTRUMENT_COLOR(name, PERFORMANCEAPI_MAKE_COLOR(254, 254, 254));
            m_job.m_delegate();
        }
        JobCounter* handle = m_job.m_handle;
        m_job = JobDesc();
        ASSERT_MSG(handle != nullptr, "Handle is null");
        int64_t value = handle->m_counter.fetch_sub(1);
        ASSERT_MSG(value > 0, "handle issue when decrement");
    }
    static void FiberFunc(void* data) {
        PerformanceAPI_RegisterFiber((uint64_t)GetCurrentFiber());
        FiberJob* fiberJob = reinterpret_cast<FiberJob*>(data);

        while(true)
        {
            fiberJob->Invoke();

            SWITCH_TO_FIBER(g_threadFiber);
        }

        PerformanceAPI_UnregisterFiber((uint64_t)GetCurrentFiber());
        SWITCH_TO_FIBER(g_threadFiber);
    }

    void SetWaitingHandle(JobCounter& handle, int64_t value, int64_t reset, const std::source_location location) {
        m_waitingHandle = &handle;
        m_waitedValue = value;
        m_resetValue = reset;
        m_location = location;
    }

    void* GetFiberHandle() { return m_fiber; }
    void AssertValid() {
        ASSERT_MSG(m_job.m_handle == nullptr || m_job.m_handle->m_test == 0xdeadbeef, "invalid handle");
    }
    bool IsDone() const { 
        if (m_job.m_name == nullptr) {
            ASSERT_MSG(!m_job.m_delegate, "delegate is not empty");
            ASSERT_MSG(m_job.m_handle == nullptr, "handle is not null");
            return true;
        }
        else {
            ASSERT_MSG(m_job.m_delegate, "delegate is empty");
            ASSERT_MSG(m_job.m_handle != nullptr, "handle is null");
            return false;
        }
    }

protected:
    JobDesc m_job;
    void* m_fiber = nullptr;
    JobCounter* m_waitingHandle = nullptr;
    int64_t m_waitedValue = -1;
    int64_t m_resetValue = -1;
    std::source_location m_location;
    int64_t m_useCount = 0;
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
        std::scoped_lock<SpinLock> lock(m_fiberQueueLock);
        return m_queue.size();
    }
    int64_t RemainingJobCount() const {
        return m_jobTotalNumber.load();
    }


    void ScheduleFiber() {
        std::scoped_lock<SpinLock> lock(m_jobLock);

        if (m_jobs.size() > 0) {
            JobDesc desc = m_jobs.front();
            m_jobs.pop_front();
            Create(desc);
        }
    }
    void Create(const JobDesc& jobDesc) {
        std::unique_ptr<FiberJob> job;
        int64_t value = m_jobTotalNumber.fetch_add(1);
        ASSERT_MSG(value >= 0, "job count issue when increment");
        {
            std::scoped_lock<SpinLock> lock(m_freeFiberLock);

            if (m_freeFiber.size() == 0) {
                m_freeFiber.push_back(std::make_unique<FiberJob>());
            }
            
            {
                size_t beforeSize = m_freeFiber.size();
                job = std::move(m_freeFiber.front());
                m_freeFiber.pop_front();
                size_t afterSize = m_freeFiber.size();
                ASSERT_MSG(beforeSize == afterSize + 1, "Problem of free fiber pop");
            }
        }
        job->SetJob(jobDesc);
        Push(std::move(job));
    }

    void Release(std::unique_ptr<FiberJob>&& job) {
        {
            std::scoped_lock<SpinLock> lock(m_freeFiberLock);
            m_freeFiber.push_back(std::move(job));
        }

        int64_t value = m_jobTotalNumber.fetch_sub(1);
        ASSERT_MSG(value > 0, "job count issue when decrement");
    }
    std::unique_ptr<FiberJob> Pop() {
        std::scoped_lock<SpinLock> lock(m_fiberQueueLock);

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
                    job->m_waitedValue = -1;
                    job->m_resetValue = -1;
                    job->m_waitingHandle = nullptr;
                    job->m_location = std::source_location();
                    return std::move(job);
                }
            }
        }

        return nullptr;
    }

    void Push(std::unique_ptr<FiberJob>&& job) {
        job->AssertValid();

        std::scoped_lock<SpinLock> lock(m_fiberQueueLock);
        m_queue.push_back(std::move(job));
    }

    void EnqueueJob(const JobDesc& jobDesc) {
        std::scoped_lock<SpinLock> lock(m_jobLock);
        m_jobs.push_back(jobDesc);
    }

private:
    mutable SpinLock m_jobLock;
    mutable SpinLock m_fiberQueueLock;
    mutable SpinLock m_freeFiberLock;
    std::deque<std::unique_ptr<FiberJob>> m_queue;
    std::deque<std::unique_ptr<FiberJob>> m_freeFiber;
    std::deque<JobDesc> m_jobs;
    std::vector<FiberJob> m_job;
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

    void SetWaitingHandle(JobCounter& handle, int64_t value, int64_t reset, const std::source_location location) {
        ASSERT_MSG(m_currentFiber != nullptr, "Wait can only be called from a Job");
        m_currentFiber->SetWaitingHandle(handle, value, reset, location);

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

void Job::Wait(JobCounter& handle, const std::source_location location) {
    Job::Wait(handle, 0, 0, location);
}

void Job::Wait(JobCounter& handle, int64_t value, const std::source_location location) {
    Job::Wait(handle, value, value, location);
}

void Job::Wait(JobCounter& handle, int64_t value, int64_t reset, const std::source_location location) {
    ASSERT_MSG(g_threadWorker != nullptr, "Wait can only be called from a worker");
    g_threadWorker->SetWaitingHandle(handle, value, reset, location);
    YieldJob();
}


void Job::Dispatch(const char* name, JobCounter& handle, std::function<void()> func) {
    ASSERT_MSG(g_jobQueue != nullptr, "DispatchJob can only be called from a worker");

    g_jobQueue->EnqueueJob(JobDesc(name, &handle, func));
    g_jobQueue->ScheduleFiber();
}

void JobSystem::Start(std::function<void()> mainJob) {
    JobQueue jobQueue;
    JobCounter handle;
    jobQueue.Create(JobDesc("Main Job", &handle, mainJob));

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
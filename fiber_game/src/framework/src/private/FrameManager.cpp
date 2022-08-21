#pragma once

#include <fnd/Job.h>
#include <fw/FrameManager.h>

#include <Superluminal/PerformanceAPI.h>

#include <iostream>
#include <mutex>

namespace engine
{


FrameManager::FrameManager(IFramePipeline& pipeline)
    : m_pipeline(pipeline)
    , m_lastStartFrameTime(engine::Clock::now() - std::chrono::milliseconds(16))
{
}

void FrameManager::Start()
{
    StartFrame(FrameUpdateResult::DefaultMaxFrameLatency);
    Job::Wait(m_handle);
}

void FrameManager::SetFrameLatency(int maxFrameLatency) 
{
    std::scoped_lock<SpinLock> lock(m_frameLatencyLock);
    while (m_maxFrameLatency < maxFrameLatency) {
        m_startSemaphore.Release();
        m_maxFrameLatency += 1;
    }
    while (m_maxFrameLatency > maxFrameLatency) {
        m_startSemaphore.Acquire();
        m_maxFrameLatency -= 1;
    }
    ASSERT_MSG(maxFrameLatency == m_maxFrameLatency, "error in SetFrameLatency");
}

int64_t FrameManager::AllocateFrameIndex() {
    int64_t frameIndex = m_nextFrameIndex;
    m_nextFrameIndex += 1;
    return frameIndex;
}
void FrameManager::RunFrame(int maxFrameLatency) {
    SetFrameLatency(maxFrameLatency);

    int64_t frameIndex = AllocateFrameIndex();
    
    std::string frameName = std::format("Index = {}", frameIndex);

    m_startSemaphore.Acquire();

    auto now = Clock::now();
    float deltatime = std::chrono::duration<float>(now - m_lastStartFrameTime).count();
    m_lastStartFrameTime = now;

    FrameData frameData;
    frameData.frameIndex = frameIndex;
    frameData.deltatime = deltatime;
    frameData.maxFrameLatency = maxFrameLatency;

    PERFORMANCEAPI_INSTRUMENT_DATA_COLOR("Frame", frameName.c_str(), PERFORMANCEAPI_MAKE_COLOR(34, 30, 203));
    {
        PERFORMANCEAPI_INSTRUMENT_DATA_COLOR("Update", frameName.c_str(), PERFORMANCEAPI_MAKE_COLOR(51, 217, 21));

        m_pipeline.Update(frameData);
        if (!frameData.result.stop) {
            StartFrame(frameData.result.maxFrameLatency);
        }
    }

    {
        Job::Wait(m_renderSemaphore, frameData.frameIndex);
        PERFORMANCEAPI_INSTRUMENT_DATA_COLOR("Render", frameName.c_str(), PERFORMANCEAPI_MAKE_COLOR(238, 220, 0));
        m_pipeline.Render(frameData); 
        m_renderSemaphore.Set(frameData.frameIndex + 1);
    }

    {
        Job::Wait(m_kickSemaphore, frameData.frameIndex);
        PERFORMANCEAPI_INSTRUMENT_DATA_COLOR("Kick", frameName.c_str(), PERFORMANCEAPI_MAKE_COLOR(238, 0, 60));
        m_pipeline.Kick(frameData);
        m_kickSemaphore.Set(frameData.frameIndex + 1);
    }

    {
        PERFORMANCEAPI_INSTRUMENT_DATA_COLOR("Clean", frameName.c_str(), PERFORMANCEAPI_MAKE_COLOR(150, 150, 150));
        m_pipeline.Clean(frameData);
    }
    m_startSemaphore.Release();
}
void FrameManager::StartFrame(int frameLatency) {
    Job::Dispatch("Frame Job", m_handle, [this, frameLatency] {
        RunFrame(frameLatency);
    });
}

} // namespace engine

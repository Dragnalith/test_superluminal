#pragma once

#include <imgui/imgui.h>

#include <stdint.h>
#include <vector>

struct ID3D12CommandAllocator;
struct ID3D12GraphicsCommandList;

namespace engine
{

// This data is read of the FrameManager to change its behavior
struct FrameUpdateResult
{
    bool stop = false; // Stop the FrameManager, i.e stop scheduling new frame, i.e stop the app
    int frameLatency = 3; // Number of frame which can be interleaved at the same time
};

struct RenderContext
{
    ID3D12CommandAllocator* CommandAllocator = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
};

struct DrawList
{
    ImVector<ImDrawCmd>     CmdBuffer;
    ImVector<ImDrawIdx>     IdxBuffer;
    ImVector<ImDrawVert>    VtxBuffer;
    ImDrawListFlags         Flags = 0;
};

struct DrawData
{
    bool            Valid = false;
    int             CmdListsCount = 0;
    int             TotalIdxCount = 0;
    int             TotalVtxCount = 0;
    ImVec2          DisplayPos;
    ImVec2          DisplaySize;
    ImVec2          FramebufferScale;
    std::vector<DrawList> DrawLists;
};

struct FrameData
{
    uint64_t frameIndex = 0;
    float deltatime = 0.0166f;
    bool fullscreen = false;
    bool vsync = true;
    int width = 1280;
    int height = 800;
    DrawData drawData;
    RenderContext* renderContext;
};

}
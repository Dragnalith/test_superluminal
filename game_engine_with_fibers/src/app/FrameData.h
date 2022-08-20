#pragma once

#include <imgui/imgui.h>

#include <stdint.h>
#include <vector>

namespace app
{

struct DrawList
{
    ImVector<ImDrawCmd>     CmdBuffer;
    ImVector<ImDrawIdx>     IdxBuffer;
    ImVector<ImDrawVert>    VtxBuffer;
    ImDrawListFlags         Flags;
};

struct DrawData
{
    bool            Valid;
    int             CmdListsCount;
    int             TotalIdxCount;
    int             TotalVtxCount;
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
};

}
// Deterministic RHI conformance scenes (M05-12/13/14): every rhi::Device method and every shader variant of
// render::gsShaders, drawn into offscreen targets and read back. The same code runs on upstream's Vulkan backend (the
// desktop reference, host) and on the port's OpenGL backend (host Mesa and the Switch); tools/rhi_compare.py compares
// the readbacks. Only rhi.h, gs_shaders.h and (for OpenGL statistics) rhi_gl.h: no game code, no assets.
#pragma once

#include "rhi/rhi.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace conformance
{
    struct Image
    {
        std::string name;       // scene/part, [a-z0-9_/]
        std::string kind;       // rgba8 | r32f | d32f
        uint32_t width = 0, height = 0;
        std::string tolerance;  // how backends may differ: exact | unorm:N[,pixels:K] | float:E | impl:<why>
        std::vector<uint8_t> data; // rows top-down, tightly packed
    };

    struct Sink
    {
        std::function<void(const Image &)> image;              // a readback to compare across backends
        std::function<void(const std::string &, bool, const std::string &)> check; // a self-check: name, passed, detail
        std::function<void(const std::string &)> log;
        std::function<uint64_t()> processMemory;               // bytes the process uses (0: unknown)
        std::function<void()> frame;                           // between scenes: present / pump events (may be empty)
    };

    struct Options
    {
        uint32_t churnRounds = 200;  // resource churn (M05-14)
        uint32_t resizeRounds = 60;  // offscreen / window resizes
    };

    // Runs every scene; returns the number of failed self-checks (a device error counts as one).
    int run(rhi::Device &device, Sink &sink, const Options &options);
}

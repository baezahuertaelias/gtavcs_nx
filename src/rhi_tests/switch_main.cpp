// Switch runner of the RHI conformance scenes (M05-12/13/14) and the GL threading measurement (M05-09/10).
// gtavcs_rhi_tests.nro, public (no game code). The window and GL context are made exactly as src/main.c makes them
// (SDL2, SDL_WINDOW_OPENGL, GL 3.3 core); the device gets them through rhi::GlWindow. Results go to
// sdmc:/switch/gtavcs_nx/rhi-tests/ (index.tsv + .bin readbacks, checks.tsv, bench.tsv, window.tsv, log.txt,
// summary.txt), fetched by tools/switch_m05_session.py and compared with tools/rhi_compare.py.
//
// Phases:
//   1. conformance: every scene of rhi_conformance.cpp (PS2X_GL_WORKER_CONTEXT=1, so free_threaded runs too)
//   2. threading (M05-09): a texture burst made (A) on the owner thread, the upstream design (record on workers,
//      every GL call on the main thread), and (B) on a worker thread through the loader context; main-thread frame
//      times during the burst, worker time, fences waited, contents checked
//   3. window (M05-10/11/14): the back buffer read back against the same drawing in a texture (viewports, scissors,
//      the UI pipeline), then the SDL back buffer drawn and swapped for WINDOW_SECONDS with an orientation card (red
//      top-left, green top-right, blue bottom-left, white bottom-right): docking by hand, the swap interval
//      experiments (1 / 0 / 2), the program's own SDL_SetWindowSize, docking by hand again (window() has the timeline)
#include "rhi_tests/rhi_conformance.h"

#include "platform/gl_loader_egl.h"
#include "render/gs_shaders.h"
#include "rhi/gl/rhi_gl.h"
#include "rhi/rhi.h"

#include <glad/gl.h>
#include <SDL.h>
#include <malloc.h>
#include <switch.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <vector>

namespace
{
    constexpr const char *kOut = "sdmc:/switch/gtavcs_nx/rhi-tests";
    constexpr double kWindowSeconds = 110.0;
    using Clock = std::chrono::steady_clock;

    SDL_Window *g_window = nullptr;
    std::FILE *g_log = nullptr;

    void say(const char *format, ...) __attribute__((format(printf, 1, 2)));
    void say(const char *format, ...)
    {
        char line[1024];
        va_list args;
        va_start(args, format);
        std::vsnprintf(line, sizeof(line), format, args);
        va_end(args);
        std::fputs(line, stderr);
        std::fflush(stderr);
        std::fputs(line, stdout); // nxlink, when connected
        std::fflush(stdout);
    }

    double ms(Clock::time_point a, Clock::time_point b) { return std::chrono::duration<double, std::milli>(b - a).count(); }

    // Heap bytes in use (newlib mallinfo): libnx reserves the whole heap at start, so svcGetInfo(UsedMemorySize)
    // stays constant; Mesa nouveau's buffers and textures come from this heap (the first M05 run read 3339763712
    // before and after the churn from svcGetInfo, which proves nothing)
    uint64_t usedMemory() { return static_cast<uint64_t>(mallinfo().uordblks); }

    void *procAddress(const char *name) { return SDL_GL_GetProcAddress(name); }
    void swapWindow() { SDL_GL_SwapWindow(g_window); }
    void drawableSize(uint32_t *w, uint32_t *h)
    {
        int iw = 0, ih = 0;
        SDL_GL_GetDrawableSize(g_window, &iw, &ih);
        *w = static_cast<uint32_t>(std::max(iw, 0));
        *h = static_cast<uint32_t>(std::max(ih, 0));
    }
    bool swapInterval(int interval) { return SDL_GL_SetSwapInterval(interval) == 0; }

    bool pump()
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
            if (e.type == SDL_QUIT)
                return false;
        return appletMainLoop();
    }

    double median(std::vector<double> v)
    {
        if (v.empty())
            return 0.0;
        std::sort(v.begin(), v.end());
        return v[v.size() / 2];
    }

    double percentile(std::vector<double> v, double p)
    {
        if (v.empty())
            return 0.0;
        std::sort(v.begin(), v.end());
        return v[std::min(v.size() - 1, static_cast<size_t>(p * v.size()))];
    }

    // ---- phase 2: threading ----------------------------------------------------------------------------------------
    struct Burst
    {
        double createMs = 0;      // the creating thread's time for every texture
        double ownerMs = 0;       // main-thread time spent in the burst (A: all of it; B: draws + fence waits)
        double wallMs = 0;        // first creation to every texture usable on the owner
        double worstFrameMs = 0;  // the longest main-thread frame while the burst ran
        double medianFrameMs = 0;
        uint64_t frames = 0;
        bool contentsOk = false;
    };

    // One frame of main-thread work: a pass with a fill into a 256x256 target (stands for the presenter's frame)
    struct FrameWork
    {
        rhi::Device &dev;
        std::unique_ptr<rhi::Texture> target;
        std::unique_ptr<rhi::Pipeline> fill;
        std::unique_ptr<rhi::Buffer> cb;
        explicit FrameWork(rhi::Device &d) : dev(d)
        {
            rhi::TextureDesc td;
            td.width = td.height = 256;
            td.usage = rhi::UsageRenderTarget | rhi::UsageSampled;
            td.name = "frame";
            target = dev.createTexture(td);
            const render::GsShaders &s = render::gsShaders(dev.backend());
            rhi::PipelineDesc pd;
            pd.vertex = s.quadVS;
            pd.pixel = s.fillColorPS;
            pd.topology = rhi::Topology::TriangleStrip;
            pd.name = "frame fill";
            fill = dev.createPipeline(pd);
            cb = dev.createBuffer({64, rhi::BufferUsage::Constant, true, "frame constants"});
        }
        void run(int k)
        {
            const float q[16] = {-1, 1, 1, -1, 0, 0, 1, 1, (k % 7) / 7.0f, 0.5f, 0.25f, 1, 0, 0, 0, 0};
            rhi::PassDesc p;
            p.color = target.get();
            p.name = "frame";
            dev.beginPass(p);
            dev.updateBuffer(*cb, q, sizeof(q));
            dev.setPipeline(*fill);
            dev.setConstants(0, *cb);
            dev.draw(4);
            dev.endPass();
            uint8_t px[4];
            dev.readTexture(*target, 0, 0, 1, 1, px); // waits for the GPU: a frame's work, not just its recording
        }
    };

    bool checkTexture(rhi::Device &dev, rhi::Texture &tex, const std::vector<uint32_t> &level0)
    {
        // sampled on the owner through the present shader, read back, compared with level 0
        const render::GsShaders &s = render::gsShaders(dev.backend());
        rhi::PipelineDesc pd;
        pd.vertex = s.presentVS;
        pd.pixel = s.presentPS;
        pd.topology = rhi::Topology::TriangleStrip;
        pd.name = "check";
        auto pipe = dev.createPipeline(pd);
        rhi::TextureDesc td;
        td.width = td.height = 128;
        td.usage = rhi::UsageRenderTarget | rhi::UsageSampled;
        td.name = "check";
        auto t = dev.createTexture(td);
        auto cb = dev.createBuffer({32, rhi::BufferUsage::Constant, true, "check constants"});
        auto sampler = dev.createSampler({rhi::Filter::Nearest, rhi::Wrap::Clamp, rhi::Wrap::Clamp});
        if (!pipe || !t || !cb || !sampler)
            return false;
        const float params[8] = {-1, 1, 1, -1, 1, 1, 0, 0};
        rhi::PassDesc p;
        p.color = t.get();
        dev.beginPass(p);
        dev.updateBuffer(*cb, params, sizeof(params));
        dev.setPipeline(*pipe);
        dev.setConstants(0, *cb);
        dev.setTexture(0, &tex, sampler.get());
        dev.draw(4);
        dev.setTexture(0, nullptr, nullptr);
        dev.endPass();
        std::vector<uint32_t> px(128 * 128);
        return dev.readTexture(*t, 0, 0, 128, 128, px.data()) && std::memcmp(px.data(), level0.data(), px.size() * 4) == 0;
    }

    void threading(rhi::Device &dev, std::ofstream &bench)
    {
        constexpr int kTextures = 256, kRounds = 3;
        // 128x128 with the full chain (8 levels): the size and shape of a typical game texture burst (gs_hw.cpp makes
        // a few hundred at a loading screen; PERFORMANCE.md 25)
        std::vector<std::vector<std::vector<uint32_t>>> data(kTextures);
        for (int i = 0; i < kTextures; ++i)
            for (uint32_t s = 128, l = 0; s >= 1; s >>= 1, ++l)
            {
                std::vector<uint32_t> level(static_cast<size_t>(s) * s);
                uint32_t x = 0x9E3779B9u * (i + 1) + l;
                for (auto &v : level)
                    v = (x = x * 1664525u + 1013904223u);
                data[i].push_back(std::move(level));
            }
        FrameWork frame(dev);
        bench << "mode\tround\ttextures\tcreate_ms\towner_ms\twall_ms\tframes\tmedian_frame_ms\tworst_frame_ms\tcontents\n";
        for (int round = 0; round < kRounds; ++round)
            for (int mode = 0; mode < 2; ++mode)
            {
                if (mode == 1 && !dev.freeThreadedCreation())
                    continue;
                Burst b;
                std::vector<std::unique_ptr<rhi::Texture>> made(kTextures);
                std::vector<double> frames;
                auto make = [&](int i) {
                    const void *levels[8];
                    for (int l = 0; l < 8; ++l)
                        levels[l] = data[i][l].data();
                    rhi::TextureDesc d;
                    d.width = d.height = 128;
                    d.name = "burst";
                    made[i] = dev.createTextureLevels(d, levels, 8);
                };
                for (int k = 0; k < 5; ++k)
                    frame.run(k); // warm
                const auto t0 = Clock::now();
                if (mode == 0)
                {
                    // A: the owner makes the burst inside one frame (what the upstream design does on the main thread)
                    const auto f0 = Clock::now();
                    for (int i = 0; i < kTextures; ++i)
                        make(i);
                    b.createMs = ms(f0, Clock::now());
                    frame.run(0);
                    frames.push_back(ms(f0, Clock::now()));
                    for (int k = 1; k < 30; ++k)
                    {
                        const auto f = Clock::now();
                        frame.run(k);
                        frames.push_back(ms(f, Clock::now()));
                    }
                    b.ownerMs = b.createMs;
                }
                else
                {
                    // B: a worker makes them on the loader context while the owner keeps making frames
                    std::atomic<bool> done{false};
                    double workerMs = 0;
                    std::thread worker([&] {
                        const auto w0 = Clock::now();
                        for (int i = 0; i < kTextures; ++i)
                            make(i);
                        workerMs = ms(w0, Clock::now());
                        done = true;
                    });
                    int k = 0;
                    while (!done.load() || k < 30)
                    {
                        const auto f = Clock::now();
                        frame.run(k++);
                        frames.push_back(ms(f, Clock::now()));
                    }
                    worker.join();
                    b.createMs = workerMs;
                }
                // first use on the owner: B waits for each texture's fence here
                rhi::GlStats s0{}, s1{};
                rhi::openGLStats(dev, s0);
                const auto u0 = Clock::now();
                b.contentsOk = true;
                for (int i = 0; i < kTextures; ++i)
                    if (!made[i] || (i % 32 == 0 && !checkTexture(dev, *made[i], data[i][0])))
                        b.contentsOk = false;
                const double useMs = ms(u0, Clock::now());
                rhi::openGLStats(dev, s1);
                if (mode == 1)
                    b.ownerMs = useMs;
                b.wallMs = ms(t0, Clock::now());
                b.frames = frames.size();
                b.medianFrameMs = median(frames);
                b.worstFrameMs = frames.empty() ? 0 : *std::max_element(frames.begin(), frames.end());
                char row[256];
                std::snprintf(row, sizeof(row), "%s\t%d\t%d\t%.3f\t%.3f\t%.3f\t%llu\t%.3f\t%.3f\t%s\n", mode ? "B_worker_context" : "A_owner",
                              round, kTextures, b.createMs, b.ownerMs, b.wallMs, static_cast<unsigned long long>(b.frames), b.medianFrameMs,
                              b.worstFrameMs, b.contentsOk ? "ok" : "BAD");
                bench << row;
                bench.flush();
                say("[bench] %s", row);
                say("[bench] fences waited %llu (use %.3f ms)\n", static_cast<unsigned long long>(s1.fencesWaited - s0.fencesWaited), useMs);
                made.clear();
                dev.present(rhi::Device::PresentMode::Immediate);
                pump();
            }
    }

    // ---- phase 3a: the window's back buffer read back (first M05 review: the window path had no readback) --------
    // The same drawing into the SDL back buffer and into a texture of its size: rectangles through sub-viewports and
    // scissors (the window's bottom-left conversion in rhi_gl.cpp), and the Dear ImGui pipeline (UByte4Norm colours,
    // 16-bit indices, a clip scissor, its own projection). The back buffer is read with glReadPixels before the swap
    // and its rows flipped (GL's window rows are bottom-up); the two must be identical. Edges lie both on pixel
    // boundaries and on pixel centres (second M05 review): rectangles from x + 0.5 to x1 + 0.5 as the GS renderer's
    // half-pixel shift makes every GS edge, and UI quads at half-pixel positions as Dear ImGui's anti-aliased fringes
    // are, so the window's GL_LOWER_LEFT convention is compared with the textures' GL_UPPER_LEFT on tie edges too.
    struct UiVertex
    {
        float pos[2];
        float uv[2];
        uint32_t col;
    };

    bool windowReadback(rhi::Device &dev, std::ofstream &checks, std::ofstream &out)
    {
        uint32_t w = 0, h = 0;
        drawableSize(&w, &h);
        dev.resize(w, h);
        const render::GsShaders &s = render::gsShaders(dev.backend());
        rhi::PipelineDesc fd;
        fd.vertex = s.quadVS;
        fd.pixel = s.fillColorPS;
        fd.topology = rhi::Topology::TriangleStrip;
        fd.name = "window fill";
        auto fill = dev.createPipeline(fd);
        rhi::PipelineDesc ud;
        ud.vertex = s.uiVS;
        ud.pixel = s.uiPS;
        ud.attributes = {{"POSITION", 0, rhi::VertexFormat::Float2, 0}, {"TEXCOORD", 0, rhi::VertexFormat::Float2, 8},
                         {"COLOR", 0, rhi::VertexFormat::UByte4Norm, 16}};
        ud.stride = sizeof(UiVertex);
        ud.blend.enable = true;
        ud.blend.srcColor = rhi::BlendFactor::SrcAlpha;
        ud.blend.dstColor = rhi::BlendFactor::InvSrcAlpha;
        ud.blend.srcAlpha = rhi::BlendFactor::One;
        ud.blend.dstAlpha = rhi::BlendFactor::InvSrcAlpha;
        ud.scissor = true;
        ud.name = "window ui";
        auto ui = dev.createPipeline(ud);
        auto quad = dev.createBuffer({64, rhi::BufferUsage::Constant, true, "window quad"});
        auto uiCb = dev.createBuffer({16, rhi::BufferUsage::Constant, true, "window ui constants"});
        const uint32_t white = 0xFFFFFFFFu;
        rhi::TextureDesc wt;
        wt.width = wt.height = 1;
        wt.name = "white";
        auto whiteTex = dev.createTexture(wt, &white);
        auto linear = dev.createSampler({rhi::Filter::Linear, rhi::Wrap::Clamp, rhi::Wrap::Clamp});
        const float W = static_cast<float>(w), H = static_cast<float>(h);
        // UI: two quads in ImGui's top-left pixel space, one clipped by a scissor through its middle
        const UiVertex verts[] = {{{W * 0.1f, H * 0.6f}, {0, 0}, 0xFF0000FFu}, {{W * 0.45f, H * 0.6f}, {1, 0}, 0xC000FF00u},
                                  {{W * 0.1f, H * 0.9f}, {0, 1}, 0x80FF0000u}, {{W * 0.45f, H * 0.9f}, {1, 1}, 0xFFFFFFFFu},
                                  {{W * 0.55f, H * 0.55f}, {0, 0}, 0xFF00FFFFu}, {{W * 0.95f, H * 0.55f}, {1, 0}, 0xFF00FFFFu},
                                  {{W * 0.55f, H * 0.95f}, {0, 1}, 0xFFFF00FFu}, {{W * 0.95f, H * 0.95f}, {1, 1}, 0xFFFF00FFu},
                                  // half-pixel corners (an anti-aliased fringe's positions): edges on pixel centres
                                  {{W * 0.5f + 10.5f, H * 0.1f + 0.5f}, {0, 0}, 0xFF80FF40u}, {{W * 0.5f + 60.5f, H * 0.1f + 0.5f}, {1, 0}, 0xFF80FF40u},
                                  {{W * 0.5f + 10.5f, H * 0.1f + 40.5f}, {0, 1}, 0xFF80FF40u}, {{W * 0.5f + 60.5f, H * 0.1f + 40.5f}, {1, 1}, 0xFF80FF40u}};
        const uint16_t idx[] = {0, 1, 2, 1, 3, 2, 4, 5, 6, 5, 7, 6, 8, 9, 10, 9, 11, 10};
        auto vb = dev.createBuffer({sizeof(verts), rhi::BufferUsage::Vertex, false, "window ui vertices"}, verts);
        auto ib = dev.createBuffer({sizeof(idx), rhi::BufferUsage::Index, false, "window ui indices"}, idx);
        rhi::TextureDesc td;
        td.width = w;
        td.height = h;
        td.usage = rhi::UsageRenderTarget | rhi::UsageSampled;
        td.name = "window twin";
        auto twin = dev.createTexture(td);
        if (!fill || !ui || !quad || !uiCb || !whiteTex || !linear || !vb || !ib || !twin)
        {
            checks << "window/set_up\tFAIL\t\n";
            return false;
        }
        auto draw = [&](rhi::Texture *target) {
            rhi::PassDesc p;
            p.color = target;
            p.clearColor = true;
            p.clearRgba[0] = 0.0f, p.clearRgba[1] = 0.0f, p.clearRgba[2] = 64 / 255.0f, p.clearRgba[3] = 1.0f;
            p.name = "window readback";
            dev.beginPass(p);
            const uint32_t iw = w / 8u, ih = h / 8u;
            struct Rect
            {
                uint32_t vx, vy, vw, vh, sx, sy, sw, sh;
                float r, g, b;
            } rects[] = {
                {0, 0, iw * 3, ih * 2, 0, 0, iw * 3, ih * 2, 1, 0, 0},                 // top-left
                {iw * 5, 0, iw * 3, ih * 2, iw * 6, ih / 2, iw * 2, ih, 0, 1, 0},     // top-right, scissored
                {iw * 3, ih * 2, iw * 2, ih * 2, iw * 3, ih * 3, iw * 2, ih, 1, 1, 0}, // middle, lower half only
            };
            for (const Rect &r : rects)
            {
                const float q[16] = {-1, 1, 1, -1, 0, 0, 1, 1, r.r, r.g, r.b, 1, 0, 0, 0, 0};
                dev.setViewport(static_cast<float>(r.vx), static_cast<float>(r.vy), static_cast<float>(r.vw), static_cast<float>(r.vh));
                dev.setScissor(static_cast<int>(r.sx), static_cast<int>(r.sy), static_cast<int>(r.sw), static_cast<int>(r.sh));
                dev.updateBuffer(*quad, q, sizeof(q));
                dev.setPipeline(*fill);
                dev.setConstants(0, *quad);
                dev.draw(4);
            }
            dev.setViewport(0, 0, W, H);
            dev.setScissor(0, 0, static_cast<int>(w), static_cast<int>(h));
            // rectangles whose edges lie exactly on pixel centres (x0 + 0.5 .. x1 + 0.5), of 1 to 9 pixels, as GS
            // sprites are drawn after gs_hw.cpp's half-pixel shift: D3D's top-left rule decides every edge
            for (int k = 0; k < 9; ++k)
            {
                const float x0 = 20.0f + k * 14.0f + 0.5f, y0 = H * 0.5f + (k % 3) * 3.0f + 0.5f;
                const float x1 = x0 + 1.0f + k, y1 = y0 + 1.0f + (k * 2) % 9;
                const float q[16] = {x0 / W * 2.0f - 1.0f, 1.0f - y0 / H * 2.0f, x1 / W * 2.0f - 1.0f, 1.0f - y1 / H * 2.0f, 0, 0, 1, 1,
                                     (k & 1) ? 1.0f : 0.5f, (k & 2) ? 1.0f : 0.25f, (k & 4) ? 1.0f : 0.0f, 1, 0, 0, 0, 0};
                dev.updateBuffer(*quad, q, sizeof(q));
                dev.setPipeline(*fill);
                dev.setConstants(0, *quad);
                dev.draw(4);
            }
            const float so[4] = {2.0f / W, -2.0f / H, -1.0f, 1.0f};
            dev.updateBuffer(*uiCb, so, sizeof(so));
            dev.setPipeline(*ui);
            dev.setConstants(0, *uiCb);
            dev.setVertexBuffer(*vb, 0);
            dev.setIndexBuffer(*ib, false);
            dev.setTexture(0, whiteTex.get(), linear.get());
            dev.drawIndexed(6, 0, 0);
            dev.setScissor(static_cast<int>(w * 0.55f), static_cast<int>(h * 0.55f), static_cast<int>(w * 0.2f), static_cast<int>(h * 0.25f));
            dev.drawIndexed(6, 6, 0);
            dev.setScissor(0, 0, static_cast<int>(w), static_cast<int>(h));
            dev.drawIndexed(6, 12, 0);
            dev.setTexture(0, nullptr, nullptr);
            dev.endPass();
        };
        draw(twin.get());
        std::vector<uint8_t> expect(static_cast<size_t>(w) * h * 4), got(expect.size()), rows(expect.size());
        dev.readTexture(*twin, 0, 0, w, h, expect.data());
        // the half-pixel rectangles really are tie cases: in the texture (D3D's rule, proven by pixel_centre) each
        // covers columns floor(x0)..floor(x1)-1 and rows floor(y0)..floor(y1)-1, its edge pixels' centres on the edges
        bool ties = true;
        for (int k = 0; k < 9 && ties; ++k)
        {
            const int x0 = 20 + k * 14, y0 = static_cast<int>(h / 2) + (k % 3) * 3, x1 = x0 + 1 + k, y1 = y0 + 1 + (k * 2) % 9;
            for (int y = y0 - 1; y <= y1; ++y)
                for (int x = x0 - 1; x <= x1; ++x)
                {
                    const uint8_t *px = expect.data() + (static_cast<size_t>(y) * w + x) * 4;
                    const bool lit = px[0] >= 128 || px[1] >= 64 || px[2] >= 128; // the rectangle colours over the clear
                    const bool in = x >= x0 && x < x1 && y >= y0 && y < y1;
                    if (lit != in)
                        ties = false;
                }
        }
        checks << "window/half_pixel_edges_top_left_rule\t" << (ties ? "PASS" : "FAIL") << "\tthe texture twin's tie rectangles\n";
        draw(nullptr);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glReadBuffer(GL_BACK);
        glReadPixels(0, 0, static_cast<GLsizei>(w), static_cast<GLsizei>(h), GL_RGBA, GL_UNSIGNED_BYTE, rows.data());
        for (uint32_t y = 0; y < h; ++y) // the window's rows come bottom-up
            std::memcpy(got.data() + static_cast<size_t>(y) * w * 4, rows.data() + static_cast<size_t>(h - 1 - y) * w * 4, w * 4u);
        size_t differing = 0, first = SIZE_MAX;
        for (size_t i = 0; i < static_cast<size_t>(w) * h; ++i)
            if (std::memcmp(got.data() + i * 4, expect.data() + i * 4, 4) != 0)
            {
                ++differing;
                first = std::min(first, i);
            }
        dev.present(rhi::Device::PresentMode::Vsync);
        char detail[200];
        std::snprintf(detail, sizeof(detail), "%ux%u, %zu differing pixels%s", w, h, differing,
                      first == SIZE_MAX ? "" : (", first at " + std::to_string(first % w) + "," + std::to_string(first / w)).c_str());
        checks << "window/back_buffer_equals_texture\t" << (differing == 0 ? "PASS" : "FAIL") << '\t' << detail << '\n';
        checks.flush();
        out << "0\twindow_readback\t" << w << '\t' << h << '\t' << detail << '\n';
        say("[window] back buffer readback vs texture: %s\n", detail);
        return differing == 0 && ties;
    }

    // ---- phase 3a': present-interval probe (first M05 review, finding 2) --------------------------------------------
    // 3 s steps, each recording swaps per second and SDL_GL_GetSwapInterval: does the device's own interval change take
    // effect after Immediate, after a direct SDL change, after a window resize? (A 60 Hz display: Vsync = 60 per
    // second; interval 0 = 120 on this console; interval 2 = 30.)
    void presentProbe(rhi::Device &dev, std::ofstream &out)
    {
        struct Step
        {
            const char *what;
            rhi::Device::PresentMode mode;
            int direct;  // >= 0: SDL_GL_SetSwapInterval(direct) before the step (the device does not know)
            bool resize; // SDL_SetWindowSize to the current size before the step
        } steps[] = {
            {"device Immediate", rhi::Device::PresentMode::Immediate, -1, false},
            {"device Vsync", rhi::Device::PresentMode::Vsync, -1, false},
            {"device Immediate", rhi::Device::PresentMode::Immediate, -1, false},
            {"device Vsync", rhi::Device::PresentMode::Vsync, -1, false},
            {"direct 2, device Vsync", rhi::Device::PresentMode::Vsync, 2, false},
            {"device Immediate", rhi::Device::PresentMode::Immediate, -1, false},
            {"device Vsync", rhi::Device::PresentMode::Vsync, -1, false},
            {"window resized, device Vsync", rhi::Device::PresentMode::Vsync, -1, true},
            {"device Immediate", rhi::Device::PresentMode::Immediate, -1, false},
            {"device Vsync", rhi::Device::PresentMode::Vsync, -1, false},
        };
        int index = 0;
        for (const Step &step : steps)
        {
            if (step.direct >= 0)
                SDL_GL_SetSwapInterval(step.direct);
            if (step.resize)
            {
                int ww = 0, wh = 0;
                SDL_GetWindowSize(g_window, &ww, &wh);
                SDL_SetWindowSize(g_window, ww, wh);
            }
            std::vector<double> intervals;
            const auto start = Clock::now();
            auto prev = start;
            while (ms(start, Clock::now()) < 3000.0 && pump())
            {
                rhi::PassDesc pass;
                pass.clearColor = true;
                pass.clearRgba[0] = (index % 2) ? 0.2f : 0.0f;
                pass.name = "probe";
                dev.beginPass(pass);
                dev.endPass();
                dev.present(step.mode);
                const auto now = Clock::now();
                if (ms(start, now) > 500.0)
                    intervals.push_back(ms(prev, now));
                prev = now;
            }
            double total = 0;
            for (double v : intervals)
                total += v;
            char row[256];
            std::snprintf(row, sizeof(row), "probe\t%d\t%.1f per s\tSDL interval %d\t%s (median %.3f ms)\n", index,
                          total > 0 ? intervals.size() * 1000.0 / total : 0.0, SDL_GL_GetSwapInterval(), step.what, median(intervals));
            out << row;
            out.flush();
            say("[window] %s", row);
            ++index;
        }
    }

    // ---- phase 3b: window, docking and presentation ---------------------------------------------------------------
    // Timeline (seconds after the card appears):
    //    0-40   docking by hand, swap interval 1 (Vsync), nothing resized by the program
    //   40-55   presentation without docking: interval 1 (40-45), 0 (45-50), 2 (50-55; SDL_GL_SetSwapInterval directly)
    //   55-67   SDL_SetWindowSize 1920x1080 / 960x540 / 1280x720 / 1280x720 (3 s each)
    //   67-110  docking by hand again, after the program's own resizes (the rhi-attended2 black screen's order)
    // Logged: every operation-mode / SDL window / drawable change, and every second the swaps, their median / max
    // interval and SDL_GL_GetSwapInterval.
    void window(rhi::Device &dev, std::ofstream &out)
    {
        // the orientation card: quadrants red (top-left), green (top-right), blue (bottom-left), white (bottom-right),
        // a one-pixel yellow frame at the very edges
        constexpr uint32_t W = 64, H = 36;
        std::vector<uint32_t> card(W * H);
        for (uint32_t y = 0; y < H; ++y)
            for (uint32_t x = 0; x < W; ++x)
            {
                const bool right = x >= W / 2, bottom = y >= H / 2;
                uint32_t c = !right && !bottom ? 0xFF0000FFu : right && !bottom ? 0xFF00FF00u : !right ? 0xFFFF0000u : 0xFFFFFFFFu;
                if (x == 0 || y == 0 || x == W - 1 || y == H - 1)
                    c = 0xFF00FFFFu;
                card[y * W + x] = c;
            }
        rhi::TextureDesc td;
        td.width = W;
        td.height = H;
        td.name = "orientation card";
        auto tex = dev.createTexture(td, card.data());
        const render::GsShaders &s = render::gsShaders(dev.backend());
        rhi::PipelineDesc pd;
        pd.vertex = s.presentVS;
        pd.pixel = s.presentPS;
        pd.topology = rhi::Topology::TriangleStrip;
        pd.name = "window";
        auto pipe = dev.createPipeline(pd);
        auto cb = dev.createBuffer({32, rhi::BufferUsage::Constant, true, "window constants"});
        auto nearest = dev.createSampler({rhi::Filter::Nearest, rhi::Wrap::Clamp, rhi::Wrap::Clamp});
        if (!tex || !pipe || !cb || !nearest)
        {
            say("[window] set-up failed\n");
            return;
        }
        const auto t0 = Clock::now();
        uint32_t lastW = 0, lastH = 0;
        int lastMode = -1, lastWinW = 0, lastWinH = 0, phase = -100;
        double secondStart = 0.0;
        std::vector<double> second;
        std::vector<double> perInterval[3];
        auto prev = Clock::now();
        const int sizes[][2] = {{1920, 1080}, {960, 540}, {1280, 720}, {1280, 720}};
        while (pump())
        {
            const double t = std::chrono::duration<double>(Clock::now() - t0).count();
            if (t > kWindowSeconds)
                break;
            int p = 0; // 0 dock by hand #1, 1..3 interval 1/0/2, 4..7 programmatic sizes, 8 dock by hand #2
            if (t < 40.0)
                p = 0;
            else if (t < 55.0)
                p = 1 + static_cast<int>((t - 40.0) / 5.0);
            else if (t < 67.0)
                p = 4 + static_cast<int>((t - 55.0) / 3.0);
            else
                p = 8;
            if (p != phase)
            {
                phase = p;
                if (p == 0)
                    out << t << "\thand\t0\t0\tdock / undock by hand (0-40 s), nothing resized by the program\n";
                else if (p >= 1 && p <= 3)
                {
                    const int interval = p == 1 ? 1 : p == 2 ? 0 : 2;
                    // the device keeps the interval it set last; set it here directly (the device presents Immediate
                    // in this stretch so it never changes it back)
                    SDL_GL_SetSwapInterval(interval);
                    out << t << "\tswap_interval\t" << interval << "\t" << SDL_GL_GetSwapInterval() << "\tSDL_GL_SetSwapInterval / Get\n";
                }
                else if (p >= 4 && p <= 7)
                {
                    SDL_SetWindowSize(g_window, sizes[p - 4][0], sizes[p - 4][1]);
                    out << t << "\tset_window_size\t" << sizes[p - 4][0] << '\t' << sizes[p - 4][1] << "\tSDL_SetWindowSize\n";
                }
                else
                    out << t << "\thand\t0\t0\tdock / undock by hand again (67-110 s), after the program's resizes\n";
                out.flush();
            }
            const int mode = appletGetOperationMode();
            int winW = 0, winH = 0;
            SDL_GetWindowSize(g_window, &winW, &winH);
            if (mode != lastMode || winW != lastWinW || winH != lastWinH)
            {
                out << t << "\tmode\t" << winW << '\t' << winH << "\toperation mode " << mode << " (was " << lastMode << ")\n";
                out.flush();
                say("[window] t=%.2f operation mode %d, SDL window %dx%d\n", t, mode, winW, winH);
                lastMode = mode;
                lastWinW = winW;
                lastWinH = winH;
            }
            uint32_t w = 0, h = 0;
            drawableSize(&w, &h);
            dev.resize(w, h);
            uint32_t bw = 0, bh = 0;
            dev.backbufferSize(bw, bh);
            if (bw != lastW || bh != lastH)
            {
                out << t << "\tdrawable\t" << bw << '\t' << bh << "\toperation mode " << mode << '\n';
                out.flush();
                lastW = bw;
                lastH = bh;
            }
            rhi::PassDesc pass;
            pass.clearColor = true;
            pass.clearRgba[0] = pass.clearRgba[1] = pass.clearRgba[2] = 0.1f;
            pass.name = "window";
            dev.beginPass(pass);
            const float params[8] = {-0.9f, 0.9f, 0.9f, -0.9f, 1, 1, 0, 0};
            dev.updateBuffer(*cb, params, sizeof(params));
            dev.setPipeline(*pipe);
            dev.setConstants(0, *cb);
            dev.setTexture(0, tex.get(), nearest.get());
            dev.draw(4);
            dev.setTexture(0, nullptr, nullptr);
            dev.endPass();
            // Vsync while docking (the device sets interval 1); Immediate during the interval experiments, whose
            // interval this loop sets itself (the device then leaves it alone)
            dev.present(p >= 1 && p <= 3 ? rhi::Device::PresentMode::Immediate : rhi::Device::PresentMode::Vsync);
            if (p >= 1 && p <= 3 && SDL_GL_GetSwapInterval() != (p == 1 ? 1 : p == 2 ? 0 : 2))
                SDL_GL_SetSwapInterval(p == 1 ? 1 : p == 2 ? 0 : 2); // the device switched to 0 on its first Immediate
            const auto now = Clock::now();
            const double interval = ms(prev, now);
            prev = now;
            second.push_back(interval);
            if (p >= 1 && p <= 3 && std::fmod(t - 40.0, 5.0) > 0.5) // skip the first half second after a change
                perInterval[p - 1].push_back(interval);
            if (t - secondStart >= 1.0)
            {
                out << t << "\tinterval\t" << second.size() << "\t" << SDL_GL_GetSwapInterval() << "\tmedian_ms " << median(second)
                    << " max_ms " << *std::max_element(second.begin(), second.end()) << " mode " << mode << " phase " << p << '\n';
                second.clear();
                secondStart = t;
            }
        }
        const char *names[3] = {"interval_1", "interval_0", "interval_2"};
        for (int i = 0; i < 3; ++i)
        {
            const double seconds = [&] {
                double sum = 0;
                for (double v : perInterval[i])
                    sum += v;
                return sum / 1000.0;
            }();
            char row[256];
            std::snprintf(row, sizeof(row), "summary\t%s\tswaps %zu in %.2f s = %.1f per s\tmedian_ms %.3f p95 %.3f\n", names[i],
                          perInterval[i].size(), seconds, seconds > 0 ? perInterval[i].size() / seconds : 0.0, median(perInterval[i]),
                          percentile(perInterval[i], 0.95));
            out << row;
            say("[window] %s", row);
        }
    }

}

int main(int, char **)
{
    int status = 1;
    const bool sockets = R_SUCCEEDED(socketInitializeDefault());
    if (sockets)
        nxlinkStdio();
    mkdir("sdmc:/switch/gtavcs_nx", 0777);
    mkdir(kOut, 0777);
    const std::string out = kOut;
    g_log = std::freopen((out + "/log.txt").c_str(), "w", stderr);
    say("[rhi-tests] start; HOS %08x applet %d\n", hosversionGet(), appletGetAppletType());
    {
        // the previous run's files go first: results.tsv lists only this run's
        std::ofstream(out + "/summary.txt") << "RUNNING\n";
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0)
    {
        say("SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    const int docked = appletGetOperationMode() == AppletOperationMode_Console;
    g_window = SDL_CreateWindow("RHI tests", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, docked ? 1920 : 1280, docked ? 1080 : 720,
                                SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
    SDL_GLContext context = g_window ? SDL_GL_CreateContext(g_window) : nullptr;
    if (!context || SDL_GL_MakeCurrent(g_window, context) < 0)
    {
        say("window / context: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    setenv("PS2X_GL_WORKER_CONTEXT", "1", 1);
    const bool loader = gl_loader::create();
    {
        rhi::DeviceDesc desc;
        desc.backend = rhi::Backend::OpenGL;
        desc.gl.getProcAddress = procAddress;
        desc.gl.swap = swapWindow;
        desc.gl.drawableSize = drawableSize;
        desc.gl.setSwapInterval = swapInterval;
        desc.gl.loaderAvailable = gl_loader::available;
        desc.gl.makeLoaderCurrent = gl_loader::makeCurrent;
        desc.width = docked ? 1920 : 1280;
        desc.height = docked ? 1080 : 720;
        desc.debug = true;
        std::string error;
        auto dev = rhi::createDevice(desc, error);
        if (!dev)
        {
            say("device: %s\n", error.c_str());
            std::ofstream(out + "/summary.txt") << "DEVICE_FAILED " << error << "\n";
        }
        else
        {
            say("[rhi-tests] %s; loader context %d; free-threaded %d\n", dev->adapterName().c_str(), loader,
                dev->freeThreadedCreation());
            rhi::GlCaps caps;
            rhi::openGLCaps(*dev, caps);
            std::ofstream capsOut(out + "/caps.txt");
            capsOut << "vendor " << caps.vendor << "\nrenderer " << caps.renderer << "\nversion " << caps.version << "\nglsl "
                    << caps.glsl << "\ngl " << caps.major << '.' << caps.minor << "\nclip_control " << caps.clipControl
                    << "\nviewport_array " << caps.viewportArray << "\nanisotropy " << caps.anisotropy << " max " << caps.maxAnisotropy
                    << "\nkhr_debug " << caps.khrDebug << "\nmax_dual_source_draw_buffers " << caps.maxDualSourceDrawBuffers
                    << "\nmax_samples " << caps.maxSamples << " color " << caps.maxColorTextureSamples << " depth "
                    << caps.maxDepthTextureSamples << "\nmax_texture_size " << caps.maxTextureSize << "\nmax_uniform_block_size "
                    << caps.maxUniformBlockSize << "\ndefault_framebuffer_encoding 0x" << std::hex << caps.defaultFramebufferEncoding
                    << std::dec << "\nloader_context " << loader << "\nused_memory_start " << usedMemory()
                    << "\noperation_mode_start " << appletGetOperationMode() << " (0 handheld, 1 docked)\n";
            capsOut.close();

            // 1. conformance
            std::ofstream index(out + "/index.tsv"), checks(out + "/checks.tsv"), scenes(out + "/scenes.txt");
            int images = 0;
            bool aborted = false;
            conformance::Sink sink;
            sink.image = [&](const conformance::Image &img) {
                std::string file = img.name;
                for (char &c : file)
                    if (c == '/')
                        c = '.';
                file += ".bin";
                std::ofstream(out + "/" + file, std::ios::binary)
                    .write(reinterpret_cast<const char *>(img.data.data()), static_cast<std::streamsize>(img.data.size()));
                index << img.name << '\t' << img.kind << '\t' << img.width << '\t' << img.height << '\t' << img.tolerance << '\t' << file << '\n';
                ++images;
            };
            sink.check = [&](const std::string &name, bool passed, const std::string &detail) {
                checks << name << '\t' << (passed ? "PASS" : "FAIL") << '\t' << detail << '\n';
                checks.flush();
                if (!passed)
                    say("FAIL %s %s\n", name.c_str(), detail.c_str());
            };
            sink.log = [&](const std::string &text) {
                scenes << text << '\n';
                scenes.flush();
                say("[rhi-tests] %s\n", text.c_str());
            };
            sink.processMemory = usedMemory;
            sink.frame = [&] { aborted = aborted || !pump(); };
            conformance::Options options;
            const auto c0 = Clock::now();
            const int failures = conformance::run(*dev, sink, options);
            say("[rhi-tests] conformance ran in operation mode %d at its end (0 handheld, 1 docked)\n", appletGetOperationMode());
            index.close();
            checks.close();
            say("[rhi-tests] conformance: %d images, %d failed checks, %.1f s\n", images, failures, ms(c0, Clock::now()) / 1000.0);

            // 2. threading
            std::ofstream bench(out + "/bench.tsv");
            if (!aborted)
                threading(*dev, bench);
            bench.close();

            int windowFailures = 0;
            // 3. window: the back buffer read back, then docking and presentation
            std::ofstream win(out + "/window.tsv");
            win << "t\tevent\ta\tb\tdetail\n";
            if (!aborted)
            {
                std::ofstream more(out + "/checks.tsv", std::ios::app);
                if (!windowReadback(*dev, more, win))
                    ++windowFailures;
                presentProbe(*dev, win);
                window(*dev, win);
            }
            win.close();

            rhi::GlStats st;
            rhi::openGLStats(*dev, st);
            std::ofstream summary(out + "/summary.txt");
            summary << (aborted ? "ABORTED" : "DONE") << "\nimages " << images << "\nfailed_checks " << failures + windowFailures << "\ngl_errors " << st.glErrors
                    << "\ndebug_messages " << st.debugMessages << "\nincomplete_framebuffers " << st.incompleteFramebuffers
                    << "\nowner_violations " << st.ownerViolations << "\nrejected " << st.rejected << "\nhazards " << st.hazards
                    << "\ncompile_failures " << st.compileFailures << "\noff_owner_created " << st.offOwnerCreated << "\nfences_waited "
                    << st.fencesWaited << "\ntextures_left " << st.textures << "\nused_memory_end " << usedMemory() << '\n';
            status = (failures || windowFailures || aborted) ? 1 : 0;
        }
    }
    gl_loader::destroy();
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(g_window);
    SDL_Quit();
    say("[rhi-tests] exit %d\n", status);
    if (g_log)
        std::fclose(g_log);
    if (sockets)
        socketExit();
    return status;
}

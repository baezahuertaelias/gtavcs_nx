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
//   3. window (M05-10/14): the SDL back buffer drawn and swapped for WINDOW_SECONDS: an orientation card (red top-left,
//      green top-right, blue bottom-left, white bottom-right), drawable sizes as SDL reports them (programmatic
//      SDL_SetWindowSize first, then docking by hand), present intervals with Vsync and Immediate
#include "rhi_tests/rhi_conformance.h"

#include "platform/gl_loader_egl.h"
#include "render/gs_shaders.h"
#include "rhi/gl/rhi_gl.h"
#include "rhi/rhi.h"

#include <SDL.h>
#include <switch.h>

#include <algorithm>
#include <atomic>
#include <chrono>
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
    constexpr double kWindowSeconds = 75.0;
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

    uint64_t usedMemory()
    {
        uint64_t used = 0;
        if (R_FAILED(svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0)))
            return 0;
        return used;
    }

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

    // ---- phase 3: window -------------------------------------------------------------------------------------------
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
        out << "t\tevent\twidth\theight\tdetail\n";
        const auto t0 = Clock::now();
        uint32_t lastW = 0, lastH = 0;
        std::vector<double> vsync, immediate;
        auto prev = Clock::now();
        int phase = -1;
        const int sizes[][2] = {{1280, 720}, {1920, 1080}, {960, 540}, {1280, 720}};
        while (pump())
        {
            const double t = std::chrono::duration<double>(Clock::now() - t0).count();
            if (t > kWindowSeconds)
                break;
            // 0-12 s: programmatic sizes, 3 s each; then the window stays as the system sets it (dock / undock by hand)
            const int p = t < 12.0 ? static_cast<int>(t / 3.0) : 4;
            if (p != phase)
            {
                phase = p;
                if (p < 4)
                {
                    SDL_SetWindowSize(g_window, sizes[p][0], sizes[p][1]);
                    out << t << "\tset_window_size\t" << sizes[p][0] << '\t' << sizes[p][1] << "\tSDL_SetWindowSize\n";
                }
                else
                    out << t << "\thand\t0\t0\tdock / undock now\n";
            }
            uint32_t w = 0, h = 0;
            drawableSize(&w, &h);
            dev.resize(w, h);
            uint32_t bw = 0, bh = 0;
            dev.backbufferSize(bw, bh);
            if (bw != lastW || bh != lastH)
            {
                out << t << "\tdrawable\t" << bw << '\t' << bh << "\toperation mode " << appletGetOperationMode() << '\n';
                out.flush();
                say("[window] t=%.2f back buffer %ux%u (mode %d)\n", t, bw, bh, appletGetOperationMode());
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
            // 0-30 s immediate, 30-45 s vsync, then immediate (the interval follows the mode)
            const bool vs = t >= 30.0 && t < 45.0;
            dev.present(vs ? rhi::Device::PresentMode::Vsync : rhi::Device::PresentMode::Immediate);
            const auto now = Clock::now();
            const double interval = ms(prev, now);
            prev = now;
            if (t > 31.0 && t < 45.0)
                vsync.push_back(interval);
            else if (t > 46.0)
                immediate.push_back(interval);
        }
        char row[256];
        std::snprintf(row, sizeof(row), "summary\tvsync_median_ms %.3f p95 %.3f frames %zu\timmediate_median_ms %.3f p95 %.3f frames %zu\n",
                      median(vsync), percentile(vsync, 0.95), vsync.size(), median(immediate), percentile(immediate, 0.95), immediate.size());
        out << row;
        say("[window] %s", row);
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
                    << std::dec << "\nloader_context " << loader << "\nused_memory_start " << usedMemory() << '\n';
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
            index.close();
            checks.close();
            say("[rhi-tests] conformance: %d images, %d failed checks, %.1f s\n", images, failures, ms(c0, Clock::now()) / 1000.0);

            // 2. threading
            std::ofstream bench(out + "/bench.tsv");
            if (!aborted)
                threading(*dev, bench);
            bench.close();

            // 3. window
            std::ofstream win(out + "/window.tsv");
            if (!aborted)
                window(*dev, win);
            win.close();

            rhi::GlStats st;
            rhi::openGLStats(*dev, st);
            std::ofstream summary(out + "/summary.txt");
            summary << (aborted ? "ABORTED" : "DONE") << "\nimages " << images << "\nfailed_checks " << failures << "\ngl_errors " << st.glErrors
                    << "\ndebug_messages " << st.debugMessages << "\nincomplete_framebuffers " << st.incompleteFramebuffers
                    << "\nowner_violations " << st.ownerViolations << "\nrejected " << st.rejected << "\nhazards " << st.hazards
                    << "\ncompile_failures " << st.compileFailures << "\noff_owner_created " << st.offOwnerCreated << "\nfences_waited "
                    << st.fencesWaited << "\ntextures_left " << st.textures << "\nused_memory_end " << usedMemory() << '\n';
            status = (failures || aborted) ? 1 : 0;
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

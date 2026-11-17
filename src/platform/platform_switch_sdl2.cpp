// SDL2 implementation of upstream's platform.h for the Nintendo Switch (libnx + switch-sdl2 2.28).
//
// Translated from upstream's SDL3 platform_sdl3.cpp at the pinned revision. Differences, all deliberate:
//   - No window is created here. src/main.c owns SDL_Init, the one SDL_WINDOW_OPENGL window and its GL context;
//     init() adopts that window (SDL_GL_GetCurrentWindow) and refuses to start without it. shutdown() releases what
//     init() made (Dear ImGui, the gamepad, audio) and leaves the window, the context and SDL_Quit to main.c.
//   - The desktop window placement, full screen, mouse capture and cursor hiding have no Switch counterpart: they
//     are accepted and remembered so the settings registry keeps working, and logged once.
//   - Vulkan entry points report unavailable (the Switch baseline is OpenGL through SDL2/EGL; no Vulkan driver).
//   - The OpenGL renderer (rhi::GlWindow, M05) gets main.c's context through SDL: its entry points, the swap, the
//     drawable size and the swap interval. Nothing here makes a context or surface.
//   - SDL2 game controllers: buttons by physical position (SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS=0), so
//     south = Cross on every controller (platform.h's contract).
//   - Audio: SDL2's callback device (SDL owns audren), pulled from upstream's producer ring by AudioFill on SDL's
//     audio thread. setFocus(false) pauses and resumes it across HOME / sleep.
// Thread rules are platform.h's: the main thread pumps events and owns the frame calls; queries read a snapshot.
#include "platform/platform.h"
#include "platform/platform_switch.h"
#include "platform/gl_loader_egl.h"

#include <SDL.h>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include <zlib.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

namespace platform
{
    namespace
    {
        SDL_Window *g_window = nullptr; // main.c's; never destroyed here
        bool g_quit = false;
        bool g_imgui = false;
        uint64_t g_nextFrameNs = 0;
        std::atomic<uint64_t> g_framePeriodNs{1000000000ull / 60u}; // setFrameRate

        std::array<std::atomic<bool>, KEY_COUNT> g_keys{};
        std::array<bool, KEY_COUNT> g_keysPrev{}; // main thread
        std::array<bool, KEY_COUNT> g_keysPressed{};
        std::array<bool, KEY_COUNT> g_edgesPending{};
        std::array<std::atomic<bool>, GAMEPAD_BUTTON_COUNT> g_padButtons{};
        std::array<std::atomic<float>, AXIS_COUNT> g_padAxes{};
        std::atomic<bool> g_padConnected{false};
        SDL_GameController *g_gamepad = nullptr;

        SDL_AudioDeviceID g_audio = 0;
        AudioFill g_audioFill = nullptr;
        std::atomic<bool> g_audioWanted{false}; // open and not paused for focus
        std::atomic<uint64_t> g_audioCallbacks{0}, g_audioFrames{0};

        bool g_fullscreen = false;
        bool g_hiddenWindow = false;
        bool g_focused = true;
        std::atomic<int64_t> g_snapshotNs{0};
        uint64_t g_leadNs = 0;
        FrameLead g_atLead = nullptr;
        bool g_leadRan = false;
        bool g_slept = false;
        std::atomic<uint32_t> g_rumbleWanted{0}; // low << 16 | high
        uint32_t g_rumbleSent = 0;
        uint64_t g_rumbleSentNs = 0;
        std::atomic<uint64_t> g_pumps{0};

        uint64_t nowNs()
        {
            return static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
        }

        void sleepNs(uint64_t ns)
        {
            // SDL2 has no SDL_DelayPrecise: sleep most of it, then yield to the deadline (libnx sleeps in ticks)
            const uint64_t deadline = nowNs() + ns;
            if (ns > 2000000ull)
                std::this_thread::sleep_for(std::chrono::nanoseconds(ns - 1000000ull));
            while (nowNs() < deadline)
                std::this_thread::yield();
        }

        void unsupportedOnce(const char *what)
        {
            static std::mutex mutex;
            static std::vector<const char *> said;
            std::lock_guard<std::mutex> lock(mutex);
            if (std::find(said.begin(), said.end(), what) != said.end())
                return;
            said.push_back(what);
            std::fprintf(stderr, "[platform] %s: not on the Switch (accepted, no effect)\n", what);
        }

        void openGamepad(int index)
        {
            if (g_gamepad || !SDL_IsGameController(index))
                return;
            g_gamepad = SDL_GameControllerOpen(index);
            if (!g_gamepad)
            {
                std::fprintf(stderr, "[platform] gamepad %d: %s\n", index, SDL_GetError());
                return;
            }
            char *mapping = SDL_GameControllerMapping(g_gamepad);
            std::fprintf(stderr, "[platform] gamepad: %s (instance %d) mapping %s\n", SDL_GameControllerName(g_gamepad),
                         SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(g_gamepad)), mapping ? mapping : "(none)");
            SDL_free(mapping);
        }

        void openFirstGamepad()
        {
            for (int i = 0; !g_gamepad && i < SDL_NumJoysticks(); ++i)
                openGamepad(i);
        }

        void snapshotGamepad()
        {
            static constexpr SDL_GameControllerButton kButtons[GAMEPAD_BUTTON_COUNT] = {
                SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_Y,
                SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_START, SDL_CONTROLLER_BUTTON_LEFTSTICK,
                SDL_CONTROLLER_BUTTON_RIGHTSTICK, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
                SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_DOWN, SDL_CONTROLLER_BUTTON_DPAD_LEFT,
                SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
            };
            static constexpr SDL_GameControllerAxis kAxes[AXIS_COUNT] = {
                SDL_CONTROLLER_AXIS_LEFTX, SDL_CONTROLLER_AXIS_LEFTY, SDL_CONTROLLER_AXIS_RIGHTX, SDL_CONTROLLER_AXIS_RIGHTY,
                SDL_CONTROLLER_AXIS_TRIGGERLEFT, SDL_CONTROLLER_AXIS_TRIGGERRIGHT,
            };
            // out of focus (HOME, sleep) or without a pad: neutral, so nothing stays held
            const bool live = g_gamepad && g_focused && SDL_GameControllerGetAttached(g_gamepad);
            g_padConnected.store(g_gamepad != nullptr, std::memory_order_relaxed);
            for (int b = 0; b < GAMEPAD_BUTTON_COUNT; ++b)
                g_padButtons[b].store(live && SDL_GameControllerGetButton(g_gamepad, kButtons[b]), std::memory_order_relaxed);
            for (int a = 0; a < AXIS_COUNT; ++a)
            {
                // SDL2 axes are -32768..32767 (triggers 0..32767); platform.h: sticks -1..1, triggers 0..1
                const float v = live ? SDL_GameControllerGetAxis(g_gamepad, kAxes[a]) / 32767.0f : 0.0f;
                g_padAxes[a].store(std::clamp(v, -1.0f, 1.0f), std::memory_order_relaxed);
            }
        }

        void SDLCALL audioCallback(void *, Uint8 *stream, int bytes)
        {
            const uint32_t frames = static_cast<uint32_t>(bytes) / 4u;
            AudioFill fill = g_audioFill;
            if (!frames)
                return;
            if (!fill || !g_audioWanted.load(std::memory_order_acquire))
            {
                std::memset(stream, 0, static_cast<size_t>(bytes));
                return;
            }
            fill(reinterpret_cast<int16_t *>(stream), frames); // interleaved S16 stereo, the format opened below
            g_audioCallbacks.fetch_add(1, std::memory_order_relaxed);
            g_audioFrames.fetch_add(frames, std::memory_order_relaxed);
        }

        uint32_t pngCrc32(uint32_t crc, const uint8_t *p, size_t n)
        {
            static const auto table = [] {
                std::array<uint32_t, 256> t{};
                for (uint32_t i = 0; i < 256; ++i)
                {
                    uint32_t c = i;
                    for (int k = 0; k < 8; ++k)
                        c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
                    t[i] = c;
                }
                return t;
            }();
            crc = ~crc;
            while (n--)
                crc = table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
            return ~crc;
        }

        void put32(std::vector<uint8_t> &v, uint32_t x)
        {
            v.push_back(static_cast<uint8_t>(x >> 24));
            v.push_back(static_cast<uint8_t>(x >> 16));
            v.push_back(static_cast<uint8_t>(x >> 8));
            v.push_back(static_cast<uint8_t>(x));
        }

        void chunk(std::vector<uint8_t> &out, const char *type, const std::vector<uint8_t> &data)
        {
            put32(out, static_cast<uint32_t>(data.size()));
            const size_t start = out.size();
            out.insert(out.end(), type, type + 4);
            out.insert(out.end(), data.begin(), data.end());
            put32(out, pngCrc32(0, out.data() + start, out.size() - start));
        }
    }

    void setStartPlacement(const WindowPlacement &) { unsupportedOnce("window placement"); }
    void onPlacementChanged(PlacementChanged) {}

    void setFullscreen(bool on)
    {
        // the Switch output is always the whole screen; remember the setting's value for the menu
        if (on != g_fullscreen)
            unsupportedOnce("full screen switch");
        g_fullscreen = on;
    }

    bool fullscreen() { return g_fullscreen; }

    bool init(const WindowDesc &desc)
    {
        if (g_window)
            return true;
        g_window = SDL_GL_GetCurrentWindow();
        if (!g_window || !SDL_GL_GetCurrentContext())
        {
            // main.c makes the window and context first: a second window would be a second EGL surface
            std::fprintf(stderr, "[platform] no current SDL GL window: main.c must create it before the runtime\n");
            g_window = nullptr;
            return false;
        }
        if ((SDL_WasInit(SDL_INIT_GAMECONTROLLER) & SDL_INIT_GAMECONTROLLER) == 0)
        {
            std::fprintf(stderr, "[platform] SDL game controller subsystem not initialised by main.c\n");
            g_window = nullptr;
            return false;
        }
        g_hiddenWindow = desc.hidden;
        g_quit = false;
        g_nextFrameNs = 0;
        SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0"); // positions: south = Cross (platform.h)
        openFirstGamepad();

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        ImGui::StyleColorsDark();
        g_imgui = ImGui_ImplSDL2_InitForOpenGL(g_window, SDL_GL_GetCurrentContext());
        // M05-09's second threading prototype: a loader context for the OpenGL RHI's free-threaded textures
        if (const char *e = std::getenv("PS2X_GL_WORKER_CONTEXT"); e && *e && *e != '0')
            gl_loader::create();
        if (!g_imgui)
            std::fprintf(stderr, "[platform] Dear ImGui SDL2 input backend failed\n");
        int w = 0, h = 0;
        SDL_GL_GetDrawableSize(g_window, &w, &h);
        std::fprintf(stderr, "[platform] SDL2 %d.%d.%d adopted main.c's window %dx%d (asked %dx%d%s); ImGui %s\n",
                     SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL, w, h, desc.width, desc.height,
                     desc.hidden ? ", hidden" : "", g_imgui ? "on" : "off");
        if (desc.vulkan)
            std::fprintf(stderr, "[platform] Vulkan window asked for: no Vulkan on the Switch baseline\n");
        return true;
    }

    void shutdown()
    {
        if (!g_window)
            return;
        audioClose();
        if (g_imgui)
        {
            ImGui_ImplSDL2_Shutdown();
            g_imgui = false;
        }
        if (ImGui::GetCurrentContext())
            ImGui::DestroyContext();
        if (g_gamepad)
        {
            SDL_GameControllerRumble(g_gamepad, 0, 0, 0);
            SDL_GameControllerClose(g_gamepad);
        }
        g_gamepad = nullptr;
        g_padConnected.store(false, std::memory_order_relaxed);
        gl_loader::destroy(); // the presenter's device (its only user) is gone by now
        g_window = nullptr; // main.c destroys it, after the core has shut down
        std::fprintf(stderr, "[platform] shut down (window, context and SDL left to main.c)\n");
    }

    bool ready() { return g_window != nullptr; }
    void *nativeWindow() { return nullptr; }
    bool windowHidden() { return g_hiddenWindow; }

    const char *const *vulkanInstanceExtensions(uint32_t *count)
    {
        if (count)
            *count = 0;
        unsupportedOnce("Vulkan");
        return nullptr;
    }

    bool vulkanCreateSurface(void *, uint64_t *)
    {
        unsupportedOnce("Vulkan");
        return false;
    }

    // the OpenGL renderer: main.c's one SDL GL context, current on the main thread (rhi::GlWindow)
    void *glGetProcAddress(const char *name) { return SDL_GL_GetProcAddress(name); }

    void glSwapWindow()
    {
        if (g_window)
            SDL_GL_SwapWindow(g_window);
    }

    void glDrawableSize(uint32_t *width, uint32_t *height)
    {
        int w = 0, h = 0;
        if (g_window)
            SDL_GL_GetDrawableSize(g_window, &w, &h);
        *width = static_cast<uint32_t>(std::max(w, 0));
        *height = static_cast<uint32_t>(std::max(h, 0));
    }

    bool glSetSwapInterval(int interval) { return SDL_GL_SetSwapInterval(interval) == 0; }
    bool glLoaderAvailable() { return gl_loader::available(); }
    bool glMakeLoaderCurrent(bool current) { return gl_loader::makeCurrent(current); }

    namespace
    {
        void noteEdge(int k, bool edge, bool frameStart)
        {
            if (!frameStart)
            {
                g_edgesPending[k] = g_edgesPending[k] || edge;
                return;
            }
            g_keysPressed[k] = edge || g_edgesPending[k];
            g_edgesPending[k] = false;
        }
    }

    void pumpEvents(bool frameStart)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (g_imgui)
                ImGui_ImplSDL2_ProcessEvent(&e);
            switch (e.type)
            {
            case SDL_QUIT:
                // switch-sdl2 sends SDL_QUIT when appletMainLoop() reports the applet's exit request (the player
                // closed the game from HOME; main.c holds the exit lock so it arrives here)
                if (!g_quit)
                    std::fprintf(stderr, "[platform] quit requested (applet exit request): stopping in order\n");
                g_quit = true;
                break;
            case SDL_CONTROLLERDEVICEADDED:
                openGamepad(e.cdevice.which);
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                if (g_gamepad && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(g_gamepad)) == e.cdevice.which)
                {
                    std::fprintf(stderr, "[platform] gamepad removed (instance %d): neutral until another opens\n",
                                 e.cdevice.which);
                    SDL_GameControllerClose(g_gamepad);
                    g_gamepad = nullptr;
                    openFirstGamepad();
                }
                break;
            default:
                break;
            }
        }
        // no keyboard or mouse on the Switch: keys stay up (the control channel presses keys through ps2control)
        for (int k = 0; k < KEY_COUNT; ++k)
        {
            noteEdge(k, false, frameStart);
            g_keysPrev[k] = false;
            g_keys[k].store(false, std::memory_order_relaxed);
        }
        snapshotGamepad();

        const uint64_t now = nowNs();
        const uint32_t rumble = g_focused ? g_rumbleWanted.load(std::memory_order_relaxed) : 0u;
        if (g_gamepad && (rumble != g_rumbleSent || (rumble && now - g_rumbleSentNs > 500000000ull)))
        {
            SDL_GameControllerRumble(g_gamepad, static_cast<Uint16>(rumble >> 16), static_cast<Uint16>(rumble & 0xFFFF),
                                     rumble ? 1000 : 0);
            g_rumbleSent = rumble;
            g_rumbleSentNs = now;
        }
        g_pumps.fetch_add(1, std::memory_order_relaxed);
        g_snapshotNs.store(static_cast<int64_t>(now), std::memory_order_release);
    }

    int64_t inputSnapshotNs() { return g_snapshotNs.load(std::memory_order_acquire); }
    bool quitRequested() { return g_quit; }

    void beginFrame()
    {
        if (!g_imgui)
            return;
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
    }

    void endUiFrame()
    {
        if (g_imgui)
            ImGui::Render();
    }

    void setFrameRate(double hz)
    {
        g_framePeriodNs.store(static_cast<uint64_t>(1e9 / std::clamp(hz, 24.0, 1000.0)), std::memory_order_relaxed);
    }

    double displayRefreshHz()
    {
        SDL_DisplayMode mode{};
        const int display = g_window ? SDL_GetWindowDisplayIndex(g_window) : 0;
        return SDL_GetCurrentDisplayMode(display < 0 ? 0 : display, &mode) == 0 && mode.refresh_rate > 1 ? mode.refresh_rate : 60.0;
    }

    void delayPrecise(uint64_t ns) { sleepNs(ns); }

    void waitNextFrame()
    {
        const uint64_t kPeriodNs = g_framePeriodNs.load(std::memory_order_relaxed);
        const uint64_t now = nowNs();
        g_nextFrameNs = g_nextFrameNs ? g_nextFrameNs + kPeriodNs : now + kPeriodNs;
        g_leadRan = false;
        g_slept = false;
        if (g_nextFrameNs < now)
        {
            g_nextFrameNs = now; // fell behind: don't try to catch up
            return;
        }
        g_slept = true;
        const uint64_t lead = std::min(g_leadNs, kPeriodNs);
        if (g_atLead && lead && g_nextFrameNs - lead > now)
        {
            sleepNs(g_nextFrameNs - lead - now);
            g_atLead();
            g_leadRan = true;
            const uint64_t after = nowNs();
            if (g_nextFrameNs > after)
                sleepNs(g_nextFrameNs - after);
            return;
        }
        sleepNs(g_nextFrameNs - now);
    }

    void setFrameLead(uint64_t leadNs, FrameLead atLead)
    {
        g_leadNs = leadNs;
        g_atLead = atLead;
    }

    bool frameLeadRan() { return g_leadRan; }
    bool frameSlept() { return g_slept; }

    void windowSize(int &width, int &height)
    {
        width = height = 0;
        if (g_window)
            SDL_GL_GetDrawableSize(g_window, &width, &height);
    }

    bool windowFocused() { return g_window && !g_hiddenWindow && g_focused; }

    bool keyDown(int key) { return key >= 0 && key < KEY_COUNT && g_keys[key].load(std::memory_order_relaxed); }
    bool keyPressed(int key) { return key >= 0 && key < KEY_COUNT && g_keysPressed[key]; }
    bool mouseDown(MouseButton) { return false; }
    bool mousePosition(float &x, float &y)
    {
        x = y = 0.0f;
        return false;
    }
    bool gamepadConnected() { return g_padConnected.load(std::memory_order_relaxed); }
    bool gamepadButton(GamepadButton button) { return g_padButtons[button].load(std::memory_order_relaxed); }
    float gamepadAxis(GamepadAxis axis) { return g_padAxes[axis].load(std::memory_order_relaxed); }

    const char *gamepadName()
    {
        const char *name = g_gamepad ? SDL_GameControllerName(g_gamepad) : nullptr;
        return name ? name : "";
    }

    void gamepadRumble(float low, float high)
    {
        auto level = [](float v) { return static_cast<uint32_t>(std::clamp(v, 0.0f, 1.0f) * 65535.0f + 0.5f); };
        g_rumbleWanted.store(level(low) << 16 | level(high), std::memory_order_relaxed);
    }

    void setMouseCaptured(bool on)
    {
        if (on)
            unsupportedOnce("mouse capture");
    }
    bool mouseCaptured() { return false; }
    void takeMouseMotion(float &dx, float &dy) { dx = dy = 0.0f; }

    const char *keyName(int key)
    {
        static thread_local char buffer[16];
        if ((key >= KEY_A && key <= KEY_Z) || (key >= KEY_ZERO && key <= KEY_NINE))
            return (buffer[0] = static_cast<char>(key), buffer[1] = 0, buffer);
        if (key >= KEY_F1 && key <= KEY_F12)
            return (std::snprintf(buffer, sizeof(buffer), "F%d", key - KEY_F1 + 1), buffer);
        switch (key)
        {
        case KEY_ESCAPE: return "Escape";
        case KEY_ENTER: return "Enter";
        case KEY_SPACE: return "Space";
        default: return "?";
        }
    }

    bool audioAvailable() { return (SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) != 0 && SDL_GetNumAudioDevices(0) != 0; }

    bool audioOpen(uint32_t rate, uint32_t bufferFrames, AudioFill fill)
    {
        if (g_audio || (SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0)
            return false;
        SDL_AudioSpec want{}, have{};
        want.freq = static_cast<int>(rate);
        want.format = AUDIO_S16SYS;
        want.channels = 2;
        want.samples = static_cast<Uint16>(std::clamp<uint32_t>(bufferFrames, 256u, 4096u));
        want.callback = audioCallback;
        g_audioFill = fill;
        // no allowed changes: SDL converts if audren wants another format, the callback always sees S16 stereo
        g_audio = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
        if (!g_audio)
        {
            std::fprintf(stderr, "[platform] audio device: %s\n", SDL_GetError());
            g_audioFill = nullptr;
            return false;
        }
        std::fprintf(stderr, "[platform] audio device %u: %d Hz, %u channels, %u frames (%s)\n", g_audio, have.freq,
                     have.channels, have.samples, SDL_GetCurrentAudioDriver());
        g_audioWanted.store(true, std::memory_order_release);
        SDL_PauseAudioDevice(g_audio, g_focused ? 0 : 1);
        return true;
    }

    void audioClose()
    {
        if (!g_audio)
            return;
        g_audioWanted.store(false, std::memory_order_release);
        SDL_CloseAudioDevice(g_audio); // waits for a callback in progress: AudioFill is not called after this
        std::fprintf(stderr, "[platform] audio closed after %llu callbacks, %llu frames\n",
                     static_cast<unsigned long long>(g_audioCallbacks.load()), static_cast<unsigned long long>(g_audioFrames.load()));
        g_audio = 0;
        g_audioFill = nullptr;
    }

    bool writePng(const char *path, const uint8_t *rgb, int width, int height)
    {
        // upstream's PNG writer: per-row filter by smallest signed sum, then zlib
        const size_t stride = static_cast<size_t>(width) * 3u;
        std::vector<uint8_t> raw(static_cast<size_t>(height) * (stride + 1u));
        std::vector<uint8_t> trial(stride);
        for (int y = 0; y < height; ++y)
        {
            const uint8_t *row = rgb + static_cast<size_t>(y) * stride;
            const uint8_t *above = y ? row - stride : nullptr;
            uint8_t *out = raw.data() + static_cast<size_t>(y) * (stride + 1u);
            uint64_t best = ~0ull;
            for (uint8_t filter = 0; filter < 5; ++filter)
            {
                if (filter == 3)
                    continue;
                uint64_t cost = 0;
                for (size_t i = 0; i < stride; ++i)
                {
                    const int a = i >= 3 ? row[i - 3] : 0, b = above ? above[i] : 0, c = (i >= 3 && above) ? above[i - 3] : 0;
                    int predicted = 0;
                    if (filter == 1)
                        predicted = a;
                    else if (filter == 2)
                        predicted = b;
                    else if (filter == 4)
                    {
                        const int p = a + b - c, pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
                        predicted = (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
                    }
                    trial[i] = static_cast<uint8_t>(row[i] - predicted);
                    cost += static_cast<uint64_t>(std::abs(static_cast<int8_t>(trial[i])));
                }
                if (cost < best)
                {
                    best = cost;
                    out[0] = filter;
                    std::memcpy(out + 1, trial.data(), stride);
                }
            }
        }
        uLongf zSize = compressBound(static_cast<uLong>(raw.size()));
        std::vector<uint8_t> z(zSize);
        if (compress2(z.data(), &zSize, raw.data(), static_cast<uLong>(raw.size()), 6) != Z_OK)
            return false;
        z.resize(zSize);
        std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
        std::vector<uint8_t> ihdr;
        put32(ihdr, static_cast<uint32_t>(width));
        put32(ihdr, static_cast<uint32_t>(height));
        ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});
        chunk(png, "IHDR", ihdr);
        chunk(png, "IDAT", z);
        chunk(png, "IEND", {});
        FILE *f = std::fopen(path, "wb");
        if (!f)
            return false;
        const bool ok = std::fwrite(png.data(), 1, png.size(), f) == png.size();
        return std::fclose(f) == 0 && ok;
    }
}

namespace platform_switch
{
    void setFocus(bool focused)
    {
        using namespace platform;
        if (focused == g_focused)
            return;
        g_focused = focused;
        if (g_audio)
        {
            // HOME / sleep: the callback device stops pulling; the producer ring drops what it cannot hold (upstream
            // counts it) and nothing stale is queued on the SDL side (a callback device has no queue)
            SDL_PauseAudioDevice(g_audio, focused ? 0 : 1);
        }
        g_nextFrameNs = 0; // no catch-up burst after a suspension
        std::fprintf(stderr, "[platform] focus %s: audio %s, pad %s\n", focused ? "back" : "lost",
                     focused ? "resumed" : "paused", focused ? "live" : "neutral");
    }

    Stats stats()
    {
        using namespace platform;
        Stats s;
        s.pumps = g_pumps.load(std::memory_order_relaxed);
        s.audioCallbacks = g_audioCallbacks.load(std::memory_order_relaxed);
        s.audioFrames = g_audioFrames.load(std::memory_order_relaxed);
        s.gamepad = g_padConnected.load(std::memory_order_relaxed);
        return s;
    }
}

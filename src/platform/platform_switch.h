// Port-only additions to upstream's platform.h for the Switch SDL2 adapter (platform_switch_sdl2.cpp).
#pragma once

#include <cstdint>

namespace platform_switch
{
    // Main thread. HOME / sleep / focus changes from src/main.c's applet monitoring: out of focus the audio device is
    // paused and the pad reads neutral; back in focus both resume and frame pacing restarts without a catch-up burst.
    void setFocus(bool focused);

    struct Stats
    {
        uint64_t pumps = 0;          // pumpEvents calls
        uint64_t audioCallbacks = 0; // SDL audio callbacks that pulled from the runtime
        uint64_t audioFrames = 0;    // stereo frames pulled
        bool gamepad = false;
    };
    Stats stats();
}

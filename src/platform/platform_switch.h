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
        // M08-14: the L3 + R3 chord filter (src/devmenu/chord.h) every L3 / R3 press goes through
        uint64_t chords = 0;          // L3 + R3 chords (none of them reached the game)
        uint64_t stickPresses = 0;    // L3 / R3 presses (each withheld for the window, or part of a chord)
        uint64_t stickDelivered = 0;  // ... handed on to the game held (window ran out) or as a tap's pulse
        uint64_t maxWithheldNs = 0;   // the longest a delivered press waited: the added input latency
        uint64_t chordWindowNs = 0;
        uint64_t touches = 0;         // touch screen presses, in Dear ImGui's coordinates (M08-17)
    };
    Stats stats();

    // M08: true once for each L3 + R3 chord since the last call (main thread; src/devmenu/dev_menu.cpp toggles the
    // developer menu on it). The game never sees the chord's L3 or R3.
    bool takeMenuChord();
    // The raw L3 / R3 as the controller reports them, before the chord filter (main thread; diagnostics).
    bool rawStickButton(int which); // 0 = L3, 1 = R3
}

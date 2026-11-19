// The L3 + R3 chord that opens the developer menu on the Switch (M08-14), kept from the game.
//
// Every L3 or R3 press is withheld from the game for a short window. If the other stick button goes down inside that
// window the two are a chord: the menu toggles and neither button reaches the game until it is let go. Otherwise the
// press is handed on when the window ends: a button still held is reported down from then on, one already let go (a
// tap) is reported down for a short pulse, so the game sees every press, late by at most the window. A press while the
// other button is already handed on is not a chord (both edges must fall inside one window).
//
// Pure logic, no SDL: platform_switch_sdl2.cpp feeds it the raw buttons once per event pump; tests/core/chord_tests.cpp
// checks it on the host.
#pragma once

#include <cstdint>

namespace devmenu
{
    class ChordFilter
    {
    public:
        enum Button : int
        {
            L3,
            R3,
            kButtons
        };

        // What happened to a button at an update (for the log; at most one per button per update).
        enum class Event : uint8_t
        {
            None,
            Withheld,       // pressed: held back for the window
            Delivered,      // the window ended with it held: the game sees it down from now on
            TapDelivered,   // let go inside the window: the game sees a pulse
            Released,       // a delivered button or pulse went up for the game
            Chord,          // both inside one window: the menu toggles, this button suppressed
            SuppressedUp,   // a suppressed button was let go (the game never saw it)
        };

        struct Result
        {
            bool guest[kButtons] = {}; // what the game may see
            bool chord = false;        // the menu toggles (once per chord)
            Event event[kButtons] = {};
            uint64_t heldNs[kButtons] = {}; // Delivered / TapDelivered / Chord: how long the press was withheld
        };

        // windowNs: how long a press waits for the other button; pulseNs and pulseUpdates: the shortest a tap is shown
        // to the game (both must pass, so a slow frame still shows it at one update at least twice)
        explicit ChordFilter(uint64_t windowNs = 100000000ull, uint64_t pulseNs = 60000000ull, uint32_t pulseUpdates = 2)
            : m_windowNs(windowNs), m_pulseNs(pulseNs), m_pulseUpdates(pulseUpdates)
        {
        }

        // Once per event pump with the raw buttons and a monotonic time.
        Result update(bool rawL3, bool rawR3, uint64_t nowNs);
        // Everything up and forgotten (focus lost, controller gone): nothing withheld is handed on later.
        void reset();

        uint64_t windowNs() const { return m_windowNs; }

    private:
        enum class State : uint8_t
        {
            Up,
            Withheld,
            Delivered,
            Pulse,
            Suppressed,
        };

        struct Slot
        {
            State state = State::Up;
            uint64_t since = 0;       // Withheld: the press; Pulse: its start
            uint32_t pulseSeen = 0;   // Pulse: updates shown so far
        };

        uint64_t m_windowNs, m_pulseNs;
        uint32_t m_pulseUpdates;
        Slot m_slots[kButtons];
    };

    const char *eventName(ChordFilter::Event e);
}

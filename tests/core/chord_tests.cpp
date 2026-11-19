// src/devmenu/chord.cpp: the L3 + R3 chord withheld from the game (M08-14).
#include "devmenu/chord.h"

#include <cstdio>
#include <string>

namespace
{
    using devmenu::ChordFilter;
    using Event = ChordFilter::Event;

    int g_failures = 0;
    constexpr uint64_t kMs = 1000000ull;

    void check(bool ok, const std::string &what)
    {
        std::printf("%s chord %s\n", ok ? "PASS" : "FAIL", what.c_str());
        if (!ok)
            ++g_failures;
    }

    // A pump every `stepMs` from 0 to `endMs` with the raw buttons from `l3` / `r3` (ms -> down); counts what the game
    // saw and the chords.
    struct Run
    {
        int chords = 0;
        bool guestL3Ever = false, guestR3Ever = false;
        int guestL3Updates = 0, guestR3Updates = 0;
        uint64_t firstGuestL3Ms = ~0ull;
    };

    template <class L, class R>
    Run run(ChordFilter &f, uint64_t endMs, uint64_t stepMs, L l3, R r3)
    {
        Run out;
        for (uint64_t t = 0; t <= endMs; t += stepMs)
        {
            const ChordFilter::Result r = f.update(l3(t), r3(t), t * kMs);
            out.chords += r.chord ? 1 : 0;
            if (r.guest[ChordFilter::L3])
            {
                out.guestL3Ever = true;
                ++out.guestL3Updates;
                if (out.firstGuestL3Ms == ~0ull)
                    out.firstGuestL3Ms = t;
            }
            if (r.guest[ChordFilter::R3])
            {
                out.guestR3Ever = true;
                ++out.guestR3Updates;
            }
        }
        return out;
    }

    auto between(uint64_t from, uint64_t to)
    {
        return [from, to](uint64_t t) { return t >= from && t < to; };
    }
    auto never() { return [](uint64_t) { return false; }; }
}

int runChordTests()
{
    g_failures = 0;
    for (uint64_t step : {5ull, 16ull, 43ull, 84ull})
    {
        const std::string at = " (pump every " + std::to_string(step) + " ms)";
        {
            ChordFilter f;
            const Run r = run(f, 2000, step, between(100, 600), between(100 + step / 2, 600));
            check(r.chords == 1 && !r.guestL3Ever && !r.guestR3Ever, "simultaneous L3 + R3: one chord, nothing to the game" + at);
        }
        {
            ChordFilter f;
            // the second button 40 ms after the first: within the 100 ms window
            const Run r = run(f, 2000, step, between(200, 900), between(240, 900));
            check(r.chords == 1 && !r.guestL3Ever && !r.guestR3Ever, "R3 40 ms after L3: chord, nothing to the game" + at);
        }
        {
            ChordFilter f;
            // L3 held alone: handed on after the window, for as long as it is held
            const Run r = run(f, 2000, step, between(100, 1100), never());
            const uint64_t seen = (100 + step - 1) / step * step; // the first pump that sees the press
            check(r.chords == 0 && r.guestL3Ever && r.firstGuestL3Ms >= seen + 100 && r.firstGuestL3Ms < seen + 100 + step &&
                      !r.guestR3Ever,
                  "L3 held alone: delivered once the window ends (pressed at " + std::to_string(seen) + " ms, to the game at " +
                      std::to_string(r.firstGuestL3Ms) + " ms)" + at);
        }
        {
            ChordFilter f;
            // a tap one pump long (a shorter one is between two snapshots, as before the filter): shown as a pulse of at
            // least two pumps and 60 ms
            const Run r = run(f, 2000, step, between(100, 110 + step), never());
            check(r.chords == 0 && r.guestL3Updates >= 2 && r.guestL3Updates * step >= 60,
                  "L3 tap: delivered as a pulse of " + std::to_string(r.guestL3Updates) + " pumps" + at);
        }
        {
            ChordFilter f;
            // R1-then-L3 (the game's free aim) only involves L3: delivered
            const Run r = run(f, 3000, step, between(500, 2500), never());
            check(r.guestL3Ever && r.chords == 0, "L3 under a held R1 (free aim): delivered" + at);
        }
        {
            ChordFilter f;
            // L3 already handed on, then R3: not a chord, both reach the game
            const Run r = run(f, 3000, step, between(100, 2000), between(1000, 1500));
            check(r.chords == 0 && r.guestL3Ever && r.guestR3Ever, "R3 long after L3: no chord, both delivered" + at);
        }
        {
            ChordFilter f;
            // a chord held long, then let go one at a time: still nothing to the game
            const Run r = run(f, 4000, step, between(100, 3000), between(110, 2000));
            check(r.chords == 1 && !r.guestL3Ever && !r.guestR3Ever, "chord released one at a time: nothing leaks" + at);
        }
        {
            ChordFilter f;
            // open, then close: two separate chords
            const Run r = run(f, 4000, step, [](uint64_t t) { return (t >= 100 && t < 400) || (t >= 2000 && t < 2300); },
                              [](uint64_t t) { return (t >= 110 && t < 400) || (t >= 2010 && t < 2300); });
            check(r.chords == 2 && !r.guestL3Ever && !r.guestR3Ever, "open and close: two chords, nothing leaks" + at);
        }
    }
    {
        ChordFilter f;
        f.update(true, false, 0);
        f.reset();
        const ChordFilter::Result r = f.update(false, false, 500 * kMs);
        check(!r.guest[ChordFilter::L3] && r.event[ChordFilter::L3] == Event::None, "reset: a withheld press is dropped");
    }
    {
        ChordFilter f;
        ChordFilter::Result r = f.update(true, false, 0);
        check(r.event[ChordFilter::L3] == Event::Withheld && r.pressed[ChordFilter::L3] && !r.guest[ChordFilter::L3],
              "event: withheld, counted as a press");
        r = f.update(true, false, 100 * kMs);
        check(r.event[ChordFilter::L3] == Event::Delivered && r.heldNs[ChordFilter::L3] == 100 * kMs && r.guest[ChordFilter::L3],
              "event: delivered after exactly the window, held time reported");
        r = f.update(false, false, 150 * kMs);
        check(r.event[ChordFilter::L3] == Event::Released && !r.guest[ChordFilter::L3], "event: released");
    }
    std::printf("%s chord: %d failure(s)\n", g_failures ? "FAIL" : "PASS", g_failures);
    return g_failures;
}

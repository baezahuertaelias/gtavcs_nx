// See chord.h.
#include "devmenu/chord.h"

namespace devmenu
{
    ChordFilter::Result ChordFilter::update(bool rawL3, bool rawR3, uint64_t nowNs)
    {
        Result r;
        const bool raw[kButtons] = {rawL3, rawR3};

        // new presses wait for the other button
        for (int b = 0; b < kButtons; ++b)
            if (m_slots[b].state == State::Up && raw[b])
            {
                m_slots[b] = {State::Withheld, nowNs, 0};
                r.event[b] = Event::Withheld;
                r.pressed[b] = true;
            }

        // both waiting and both down: a chord. Checked before a window can run out, so a slow pump (the second press
        // landed somewhere in a long frame) errs towards the chord: nothing reaches the game either way.
        if (m_slots[L3].state == State::Withheld && m_slots[R3].state == State::Withheld && raw[L3] && raw[R3])
        {
            r.chord = true;
            for (int b = 0; b < kButtons; ++b)
            {
                r.heldNs[b] = nowNs - m_slots[b].since;
                m_slots[b].state = State::Suppressed;
                r.event[b] = Event::Chord;
            }
        }

        for (int b = 0; b < kButtons; ++b)
        {
            Slot &s = m_slots[b];
            switch (s.state)
            {
            case State::Withheld:
                if (!raw[b])
                {
                    r.heldNs[b] = nowNs - s.since;
                    s = {State::Pulse, nowNs, 1};
                    r.event[b] = Event::TapDelivered;
                }
                else if (nowNs - s.since >= m_windowNs)
                {
                    r.heldNs[b] = nowNs - s.since;
                    s.state = State::Delivered;
                    r.event[b] = Event::Delivered;
                }
                break;
            case State::Delivered:
                if (!raw[b])
                {
                    s.state = State::Up;
                    r.event[b] = Event::Released;
                }
                break;
            case State::Pulse:
                if (r.event[b] == Event::TapDelivered)
                    break; // began at this update
                if (s.pulseSeen >= m_pulseUpdates && nowNs - s.since >= m_pulseNs)
                {
                    s.state = State::Up; // a press during the pulse is taken up at the next update
                    r.event[b] = Event::Released;
                }
                else
                    ++s.pulseSeen;
                break;
            case State::Suppressed:
                if (!raw[b])
                {
                    s.state = State::Up;
                    r.event[b] = Event::SuppressedUp;
                }
                break;
            case State::Up:
                break;
            }
            r.guest[b] = s.state == State::Delivered || s.state == State::Pulse;
        }
        return r;
    }

    void ChordFilter::reset()
    {
        for (Slot &s : m_slots)
            s = {};
    }

    const char *eventName(ChordFilter::Event e)
    {
        switch (e)
        {
        case ChordFilter::Event::None: return "none";
        case ChordFilter::Event::Withheld: return "withheld";
        case ChordFilter::Event::Delivered: return "delivered";
        case ChordFilter::Event::TapDelivered: return "tap-delivered";
        case ChordFilter::Event::Released: return "released";
        case ChordFilter::Event::Chord: return "chord";
        case ChordFilter::Event::SuppressedUp: return "suppressed-up";
        }
        return "?";
    }
}

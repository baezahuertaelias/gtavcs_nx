// M04-12: which of M02's indirect-fallback sites actually run on the Switch, and whether every target they reach is
// in the generated function table.
//
// M02's generation reported 1,196 indirect-control-flow sites (docs/milestones/M02/generation_metadata.json): a
// jr / jalr whose targets the generator could not list, emitted as a call to PS2Runtime::dispatchGuestBranch with the
// computed target. Generated code calls the out-of-line PS2Runtime::dispatchGuestBranchSlow from its own objects, so
// the game link wraps that symbol (-Wl,--wrap=...); with PS2X_FAST_DISPATCH=0 every guest branch goes through it.
// With VCS_NX_BRANCH_TRACE=1 this records, per indirect site, how often it ran, how many distinct targets it reached
// and how many of those were missing from the function table; gtavcs_branch_trace_report() prints them at shutdown.
// Without the variable the wrapper only forwards (one cached flag test).
#include "ps2_runtime.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>
#include <set>

extern "C" bool __real__ZN10PS2Runtime23dispatchGuestBranchSlowEPhP12R5900ContextjjjNS_15GuestBranchKindEPKc(
    PS2Runtime *self, uint8_t *rdram, R5900Context *ctx, uint32_t targetPc, uint32_t sourcePc, uint32_t fallthroughPc,
    PS2Runtime::GuestBranchKind kind, const char *debugName);

namespace
{
    struct Site
    {
        bool call = false;
        uint64_t count = 0, missing = 0;
        std::set<uint32_t> targets; // at most kMaxTargets kept; the size is the distinct count up to that bound
    };
    constexpr size_t kMaxTargets = 256;

    bool enabled()
    {
        static const bool on = [] {
            const char *v = std::getenv("VCS_NX_BRANCH_TRACE");
            return v && v[0] == '1';
        }();
        return on;
    }

    std::mutex g_mutex;
    std::map<uint32_t, Site> g_sites;
}

extern "C" bool __wrap__ZN10PS2Runtime23dispatchGuestBranchSlowEPhP12R5900ContextjjjNS_15GuestBranchKindEPKc(
    PS2Runtime *self, uint8_t *rdram, R5900Context *ctx, uint32_t targetPc, uint32_t sourcePc, uint32_t fallthroughPc,
    PS2Runtime::GuestBranchKind kind, const char *debugName)
{
    if (enabled() && (kind == PS2Runtime::GuestBranchKind::IndirectJump ||
                      kind == PS2Runtime::GuestBranchKind::IndirectCall))
    {
        const bool known = self->hasFunction(targetPc);
        std::lock_guard<std::mutex> lock(g_mutex);
        Site &site = g_sites[sourcePc];
        site.call = kind == PS2Runtime::GuestBranchKind::IndirectCall;
        ++site.count;
        if (!known)
            ++site.missing;
        if (site.targets.size() < kMaxTargets)
            site.targets.insert(targetPc);
    }
    return __real__ZN10PS2Runtime23dispatchGuestBranchSlowEPhP12R5900ContextjjjNS_15GuestBranchKindEPKc(
        self, rdram, ctx, targetPc, sourcePc, fallthroughPc, kind, debugName);
}

extern "C" void gtavcs_branch_trace_report(void)
{
    if (!enabled())
        return;
    std::lock_guard<std::mutex> lock(g_mutex);
    uint64_t dispatches = 0, missing = 0;
    for (const auto &entry : g_sites)
    {
        dispatches += entry.second.count;
        missing += entry.second.missing;
    }
    std::fprintf(stderr, "[branch-trace] indirect sites=%zu dispatches=%llu missing=%llu\n", g_sites.size(),
                 static_cast<unsigned long long>(dispatches), static_cast<unsigned long long>(missing));
    for (const auto &[source, site] : g_sites)
        std::fprintf(stderr, "[branch-trace] site=0x%06x kind=%s count=%llu targets=%zu missing=%llu\n", source,
                     site.call ? "call" : "jump", static_cast<unsigned long long>(site.count), site.targets.size(),
                     static_cast<unsigned long long>(site.missing));
    std::fflush(stderr);
}

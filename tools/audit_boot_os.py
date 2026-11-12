"""Audit every desktop_os / thread_time portability candidate against the Switch game build (M04-07).

    python tools/audit_boot_os.py <compile_commands.json> <out.json>

The candidates are M00's inventory (docs/discovery/portability_candidates.json, lines of the pinned upstream). A
candidate is on the boot path when its file is compiled into the game NRO (compile_commands.json of
tools/build_game.py; headers by the list below). Each compiled candidate gets the ruling for its token, and every
std::thread site its owner and how it ends. Writes metadata only.
"""
import argparse
import collections
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]

# Headers compiled into the game (included by compiled sources; checked by hand against their includers).
COMPILED_HEADERS = {
    "third_party/PS2Recomp/ps2xRuntime/include/runtime/ps2_host_std.h",
    "third_party/PS2Recomp/ps2xRuntime/include/ThreadNaming.h",
    "third_party/PS2Recomp/ps2xRuntime/include/runtime/ee_scheduler.h",
    "third_party/PS2Recomp/ps2xRuntime/include/runtime/ps2_host_profile.h",
    "third_party/PS2Recomp/ps2xRuntime/src/lib/Kernel/Syscalls/Helpers/Runtime.h",
    "third_party/PS2Recomp/ps2xRuntime/src/lib/Kernel/Syscalls/Common.h",
    "third_party/PS2Recomp/ps2xRuntime/src/lib/gs/gs_stats.h",
    "third_party/PS2Recomp/ps2xRuntime/include/ps2_log.h",
    "src/platform/platform.h",
    "src/rhi/rhi.h",
}
# Not compiled: debug/win_symbols.h is included by crash.cpp / profiler.cpp under _WIN32 only.

TOKEN_RULES = {
    "steady_clock": "std::chrono::steady_clock: newlib clock_gettime(CLOCK_MONOTONIC) on the libnx system tick; "
                    "portable, monotonic (pacing, timeouts, statistics)",
    "thread_local": "TLS: devkitA64/libnx thread pointer (M02-09 measured TLS on hardware)",
    "condition_variable": "std::condition_variable on libnx condvars; waits are bounded or woken at shutdown "
                          "(see the std::thread sites)",
    "fesetround": "FPCR rounding mode per thread on AArch64 (FE_TOWARDZERO exists); VU rounding verified in M02-07",
    "std::thread": "std::thread on libnx pthread; default-size threads get 8 MiB (src/switch/thread_stack.c)",
}
DESKTOP_TOKENS = {"_WIN32", "__linux__", "windows.h", "mmap", "VirtualAlloc", "XDG_DATA_HOME", "pthread_setname_np",
                  "pthread_self"}

# Every std::thread construction compiled into the game, by file: who owns it and how it ends on the Switch.
THREAD_SITES = {
    "third_party/PS2Recomp/ps2xRuntime/src/lib/ps2_runtime.cpp":
        "game thread: created by runBegin, joined by runEnd (overlay 0007); screenshot PNG writer: detached, only on "
        "a screenshot request (none on the Switch boot path: no keyboard, schedule sends none)",
    "third_party/PS2Recomp/ps2xRuntime/src/lib/ps2_render_worker.cpp":
        "render worker and GS threads: ps2_render_worker::shutdown() in runEnd joins them",
    "third_party/PS2Recomp/ps2xRuntime/src/lib/gs/gs_hw.cpp":
        "pipeline makers: joined by the HW renderer's destructor (shutdownGpuRenderer in runEnd)",
    "third_party/PS2Recomp/ps2xRuntime/src/lib/ps2_control.cpp":
        "socket control server: never started on the Switch (PS2X_CONTROL / PS2X_HIDDEN refused by nxrun.cfg; "
        "commands run in process through ps2control::submit)",
    "src/game/vcs_stream.cpp":
        "stream reader: was detached; overlay 0007 keeps it and core teardown joins it (stopStreamReader) before the "
        "folder disc is unmapped",
    "src/debug/profiler.cpp": "sampling profiler: Windows/Linux only (#else: no-op), and not started by the core",
    "src/debug/watchdog.cpp": "watchdog: detached thread, not started by the core (progress lines replace it)",
    "src/game/vcs_movie.cpp": "movie decoder: FFmpeg is not built (VCS_MOVIES=0), no thread",
}

NOT_COMPILED_REASON = {
    "src/rhi/vulkan/": "Vulkan backend: not built (no Vulkan on the Switch baseline)",
    "src/rhi/d3d11/": "D3D11 backend: Windows only, not built",
    "src/platform/platform_sdl3.cpp": "SDL3 platform: replaced by src/platform/platform_switch_sdl2.cpp",
    "third_party/PS2Recomp/ps2xRecomp/": "host recompiler: a host tool, never in the target",
    "third_party/PS2Recomp/ps2xRuntime/src/main.cpp": "the runtime's own runner main: not built",
    "third_party/PS2Recomp/ps2xRuntime/src/lib/gs/gs_gpu_parallel.cpp": "paraLLEl-GS: not built (gs_gpu_none.cpp)",
    "third_party/PS2Recomp/ps2xRuntime/src/lib/ps2_write_watch.cpp": None,  # compiled: listed for the check below
    "src/disc/iso_image.cpp": "desktop ISO reader: not built (the folder disc replaces it)",
    "src/debug/win_symbols.h": "Windows symbol helper, included under _WIN32 only",
    "third_party/PS2Recomp/ps2xIOP/tests/": "IOP unit tests: not in the game",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("compile_commands", type=Path)
    parser.add_argument("out", type=Path)
    args = parser.parse_args()
    candidates = json.loads((ROOT / "docs/discovery/portability_candidates.json").read_text())
    candidates = [c for c in candidates if c["category"] in ("desktop_os", "thread_time")]
    compiled = set()
    for entry in json.loads(args.compile_commands.read_text()):
        path = entry["file"].replace("\\", "/")
        match = re.search(r"/build/m04/game-source-[0-9a-f]+/(.*)$", path)
        if match:
            compiled.add(match.group(1))
        elif "/work/src/" in path:
            compiled.add("port:" + path.split("/work/", 1)[1])
    rows, unresolved = [], []
    for c in candidates:
        f = c["file"]
        on_path = f in compiled or f in COMPILED_HEADERS
        row = {"file": f, "line": c["line"], "category": c["category"], "tokens": c["tokens"], "compiled": on_path}
        if not on_path:
            reason = next((r for prefix, r in NOT_COMPILED_REASON.items() if f.startswith(prefix) and r), None)
            row["ruling"] = reason or "not compiled into the game NRO"
        else:
            rulings = []
            for t in c["tokens"]:
                if t in DESKTOP_TOKENS:
                    rulings.append(f"{t}: desktop branch; the Switch build compiles the other branch of this "
                                   "conditional (built with __SWITCH__, neither _WIN32 nor __linux__)")
                elif t in TOKEN_RULES:
                    rulings.append(f"{t}: " + TOKEN_RULES[t])
                else:
                    unresolved.append(row)
            source = (ROOT / "upstream" / f).read_text(encoding="utf-8", errors="replace").splitlines()
            text = source[c["line"] - 1] if 0 < c["line"] <= len(source) else ""
            if "std::thread" in c["tokens"] and ("std::thread::id" in text or "get_id()" in text):
                rulings.append("thread identity only (std::thread::id): no thread is created here")
            elif "std::thread" in c["tokens"]:
                site = THREAD_SITES.get(f)
                if not site:
                    unresolved.append(row)
                rulings.append("lifetime: " + (site or "UNREVIEWED"))
            row["ruling"] = "; ".join(rulings)
        rows.append(row)
    by_file = collections.Counter((r["file"], r["compiled"]) for r in rows)
    summary = {
        "candidates": len(rows),
        "compiled": sum(r["compiled"] for r in rows),
        "not_compiled": sum(not r["compiled"] for r in rows),
        "compiled_files": sorted({r["file"] for r in rows if r["compiled"]}),
        "unresolved": len(unresolved),
        "std_thread_sites_compiled": sum(1 for r in rows if r["compiled"] and "std::thread" in r["tokens"]),
    }
    args.out.write_text(json.dumps({"summary": summary, "candidates": rows}, indent=1) + "\n")
    print(json.dumps(summary, indent=2))
    for r in unresolved:
        print("UNRESOLVED", r)
    return 0 if not unresolved else 1


if __name__ == "__main__":
    sys.exit(main())

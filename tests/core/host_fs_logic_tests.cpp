// The Switch branch of runtime/ps2_host_fs.h compiled on the host (-D__SWITCH__ for this file only): the aside-and-
// restore sequence of replaceFile and the walk of removeTree, under the sanitizers. Hardware behaviour is covered by
// tests/core/host_fs_tests.cpp in the runtime NRO.
#include "runtime/ps2_host_fs.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

namespace
{
    namespace fs = std::filesystem;
    int g_failures = 0;

    void check(bool ok, const char *what)
    {
        std::printf("%s host-fs-logic %s\n", ok ? "PASS" : "FAIL", what);
        if (!ok)
            ++g_failures;
    }

    void put(const fs::path &p, const std::string &s) { std::ofstream(p, std::ios::binary) << s; }
    std::string get(const fs::path &p)
    {
        std::ifstream in(p, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }
}

int runHostFsLogicTests()
{
#if !defined(__SWITCH__)
    std::puts("FAIL host-fs-logic built without __SWITCH__: the Switch branch is not under test");
    return 1;
#else
    std::error_code ec;
    const fs::path base = fs::temp_directory_path() / "host-fs-logic";
    fs::remove_all(base, ec);
    fs::create_directories(base / "sub", ec);
    const fs::path target = base / "f", tmp = base / "f.tmp", aside = base / "f.old";

    put(tmp, "1");
    ps2x::replaceFile(tmp, target, ec);
    check(!ec && get(target) == "1" && !fs::exists(tmp), "new target");
    put(aside, "stale");
    put(tmp, "2");
    ps2x::replaceFile(tmp, target, ec);
    check(!ec && get(target) == "2" && !fs::exists(aside) && !fs::exists(tmp), "existing target, stale aside file removed");
    // a source that cannot be renamed (missing): the target goes aside and comes back unchanged
    ps2x::replaceFile(base / "nope", target, ec);
    check(ec && get(target) == "2" && !fs::exists(aside), "failed rename restores the target");

    put(base / "sub" / "a", "a");
    fs::create_directories(base / "sub" / "x" / "y", ec);
    put(base / "sub" / "x" / "y" / "b", "b");
    ps2x::removeTree(base / "sub", ec);
    check(!ec && !fs::exists(base / "sub"), "nested tree removed");
    ps2x::removeTree(base / "sub", ec);
    check(!ec, "missing tree is no error");
    ps2x::removeTree(target, ec);
    check(!ec && !fs::exists(target), "a file given to removeTree is removed");
    fs::remove_all(base, ec);
    std::printf("[host-fs-logic] failures=%d\n", g_failures);
    return g_failures;
#endif
}

// ps2x::replaceFile / removeTree (runtime/ps2_host_fs.h, overlay 0007) and the memory card operations that use them:
// settings and card backups replace a file that exists, card renames move onto an existing name, card format and
// unformat clear a folder holding files. On the Switch the std::filesystem forms fail (rename onto a file: EEXIST,
// remove_all: EIO; docs/milestones/M03/HARDWARE_RESULTS.md), so this suite runs in the public runtime NRO there.
#include "ps2_runtime.h"
#include "ps2_stubs.h"
#include "runtime/ps2_host_fs.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    int g_failures = 0;

    void check(bool ok, const char *what)
    {
        std::printf("%s host-fs %s\n", ok ? "PASS" : "FAIL", what);
        if (!ok)
            ++g_failures;
    }

    void writeText(const fs::path &path, const std::string &text)
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << text;
    }

    std::string readText(const fs::path &path)
    {
        std::ifstream in(path, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }

    void setReg(R5900Context &ctx, int reg, uint32_t value) { ctx.r[reg] = _mm_set_epi64x(0, static_cast<int64_t>(value)); }

    int32_t syncMc(std::vector<uint8_t> &rdram)
    {
        constexpr uint32_t kCmd = 0x3C00, kResult = 0x3C04;
        R5900Context ctx{};
        setReg(ctx, 4, 0);
        setReg(ctx, 5, kCmd);
        setReg(ctx, 6, kResult);
        ps2_stubs::sceMcSync(rdram.data(), &ctx, nullptr);
        int32_t result = 0;
        std::memcpy(&result, rdram.data() + kResult, sizeof(result));
        return result;
    }

    size_t entries(const fs::path &dir)
    {
        std::error_code ec;
        size_t n = 0;
        for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
            ++n;
        return n;
    }
}

int runHostFsTests()
{
    g_failures = 0;
    std::error_code ec;
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path base = fs::temp_directory_path(ec) / ("host-fs-" + std::to_string(stamp));
    fs::create_directories(base, ec);
    check(!ec && fs::is_directory(base), "work folder created");

    // replaceFile: onto nothing, then onto an existing file twice (a second settings save)
    const fs::path target = base / "vcs.ini", tmp = base / "vcs.ini.tmp";
    writeText(tmp, "first\n");
    ps2x::replaceFile(tmp, target, ec);
    check(!ec && readText(target) == "first\n" && !fs::exists(tmp), "replaceFile onto a missing target");
    for (const char *text : {"second\n", "third\n"})
    {
        writeText(tmp, text);
        ps2x::replaceFile(tmp, target, ec);
        fs::path aside = target;
        aside += ".old";
        check(!ec && readText(target) == text && !fs::exists(tmp) && !fs::exists(aside),
              "replaceFile onto an existing target, nothing left aside");
    }
    ps2x::replaceFile(base / "missing.tmp", target, ec);
    check(ec && readText(target) == "third\n", "replaceFile from a missing source fails and keeps the target");

    // removeTree: nested folders with files; then a missing path is no error
    const fs::path tree = base / "tree";
    fs::create_directories(tree / "a" / "b", ec);
    writeText(tree / "top.bin", "x");
    writeText(tree / "a" / "mid.bin", "y");
    writeText(tree / "a" / "b" / "deep.bin", "z");
    ps2x::removeTree(tree, ec);
    check(!ec && !fs::exists(tree), "removeTree removes nested files and folders");
    ps2x::removeTree(tree, ec);
    check(!ec, "removeTree of a missing path succeeds");

    // the memory card on top of them: format and unformat clear a card with saves, rename onto an existing name
    {
        PS2Runtime runtime;
        std::vector<uint8_t> rdram(PS2_RAM_SIZE, 0);
        const fs::path data = base / "userdata", mc0 = data / "mc0";
        fs::create_directories(mc0 / "SAVE1", ec);
        writeText(mc0 / "SAVE1" / "slot.dat", "save");
        writeText(mc0 / "SAVE1" / "icon.sys", "icon");
        PS2Runtime::IoPaths io;
        io.elfDirectory = base;
        io.hostRoot = base;
        io.cdRoot = base;
        io.mcRoot = mc0;
        PS2Runtime::setIoPaths(io);

        R5900Context ctx{};
        constexpr uint32_t kOld = 0x1000, kNew = 0x1100;
        auto name = [&](uint32_t at, const char *text) { std::memcpy(rdram.data() + at, text, std::strlen(text) + 1); };
        writeText(mc0 / "SAVE1" / "new.dat", "renamed");
        name(kOld, "/SAVE1/new.dat");
        name(kNew, "/SAVE1/slot.dat");
        setReg(ctx, 4, 0);
        setReg(ctx, 5, 0);
        setReg(ctx, 6, kOld);
        setReg(ctx, 7, kNew);
        ps2_stubs::sceMcRename(rdram.data(), &ctx, nullptr);
        const int32_t renamed = syncMc(rdram);
        check(renamed == 0 && readText(mc0 / "SAVE1" / "slot.dat") == "renamed" && !fs::exists(mc0 / "SAVE1" / "new.dat"),
              "card rename onto an existing name");
        check(fs::is_directory(data / "backups") && entries(data / "backups") >= 1, "card backup written before the change");

        ctx = R5900Context{};
        setReg(ctx, 4, 0);
        setReg(ctx, 5, 0);
        ps2_stubs::sceMcFormat(rdram.data(), &ctx, nullptr);
        check(syncMc(rdram) == 0 && fs::is_directory(mc0) && entries(mc0) == 0, "card format clears a card holding saves");

        fs::create_directories(mc0 / "SAVE2", ec);
        writeText(mc0 / "SAVE2" / "slot.dat", "again");
        ctx = R5900Context{};
        setReg(ctx, 4, 0);
        setReg(ctx, 5, 0);
        ps2_stubs::sceMcUnformat(rdram.data(), &ctx, nullptr);
        check(syncMc(rdram) == 0 && entries(mc0) == 0, "card unformat clears a card holding saves");
        std::printf("[host-fs] backups=%zu\n", entries(data / "backups"));
    }

    ps2x::removeTree(base, ec);
    check(!ec && !fs::exists(base), "work folder removed");
    std::printf("[host-fs] failures=%d\n", g_failures);
    return g_failures;
}

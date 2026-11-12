// src/core/run_config.cpp: the bounded nxrun.cfg schedule of scripted Switch runs (M04-11).
#include "core/run_config.h"

#include <cstdio>
#include <string>

namespace
{
    int g_failures = 0;

    void check(bool ok, const std::string &what)
    {
        std::printf("%s run-config %s\n", ok ? "PASS" : "FAIL", what.c_str());
        if (!ok)
            ++g_failures;
    }

    bool rejects(const std::string &text, const std::string &expect)
    {
        RunConfig config;
        std::string error;
        const bool ok = parseRunConfig(text, config, error);
        if (ok || error.find(expect) == std::string::npos)
        {
            std::printf("  got ok=%d error='%s', expected '%s'\n", ok ? 1 : 0, error.c_str(), expect.c_str());
            return false;
        }
        return true;
    }
}

int runRunConfigTests()
{
    g_failures = 0;
    {
        RunConfig config;
        std::string error;
        const bool ok = parseRunConfig("# boot\nprofile m04-boot\r\nenv VCS_SKIP_MOVIES=1\nseconds 240\nreport 5\n"
                                       "at 30 gameinfo\nevery 10 5 25 frames\nat 1 press cross\nverify-files\n",
                                       config, error);
        check(ok && error.empty(), "full file parses");
        check(config.present && config.profile == "m04-boot" && config.seconds == 240.0 && config.report == 5.0 &&
                  config.verifyFiles,
              "profile, seconds, report, verify-files");
        check(config.env.size() == 1 && config.env[0].first == "VCS_SKIP_MOVIES" && config.env[0].second == "1", "env");
        check(config.commands.size() == 5, "at + every expand to 5 commands");
        bool sorted = true;
        for (size_t i = 1; i < config.commands.size(); ++i)
            sorted = sorted && config.commands[i - 1].at <= config.commands[i].at;
        check(sorted && config.commands.front().line == "press cross" && config.commands.back().line == "gameinfo",
              "commands sorted by time");
        check(config.commands[1].at == 5.0 && config.commands[2].at == 15.0 && config.commands[3].at == 25.0,
              "every includes its last time");
    }
    {
        RunConfig config;
        std::string error;
        check(parseRunConfig("", config, error) && config.commands.empty() && config.profile.empty(), "empty file");
    }
    check(rejects("bogus 1\n", "nxrun.cfg:1: unknown directive"), "unknown directive named with its line");
    check(rejects("profile a\nprofile b\n", "nxrun.cfg:2: profile given twice"), "profile twice");
    check(rejects("profile ../x\n", "profile needs"), "unsafe profile");
    check(rejects("env HOME=/x\n", "env needs"), "env outside VCS_/PS2X_");
    check(rejects("env VCS_x=1\n", "env needs"), "lower-case env name");
    check(rejects("env PS2X_CONTROL=1\n", "env needs"), "socket control channel refused");
    check(rejects("env PS2X_HIDDEN=1\n", "env needs"), "hidden mode refused");
    check(rejects("env VCS_DATA_DIR=/x\n", "env needs"), "data folder override refused");
    check(rejects("seconds 0\n", "seconds needs"), "seconds below range");
    check(rejects("seconds -5\n", "seconds needs"), "negative seconds");
    check(rejects("seconds 1e9\n", "seconds needs"), "exponent refused");
    check(rejects("at 5\n", "at needs"), "at without a command");
    check(rejects("every 0 0 10 frames\n", "every needs"), "every with a zero period");
    check(rejects("every 1 10 5 frames\n", "every needs"), "every ending before it starts");
    check(rejects("every 0.1 0 100 frames\n", "more than 512 commands"), "every bounded by the command limit");
    check(rejects("verify-files now\n", "takes no argument"), "verify-files argument");
    check(rejects("at 1 press\x01\n", "non-printable"), "control characters");
    check(rejects(std::string(300, 'a') + "\n", "longer than 256"), "long line");
    {
        std::string many;
        for (int i = 0; i < 130; ++i)
            many += "# comment\n";
        check(rejects(many, "more than 128 lines"), "line limit");
    }
    {
        RunConfig config;
        std::string error;
        check(loadRunConfig("/nonexistent-dir/nxrun.cfg", config, error) && !config.present, "missing file is a normal run");
    }
    std::printf("[run-config] failures=%d\n", g_failures);
    return g_failures;
}

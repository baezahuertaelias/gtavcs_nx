#include "core/run_config.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>

namespace
{
    std::string_view trim(std::string_view s)
    {
        while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
            s.remove_prefix(1);
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
            s.remove_suffix(1);
        return s;
    }

    bool number(std::string_view text, double lo, double hi, double &out)
    {
        if (text.empty() || text.size() > 16)
            return false;
        for (char c : text)
            if (!((c >= '0' && c <= '9') || c == '.'))
                return false;
        const std::string copy(text);
        char *end = nullptr;
        errno = 0;
        const double v = std::strtod(copy.c_str(), &end);
        if (errno || !end || *end || v < lo || v > hi)
            return false;
        out = v;
        return true;
    }

    bool envName(std::string_view name)
    {
        if (!(name.rfind("VCS_", 0) == 0 || name.rfind("PS2X_", 0) == 0) || name.size() > 64)
            return false;
        return std::all_of(name.begin(), name.end(), [](char c) { return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; });
    }

    bool printable(std::string_view s)
    {
        return std::all_of(s.begin(), s.end(), [](char c) { return c >= 0x20 && c < 0x7f; });
    }
}

bool parseRunConfig(std::string_view text, RunConfig &out, std::string &error)
{
    out = RunConfig{};
    out.present = true;
    size_t lineNo = 0;
    while (!text.empty())
    {
        const size_t nl = text.find('\n');
        std::string_view raw = text.substr(0, nl);
        text = nl == std::string_view::npos ? std::string_view{} : text.substr(nl + 1);
        ++lineNo;
        auto fail = [&](const std::string &why) {
            error = "nxrun.cfg:" + std::to_string(lineNo) + ": " + why;
            return false;
        };
        if (lineNo > RunConfig::kMaxLines)
            return fail("more than " + std::to_string(RunConfig::kMaxLines) + " lines");
        if (raw.size() > RunConfig::kMaxLineBytes)
            return fail("line longer than " + std::to_string(RunConfig::kMaxLineBytes) + " bytes");
        if (const size_t hash = raw.find('#'); hash != std::string_view::npos)
            raw = raw.substr(0, hash);
        const std::string_view line = trim(raw);
        if (line.empty())
            continue;
        if (!printable(line))
            return fail("non-printable character");
        const size_t space = line.find(' ');
        const std::string_view verb = line.substr(0, space);
        const std::string_view rest = space == std::string_view::npos ? std::string_view{} : trim(line.substr(space + 1));
        if (verb == "profile")
        {
            if (rest.empty() || rest.size() > 64 ||
                !std::all_of(rest.begin(), rest.end(), [](char c) {
                    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
                }))
                return fail("profile needs a name of letters, digits, '-' or '_' (at most 64)");
            if (!out.profile.empty())
                return fail("profile given twice");
            out.profile = rest;
        }
        else if (verb == "env")
        {
            const size_t eq = rest.find('=');
            if (eq == std::string_view::npos || !envName(rest.substr(0, eq)))
                return fail("env needs NAME=value with NAME starting VCS_ or PS2X_");
            if (out.env.size() >= RunConfig::kMaxEnv)
                return fail("more than " + std::to_string(RunConfig::kMaxEnv) + " env lines");
            out.env.emplace_back(std::string(rest.substr(0, eq)), std::string(rest.substr(eq + 1)));
        }
        else if (verb == "seconds")
        {
            if (!number(rest, 1.0, 86400.0, out.seconds))
                return fail("seconds needs 1..86400");
        }
        else if (verb == "report")
        {
            if (!number(rest, 1.0, 3600.0, out.report))
                return fail("report needs 1..3600");
        }
        else if (verb == "at")
        {
            const size_t gap = rest.find(' ');
            RunCommand command;
            if (gap == std::string_view::npos || !number(rest.substr(0, gap), 0.0, 86400.0, command.at))
                return fail("at needs <seconds> <command>");
            command.line = trim(rest.substr(gap + 1));
            if (command.line.empty())
                return fail("at needs a command");
            if (out.commands.size() >= RunConfig::kMaxCommands)
                return fail("more than " + std::to_string(RunConfig::kMaxCommands) + " commands");
            out.commands.push_back(std::move(command));
        }
        else if (verb == "verify-files")
        {
            if (!rest.empty())
                return fail("verify-files takes no argument");
            out.verifyFiles = true;
        }
        else
            return fail("unknown directive '" + std::string(verb) + "'");
    }
    std::stable_sort(out.commands.begin(), out.commands.end(),
                     [](const RunCommand &a, const RunCommand &b) { return a.at < b.at; });
    return true;
}

bool loadRunConfig(const std::string &path, RunConfig &out, std::string &error)
{
    out = RunConfig{};
    std::FILE *f = std::fopen(path.c_str(), "rb");
    if (!f)
    {
        if (errno == ENOENT)
            return true; // no file: a normal run
        error = "cannot open " + path;
        return false;
    }
    std::string text;
    char buffer[1024];
    size_t n;
    while ((n = std::fread(buffer, 1, sizeof(buffer), f)) > 0)
    {
        text.append(buffer, n);
        if (text.size() > RunConfig::kMaxLines * (RunConfig::kMaxLineBytes + 2))
        {
            std::fclose(f);
            error = "nxrun.cfg: file too large";
            return false;
        }
    }
    const bool readError = std::ferror(f) != 0;
    std::fclose(f);
    if (readError)
    {
        error = "cannot read " + path;
        return false;
    }
    return parseRunConfig(text, out, error);
}

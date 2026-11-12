// A bounded run description for scripted Switch sessions: gamefolder/userdata/nxrun.cfg.
//
// The Switch has no command line or test client, so a session that needs its own saves, settings overrides, a time
// limit or control-channel queries puts them in this file; without it the game starts as a player's normal run.
// One directive per line, '#' starts a comment:
//   profile <name>          GamePaths test profile: settings, cache, cards and backups under userdata/tests/<name>
//   env <NAME>=<value>      an environment override for this run (NAME starts with VCS_ or PS2X_)
//   seconds <n>             stop in an orderly way after n seconds of frames (1..86400)
//   report <n>              progress counters to the log every n seconds (1..3600; default 5)
//   at <seconds> <command>  a control-channel command (runtime/ps2_control.h) at that time; answers are logged
//   every <period> <from> <to> <command>   the command at from, from + period, ... up to to (inclusive)
//   verify-files            hash every disc file against the manifest before booting (FolderDisc::verifyFiles)
// Errors name the line ("nxrun.cfg:3: ..."); a bad file stops the boot instead of running something else.
#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct RunCommand
{
    double at = 0.0;     // seconds after the first frame
    std::string line;    // the control-channel command
};

struct RunConfig
{
    bool present = false; // the file existed
    std::string profile;
    std::vector<std::pair<std::string, std::string>> env;
    double seconds = 0.0; // 0: until the game or the player ends it
    double report = 5.0;
    std::vector<RunCommand> commands; // sorted by time, stable
    bool verifyFiles = false;

    static constexpr size_t kMaxLines = 128;
    static constexpr size_t kMaxLineBytes = 256;
    static constexpr size_t kMaxCommands = 512;
    static constexpr size_t kMaxEnv = 32;
};

// Parses the file's text; false with `error` naming the first bad line.
bool parseRunConfig(std::string_view text, RunConfig &out, std::string &error);
// Reads `path` (a missing file is an empty, valid config); false with `error` on a read or parse failure.
bool loadRunConfig(const std::string &path, RunConfig &out, std::string &error);

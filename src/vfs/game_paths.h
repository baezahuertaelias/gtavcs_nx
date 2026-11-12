// Where the port keeps assets, settings, cache and saves under the game folder.
#pragma once

#include <string>
#include <string_view>

struct GamePaths
{
    std::string root;     // the asset tree, rooted
    std::string userdata; // settings, cache and saves; never disc files
    std::string cache;
    std::string settings; // the settings file
    std::string mc0;      // memory card roots; the card layer keeps its order file and metadata inside
    std::string mc1;
    std::string backups;  // the card layer's automatic card backups (upstream <data>/backups)
    std::string elf;      // the guest executable as extracted from the disc
};

// Paths handed to runtime std::filesystem code must not carry "sdmc:", which newlib's
// std::filesystem treats as relative and joins onto the cwd. Strips that device and
// accepts only rooted paths, so nothing depends on the cwd. Refuses other devices and ".."
// components. std::filesystem::remove_all failed on the Switch for a relative (c5903d1) and
// a rooted (27274a3) path alike; docs/milestones/M03/HARDWARE_RESULTS.md has the probe.
bool rootedGamePath(std::string_view path, std::string &out);

// Requires an existing asset root, and creates only the userdata folders inside it.
// A non-empty testProfile (letters, digits, '-', '_') gives a test run its own settings, cache,
// cards and backups under userdata/tests/<testProfile>, so tests never touch the player's saves.
bool resolveGamePaths(std::string_view root, GamePaths &out, std::string &error, std::string_view testProfile = {});

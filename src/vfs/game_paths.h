// Where the port keeps assets, settings, cache and saves under the game folder.
#pragma once

#include <string>
#include <string_view>

struct GamePaths
{
    std::string root;     // the asset tree, rooted or cwd-relative
    std::string userdata; // settings, cache and saves; never disc files
    std::string cache;
    std::string settings; // the settings file
    std::string mc0;      // memory card roots
    std::string mc1;
    std::string elf;      // the guest executable as extracted from the disc
};

// Paths handed to runtime std::filesystem code must not carry "sdmc:", which newlib's
// std::filesystem treats as relative and joins onto the cwd. Strips that device, keeps
// rooted and cwd-relative paths, and refuses other devices and ".." components.
bool rootedGamePath(std::string_view path, std::string &out);

// Requires an existing asset root, and creates only the userdata folders inside it.
bool resolveGamePaths(std::string_view root, GamePaths &out, std::string &error);

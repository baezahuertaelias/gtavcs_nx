#include "game_paths.h"

#include <algorithm>
#include <cerrno>
#include <sys/stat.h>
#if defined(_WIN32)
#include <direct.h>
#endif

namespace
{
    bool isDirectory(const std::string &path)
    {
        struct stat info{};
        return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
    }

    bool ensureDirectory(const std::string &path)
    {
#if defined(_WIN32)
        const int result = _mkdir(path.c_str());
#else
        const int result = mkdir(path.c_str(), 0777);
#endif
        return (result == 0 || errno == EEXIST) && isDirectory(path);
    }
}

bool rootedGamePath(std::string_view path, std::string &out)
{
    out.clear();
    static constexpr std::string_view kDevice = "sdmc:";
    if (path.substr(0, kDevice.size()) == kDevice)
    {
        path.remove_prefix(kDevice.size());
        if (path.empty() || path[0] != '/')
        {
            return false;
        }
    }
    if (path.empty() || path.find(':') != std::string_view::npos || path.find('\\') != std::string_view::npos)
    {
        return false;
    }
    size_t start = 0;
    while (start <= path.size())
    {
        const size_t stop = std::min(path.find('/', start), path.size());
        if (path.substr(start, stop - start) == "..")
        {
            return false;
        }
        start = stop + 1;
    }
    out.assign(path);
    while (out.size() > 1 && out.back() == '/')
    {
        out.pop_back();
    }
    return true;
}

bool resolveGamePaths(std::string_view root, GamePaths &out, std::string &error)
{
    std::string base;
    if (!rootedGamePath(root, base))
    {
        error = "invalid game folder path: " + std::string(root);
        return false;
    }
    if (!isDirectory(base))
    {
        error = "game folder does not exist: " + base;
        return false;
    }
    out.root = base;
    out.userdata = base + "/userdata";
    out.cache = out.userdata + "/cache";
    out.settings = out.userdata + "/vcs.ini";
    out.mc0 = out.userdata + "/mc0";
    out.mc1 = out.userdata + "/mc1";
    out.elf = base + "/SLUS_215.90";
    for (const std::string *path : {&out.userdata, &out.cache, &out.mc0, &out.mc1})
    {
        if (!ensureDirectory(*path))
        {
            error = "cannot create " + *path;
            return false;
        }
    }
    return true;
}

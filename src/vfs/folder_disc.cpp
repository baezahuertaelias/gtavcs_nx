#include "folder_disc.h"

#include <algorithm>
#include <charconv>
#include <cstring>
#include <sys/stat.h>

namespace
{
    constexpr uint64_t kMaxRawBytes = 16u * 1024u * 1024u;

    bool parseNumber(std::string_view text, uint64_t &out)
    {
        const char *end = text.data() + text.size();
        const auto result = std::from_chars(text.data(), end, out);
        return !text.empty() && result.ec == std::errc{} && result.ptr == end;
    }

    std::vector<std::string_view> splitTabs(std::string_view line)
    {
        std::vector<std::string_view> fields;
        size_t start = 0;
        while (true)
        {
            const size_t tab = line.find('\t', start);
            if (tab == std::string_view::npos)
            {
                fields.push_back(line.substr(start));
                return fields;
            }
            fields.push_back(line.substr(start, tab - start));
            start = tab + 1;
        }
    }

    bool readWholeFile(const std::string &path, uint64_t limit, std::vector<uint8_t> &out)
    {
        std::FILE *file = std::fopen(path.c_str(), "rb");
        if (!file)
        {
            return false;
        }
        out.clear();
        uint8_t buffer[16384];
        size_t count = 0;
        while ((count = std::fread(buffer, 1, sizeof(buffer), file)) != 0)
        {
            if (out.size() + count > limit)
            {
                std::fclose(file);
                return false;
            }
            out.insert(out.end(), buffer, buffer + count);
        }
        const bool ok = std::ferror(file) == 0;
        std::fclose(file);
        return ok;
    }

    bool seekTo(std::FILE *file, uint64_t offset)
    {
#if defined(_WIN32)
        return _fseeki64(file, static_cast<long long>(offset), SEEK_SET) == 0;
#else
        return fseeko(file, static_cast<off_t>(offset), SEEK_SET) == 0;
#endif
    }

    std::string parentOf(const std::string &path)
    {
        const size_t slash = path.rfind('/');
        return slash == std::string::npos ? std::string() : path.substr(0, slash);
    }

    std::string leafOf(const std::string &path)
    {
        const size_t slash = path.rfind('/');
        return slash == std::string::npos ? path : path.substr(slash + 1);
    }
}

bool FolderDisc::normalize(std::string_view path, std::string &out)
{
    out.clear();
    static constexpr std::string_view kDevice = "cdrom0:";
    if (path.size() >= kDevice.size())
    {
        bool device = true;
        for (size_t i = 0; i < kDevice.size(); ++i)
        {
            const char c = path[i];
            device = device && (c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c) == kDevice[i];
        }
        if (device)
        {
            path.remove_prefix(kDevice.size());
        }
    }
    if (const size_t semi = path.find(';'); semi != std::string_view::npos)
    {
        const std::string_view version = path.substr(semi + 1);
        uint64_t number = 0;
        if (!parseNumber(version, number))
        {
            return false;
        }
        path = path.substr(0, semi);
    }
    size_t start = 0;
    while (start <= path.size())
    {
        size_t stop = start;
        while (stop < path.size() && path[stop] != '/' && path[stop] != '\\')
        {
            ++stop;
        }
        std::string_view part = path.substr(start, stop - start);
        start = stop + 1;
        if (part.empty())
        {
            continue;
        }
        if (part == "." || part == "..")
        {
            return false;
        }
        for (const char c : part)
        {
            if (static_cast<unsigned char>(c) < 0x20 || c == ':' || c == ';')
            {
                return false;
            }
        }
        if (!out.empty())
        {
            out.push_back('/');
        }
        for (const char c : part)
        {
            out.push_back(c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c);
        }
    }
    return true;
}

bool FolderDisc::parseManifest(const std::string &root, std::string &error)
{
    const std::string manifestPath = root + "/.nxdisc/manifest.tsv";
    std::vector<uint8_t> bytes;
    if (!readWholeFile(manifestPath, 16u * 1024u * 1024u, bytes))
    {
        error = "cannot read " + manifestPath + "; run tools/extract_disc.py and copy .nxdisc to the SD card";
        return false;
    }
    const std::string_view text(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    size_t position = 0, lineNumber = 0;
    const auto fail = [&](const std::string &why) {
        error = manifestPath + " line " + std::to_string(lineNumber) + ": " + why;
        return false;
    };
    while (position < text.size())
    {
        size_t end = text.find('\n', position);
        if (end == std::string_view::npos)
        {
            ++lineNumber;
            return fail("truncated manifest");
        }
        const std::string_view line = text.substr(position, end - position);
        position = end + 1;
        ++lineNumber;
        const std::vector<std::string_view> fields = splitTabs(line);
        if (lineNumber == 1)
        {
            if (line != "NXDISC\t1")
            {
                return fail("unsupported manifest version");
            }
            continue;
        }
        if (fields[0] == "sectors" && fields.size() == 2)
        {
            uint64_t sectors = 0;
            if (!parseNumber(fields[1], sectors) || sectors == 0 || sectors > 0x7fffffffu)
            {
                return fail("invalid sector count");
            }
            m_totalSectors = static_cast<uint32_t>(sectors);
        }
        else if ((fields[0] == "D" || fields[0] == "F") && fields.size() == 5)
        {
            if (m_totalSectors == 0)
            {
                return fail("entry before the sector count");
            }
            const bool directory = fields[0] == "D";
            uint64_t lba = 0, size = 0;
            if (!parseNumber(fields[1], lba) || !parseNumber(fields[2], size) || size > 0xffffffffu)
            {
                return fail("invalid extent");
            }
            const uint64_t sectors = (size + kSectorSize - 1) / kSectorSize;
            if (lba + sectors > m_totalSectors)
            {
                return fail("extent past the end of the disc");
            }
            std::string key;
            if (!normalize(fields[4], key) || key.size() != fields[4].size() || (key.empty() && !directory))
            {
                return fail("unsafe path");
            }
            const std::string exact(fields[4]);
            if (key != exact || m_entries.count(key))
            {
                return fail(key != exact ? "path is not in disc form" : "duplicate path");
            }
            m_entries[key] = Entry{static_cast<uint32_t>(lba), static_cast<uint32_t>(size), directory};
            if (directory)
            {
                m_children.try_emplace(key);
            }
            else
            {
                Extent extent;
                extent.begin = lba * kSectorSize;
                extent.end = extent.begin + sectors * kSectorSize;
                extent.fileEnd = extent.begin + size;
                extent.file = static_cast<uint32_t>(m_files.size());
                m_extents.push_back(extent);
                m_files.push_back(File{root + "/" + exact});
            }
            if (!key.empty())
            {
                const auto parent = m_children.find(parentOf(key));
                if (parent == m_children.end())
                {
                    return fail("entry before its parent directory");
                }
                parent->second.push_back(DirEntry{leafOf(key), static_cast<uint32_t>(lba), static_cast<uint32_t>(size), directory});
            }
        }
        else if (fields[0] == "R" && fields.size() == 4)
        {
            uint64_t lba = 0, sectors = 0, offset = 0;
            if (!parseNumber(fields[1], lba) || !parseNumber(fields[2], sectors) || !parseNumber(fields[3], offset) ||
                sectors == 0 || m_totalSectors == 0 || lba + sectors > m_totalSectors)
            {
                return fail("invalid raw extent");
            }
            Extent extent;
            extent.begin = lba * kSectorSize;
            extent.end = extent.begin + sectors * kSectorSize;
            extent.fileEnd = extent.end;
            extent.file = kRawExtent;
            extent.source = offset;
            m_extents.push_back(extent);
        }
        else if (fields[0] == "D" || fields[0] == "F" || fields[0] == "R" || fields.size() != 2)
        {
            return fail("malformed record");
        }
    }
    if (m_totalSectors == 0 || !m_entries.count(std::string()))
    {
        error = manifestPath + ": no sector count or root directory";
        return false;
    }
    return true;
}

bool FolderDisc::finishExtents(const std::string &root, std::string &error)
{
    std::sort(m_extents.begin(), m_extents.end(), [](const Extent &a, const Extent &b) { return a.begin < b.begin; });
    uint64_t position = 0, rawBytes = 0;
    struct stat modsInfo{};
    const bool mods = stat((root + "/mods").c_str(), &modsInfo) == 0 && S_ISDIR(modsInfo.st_mode);
    for (const Extent &extent : m_extents)
    {
        if (extent.begin < position)
        {
            error = "manifest extents overlap at sector " + std::to_string(extent.begin / kSectorSize);
            return false;
        }
        position = extent.end;
        if (extent.file == kRawExtent)
        {
            if (extent.source != rawBytes)
            {
                error = "manifest raw extents are out of order";
                return false;
            }
            rawBytes += extent.end - extent.begin;
        }
        else
        {
            struct stat info{};
            const std::string &path = m_files[extent.file].hostPath;
            if (stat(path.c_str(), &info) != 0 || !S_ISREG(info.st_mode))
            {
                error = "missing game file: " + path;
                return false;
            }
            if (static_cast<uint64_t>(info.st_size) != extent.fileEnd - extent.begin)
            {
                error = "wrong size for game file: " + path;
                return false;
            }
            if (mods)
            {
                // A different size would move every later LBA the game expects, so it is refused.
                const std::string modPath = root + "/mods/" + path.substr(root.size() + 1);
                struct stat modInfo{};
                if (stat(modPath.c_str(), &modInfo) == 0)
                {
                    if (S_ISREG(modInfo.st_mode) && modInfo.st_size == info.st_size)
                    {
                        m_files[extent.file].hostPath = modPath;
                        ++m_modsApplied;
                    }
                    else
                    {
                        m_warnings.push_back("ignored mod, size differs from the disc file: " + modPath);
                    }
                }
            }
        }
    }
    if (rawBytes > kMaxRawBytes)
    {
        error = "manifest raw extents are too large";
        return false;
    }
    const std::string rawPath = root + "/.nxdisc/raw.bin";
    if (!readWholeFile(rawPath, kMaxRawBytes, m_raw) || m_raw.size() != rawBytes)
    {
        error = "missing or wrong size: " + rawPath;
        return false;
    }
    return true;
}

std::unique_ptr<FolderDisc> FolderDisc::load(const std::string &root, std::string &error)
{
    std::unique_ptr<FolderDisc> disc(new FolderDisc());
    if (!disc->parseManifest(root, error) || !disc->finishExtents(root, error))
    {
        return nullptr;
    }
    return disc;
}

FolderDisc::~FolderDisc()
{
    for (Slot &slot : m_slots)
    {
        if (slot.handle)
        {
            std::fclose(slot.handle);
        }
    }
}

bool FolderDisc::findFile(std::string_view ps2Path, uint32_t &lsn, uint32_t &sizeBytes, bool &isDirectory) const
{
    std::string key;
    if (!normalize(ps2Path, key))
    {
        return false;
    }
    const auto found = m_entries.find(key);
    if (found == m_entries.end())
    {
        return false;
    }
    lsn = found->second.lsn;
    sizeBytes = found->second.sizeBytes;
    isDirectory = found->second.isDirectory;
    return true;
}

bool FolderDisc::listDirectory(std::string_view ps2Path, std::vector<DirEntry> &out) const
{
    out.clear();
    std::string key;
    if (!normalize(ps2Path, key))
    {
        return false;
    }
    const auto found = m_children.find(key);
    if (found == m_children.end())
    {
        return false;
    }
    out = found->second;
    return true;
}

FolderDisc::Slot *FolderDisc::acquire(uint32_t file) const
{
    std::unique_lock<std::mutex> lock(m_mutex);
    while (true)
    {
        Slot *same = nullptr, *victim = nullptr;
        for (Slot &slot : m_slots)
        {
            if (slot.handle && slot.file == file)
            {
                same = &slot;
            }
            if (!slot.busy && (!victim || !slot.handle || (victim->handle && slot.lastUse < victim->lastUse)))
            {
                victim = &slot;
            }
        }
        if (same && same->busy)
        {
            m_released.wait(lock);
            continue;
        }
        Slot *chosen = same ? same : victim;
        if (!chosen)
        {
            m_released.wait(lock);
            continue;
        }
        if (!same)
        {
            if (chosen->handle)
            {
                std::fclose(chosen->handle);
            }
            chosen->handle = std::fopen(m_files[file].hostPath.c_str(), "rb");
            chosen->file = file;
            if (!chosen->handle)
            {
                return nullptr;
            }
        }
        chosen->busy = true;
        chosen->lastUse = ++m_clock;
        return chosen;
    }
}

void FolderDisc::release(Slot *slot) const
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        slot->busy = false;
    }
    m_released.notify_all();
}

bool FolderDisc::readFile(uint32_t file, uint64_t offset, size_t count, uint8_t *destination) const
{
    Slot *slot = acquire(file);
    const bool ok = slot && seekTo(slot->handle, offset) && std::fread(destination, 1, count, slot->handle) == count;
    if (slot)
    {
        release(slot);
    }
    if (!ok)
    {
        std::fprintf(stderr, "[folder-disc] read failed: %s offset=%llu count=%zu\n", m_files[file].hostPath.c_str(),
                     static_cast<unsigned long long>(offset), count);
    }
    return ok;
}

bool FolderDisc::readBytes(uint64_t offset, size_t byteCount, void *destination) const
{
    if (byteCount == 0)
    {
        return true;
    }
    if (!destination)
    {
        return false;
    }
    uint8_t *out = static_cast<uint8_t *>(destination);
    std::memset(out, 0, byteCount);
    const uint64_t discBytes = uint64_t(m_totalSectors) * kSectorSize;
    if (offset >= discBytes)
    {
        return false;
    }
    const bool complete = byteCount <= discBytes - offset;
    const uint64_t stop = complete ? offset + byteCount : discBytes;

    auto extent = std::upper_bound(m_extents.begin(), m_extents.end(), offset,
                                   [](uint64_t value, const Extent &item) { return value < item.begin; });
    if (extent != m_extents.begin())
    {
        --extent;
    }
    uint64_t position = offset;
    for (; extent != m_extents.end() && extent->begin < stop; ++extent)
    {
        const uint64_t begin = std::max(position, extent->begin);
        if (begin >= extent->end)
        {
            continue; // the search can land on the extent before offset
        }
        const uint64_t dataEnd = std::min(stop, extent->fileEnd);
        if (begin < dataEnd)
        {
            uint8_t *target = out + (begin - offset);
            const size_t count = static_cast<size_t>(dataEnd - begin);
            if (extent->file == kRawExtent)
            {
                std::memcpy(target, m_raw.data() + extent->source + (begin - extent->begin), count);
            }
            else if (!readFile(extent->file, begin - extent->begin, count, target))
            {
                std::memset(out, 0, byteCount);
                return false;
            }
        }
        position = extent->end; // gaps and file padding stay zero from the memset above
    }
    return complete;
}

// Desktop verification only: answers from the ISO and checks every live request against
// the folder form. Copied into the disposable reference tree by tools/desktop_folder_disc.py.
#pragma once

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "disc/iso_image.h"
#include "vfs/folder_disc.h"

class TeeDisc final : public IPS2DiscImage
{
public:
    TeeDisc(IPS2DiscImage &iso, const FolderDisc &folder, const vcs::IsoImage &image, std::string logPath)
        : m_iso(iso), m_folder(folder), m_logPath(std::move(logPath))
    {
        for (const auto &[path, entry] : image.entries())
        {
            if (!entry.isDirectory && entry.size)
                m_files.push_back(Range{uint64_t(entry.lba) * 2048u, uint64_t(entry.lba) * 2048u + entry.size, path});
        }
        std::sort(m_files.begin(), m_files.end(), [](const Range &a, const Range &b) { return a.begin < b.begin; });
    }

    ~TeeDisc() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        dump();
    }

    bool findFile(std::string_view path, uint32_t &lsn, uint32_t &sizeBytes, bool &isDirectory) const override
    {
        uint32_t folderLsn = 0, folderSize = 0;
        bool folderDirectory = false;
        const bool a = m_iso.findFile(path, lsn, sizeBytes, isDirectory);
        const bool b = m_folder.findFile(path, folderLsn, folderSize, folderDirectory);
        if (a != b || (a && (lsn != folderLsn || sizeBytes != folderSize || isDirectory != folderDirectory)))
            note("findFile", std::string(path), 0, 0);
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_lookups;
        return a;
    }

    bool readBytes(uint64_t offset, size_t byteCount, void *destination) const override
    {
        const bool a = m_iso.readBytes(offset, byteCount, destination);
        std::vector<uint8_t> other(byteCount);
        const bool b = m_folder.readBytes(offset, byteCount, other.data());
        const bool same = a == b && (!byteCount || std::memcmp(destination, other.data(), byteCount) == 0);
        if (!same)
            note("readBytes", "", offset, byteCount);
        record(offset, byteCount);
        return a;
    }

    uint32_t totalSectors() const override { return m_iso.totalSectors(); }

    bool listDirectory(std::string_view path, std::vector<DirEntry> &out) const override
    {
        std::vector<DirEntry> other;
        const bool a = m_iso.listDirectory(path, out);
        const bool b = m_folder.listDirectory(path, other);
        bool same = a == b && out.size() == other.size();
        for (size_t i = 0; same && i < out.size(); ++i)
            same = out[i].name == other[i].name && out[i].lsn == other[i].lsn &&
                   out[i].sizeBytes == other[i].sizeBytes && out[i].isDirectory == other[i].isDirectory;
        if (!same)
            note("listDirectory", std::string(path), 0, 0);
        return a;
    }

private:
    struct Range
    {
        uint64_t begin, end;
        std::string path;
    };
    struct Count
    {
        uint64_t reads = 0, bytes = 0;
    };

    void note(const char *what, const std::string &path, uint64_t offset, size_t count) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_mismatches;
        std::fprintf(stderr, "[tee-disc] MISMATCH %s %s offset=%llu count=%zu\n", what, path.c_str(),
                     static_cast<unsigned long long>(offset), count);
        dump();
    }

    const Range *fileAt(uint64_t offset) const
    {
        auto it = std::upper_bound(m_files.begin(), m_files.end(), offset,
                                   [](uint64_t value, const Range &range) { return value < range.begin; });
        if (it == m_files.begin())
            return nullptr;
        --it;
        return offset < it->end ? &*it : nullptr;
    }

    void record(uint64_t offset, size_t count) const
    {
        const Range *range = fileAt(offset);
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_reads;
        m_bytes += count;
        m_unaligned += offset % 2048u != 0;
        m_crossing += !range || offset + count > range->end;
        Count &bucket = m_perFile[range ? range->path : std::string("(non-file)")];
        ++bucket.reads;
        bucket.bytes += count;
        // Every read: upstream may leave with std::_Exit, so the destructor cannot be relied on.
        dump();
    }

    void dump() const
    {
        std::FILE *file = std::fopen(m_logPath.c_str(), "w");
        if (!file)
            return;
        std::fprintf(file, "reads %llu\nbytes %llu\nlookups %llu\nmismatches %llu\nunaligned %llu\ncrossing %llu\n",
                     static_cast<unsigned long long>(m_reads), static_cast<unsigned long long>(m_bytes),
                     static_cast<unsigned long long>(m_lookups), static_cast<unsigned long long>(m_mismatches),
                     static_cast<unsigned long long>(m_unaligned), static_cast<unsigned long long>(m_crossing));
        for (const auto &[path, count] : m_perFile)
            std::fprintf(file, "file %llu %llu %s\n", static_cast<unsigned long long>(count.reads),
                         static_cast<unsigned long long>(count.bytes), path.c_str());
        std::fclose(file);
    }

    IPS2DiscImage &m_iso;
    const FolderDisc &m_folder;
    std::string m_logPath;
    std::vector<Range> m_files;
    mutable std::mutex m_mutex;
    mutable std::map<std::string, Count> m_perFile;
    mutable uint64_t m_reads = 0, m_bytes = 0, m_lookups = 0, m_mismatches = 0, m_unaligned = 0, m_crossing = 0;
};

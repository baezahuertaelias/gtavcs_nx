// Desktop verification only: FolderDisc against the source image on real, private data.
// Prints counts only. Usage: disc_compare <iso> <gamefolder>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "disc/iso_image.h"
#include "vfs/folder_disc.h"

namespace {
uint64_t compared=0, bytes=0, failures=0;

void compare(const vcs::IsoImage &iso,const FolderDisc &folder,uint64_t offset,size_t size,std::vector<uint8_t> &a,std::vector<uint8_t> &b) {
    a.resize(size); b.resize(size);
    const bool first=iso.readBytes(offset,size,a.data());
    const bool second=folder.readBytes(offset,size,b.data());
    ++compared; bytes+=size;
    if(first!=second || (size && std::memcmp(a.data(),b.data(),size)!=0)) {
        ++failures;
        if(failures<20) std::printf("FAIL offset=%llu size=%zu iso=%d folder=%d\n",static_cast<unsigned long long>(offset),size,first,second);
    }
}
}

int main(int argc,char **argv) {
    if(argc!=3) return 2;
    vcs::IsoImage iso;
    if(!iso.open(argv[1])) { std::puts(iso.error().c_str()); return 2; }
    std::string error;
    auto folder=FolderDisc::load(argv[2],error);
    if(!folder) { std::puts(error.c_str()); return 2; }
    std::vector<uint8_t> a,b;
    const uint64_t total=uint64_t(iso.totalSectors())*2048u;
    if(folder->totalSectors()!=iso.totalSectors()) ++failures;

    uint64_t files=0;
    for(const auto &[path,entry] : iso.entries()) {
        uint32_t lsn=0,size=0; bool directory=false;
        if(!folder->findFile("cdrom0:\\"+path+";1",lsn,size,directory) || lsn!=entry.lba || size!=entry.size || directory!=entry.isDirectory) ++failures;
        if(entry.isDirectory) {
            std::vector<IPS2DiscImage::DirEntry> list;
            const auto *children=iso.children(path);
            bool same=children && folder->listDirectory(path,list) && list.size()==children->size();
            for(size_t i=0;same && i<list.size();++i) same=list[i].name==(*children)[i].first && list[i].lsn==(*children)[i].second.lba;
            if(!same) ++failures;
            continue;
        }
        ++files;
        const uint64_t start=uint64_t(entry.lba)*2048u, end=start+entry.size;
        compare(iso,*folder,start,std::min<size_t>(entry.size,65536),a,b);       // head of every file
        compare(iso,*folder,end>4096 ? end-4096 : start,8192,a,b);               // across the end and its padding
        compare(iso,*folder,start>1 ? start-1 : start,3,a,b);                    // unaligned across the start
        compare(iso,*folder,end>700 ? end-700 : start,1401,a,b);                 // unaligned, past the end
    }
    std::mt19937_64 random(0x4d3033);
    for(unsigned i=0;i<20000;++i) {
        const size_t size=random()%(1u<<20)+1;
        compare(iso,*folder,random()%(total-size),size,a,b);
    }
    for(uint64_t offset=0;offset<total;offset+=1u<<20)
        compare(iso,*folder,offset,static_cast<size_t>(std::min<uint64_t>(1u<<20,total-offset)),a,b);
    compare(iso,*folder,total-16,64,a,b);                                        // both must fail the same way
    compare(iso,*folder,total+4096,64,a,b);
    std::printf("%s disc compare: files=%llu comparisons=%llu bytes=%llu failures=%llu\n",failures ? "FAIL" : "PASS",
        static_cast<unsigned long long>(files),static_cast<unsigned long long>(compared),static_cast<unsigned long long>(bytes),static_cast<unsigned long long>(failures));
    return failures ? 1 : 0;
}

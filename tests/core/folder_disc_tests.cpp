// Folder-backed disc checks against an in-memory reference; synthetic data only.
#include "../../src/vfs/folder_disc.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <vector>
#if defined(_WIN32)
#include <direct.h>
#else
#include <unistd.h>
#endif

namespace {
constexpr uint32_t kSector=FolderDisc::kSectorSize;
constexpr uint32_t kSectors=96;
constexpr unsigned kManyFiles=12; // more than the open-handle limit

struct Fixture {
    std::string root,manifest;
    std::vector<uint8_t> image,raw;
};

void makeDirectory(const std::string &path) {
#if defined(_WIN32)
    _mkdir(path.c_str());
#else
    mkdir(path.c_str(),0777);
#endif
}

bool writeFile(const std::string &path,const void *data,size_t size) {
    std::FILE *file=std::fopen(path.c_str(),"wb");
    if(!file) return false;
    const bool ok=(size==0 || std::fwrite(data,1,size,file)==size) && std::fclose(file)==0;
    return ok;
}

void fillPattern(uint8_t *out,size_t size,uint32_t seed) {
    for(size_t i=0;i<size;++i) out[i]=static_cast<uint8_t>((i*31u+seed*7u+(i>>8))^seed);
}

// Sectors: 2-3 directories, 4-5 raw data, then files with padding and gaps between them.
bool buildFixture(Fixture &fixture,const std::string &root) {
    fixture.root=root;
    fixture.image.assign(size_t(kSectors)*kSector,0);
    makeDirectory(root); makeDirectory(root+"/.nxdisc"); makeDirectory(root+"/SUB");
    std::string lines="NXDISC\t1\nsectors\t"+std::to_string(kSectors)+"\nvolume\tTEST\n";
    lines+="D\t2\t2048\t-\t\nD\t3\t2048\t-\tSUB\n";
    fillPattern(fixture.image.data()+2*kSector,4*kSector,0x51);
    fixture.raw.assign(fixture.image.begin()+2*kSector,fixture.image.begin()+6*kSector);
    uint32_t lba=8;
    for(unsigned i=0;i<kManyFiles;++i) {
        const uint32_t size=kSector*(i%3u+1u)-(i%2u ? 700u : 0u);
        const std::string name=(i%2u ? "SUB/F" : "F")+std::to_string(i)+".BIN";
        fillPattern(fixture.image.data()+size_t(lba)*kSector,size,i+1);
        if(!writeFile(root+"/"+name,fixture.image.data()+size_t(lba)*kSector,size)) return false;
        lines+="F\t"+std::to_string(lba)+"\t"+std::to_string(size)+"\t-\t"+name+"\n";
        lba+=(size+kSector-1)/kSector+1; // one empty sector between files
    }
    lines+="R\t2\t4\t0\n";
    fixture.manifest=lines;
    return writeFile(root+"/.nxdisc/manifest.tsv",lines.data(),lines.size()) &&
        writeFile(root+"/.nxdisc/raw.bin",fixture.raw.data(),fixture.raw.size());
}

int failures=0;
void expect(bool condition,const char *what) {
    if(!condition) { std::printf("FAIL folder disc: %s\n",what); ++failures; }
}

bool rangeMatches(const FolderDisc &disc,const Fixture &fixture,uint64_t offset,size_t size) {
    std::vector<uint8_t> got(size+1,0xcc);
    return disc.readBytes(offset,size,got.data()) && got[size]==0xcc &&
        std::memcmp(got.data(),fixture.image.data()+offset,size)==0;
}

void checkPaths(const FolderDisc &disc) {
    uint32_t lsn=0,size=0; bool directory=true;
    expect(disc.findFile("cdrom0:\\F0.BIN;1",lsn,size,directory) && lsn==8 && !directory,"device, backslash and version");
    expect(disc.findFile("CDROM0:/f0.bin",lsn,size,directory) && lsn==8,"device and name case");
    expect(disc.findFile("sub\\f1.bin;1",lsn,size,directory) && !directory,"nested file");
    expect(disc.findFile("\\\\SUB//F1.BIN",lsn,size,directory),"repeated separators");
    expect(disc.findFile("SUB",lsn,size,directory) && directory && lsn==3,"directory lookup");
    expect(disc.findFile("",lsn,size,directory) && directory && lsn==2,"root lookup");
    for(const char *bad : {"../F0.BIN","SUB/../F0.BIN","SUB\\..\\..\\F0.BIN","./F0.BIN","F0.BIN;x","F0.BIN;",
                           "sdmc:/F0.BIN","C:\\F0.BIN","F0.BIN;1;1","NOPE.BIN"})
        expect(!disc.findFile(bad,lsn,size,directory),bad);
    std::vector<IPS2DiscImage::DirEntry> children;
    expect(disc.listDirectory("",children) && children.size()==7 && children[0].name=="SUB" &&
        children[0].isDirectory && children[1].name=="F0.BIN","root listing order");
    expect(disc.listDirectory("cdrom0:\\SUB\\",children) && children.size()==6 && children[0].name=="F1.BIN","nested listing");
    expect(!disc.listDirectory("F0.BIN",children),"listing a file");
}

void checkReads(const FolderDisc &disc,const Fixture &fixture) {
    const uint64_t total=fixture.image.size();
    expect(disc.totalSectors()==kSectors,"sector count");
    expect(rangeMatches(disc,fixture,0,fixture.image.size()),"whole image");
    for(uint64_t offset : {uint64_t(0),uint64_t(1),uint64_t(2047),uint64_t(2048),uint64_t(2*kSector-3),uint64_t(6*kSector-1)})
        expect(rangeMatches(disc,fixture,offset,5000),"boundary read");
    for(uint64_t offset=0;offset+kSector<total;offset+=kSector/2+37) // crosses files, padding, gaps and raw data
        expect(rangeMatches(disc,fixture,offset,kSector+311),"unaligned read");
    expect(rangeMatches(disc,fixture,total-1,1),"last byte");
    uint8_t buffer[64];
    std::memset(buffer,0xaa,sizeof(buffer));
    expect(!disc.readBytes(total-16,sizeof(buffer),buffer),"read past the end fails");
    expect(std::memcmp(buffer,fixture.image.data()+total-16,16)==0 && buffer[16]==0 && buffer[63]==0,"partial read is zero filled");
    std::memset(buffer,0xaa,sizeof(buffer));
    expect(!disc.readBytes(total,sizeof(buffer),buffer) && buffer[0]==0,"read at the end");
    expect(!disc.readBytes(~uint64_t(0)-4,sizeof(buffer),buffer),"offset overflow");
    expect(disc.readBytes(total+10,0,buffer),"empty read");
    expect(!disc.readBytes(0,4,nullptr),"null destination");
}

void checkConcurrent(const FolderDisc &disc,const Fixture &fixture) {
    std::atomic<unsigned> wrong{0};
    std::vector<std::thread> threads;
    for(unsigned id=0;id<4;++id) threads.emplace_back([&,id] {
        std::mt19937 random(0x1234u+id);
        std::vector<uint8_t> buffer(3*kSector);
        for(unsigned i=0;i<400;++i) {
            const size_t size=random()%buffer.size()+1;
            const uint64_t offset=random()%(fixture.image.size()-size);
            if(!disc.readBytes(offset,size,buffer.data()) ||
               std::memcmp(buffer.data(),fixture.image.data()+offset,size)!=0) ++wrong;
        }
    });
    for(std::thread &thread : threads) thread.join();
    expect(wrong.load()==0,"concurrent random reads across more files than handles");
}

bool loadFails(const std::string &root,const char *expected) {
    std::string error;
    const auto disc=FolderDisc::load(root,error);
    if(disc || error.find(expected)==std::string::npos) {
        std::printf("FAIL folder disc: expected load error containing '%s', got '%s'\n",expected,error.c_str());
        return false;
    }
    return true;
}

void checkManifest(Fixture &fixture,const std::string &name,const std::string &from,const std::string &to,const char *expected) {
    std::string changed=fixture.manifest;
    const size_t at=changed.find(from);
    expect(at!=std::string::npos,name.c_str());
    if(at==std::string::npos) return;
    changed.replace(at,from.size(),to);
    expect(writeFile(fixture.root+"/.nxdisc/manifest.tsv",changed.data(),changed.size()),"write changed manifest");
    expect(loadFails(fixture.root,expected),name.c_str());
}

void checkRejections(Fixture &fixture) {
    expect(loadFails(fixture.root+"-missing","run tools/extract_disc.py"),"missing root");
    checkManifest(fixture,"version","NXDISC\t1","NXDISC\t2","version");
    checkManifest(fixture,"traversal","SUB/F1.BIN","SUB/../F1.BIN","unsafe path");
    checkManifest(fixture,"lower case","\tF0.BIN","\tf0.BIN","disc form");
    checkManifest(fixture,"past end","F\t8\t","F\t96\t","past the end");
    checkManifest(fixture,"overlap","F\t10\t","F\t8\t","overlap");
    checkManifest(fixture,"bad number","F\t8\t","F\tx\t","invalid extent");
    checkManifest(fixture,"orphan","F\t8\t2048\t-\tF0.BIN","F\t8\t2048\t-\tNODIR/F0.BIN","parent");
    checkManifest(fixture,"duplicate","D\t3\t2048\t-\tSUB\n","D\t3\t2048\t-\tSUB\nD\t3\t2048\t-\tSUB\n","duplicate");
    checkManifest(fixture,"wrong size","F\t8\t2048\t","F\t8\t2047\t","wrong size");
    checkManifest(fixture,"raw offset","R\t2\t4\t0","R\t2\t4\t2048","out of order");
    expect(writeFile(fixture.root+"/.nxdisc/manifest.tsv",fixture.manifest.data(),fixture.manifest.size()),"restore manifest");
    expect(writeFile(fixture.root+"/.nxdisc/raw.bin",fixture.raw.data(),fixture.raw.size()-1),"truncate raw");
    expect(loadFails(fixture.root,"wrong size"),"truncated raw data");
    expect(writeFile(fixture.root+"/.nxdisc/raw.bin",fixture.raw.data(),fixture.raw.size()),"restore raw");
    std::remove((fixture.root+"/F0.BIN").c_str());
    expect(loadFails(fixture.root,"missing game file"),"missing game file");
}
}

int runFolderDiscTests() {
    failures=0;
    Fixture fixture;
    if(!buildFixture(fixture,"folder-disc-fixture")) { std::puts("FAIL folder disc: fixture"); return 1; }
    std::string error;
    auto disc=FolderDisc::load(fixture.root,error);
    if(!disc) { std::printf("FAIL folder disc: load: %s\n",error.c_str()); return 1; }
    checkPaths(*disc); checkReads(*disc,fixture); checkConcurrent(*disc,fixture);
    disc.reset(); // releases open handles so the rejection cases can change files
    checkRejections(fixture);
    if(!failures) std::puts("PASS folder disc: paths, listing, whole-image and unaligned reads, end of disc, 4 readers over 12 files, 13 rejected layouts");
    return failures;
}

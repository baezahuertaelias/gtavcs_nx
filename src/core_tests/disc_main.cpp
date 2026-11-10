// M03 hardware check: the folder disc against the real extracted game tree on the SD card.
// Holds no game data; every byte comes from gamefolder and is compared with its manifest.
#include <switch.h>
#include <atomic>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>
#include "resources.h"
#include "../vfs/folder_disc.h"
#include "../vfs/game_paths.h"

namespace {
constexpr const char *kDeviceRoot="sdmc:/switch/gtavcs_nx/gamefolder";
constexpr size_t kChunk=1u<<20;
using Clock=std::chrono::steady_clock;

int failures=0;
void check(bool condition,const char *what) {
    std::printf("%s %s\n",condition ? "PASS" : "FAIL",what);
    if(!condition) ++failures;
}

double secondsSince(Clock::time_point start) {
    return std::chrono::duration<double>(Clock::now()-start).count();
}

std::string manifestValue(const char *key) {
    std::FILE *file=std::fopen((std::string(kDeviceRoot)+"/.nxdisc/manifest.tsv").c_str(),"r");
    if(!file) return {};
    char line[512]; std::string value;
    const std::string prefix=std::string(key)+"\t";
    while(std::fgets(line,sizeof(line),file)) {
        if(std::strncmp(line,prefix.c_str(),prefix.size())==0) {
            value=line+prefix.size();
            while(!value.empty() && (value.back()=='\n' || value.back()=='\r')) value.pop_back();
            break;
        }
    }
    std::fclose(file);
    return value;
}

void checkPaths() {
    GamePaths paths; std::string error;
    const bool resolved=resolveGamePaths(kDeviceRoot,paths,error);
    check(resolved,"resolve device-prefixed game folder");
    if(!resolved) { std::printf("  %s\n",error.c_str()); return; }
    check(paths.root=="/switch/gtavcs_nx/gamefolder","game folder rooted");
    namespace fs=std::filesystem;
    check(fs::path(paths.mc0).is_absolute(),"rooted path is absolute to std::filesystem");
    check(!fs::path(kDeviceRoot).is_absolute(),"device path is the known relative case");
    const std::string probe=paths.cache+"/path-check.tmp";
    std::error_code code;
    fs::remove(probe,code);
    { std::FILE *file=std::fopen(probe.c_str(),"w"); if(file) { std::fputs("x",file); std::fclose(file); } }
    check(fs::exists(fs::path(probe),code),"std::filesystem finds a rooted file");
    std::FILE *viaDevice=std::fopen((std::string(kDeviceRoot)+"/userdata/cache/path-check.tmp").c_str(),"r");
    check(viaDevice!=nullptr,"same file reached through the device path");
    if(viaDevice) std::fclose(viaDevice);
    fs::remove(probe,code);
    check(!fs::exists(fs::path(kDeviceRoot)/"sdmc:",code),"no cwd-doubled device folder");
}

void checkCase(const FolderDisc &disc) {
    uint32_t lsn=0,size=0; bool directory=false;
    check(disc.findFile("cdrom0:\\system.cnf;1",lsn,size,directory) && !directory,"lowercase request finds the disc file");
    std::FILE *lower=std::fopen((std::string(kDeviceRoot)+"/system.cnf").c_str(),"rb");
    std::printf("[case] lowercase host open of SYSTEM.CNF: %s\n",lower ? "found" : "not found");
    if(lower) std::fclose(lower);
}

void wholeDisc(const FolderDisc &disc) {
    const std::string expected=manifestValue("iso_sha256");
    std::vector<uint8_t> buffer(kChunk);
    Sha256Context context;
    sha256ContextCreate(&context);
    const uint64_t total=uint64_t(disc.totalSectors())*FolderDisc::kSectorSize;
    const auto start=Clock::now();
    bool reads=true;
    for(uint64_t offset=0;offset<total;offset+=kChunk) {
        const size_t count=static_cast<size_t>(std::min<uint64_t>(kChunk,total-offset));
        reads=disc.readBytes(offset,count,buffer.data()) && reads;
        sha256ContextUpdate(&context,buffer.data(),count);
    }
    uint8_t digest[SHA256_HASH_SIZE];
    sha256ContextGetHash(&context,digest);
    char text[SHA256_HASH_SIZE*2+1];
    for(unsigned i=0;i<SHA256_HASH_SIZE;++i) std::snprintf(text+i*2,3,"%02x",digest[i]);
    const double seconds=secondsSince(start);
    std::printf("[whole-disc] bytes=%" PRIu64 " seconds=%.1f MiB/s=%.1f sha256=%s\n",total,seconds,total/1048576.0/seconds,text);
    check(reads,"every sequential read succeeded");
    check(expected.size()==64 && expected==text,"whole disc through FolderDisc equals the manifest digest");
}

void randomReaders(const FolderDisc &disc) {
    std::atomic<uint32_t> wrong{0}; std::atomic<uint64_t> reads{0};
    const uint64_t total=uint64_t(disc.totalSectors())*FolderDisc::kSectorSize;
    std::vector<std::thread> threads;
    const auto start=Clock::now();
    for(unsigned id=0;id<4;++id) threads.emplace_back([&,id] {
        std::mt19937_64 random(0x5eed+id);
        std::vector<uint8_t> first(64u*1024u),second(first.size());
        for(unsigned i=0;i<600;++i) {
            const uint64_t offset=random()%(total-first.size());
            if(!disc.readBytes(offset,first.size(),first.data()) || !disc.readBytes(offset,second.size(),second.data()) ||
               first!=second) ++wrong;
            reads+=2;
        }
    });
    for(std::thread &thread : threads) thread.join();
    std::printf("[random] readers=4 reads=%" PRIu64 " size=65536 seconds=%.1f\n",reads.load(),secondsSince(start));
    check(wrong.load()==0,"concurrent random reads stay consistent");
}

void shutdownDuringReads() {
    std::string error;
    auto disc=FolderDisc::load(kDeviceRoot,error);
    if(!disc) { check(false,"load for shutdown test"); return; }
    std::atomic<bool> stop{false}; std::atomic<uint64_t> reads{0};
    const uint64_t total=uint64_t(disc->totalSectors())*FolderDisc::kSectorSize;
    std::vector<std::thread> threads;
    for(unsigned id=0;id<4;++id) threads.emplace_back([&,id] {
        std::mt19937_64 random(0xabc0+id);
        std::vector<uint8_t> buffer(256u*1024u);
        while(!stop.load(std::memory_order_acquire)) {
            disc->readBytes(random()%(total-buffer.size()),buffer.size(),buffer.data());
            ++reads;
        }
    });
    svcSleepThread(500'000'000);
    const auto start=Clock::now();
    stop.store(true,std::memory_order_release);
    for(std::thread &thread : threads) thread.join();
    disc.reset();
    std::printf("[shutdown] reads=%" PRIu64 " join_and_close_ms=%.1f\n",reads.load(),secondsSince(start)*1000.0);
    check(reads.load()>0 && secondsSince(start)<5.0,"readers stop and the disc closes during pending reads");
}

int run() {
    if(!reportCpuResources("disc-tests-start",4)) ++failures;
    checkPaths();
    const auto start=Clock::now();
    std::string error;
    auto disc=FolderDisc::load(kDeviceRoot,error);
    if(!disc) { std::printf("FAIL load: %s\n",error.c_str()); return failures+1; }
    std::printf("[load] seconds=%.3f sectors=%u mods=%zu warnings=%zu\n",secondsSince(start),disc->totalSectors(),
        disc->modsApplied(),disc->warnings().size());
    checkCase(*disc);
    randomReaders(*disc);
    wholeDisc(*disc);
    disc.reset();
    shutdownDuringReads();
    if(!reportCpuResources("disc-tests-end",4)) ++failures;
    return failures;
}
}

int main() {
    FILE *log=freopen("sdmc:/switch/gtavcs_nx/disc-tests-M03.log","w",stdout);
    if(!log) return EXIT_FAILURE;
    setvbuf(log,nullptr,_IOLBF,0);
    std::printf("[disc-tests] HOS=%08x applet=%d\n",hosversionGet(),appletGetAppletType());
    int result=EXIT_FAILURE;
    if((appletGetAppletType()==AppletType_Application || appletGetAppletType()==AppletType_SystemApplication) &&
       chdir("sdmc:/switch/gtavcs_nx/gamefolder")==0) {
        try { result=run(); }
        catch(const std::exception &error) { std::printf("FAIL disc tests: %s\n",error.what()); result=1; }
        catch(...) { std::puts("FAIL disc tests: unknown exception"); result=1; }
    } else {
        std::puts("FAIL launch preconditions");
    }
    std::printf("[disc-tests] finished failures=%d\n",result);
    std::fclose(stdout);
    return result ? EXIT_FAILURE : EXIT_SUCCESS;
}

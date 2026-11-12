// Folder-backed disc checks against an in-memory reference; synthetic data only.
#include "../../src/vfs/folder_disc.h"
#include "../../src/vfs/game_paths.h"

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
    std::vector<std::string> hashes; // SHA-256 of each Fn.BIN
};

void makeDirectory(const std::string &path) {
#if defined(_WIN32)
    _mkdir(path.c_str());
#else
    mkdir(path.c_str(),0777);
#endif
}

// The mods checkMods creates, removed one by one: std::filesystem::remove_all on a relative path
// silently left them in place on the Switch, so a stale mod changed later reads.
bool removeMods(const std::string &root) {
    const std::string mods=root+"/mods";
    for(const char *file : {"/F0.BIN","/SUB/F1.BIN"}) std::remove((mods+file).c_str());
    for(const char *directory : {"/F2.BIN","/SUB",""}) {
#if defined(_WIN32)
        _rmdir((mods+directory).c_str());
#else
        rmdir((mods+directory).c_str());
#endif
    }
    struct stat info{};
    return stat(mods.c_str(),&info)!=0;
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
    fillPattern(fixture.image.data()+2*kSector,4*kSector,0x51);
    fixture.raw.assign(fixture.image.begin()+2*kSector,fixture.image.begin()+6*kSector);
    std::string lines="NXDISC\t1\nsectors\t"+std::to_string(kSectors)+"\nvolume\tTEST\n";
    lines+="raw_sha256\t"+FolderDisc::sha256Hex(fixture.raw.data(),fixture.raw.size())+"\n";
    lines+="D\t2\t2048\t-\t\nD\t3\t2048\t-\tSUB\n";
    uint32_t lba=8;
    for(unsigned i=0;i<kManyFiles;++i) {
        const uint32_t size=kSector*(i%3u+1u)-(i%2u ? 700u : 0u);
        const std::string name=(i%2u ? "SUB/F" : "F")+std::to_string(i)+".BIN";
        fillPattern(fixture.image.data()+size_t(lba)*kSector,size,i+1);
        if(!writeFile(root+"/"+name,fixture.image.data()+size_t(lba)*kSector,size)) return false;
        fixture.hashes.push_back(FolderDisc::sha256Hex(fixture.image.data()+size_t(lba)*kSector,size));
        lines+="F\t"+std::to_string(lba)+"\t"+std::to_string(size)+"\t"+fixture.hashes.back()+"\t"+name+"\n";
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

void checkMods(const Fixture &fixture) {
    makeDirectory(fixture.root+"/mods"); makeDirectory(fixture.root+"/mods/SUB");
    std::vector<uint8_t> replacement(kSector);
    fillPattern(replacement.data(),replacement.size(),0xee);
    expect(writeFile(fixture.root+"/mods/F0.BIN",replacement.data(),replacement.size()),"write mod");
    expect(writeFile(fixture.root+"/mods/SUB/F1.BIN",replacement.data(),17),"write wrong-size mod");
    makeDirectory(fixture.root+"/mods/F2.BIN");
    std::string error;
    auto disc=FolderDisc::load(fixture.root,error);
    if(!disc) { expect(false,"load with mods"); return; }
    expect(disc->modsApplied()==1,"one mod applied");
    const auto warned=[&](const char *text,const char *path) {
        for(const std::string &warning : disc->warnings())
            if(warning.find(text)!=std::string::npos && warning.find(path)!=std::string::npos) return true;
        return false;
    };
    expect(disc->warnings().size()==2,"two mods refused");
    expect(warned("size differs","mods/SUB/F1.BIN"),"wrong-size mod refused by name");
    expect(warned("not a regular file","mods/F2.BIN"),"directory in place of a mod refused by name");
    std::vector<uint8_t> got(kSector);
    expect(disc->readBytes(8*kSector,kSector,got.data()) && got==replacement,"mod replaces the disc file");
    uint32_t lsn=0,size=0; bool directory=false;
    expect(disc->findFile("SUB/F1.BIN",lsn,size,directory),"refused mod leaves the disc file");
    std::vector<uint8_t> file(size);
    expect(disc->readBytes(uint64_t(lsn)*kSector,size,file.data()) &&
        std::memcmp(file.data(),fixture.image.data()+uint64_t(lsn)*kSector,size)==0,"disc file still served");
    disc.reset();
    expect(removeMods(fixture.root),"mods removed after the mod checks");
}

void checkCorruption(const Fixture &fixture) {
    std::string error;
    auto disc=FolderDisc::load(fixture.root,error);
    uint32_t lsn=0,size=0; bool directory=false;
    if(!disc || !disc->findFile("F2.BIN",lsn,size,directory)) { expect(false,"load for corruption"); return; }
    const uint8_t *original=fixture.image.data()+uint64_t(lsn)*kSector;
    expect(writeFile(fixture.root+"/F2.BIN",original,size/2),"shorten file after load");
    std::vector<uint8_t> got(size,0xaa);
    expect(!disc->readBytes(uint64_t(lsn)*kSector,size,got.data()),"short file read fails");
    expect(got[0]==0 && got[size-1]==0,"failed read is zero filled");
    disc.reset();
    expect(writeFile(fixture.root+"/F2.BIN",original,size),"restore file");
}

bool flipByte(const std::string &path,long offset) {
    std::FILE *file=std::fopen(path.c_str(),"r+b");
    if(!file) return false;
    int value=-1;
    const bool ok=std::fseek(file,offset,SEEK_SET)==0 && (value=std::fgetc(file))!=EOF &&
        std::fseek(file,offset,SEEK_SET)==0 && std::fputc(value^0x01,file)!=EOF;
    return std::fclose(file)==0 && ok;
}

bool reported(const std::vector<std::string> &problems,const char *text,const char *path) {
    return problems.size()==1 && problems[0].find(text)!=std::string::npos && problems[0].find(path)!=std::string::npos;
}

// Load checks sizes only; verifyFiles finds a same-size corrupt file by its manifest SHA-256.
void checkVerify(const Fixture &fixture) {
    expect(FolderDisc::sha256Hex("",0)=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","SHA-256 of nothing");
    expect(FolderDisc::sha256Hex("abc",3)=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","SHA-256 of abc");
    const char *twoBlocks="abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    expect(FolderDisc::sha256Hex(twoBlocks,std::strlen(twoBlocks))=="248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1","SHA-256 padding into a second block");
    const std::string million(1000000,'a');
    expect(FolderDisc::sha256Hex(million.data(),million.size())=="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0","SHA-256 of a million bytes");
    std::string error;
    auto disc=FolderDisc::load(fixture.root,error);
    if(!disc) { expect(false,"load for verify"); return; }
    expect(disc->verifyFiles().empty(),"intact tree verifies");
    disc.reset();

    expect(flipByte(fixture.root+"/F2.BIN",100),"flip one byte of a file");
    disc=FolderDisc::load(fixture.root,error);
    expect(disc!=nullptr,"same-size corrupt file passes the size check at load");
    if(disc) expect(reported(disc->verifyFiles(),"corrupt game file","/F2.BIN"),"same-size corrupt file named by verify");
    disc.reset();
    expect(flipByte(fixture.root+"/F2.BIN",100),"restore the flipped byte");

    expect(flipByte(fixture.root+"/.nxdisc/raw.bin",5),"flip one byte of raw.bin");
    disc=FolderDisc::load(fixture.root,error);
    if(disc) expect(reported(disc->verifyFiles(),"corrupt raw sectors","raw.bin"),"corrupt raw sectors named by verify");
    else expect(false,"load with corrupt raw sectors");
    disc.reset();
    expect(flipByte(fixture.root+"/.nxdisc/raw.bin",5),"restore raw.bin");

    // A mod is meant to differ from the disc; verify hashes the disc copy under it.
    makeDirectory(fixture.root+"/mods");
    std::vector<uint8_t> replacement(kSector);
    fillPattern(replacement.data(),replacement.size(),0xee);
    expect(writeFile(fixture.root+"/mods/F0.BIN",replacement.data(),replacement.size()),"write mod for verify");
    disc=FolderDisc::load(fixture.root,error);
    if(disc) expect(disc->modsApplied()==1 && disc->verifyFiles().empty(),"mod does not fail verify");
    else expect(false,"load with mod for verify");
    disc.reset();
    expect(removeMods(fixture.root),"mods removed after verify");

    std::string changed=fixture.manifest;
    const std::string line="\t"+fixture.hashes[0]+"\tF0.BIN\n";
    const size_t at=changed.find(line);
    expect(at!=std::string::npos,"F0.BIN manifest line");
    if(at!=std::string::npos) {
        changed.replace(at,line.size(),"\t-\tF0.BIN\n");
        expect(writeFile(fixture.root+"/.nxdisc/manifest.tsv",changed.data(),changed.size()),"write manifest without a hash");
        disc=FolderDisc::load(fixture.root,error);
        if(disc) expect(reported(disc->verifyFiles(),"no recorded SHA-256","/F0.BIN"),"file without a hash named by verify");
        else expect(false,"load without a hash");
        disc.reset();
    }
    expect(writeFile(fixture.root+"/.nxdisc/manifest.tsv",fixture.manifest.data(),fixture.manifest.size()),"restore manifest after verify");
}

// Zero-length files hold no sectors: one inside the raw span and one sharing F0.BIN's sector, in either order.
void checkEmptyFiles(const Fixture &fixture) {
    expect(writeFile(fixture.root+"/EMPTY.BIN",nullptr,0) && writeFile(fixture.root+"/EMPTY2.BIN",nullptr,0),"write empty files");
    const std::string first="D\t3\t2048\t-\tSUB\n";
    const std::string empty="F\t4\t0\t-\tEMPTY.BIN\nF\t8\t0\t-\tEMPTY2.BIN\n";
    for(int order=0;order<2;++order) {
        std::string changed=fixture.manifest;
        const size_t at=order ? changed.find("R\t") : changed.find(first)+first.size();
        changed.insert(at,empty);
        expect(writeFile(fixture.root+"/.nxdisc/manifest.tsv",changed.data(),changed.size()),"write manifest with empty files");
        std::string error;
        auto disc=FolderDisc::load(fixture.root,error);
        if(!disc) { std::printf("FAIL folder disc: empty files order %d: %s\n",order,error.c_str()); ++failures; continue; }
        uint32_t lsn=0,size=1; bool directory=true;
        expect(disc->findFile("EMPTY2.BIN",lsn,size,directory) && lsn==8 && size==0 && !directory,"empty file found");
        expect(rangeMatches(*disc,fixture,8*kSector,kSector) && rangeMatches(*disc,fixture,2*kSector,4*kSector),
               "empty files hide no sectors");
    }
    expect(writeFile(fixture.root+"/.nxdisc/manifest.tsv",fixture.manifest.data(),fixture.manifest.size()),"restore manifest");
    std::remove((fixture.root+"/EMPTY.BIN").c_str());
    std::remove((fixture.root+"/EMPTY2.BIN").c_str());
}

void checkGamePaths(const Fixture &fixture) {
    std::string out;
    expect(rootedGamePath("sdmc:/switch/gtavcs_nx/gamefolder",out) && out=="/switch/gtavcs_nx/gamefolder","device stripped");
    expect(rootedGamePath("/switch/x/",out) && out=="/switch/x","rooted path kept");
    expect(rootedGamePath("game/data",out) && out=="game/data","relative path kept");
    for(const char *bad : {"","sdmc:x","sdmc:","romfs:/x","C:/x","/a/../b","a\\b","../x"})
        expect(!rootedGamePath(bad,out),bad);
    GamePaths paths; std::string error;
    expect(!resolveGamePaths(fixture.root+"-missing",paths,error) && error.find("does not exist")!=std::string::npos,"missing game folder is fatal");
    expect(resolveGamePaths(fixture.root,paths,error),"resolve game folder");
    expect(paths.userdata==fixture.root+"/userdata" && paths.mc0==fixture.root+"/userdata/mc0" &&
        paths.settings==fixture.root+"/userdata/vcs.ini" && paths.elf==fixture.root+"/SLUS_215.90","layout");
    struct stat info{};
    expect(stat(paths.mc1.c_str(),&info)==0 && S_ISDIR(info.st_mode) && stat(paths.cache.c_str(),&info)==0 &&
        paths.backups==fixture.root+"/userdata/backups" && stat(paths.backups.c_str(),&info)==0,"userdata folders created");
    expect(resolveGamePaths(fixture.root,paths,error),"resolve is repeatable");
    GamePaths test;
    expect(resolveGamePaths(fixture.root,test,error,"m03-unit"),"resolve a test profile");
    const std::string profile=fixture.root+"/userdata/tests/m03-unit";
    expect(test.userdata==profile && test.mc0==profile+"/mc0" && test.mc1==profile+"/mc1" && test.cache==profile+"/cache" &&
        test.settings==profile+"/vcs.ini" && test.backups==profile+"/backups" && test.elf==paths.elf,"test profile isolates every save path");
    expect(stat(test.mc0.c_str(),&info)==0 && stat(test.backups.c_str(),&info)==0,"test profile folders created");
    for(const char *bad : {"../x","a/b","a b","sdmc:x"})
        expect(!resolveGamePaths(fixture.root,test,error,bad) && error.find("test profile")!=std::string::npos,bad);
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
    checkManifest(fixture,"orphan","\tF0.BIN\n","\tNODIR/F0.BIN\n","parent");
    checkManifest(fixture,"duplicate","D\t3\t2048\t-\tSUB\n","D\t3\t2048\t-\tSUB\nD\t3\t2048\t-\tSUB\n","duplicate");
    checkManifest(fixture,"wrong size","F\t8\t2048\t","F\t8\t2047\t","wrong size");
    checkManifest(fixture,"raw offset","R\t2\t4\t0","R\t2\t4\t2048","out of order");
    checkManifest(fixture,"wrapping file extent","F\t8\t","F\t18446744073709551615\t","past the end");
    checkManifest(fixture,"wrapping raw extent","R\t2\t4\t0","R\t18446744073709551615\t4\t0","invalid raw extent");
    checkManifest(fixture,"second sector count","R\t2\t4\t0","R\t2\t4\t0\nsectors\t10","duplicate sector count");
    checkManifest(fixture,"bad file hash",fixture.hashes[0]+"\tF0.BIN","x"+fixture.hashes[0].substr(1)+"\tF0.BIN","invalid file hash");
    checkManifest(fixture,"bad raw hash","raw_sha256\t","raw_sha256\tA","invalid raw hash");
    checkManifest(fixture,"second raw hash","R\t2\t4\t0","R\t2\t4\t0\nraw_sha256\t"+fixture.hashes[0],"duplicate raw hash");
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
    if(!removeMods("folder-disc-fixture") || !buildFixture(fixture,"folder-disc-fixture")) {
        std::puts("FAIL folder disc: fixture");
        return 1;
    }
    std::string error;
    auto disc=FolderDisc::load(fixture.root,error);
    if(!disc) { std::printf("FAIL folder disc: load: %s\n",error.c_str()); return 1; }
    checkPaths(*disc); checkReads(*disc,fixture); checkConcurrent(*disc,fixture);
    disc.reset(); // releases open handles so the later cases can change files
    checkMods(fixture); checkCorruption(fixture); checkVerify(fixture); checkEmptyFiles(fixture); checkGamePaths(fixture);
    checkRejections(fixture);
    if(!failures) std::puts("PASS folder disc: paths, listing, whole-image and unaligned reads, end of disc, 4 readers over 12 files, mods, corrupted file, SHA-256 verify, empty files, game paths, 19 rejected layouts");
    return failures;
}

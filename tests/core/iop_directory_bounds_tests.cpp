// Public boundary tests call the exact helper used by the CDVD directory writer.
// Large cursor values are synthetic; no huge allocation, files or ISO are used.
#include "ps2x/iop/detail/cdvd_directory.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <limits>

int runIopDirectoryBoundsTests() {
    namespace dir=ps2x::iop::detail::cdvd_directory;
    unsigned checks=0;
    const auto expect=[&](bool condition,const char *name) {
        ++checks; if(!condition) std::printf("FAIL IOP directory: %s\n",name); return condition;
    };
    size_t record=1234;
    if(!expect(dir::recordSize(1,record) && record==34,"special record size") ||
       !expect(dir::recordSize(202,record) && record==236,"maximum normalized filename") ||
       !expect(dir::recordSize(221,record) && record==254,"maximum encodable record") ||
       !expect(!dir::recordSize(222,record),"padded record cannot overflow its length byte") ||
       !expect(!dir::recordSize(0,record),"empty identifier rejected") ||
       !expect(!dir::recordSize(std::numeric_limits<size_t>::max(),record),"identifier addition cannot wrap")) return 1;
    uint64_t cursor=0;
    uint32_t bytes=0;
    if(!expect(dir::advanceCursor(cursor,1) && cursor==34,"self directory record") ||
       !expect(dir::advanceCursor(cursor,1) && cursor==68,"parent directory record") ||
       !expect(dir::roundedBytes(cursor,bytes) && bytes==2048,"ordinary minimum sector") ||
       !expect(dir::roundedBytes(0,bytes) && bytes==2048,"empty sizing preserves minimum")) return 2;
    cursor=1956;
    if(!expect(dir::advanceCursor(cursor,202) && cursor==2284,"record skips sector tail") ||
       !expect(dir::roundedBytes(cursor,bytes) && bytes==4096,"ordinary two-sector size")) return 3;
    cursor=dir::MaximumDirectoryBytes-34;
    if(!expect(dir::advanceCursor(cursor,1) && cursor==0xfffff800ull,"last representable aligned directory") ||
       !expect(dir::roundedBytes(cursor,bytes) && bytes==0xfffff800u,"maximum encoded byte count") ||
       !expect(!dir::advanceCursor(cursor,1) && cursor==0xfffff800ull,"overflow rejected without cursor change")) return 4;
    cursor=dir::MaximumDirectoryBytes-33;
    if(!expect(!dir::advanceCursor(cursor,1) && cursor==dir::MaximumDirectoryBytes-33,"padding cannot overflow extent")) return 5;
    bytes=0x12345678;
    if(!expect(!dir::roundedBytes(0x100000000ull,bytes) && bytes==0x12345678,"4 GiB cannot wrap to 2048") ||
       !expect(!dir::roundedBytes(std::numeric_limits<uint64_t>::max(),bytes),"wide rounded input cannot wrap")) return 6;
    cursor=std::numeric_limits<uint64_t>::max();
    if(!expect(!dir::advanceCursor(cursor,202) && cursor==std::numeric_limits<uint64_t>::max(),"wide advance input cannot wrap")) return 7;
    uint32_t lsn=20;
    if(!expect(dir::advanceLsn(lsn,2) && lsn==22,"ordinary sector numbering") ||
       !expect(dir::advanceLsn(lsn,0) && lsn==22,"empty file does not shift sectors")) return 8;
    lsn=0xfffffffeu;
    if(!expect(dir::advanceLsn(lsn,1) && lsn==0xffffffffu,"last representable volume sector count") ||
       !expect(!dir::advanceLsn(lsn,1) && lsn==0xffffffffu,"volume sector overflow rejected")) return 9;
    bytes=0;
    if(!expect(dir::fileBytes(0,bytes) && bytes==0,"empty file size preserved") ||
       !expect(dir::fileBytes(0xffffffffull,bytes) && bytes==0xffffffffu,"maximum encoded file extent") ||
       !expect(!dir::fileBytes(0x100000000ull,bytes) && bytes==0xffffffffu,"file extent overflow rejected without truncation")) return 15;

    const uint8_t special=0;
    std::array<uint8_t,64> guarded{}; guarded.fill(0xa5);
    const auto before=guarded;
    if(!expect(dir::writeRecord(guarded.data()+8,33,0x12345678,0x9abcdef0,true,&special,1)==0 && guarded==before,
               "short destination rejected without any write") ||
       !expect(dir::writeRecord(nullptr,34,0,0,true,&special,1)==0,"null destination rejected") ||
       !expect(dir::writeRecord(guarded.data()+8,34,0,0,true,nullptr,1)==0 && guarded==before,"null identifier rejected without write")) return 10;
    if(!expect(dir::writeRecord(guarded.data()+8,34,0x12345678,0x9abcdef0,true,&special,1)==34,"exact destination capacity accepted")) return 11;
    const std::array<uint8_t,34> expected{
        34,0,0x78,0x56,0x34,0x12,0x12,0x34,0x56,0x78,
        0xf0,0xde,0xbc,0x9a,0x9a,0xbc,0xde,0xf0,
        0,0,0,0,0,0,0,2,0,0,1,0,0,1,1,0};
    if(!expect(std::memcmp(guarded.data()+8,expected.data(),expected.size())==0,"normal record byte layout is unchanged") ||
       !expect(guarded[7]==0xa5 && guarded[42]==0xa5,"exact record preserves adjacent canaries")) return 12;
    std::array<uint8_t,221> identifier{};
    for(unsigned i=0;i<identifier.size();++i) identifier[i]=uint8_t(i);
    std::array<uint8_t,300> maximum{}; maximum.fill(0x5a);
    const auto untouched=maximum;
    if(!expect(dir::writeRecord(maximum.data()+1,253,0,0,false,identifier.data(),221)==0 && maximum==untouched,
               "maximum identifier still checks remaining capacity") ||
       !expect(dir::writeRecord(maximum.data()+1,299,0,0,false,identifier.data(),222)==0 && maximum==untouched,
               "unencodable record rejected without write") ||
       !expect(dir::writeRecord(maximum.data()+1,299,0,0,false,identifier.data(),std::numeric_limits<size_t>::max())==0 && maximum==untouched,
               "huge identifier rejected before reading it")) return 13;
    if(!expect(dir::writeRecord(maximum.data()+1,254,0,0,false,identifier.data(),221)==254,"maximum record accepted at exact capacity") ||
       !expect(maximum[1]==254 && maximum[33]==221 && std::memcmp(maximum.data()+34,identifier.data(),221)==0,
               "maximum identifier encoded intact") ||
       !expect(maximum[0]==0x5a && maximum[255]==0x5a,"maximum record canaries remain intact")) return 14;
    std::printf("PASS IOP directory bounds: %u checked sizing/encoding/capacity/overflow cases; no large allocations\n",checks);
    return 0;
}

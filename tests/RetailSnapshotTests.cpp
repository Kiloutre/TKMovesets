#include "Extractor_t8/RetailSnapshot.hpp"
#include "Extractor_t8/RetailGameData.hpp"
#include "AnimExtractors/AnimExtractor_t8.hpp"
#include "Editor_t8Retail/RetailEditorData.hpp"
#include <atomic>
#include <tuple>
#include "Extractor_t8/Extractor_t8.hpp"
#include "MovesetFile.hpp"
#include "FileIO.hpp"
#include "GameIDs.hpp"
#include "Extractor_t8/RetailRuntime.hpp"
#include "MovesetLimits.hpp"
#include "Importer_t8/RetailImport.hpp"
#include "Importer_t8/Importer_t8.hpp"
#include "Importer_t8/RetailImportShadow.hpp"
#include "Importer_t8/RetailAnimationArchive.hpp"
#include "Importer_t8/RetailResourceLoader.hpp"
#include "Importer_t8/RetailPreview.hpp"
#include "Importer_t8/RetailLiveEdit.hpp"
#include "Editor_t7/Editor_t7_LiveRetail.hpp"
#include "Extractor_t8/Extractor_t8cnt.hpp"
#include "AnimExtractors/AnimExtractor_t8.hpp"
#include "Compression.hpp"
#include "T7ToRetailT8.hpp"
#include "T7AnimToRetailT8.hpp"
#include "T7HandToRetailT8.hpp"
#include "RetailNames.hpp"
#include "Editor_t8Retail/Editor_t8Retail.hpp"
#include "RetailPanm.hpp"
#include <bit>
#include <iostream>
#include <cstring>
#include <filesystem>
#include <map>
#include <stdexcept>

std::vector<Byte> T7RetailConvertibleFixture();
namespace {
namespace R = RetailT8Snapshot;
GameInfo RetailInfo() {
    GameInfo info{}; info.name = "Tekken 8"; info.gameId = GameId_T8;
    info.minorVersion = GameVersions::T8::RETAIL; info.movesetNamePrefix = "T8_";
    info.dataString = info.minorDataString = "t8-retail"; info.ptrSize = 8; info.characterCount = 2;
    return info;
}
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
// The game objects our animation entries point at, read from the shipped game_addresses.ini (t8-retail).
RetailT8Resources::EngineAddresses ShippedEngine(uint64_t module) {
    GameAddressesFile file; file.Reload();
    GameAddressesWrapper game; game.addrFile=&file; game.gameKey="t8-retail"; game.minorGameKey="t8-retail";
    return RetailT8Resources::ReadEngineAddresses(game,module);
}
// The Memory fixture's player layout (matches t8-retail_motbin_offset / chara_id_offset).
constexpr RetailT8Runtime::PlayerOffsets FixtureOffsets{0x3b50, 0x168};
class Memory : public GameProcess {
public:
    std::map<gameAddr, std::vector<Byte>> regions;
    mutable std::map<gameAddr, size_t> reads;
    gameAddr unavailable = 0;
    bool changing = false;
    size_t writes = 0, failWrite = 0, pauses = 0, resumes = 0;
    bool failPause = false, failRollback = false, changeAtPause = false, changeRoundAtPause = false, corruptUpload = false;
    gameAddr roundNodeAddress = 0;
    gameAddr allocation = 0x60000000000ULL;
    gameAddr Alloc(size_t size) { auto a = allocation; allocation += 0x10000; regions[a].resize(size); return a; }
    template<class T> void Put(gameAddr address, T value) {
        auto it = regions.upper_bound(address);
        if (it != regions.begin()) {
            --it;
            if (address - it->first <= it->second.size() && sizeof(T) <= it->second.size() - (address - it->first)) {
                memcpy(it->second.data() + address - it->first, &value, sizeof(value)); return;
            }
        }
        auto& bytes = regions[address]; bytes.resize(sizeof(value)); memcpy(bytes.data(), &value, sizeof(value));
    }
    gameAddr Map(gameAddr object, size_t offset, uint32_t key, bool hashed = true) {
        auto head = Alloc(32), buckets = Alloc(128), node = Alloc(128);
        for (size_t i = 0; i < 16; ++i) Put<uint64_t>(buckets + i * 8, head);
        auto bucket = buckets + ((hashed ? RetailT8Runtime::Hash(key) : key) & 7) * 16;
        Put<uint64_t>(bucket, node); Put<uint64_t>(bucket + 8, node);
        Put<uint64_t>(head + 8, node); Put<uint64_t>(node + 8, head); Put<uint32_t>(node + 0x10, key);
        Put<uint64_t>(object + offset, head); Put<uint64_t>(object + offset + 8, 1);
        Put<uint64_t>(object + offset + 0x10, buckets); Put<uint64_t>(object + offset + 0x28, 7);
        return node;
    }
    gameAddr manager = 0, container = 0, panm = 0, player = 0, slotNode = 0, playerVector = 0;
    void Runtime(uint32_t resourceCharacter = 28) {
        Put<uint64_t>(Pointer(12) + 0x50, 0xef001c0012345678ULL);
        manager = Alloc(0x2100); container = Alloc(0x80);
        auto character = Map(manager, 0x2090, resourceCharacter);
        slotNode = Map(character, 0x20, 0x100001);
        Put<uint64_t>(slotNode + 0x18, 0x12345678);
        auto resources = Map(container, 8, resourceCharacter), vector = Alloc(8), idmap = Alloc(0x80);
        Put<uint64_t>(resources + 0x18, vector); Put<uint64_t>(resources + 0x20, vector + 8); Put<uint64_t>(vector, idmap);
        auto instance = Map(idmap, 0x10, 0x12345678), entry = Alloc(32), resource = Alloc(0x1b0);
        Put<uint64_t>(instance + 0x18, entry); Put<uint64_t>(entry + 8, resource);
        panm = Alloc(128); Put<uint64_t>(resource + 0x1a0, panm);
        Put<uint32_t>(panm, 32); Put<uint32_t>(panm + 4, 0x4d4e4150);
        Put<uint16_t>(panm + 16, 12); Put<uint16_t>(panm + 18, 8); Put<uint16_t>(panm + 26, 4);
        Put<int32_t>(panm + 32, 16); Put<uint32_t>(panm + 36, 28);
        Put<uint16_t>(panm + 44, 18); Put<uint16_t>(panm + 46, 16);
        Put<uint16_t>(panm + 58, 4); Put<uint16_t>(panm + 60, 8);
        Put<int32_t>(panm + 64, 20); Put<uint32_t>(panm + 68, 96); Put<uint32_t>(panm + 72, 32);
        mainModule.address = Alloc(0x1000);
        Put<uint32_t>(mainModule.address + 0x3c, 0x80); Put<uint32_t>(mainModule.address + 0x88, 0x6adb1ad5);
        auto registry = Alloc(0x40), map = Alloc(0x140), round = Alloc(32), list = Alloc(32), entries = Alloc(24), entryPlayer = Alloc(0x4000);
        playerVector = entries;
        Put<uint64_t>(mainModule.address + 0x959a320, registry); Put<uint32_t>(mainModule.address + 0x959a088, 0x1234);
        Put<uint64_t>(registry + 0x10, map); auto roundNode = Map(map, 0x100, 0x1234, false);
        roundNodeAddress = roundNode;
        Put<uint64_t>(roundNode + 0x18, round); Put<uint64_t>(round, list);
        Put<uint64_t>(list + 8, entries); Put<uint64_t>(list + 0x10, entries + 24); Put<uint64_t>(entries, entryPlayer);
        player = entryPlayer + 0x90; Put<uint32_t>(player + 0x168, 28); Put<uint64_t>(player + 0x3b50, Root);
        auto holder = Alloc(32); Put<uint64_t>(mainModule.address + 0x98d0c40, holder); Put<uint64_t>(holder + 8, manager);
        auto binderHolder = Alloc(8), binder = Alloc(32);
        Put<uint64_t>(mainModule.address + 0x9bd9de8, binderHolder); Put<uint64_t>(binderHolder, binder); Put<uint64_t>(binder + 0x10, container);
    }
    static constexpr gameAddr Root = 0x880000000ULL;
    Memory() {
        auto& header = regions[Root]; header.resize(R::SourceHeaderSize);
        uint32_t date = 20240719; memcpy(header.data() + 4, &date, 4);
        const uint64_t key = 0xfeedfacecafebeef;
        memcpy(header.data() + 0x170, &key, 8);
        for (size_t i = 0; i < R::Tables.size(); ++i) {
            const auto spec = R::Tables[i];
            const uint64_t address = 0x10000000ULL + (19 - i) * 0x10000000ULL, count = 1;
            regions[address].assign(spec.stride, static_cast<Byte>(i + 1));
            memcpy(header.data() + spec.pointerOffset, &address, 8);
            memcpy(header.data() + spec.countOffset, &count, 8);
        }
    }
    uint64_t Pointer(size_t table) const {
        uint64_t result; memcpy(&result, regions.at(Root).data() + R::Tables[table].pointerOffset, 8); return result;
    }
protected:
    bool WriteMemoryRaw(gameAddr address, const void* data, size_t size) override {
        ++writes;
        auto it = regions.upper_bound(address); if (it == regions.begin()) return false; --it;
        if (address - it->first > it->second.size() || size > it->second.size() - (address - it->first)) return false;
        if (writes == failWrite || (failRollback && failWrite && writes > failWrite)) {
            if (size) memcpy(it->second.data() + address - it->first, data, (size + 1) / 2);
            return false;
        }
        memcpy(it->second.data() + address - it->first, data, size);
        if (corruptUpload && writes == 1 && size) it->second[address - it->first] ^= 1;
        return true;
    }
    gameAddr AllocateMemoryRaw(size_t size, bool) override { return Alloc(size); }
    bool ReleaseMemoryRaw(gameAddr address) override { return regions.erase(address) != 0; }
    bool SuspendProcessRaw() override {
        ++pauses;
        if (changeAtPause) Put<uint64_t>(player + 0x3b50, Root + 8);
        if (changeRoundAtPause) Put<uint64_t>(roundNodeAddress + 0x18, 0xdeadbeef);
        return !failPause;
    }
    bool ResumeProcessRaw() override { ++resumes; return true; }
    bool ReadMemoryRaw(gameAddr address, void* data, size_t size) const override {
        ++reads[address];
        auto it = regions.upper_bound(address); if (it == regions.begin()) return false; --it;
        if (it->first == unavailable || address - it->first > it->second.size() ||
            size > it->second.size() - (address - it->first)) return false;
        memcpy(data, it->second.data() + address - it->first, size);
        if (changing && address == Root && reads.at(Root) > 1) static_cast<Byte*>(data)[4] ^= 1;
        return true;
    }
};
template<class Change> void Reject(Change change) {
    Memory memory; change(memory); auto original = memory.regions;
    bool failed = false;
    try { R::Capture(memory, Memory::Root); } catch (const std::exception&) { failed = true; }
    Check(failed && original == memory.regions, "Malformed retail capture accepted or source changed");
}
class Exporter : public ExtractorT8 {
    std::string directory;
public:
    Exporter(GameProcess& process, GameData& game, const GameInfo* info, const std::filesystem::path& path)
        : ExtractorT8(process, game, info), directory(path.string()) { cm_extractionDir = directory.c_str(); }
};
}
namespace {
// Memory-only registration: domain maps, bucket-only publication, content ids, teardown reachability.
void RunMemoryOnlyRegistrationTests() {
    namespace RT=RetailT8Runtime;namespace RS=RetailT8Resources;
    Memory m;m.Runtime(28);
    const auto map=RT::DomainMap(m,m.container,28);
    Check(map && !RT::DomainMap(m,m.container,0x81) && RT::MapNode(m,map,0x12345678),"Domain map helpers changed");
    const auto native=RT::MapNode(m,map,0x12345678);
    m.Put<uint64_t>(m.readUInt64(map+0x10),native);m.Put<uint64_t>(native,m.readUInt64(map+0x10)); // the fixture links prev only
    const auto nativeBucket=RT::Bucket(m,map,0x12345678);
    Check(nativeBucket.lo==native && nativeBucket.hi==native && !nativeBucket.Empty(),"Bucket helper changed");
    RetailPanm::Animation animation{2,{{"Top",std::vector<RetailPanm::Sample>(2)},{"Hip",std::vector<RetailPanm::Sample>(1)}}};
    const auto panm=RetailPanm::Encode(animation);
    const auto engine=ShippedEngine(m.mainModule.address);
    // Unpopulated maps are refused; then mark it populated like the engine (map+0x48).
    {
        auto base=m.Alloc(RS::BuildRegisteredAnimation(panm,0,engine,1).image.size());
        auto entry=RS::BuildRegisteredAnimation(panm,base,engine,0x80000001);
        m.writeBytes(base,entry.image.data(),entry.image.size());
        bool refused=false;try{RS::PublishAnimations(m,map,std::vector<RS::PublishedAnimation>{{0x80000001,entry.node}});}catch(const std::exception&){refused=true;}
        Check(refused,"Published into an unpopulated map");
    }
    m.Put<uint8_t>(map+0x48,1);
    // Ids: one sharing the native node's bucket, two sharing an empty bucket.
    std::vector<uint32_t> ids;
    for(uint32_t id=0x80000000;ids.size()<1;++id)if(RT::Bucket(m,map,id).address==nativeBucket.address)ids.push_back(id);
    uint64_t emptyBucket=0;
    for(uint32_t id=0x80000000;ids.size()<3;++id){
        auto bucket=RT::Bucket(m,map,id);if(!bucket.Empty())continue;
        if(!emptyBucket)emptyBucket=bucket.address;
        if(bucket.address==emptyBucket)ids.push_back(id);
    }
    std::vector<RS::PublishedAnimation> published;std::vector<RS::RegisteredAnimation> entries;
    for(auto id:ids){
        auto size=RS::BuildRegisteredAnimation(panm,0,engine,id).image.size();
        auto base=m.Alloc(size);auto entry=RS::BuildRegisteredAnimation(panm,base,engine,id);
        Check(entry.image.size()==size && m.writeBytes(base,entry.image.data(),entry.image.size()),"Entry upload failed");
        published.push_back({id,entry.node});entries.push_back(std::move(entry));
    }
    const auto listBefore=[&]{std::vector<gameAddr> nodes;auto head=m.readUInt64(map+0x10);for(auto n=m.readUInt64(head);n!=head;n=m.readUInt64(n))nodes.push_back(n);return nodes;}();
    Check(RS::PublishAnimations(m,map,published)==3,"Publication count changed");
    for(size_t i=0;i<ids.size();++i){
        Check(RT::MapNode(m,map,ids[i])==entries[i].node,"Published entry is not found by the readers' walk");
        Check(RT::ResolveAnimation(m,m.container,28,ids[i])==entries[i].resource,"Published entry does not resolve to its resource");
        const auto copy=RT::Panm(m,m.container,28,ids[i],0,panm.size());
        std::vector<Byte> bytes(panm.size());m.readBytes(copy,bytes.data(),bytes.size());
        Check(bytes==panm,"Published entry does not expose its PANM");
        const auto resource=entries[i].resource;
        Check(m.readUInt64(resource)==engine.animResourceVtable && m.readUInt64(resource+0x190)==entries[i].object &&
            m.readUInt32(m.readUInt64(resource+0x140)+0x30)==2,"Registered resource fields changed");
        const auto ctrl=m.readUInt64(resource+0x198);
        Check(m.readUInt32(ctrl+8)==0x40000000 && m.readUInt32(ctrl+0xc)==0x40000000 && m.readUInt64(m.readUInt64(ctrl)+8)==engine.noOp,
            "Control block can reach zero or calls game code");
    }
    Check(RT::MapNode(m,map,0x12345678)==native,"Native entry lost after publication");
    // The list (teardown, rehash, readiness walks) is untouched: our nodes are never reachable by next.
    const auto listAfter=[&]{std::vector<gameAddr> nodes;auto head=m.readUInt64(map+0x10);for(auto n=m.readUInt64(head);n!=head;n=m.readUInt64(n))nodes.push_back(n);return nodes;}();
    Check(listAfter==listBefore && m.readUInt64(map+0x18)==1,"Publication changed the engine's node list or size");
    bool duplicate=false;try{RS::PublishAnimations(m,map,std::vector<RS::PublishedAnimation>{published[0]});}catch(const std::exception&){duplicate=true;}
    Check(duplicate,"Duplicate id accepted");
    // Content ids: a fresh id for new bytes, the earlier entry for identical bytes.
    auto chosen=RS::ChooseAnimationId(m,m.container,28,panm,engine);
    Check(chosen.id&0x80000000U,"Content id lost its marker bit");
    const auto stored=RS::ChooseAnimationId(m,m.container,28,panm,engine,{});
    Check(stored.id==chosen.id,"Content id is not deterministic");
    {
        auto base=m.Alloc(RS::BuildRegisteredAnimation(panm,0,engine,chosen.id).image.size());
        auto entry=RS::BuildRegisteredAnimation(panm,base,engine,chosen.id);
        m.writeBytes(base,entry.image.data(),entry.image.size());
        RS::PublishAnimations(m,map,std::vector<RS::PublishedAnimation>{{chosen.id,entry.node}});
    }
    if(!chosen.registered){
        auto again=RS::ChooseAnimationId(m,m.container,28,panm,engine);
        Check(again.registered && again.id==chosen.id,"Identical PANM was not reused");
        auto other=panm;other[other.size()-1]^=1;
        auto different=RS::ChooseAnimationId(m,m.container,28,other,engine,{again.id});
        Check(!different.registered && different.id!=again.id,"Different PANM reused an entry");
        Check(RS::ChooseAnimationId(m,m.container,28,panm,engine,{again.id}).id!=again.id,"Taken id was reused");
    }
}
// Converted T7 hand animations: the anim manager's category index (bucket-only nodes) and the hand property rows.
void RunCategoryIndexTests() {
    namespace RT=RetailT8Runtime;namespace RS=RetailT8Resources;namespace H=T7HandToRetailT8;
    Memory m;m.Runtime(28);
    const auto map=RS::CategoryIndexMap(m,m.manager,28);
    Check(map && !RS::CategoryIndexMap(m,m.manager,29),"Category index map lookup changed");
    // Fixture: the domain's index map is the inner map of the manager's domain-28 node; its one native node is the
    // body slot key 0x100001 (index 1, category 0).
    const auto domainNode=RT::Lookup(m,m.readUInt64(m.manager+0x2090),m.readUInt64(m.manager+0x20a0),m.readUInt64(m.manager+0x20b8),28);
    Check(map==domainNode+0x18,"Category index map is not the domain node's inner map");
    m.Put<uint64_t>(m.slotNode+0x18,0xef001c0012345678ULL);
    Check(RS::CategoryHandle(m,map,0x100001)==0xef001c0012345678ULL && !RS::CategoryHandle(m,map,RS::CategoryKey(1,H::FirstRetailIndex)),
        "Category handle lookup changed");
    Check(RS::CategoryKey(1,0x2000)==0x202000 && RS::CategoryKey(4,7)==0x1000007,"Category key layout changed");
    const auto head=m.readUInt64(map+8);
    m.Put<uint64_t>(head,m.slotNode);m.Put<uint64_t>(m.slotNode,head); // the fixture links prev only
    auto list=[&]{std::vector<gameAddr> nodes;for(auto n=m.readUInt64(head);n!=head;n=m.readUInt64(n))nodes.push_back(n);return nodes;};
    const auto before=list();
    std::vector<RS::PublishedAnimation> nodes;
    for(uint32_t i=0;i<3;++i){
        const auto key=RS::CategoryKey(1,H::FirstRetailIndex+i);
        const auto image=RS::CategoryNode(key,0xef001c0080000000ULL|i);
        const auto at=m.Alloc(image.size());m.writeBytes(at,image.data(),image.size());
        nodes.push_back({key,at});
    }
    Check(RS::PublishCategoryIndex(m,map,nodes)==3,"Category publication count changed");
    for(uint32_t i=0;i<3;++i)Check(RS::CategoryHandle(m,map,nodes[i].id)==(0xef001c0080000000ULL|i),"Published hand index is not found by the readers' walk");
    Check(RS::CategoryHandle(m,map,0x100001)==0xef001c0012345678ULL,"Native category node lost after publication");
    Check(list()==before && m.readUInt64(map+0x10)==1,"Category publication changed the engine's node list or size");
    bool duplicate=false;try{RS::PublishCategoryIndex(m,map,std::vector<RS::PublishedAnimation>{nodes[0]});}catch(const std::exception&){duplicate=true;}
    Check(duplicate,"Duplicate category key accepted");
    // Hand rows: a published index is remapped, an unpublished one never fires, other rows are untouched.
    std::vector<Byte> rows(7*0x28);
    auto row=[&](size_t i,uint32_t frame,uint32_t id,uint32_t value){memcpy(rows.data()+i*0x28,&frame,4);memcpy(rows.data()+i*0x28+0x10,&id,4);memcpy(rows.data()+i*0x28+0x14,&value,4);};
    row(0,5,0x8610,0x2000);row(1,6,0x8611,0x2001);row(2,7,0x8610,3);row(3,8,0x8001,0x2000);row(4,9,0x868C,0x2000);row(5,10,0x8683,0x2000);row(6,11,0x877A,0x2000);
    auto read=[&](size_t i,size_t at){uint32_t v;memcpy(&v,rows.data()+i*0x28+at,4);return v;};
    // Hands and cameras both start at 0x2000: the map is keyed by category. Camera 0x2000 is published as 0x2001.
    const std::map<std::pair<uint32_t,uint32_t>,uint32_t> indices{{{1,0x2000},0x2005},{{4,0x2000},0x2001},{{3,0x2000},0x2002},{{2,0x2000},0x2003}};
    uint64_t silenced=0;
    Check(RetailT8Import::ApplyHandIndices(rows,&indices,&silenced)==5 && silenced==1,"Hand row changes miscounted");
    Check(read(6,0x14)==0x2003 && read(6,0)==11,"Published face index was not remapped by its own category");
    Check(read(5,0x14)==0x2002 && read(5,0)==10,"Published wing index was not remapped by its own category");
    Check(read(4,0x14)==0x2001 && read(4,0)==9,"Published camera index was not remapped by its own category");
    Check(read(0,0x14)==0x2005 && read(0,0)==5,"Published hand index was not remapped");
    Check(read(1,0)==ImportProfile::StrippedFrame && read(1,0x14)==0x2001,"Unpublished hand index still fires");
    Check(read(2,0)==7 && read(2,0x14)==3 && read(3,0)==8 && read(3,0x14)==0x2000,"Native hand rows or other properties changed");
    Check(RetailT8Import::ApplyHandIndices(rows,nullptr,&silenced)==5 && silenced==5 && read(0,0)==ImportProfile::StrippedFrame && read(2,0)==7 &&
        read(4,0)==ImportProfile::StrippedFrame,
        "Without published indices every converted hand row must be silenced");
}
}
void RunRetailSnapshotTests() {
    RunMemoryOnlyRegistrationTests();
    RunCategoryIndexTests();
    {
        // QA 1.0 (CntTablesFitBlock): the legacy CNT extractor rewrites every listed record in place; lists must
        // lie inside the copied block (from `reactions` to the end of the last list).
        namespace C = StructsT8CNT_gameAddr;
        C::MovesetTable table{};
        const gameAddr start = 0x10000000;
        table.reactions = start; table.reactionsCount = 2;
        uint64_t size = 2 * sizeof(C::Reactions);
        for (size_t i = 0; i < std::size(table.entries); ++i) {
            table.entries[i].listAddr = start + size; table.entries[i].listCount = 1; size += 0x1000;
        }
        Check(T8CNTTablesFitBlock(table, size), "Valid CNT table layout was rejected");
        auto bad = table; bad.moveCount = 0x1000000; Check(!T8CNTTablesFitBlock(bad, size), "CNT move list past its block was accepted");
        bad = table; bad.move = start - 8; Check(!T8CNTTablesFitBlock(bad, size), "CNT list before its block was accepted");
        bad = table; bad.input = start + size; Check(!T8CNTTablesFitBlock(bad, size), "CNT list at the block end was accepted");
        bad = table; bad.cancelCount = UINT64_MAX; Check(!T8CNTTablesFitBlock(bad, size), "Overflowing CNT count was accepted");
        bad = table; bad.input = 0; bad.inputCount = 0; Check(T8CNTTablesFitBlock(bad, size), "Empty CNT list was rejected");
    }
    {
        Memory m;m.Runtime();namespace RT=RetailT8Runtime;
        auto q=[&](gameAddr p){return m.readUInt64(p);};
        auto character=RT::Lookup(m,q(m.manager+0x2090),q(m.manager+0x20a0),q(m.manager+0x20b8),28);
        const auto head=q(character+0x20);
        for(unsigned type=1;type<6;++type) {
            auto node=m.Alloc(0x28);m.Put<uint64_t>(node+8,q(head+8));m.Put<uint64_t>(head+8,node);
            m.Put<uint32_t>(node+0x10,1U<<(20+type));m.Put<uint64_t>(node+0x18,0xef001c0012345678ULL);
        }
        m.Put<uint64_t>(character+0x28,6);
        const auto before=m.regions;
        auto reference=R::Capture(m,Memory::Root);
        for(unsigned mask=0;mask<64;++mask) {
            auto data=R::CaptureComplete(m,Memory::Root,m.manager,m.container,28,static_cast<uint8_t>(mask));
            auto assets=R::Assets(data);
            Check(assets.size()==std::popcount(mask),"Retail selection exported omitted animation categories");
            for(const auto& asset:assets)Check((mask&(1U<<asset.type)) && asset.size==128 &&
                !memcmp(data.data()+asset.offset,m.regions[m.panm].data(),128),"Selected retail PANM bytes/category differ");
            for(size_t table=0;table<20;++table)Check(std::equal(R::Table(data,table).begin(),R::Table(data,table).end(),R::Table(reference,table).begin()),
                "Animation selection changed record tables");
        }
        Check(m.regions==before && m.writes==0,"Selective extraction modified game memory");
        auto tables=R::CaptureComplete(m,Memory::Root,0,0,28,0);
        Check(R::Assets(tables).empty(),"Tables-only export accessed animation dependencies");
        Check(RetailAnimationSelection(ExtractSettings_T8_PANM_SELECTION)==0 &&
            RetailAnimationSelection(ExtractSettings_T8_PANM_SELECTION|ExtractSettings_T8_DEFAULT)==19 &&
            RetailAnimationSelection(ExtractSettings_CompressLZ4)==63,"Retail selection flags overlap legacy settings or lost defaults");
    }
    {
        Memory m;m.Runtime();m.unavailable=m.panm;
        auto skipped=R::CaptureComplete(m,Memory::Root,m.manager,m.container,28,2);
        Check(R::Assets(skipped).empty() && !m.reads.contains(m.panm),"Unselected animation data was read");
        bool failed=false;try{R::CaptureComplete(m,Memory::Root,m.manager,m.container,28,1);}catch(const std::exception&){failed=true;}
        Check(failed,"Unreadable selected animation was silently omitted");
    }
    {
        TKMovesetHeader header; header.gameId = GameId_T8; header.minorVersion = GameVersions::T8::RETAIL;
        header.header_size = sizeof(header); header.block_list_size = 1; header.game_specific_flags = R::RuntimeAssetsFlag;
        strcpy_s(header.version_string, "0.9.9.9.3"); header.moveset_data_start = 512;
        header.compressionType = TKMovesetCompressionType_LZ4;
        header.moveset_data_size = (600ULL << 20);
        Check(MovesetLimits::Fits(header, 100ULL << 20), "Retail PANM profile kept legacy expansion limit");
        header.moveset_data_size = MovesetLimits::RetailRuntime - header.moveset_data_start;
        Check(MovesetLimits::Fits(header, MovesetLimits::MaximumStored), "Retail codec overhead not budgeted");
        ++header.moveset_data_size;
        Check(!MovesetLimits::Fits(header, 100ULL << 20), "Oversized retail expansion accepted");
        header.gameId = GameId_T7; header.moveset_data_size = MovesetLimits::Legacy;
        Check(MovesetLimits::Expanded(header) == MovesetLimits::Legacy && !MovesetLimits::Fits(header, 100ULL << 20), "T7 inherited retail expansion budget");
        header.gameId = GameId_T8; header.minorVersion = GameVersions::T8::CLOSED_NETWORK_TEST;
        Check(MovesetLimits::Expanded(header) == MovesetLimits::Legacy, "CNT inherited retail expansion budget");
        header.minorVersion = GameVersions::T8::RETAIL; header.game_specific_flags = R::TablesOnlyFlag;
        Check(MovesetLimits::Expanded(header) == MovesetLimits::Legacy, "Table-only snapshot inherited PANM budget");
    }
    {
        Memory runtime; runtime.Runtime(); const auto original = runtime.regions;
        auto snapshot = R::CaptureComplete(runtime, Memory::Root, runtime.manager, runtime.container, 28);
        {
            const auto folder=std::filesystem::temp_directory_path()/("tkmovesets-panm-library-"+std::to_string(GetCurrentProcessId()));
            Check(RetailT8Animations::Extract(snapshot,folder)==1,"Retail animation library omitted an embedded PANM");
            std::ifstream stream(folder/"body_12345678.panm",std::ios::binary);std::vector<Byte> bytes((std::istreambuf_iterator<char>(stream)),{});stream.close();
            Check(bytes==runtime.regions[runtime.panm],"Extracted PANM library bytes differ from snapshot");
            bool rejected=false;try{RetailT8Animations::Extract(snapshot,folder);}catch(const std::exception&){rejected=true;}
            Check(rejected,"Animation extraction overwrote an existing library folder");
            std::filesystem::remove(folder/"body_12345678.panm");std::filesystem::remove(folder/"animations.json");std::filesystem::remove(folder);
            rejected=false;try{RetailT8Animations::Extract(R::Capture(runtime,Memory::Root),folder);}catch(const std::exception&){rejected=true;}
            Check(rejected && !std::filesystem::exists(folder),"Tables-only file produced a fake animation library");
            // Editor modal: listed animations, a chosen subset, and an interrupt that leaves no folder behind.
            const auto list=RetailT8Animations::List(snapshot,[](uint32_t){return std::string();});
            Check(list.size()==1 && list[0].category==0 && list[0].instance==0x12345678 && list[0].file=="body_12345678.panm" &&
                list[0].bytes==runtime.regions[runtime.panm].size(),"Listed animations differ from the snapshot");
            std::atomic<bool> stop=true;std::atomic<uint32_t> done=7;
            Check(RetailT8Animations::Extract(snapshot,list,folder,&stop,&done)==0 && done==0 && !std::filesystem::exists(folder) &&
                !std::filesystem::exists(folder.wstring()+L".partial"),"Interrupted animation extraction left files");
            stop=false;
            Check(RetailT8Animations::Extract(snapshot,list,folder,&stop,&done)==1 && done==1 && std::filesystem::exists(folder/"body_12345678.panm") &&
                !std::filesystem::exists(folder.wstring()+L".partial"),"Selected animation extraction differs");
            rejected=false;try{RetailT8Animations::Extract(snapshot,{},folder.wstring()+L"_empty");}catch(const std::exception&){rejected=true;}
            Check(rejected,"Empty animation selection was extracted");
            std::filesystem::remove_all(folder);
        }
        Check(runtime.regions == original && R::Assets(snapshot).size() == 1, "Complete capture changed source or omitted PANM");
        for (uint32_t type = 0; type < 6; ++type) {
            runtime.Put<uint32_t>(runtime.slotNode + 0x10, 1U << (20 + type));
            Check(R::Assets(R::CaptureComplete(runtime, Memory::Root, runtime.manager, runtime.container, 28))[0].type == type,
                "Retail animation category was lost");
        }
        for (size_t offset : {size_t(4), size_t(26), size_t(58)}) {
            auto corrupt = runtime.regions[runtime.panm]; corrupt[offset] ^= 0xff;
            bool rejected = false; try { R::PanmSize(corrupt); } catch (const std::exception&) { rejected = true; }
            Check(rejected, "Malformed PANM extent accepted");
        }
        auto reject = [&](auto mutation) {
            Memory bad; bad.Runtime(); mutation(bad); const auto before = bad.regions;
            bool failed = false;
            try { R::CaptureComplete(bad, Memory::Root, bad.manager, bad.container, 28); } catch (const std::exception&) { failed = true; }
            Check(failed && bad.regions == before, "Incomplete animation export succeeded or changed source");
        };
        reject([](Memory& m) { m.unavailable = m.panm; });
        reject([](Memory& m) { m.Put<uint64_t>(m.slotNode + 8, m.slotNode); });
        {
            Memory unloaded; unloaded.Runtime(); unloaded.Put<uint64_t>(unloaded.slotNode + 0x18, 0x87654321);
            const auto original = unloaded.regions;
            auto data = R::CaptureComplete(unloaded, Memory::Root, unloaded.manager, unloaded.container, 28);
            auto entries = R::Assets(data);
            Check(entries.size() == 1 && !entries[0].address && !entries[0].size && entries[0].instance == 0x87654321 && original == unloaded.regions,
                "Unavailable animation was not recorded explicitly");
            uint32_t legacyVersion = 2; memcpy(data.data() + 4, &legacyVersion, 4);
            bool rejected = false; try { R::Validate(data); } catch (const std::exception&) { rejected = true; }
            Check(rejected, "Unavailable entry accepted in v2 snapshot");
            // With the game's archive, the same slot is carried in full (bytes from the archive, never the game's memory).
            Memory filled; filled.Runtime(); filled.Put<uint64_t>(filled.slotNode + 0x18, 0x87654321);
            const auto archived = filled.regions.at(filled.panm);
            std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> asked;
            auto full = R::CaptureComplete(filled, Memory::Root, filled.manager, filled.container, 28, 63,
                [&](uint32_t domain, uint32_t type, uint32_t instance) -> std::optional<std::vector<Byte>> { asked.emplace_back(domain, type, instance); return archived; });
            R::Validate(full);
            auto fullEntries = R::Assets(full);
            Check(asked.size() == 1 && std::get<0>(asked[0]) == 28 && std::get<2>(asked[0]) == 0x87654321 && fullEntries.size() == 1 &&
                fullEntries[0].address == R::ArchiveAssetAddress && fullEntries[0].size == archived.size() &&
                std::equal(archived.begin(), archived.end(), full.begin() + fullEntries[0].offset),
                "Unloaded animation was not filled from the game's archive");
            // missing from memory and the archive: exported as unavailable (address 0, size 0), not a refused export
            auto partial = R::CaptureComplete(filled, Memory::Root, filled.manager, filled.container, 28, 63,
                [](uint32_t, uint32_t, uint32_t) -> std::optional<std::vector<Byte>> { throw std::runtime_error("not in archive"); });
            R::Validate(partial);
            auto partialEntries = R::Assets(partial);
            Check(partialEntries.size() == 1 && partialEntries[0].address == 0 && partialEntries[0].size == 0 &&
                partialEntries[0].instance == 0x87654321, "An animation missing from memory and the archive is not exported as unavailable");
        }
        reject([](Memory& m) { m.Put<uint32_t>(m.panm + 68, UINT32_MAX); });
        reject([](Memory& m) { m.Put<uint32_t>(m.panm + 4, 0); });
        auto bad = snapshot; R::Header header; memcpy(&header, bad.data(), sizeof(header));
        auto last = R::Entry{}; memcpy(&last, bad.data() + header.directoryOffset + 19 * sizeof(last), sizeof(last));
        auto directoryAt = (last.offset + last.count * last.stride + 7) & ~uint64_t(7);
        R::AssetDirectory directory; memcpy(&directory, bad.data() + directoryAt, sizeof(directory));
        bad[directory.entriesOffset + offsetof(R::AssetEntry, offset)] ^= 8;
        bool failed = false; try { R::Validate(bad); } catch (const std::exception&) { failed = true; }
        Check(failed, "Overlapping PANM directory accepted");
    }
    {
        // All animation data is synthetic process memory. No archive file or
        // game-side request exists in this fixture.
        Memory m; m.Runtime(); namespace RT = RetailT8Runtime;
        auto q = [&](gameAddr at) { return m.readUInt64(at); };
        auto node = RT::Lookup(m, q(m.container + 8), q(m.container + 0x18), q(m.container + 0x30), 28);
        auto map = q(q(node + 0x18));
        auto job = m.Alloc(16), resource = m.Alloc(0x128), owner = m.Alloc(16), archive = m.Alloc(0x200);
        m.Put<uint64_t>(map, job); m.Put<uint64_t>(job + 8, resource);
        m.Put<uint64_t>(resource + 0x118, owner); m.Put<uint64_t>(owner, archive); m.Put<uint64_t>(owner + 8, 0x200);
        m.Put<uint32_t>(archive + 4, 1); m.Put<uint64_t>(archive + 0x38, archive + 0x98);
        m.Put<uint32_t>(archive + 0x98, 0x12345678); m.Put<uint64_t>(archive + 0xa0, archive + 0xd0);
        memcpy(m.regions.at(archive).data()+0xd0,m.regions.at(m.panm).data(),128);
        // A facial PANM exists in the archive without a compiled-cache node.
        m.Put<uint32_t>(archive + 4 + 2*4,1);
        m.Put<uint64_t>(archive + 0x38 + 2*8,archive + 0x98);
        m.Put<uint32_t>(archive + 0x98,0x76543210);
        Check(RT::Panm(m,m.container,28,0x76543210,2,128)==archive+0xd0,
            "Resident auxiliary PANM required a lazy compiled-cache entry");
        m.Put<uint32_t>(archive + 4 + 2*4,0);
        m.Put<uint32_t>(archive + 0x98,0x12345678);
        memcpy(m.regions[archive].data() + 0xd0, m.regions[m.panm].data(), 128);
        m.Put<uint32_t>(m.panm + 96, 123); // Actual live edit wins over an older resident archive.
        auto edited = R::CaptureComplete(m, Memory::Root, m.manager, m.container, 29);
        auto assets = R::Assets(edited);
        Check(assets.size() == 1 && assets[0].address == m.panm &&
            !memcmp(edited.data() + assets[0].offset, m.regions[m.panm].data(), 128),
            "Memory extraction lost active edits or followed visual identity instead of animation handles");
        m.Put<uint32_t>(m.panm + 4, 0);
        const auto before = m.regions;
        auto retained = R::CaptureComplete(m, Memory::Root, m.manager, m.container, 29);
        assets = R::Assets(retained);
        Check(assets[0].address == archive + 0xd0 && assets[0].size == 128 &&
            !memcmp(retained.data() + assets[0].offset, m.regions[archive].data() + 0xd0, 128) && m.regions == before && m.writes == 0,
            "Memory extraction did not preserve bounded resident PANM bytes without writes");
        m.Put<uint64_t>(owner + 8, 0xe0); // PANM header fits, complete payload does not.
        bool rejected = false;
        try { R::CaptureComplete(m, Memory::Root, m.manager, m.container, 29); } catch (const std::exception&) { rejected = true; }
        Check(rejected && m.writes == 0, "Memory extraction accepted a PANM outside its resident archive extent");
        auto moves = R::Table(retained, 12);
        Check(RT::AnimationCharacter(moves, 29) == 28, "Cross-character capture lost its actual animation domain");
        std::vector<Byte> mixed(moves.begin(), moves.end()); mixed.insert(mixed.end(), moves.begin(), moves.end());
        const uint64_t other = 0xef001d0012345678ULL; memcpy(mixed.data() + 0x448 + 0x50, &other, 8);
        rejected = false; try { RT::AnimationCharacter(mixed, 29); } catch (const std::exception&) { rejected = true; }
        Check(rejected, "Mixed animation domains silently produced an incomplete snapshot");
    }
    Memory memory; auto original = memory.regions;
    auto bytes = R::Capture(memory, Memory::Root);
    Check(memory.regions == original && memory.reads[Memory::Root] == 2, "Retail source/header consistency failed");
    for (size_t i = 0; i < R::Tables.size(); ++i) {
        const auto table = R::Table(bytes, i);
        Check(memory.reads[memory.Pointer(i)] == 1 && table.size() == R::Tables[i].stride &&
            !memcmp(table.data(), original.at(memory.Pointer(i)).data(), table.size()), "Retail table bytes/read count differ");
    }
    Check(R::Table(bytes, 12).size() == 0x448, "Retail capture used CNT move stride");
    {
        Memory empty;
        uint64_t count = 0, address = UINT64_MAX;
        memcpy(empty.regions[Memory::Root].data() + R::Tables[3].countOffset, &count, 8);
        memcpy(empty.regions[Memory::Root].data() + R::Tables[3].pointerOffset, &address, 8);
        Check(R::Table(R::Capture(empty, Memory::Root), 3).empty(), "Empty retail table dereferenced");
    }
    Reject([](Memory& m) { m.unavailable = m.Pointer(12); });
    Reject([](Memory& m) { uint64_t n = UINT64_MAX; memcpy(m.regions[Memory::Root].data() + 0x238, &n, 8); });
    Reject([](Memory& m) { uint64_t n = 0; memcpy(m.regions[Memory::Root].data() + 0x238, &n, 8); });
    Reject([](Memory& m) { uint32_t date = 20230509; memcpy(m.regions[Memory::Root].data() + 4, &date, 4); });
    Reject([](Memory& m) { m.changing = true; });
    Reject([](Memory& m) { m.regions[m.Pointer(12)].pop_back(); });
    for (size_t length : {size_t(0), sizeof(R::Header) - 1, bytes.size() - 1}) {
        bool failed = false;
        try { R::Validate({bytes.data(), length}); } catch (const std::exception&) { failed = true; }
        Check(failed, "Truncated retail snapshot accepted");
    }
    {
        auto malformed = bytes;
        R::Header header; memcpy(&header, malformed.data(), sizeof(header));
        R::Entry entry; memcpy(&entry, malformed.data() + header.directoryOffset, sizeof(entry));
        entry.offset += 8; memcpy(malformed.data() + header.directoryOffset, &entry, sizeof(entry));
        bool failed = false;
        try { R::Validate(malformed); } catch (const std::exception&) { failed = true; }
        Check(failed, "Overlapping/shifted retail snapshot directory accepted");
    }
    const auto directory = std::filesystem::temp_directory_path() / ("tkmovesets-retail-snapshot-" + std::to_string(GetCurrentProcessId()));
    Check(std::filesystem::create_directory(directory), "Cannot create retail test directory");
    try {
        GameAddressesFile addresses; addresses.Reload(); GameData game; game.SetProcess(&memory, &addresses);
        GameInfo info{}; info.name = "Tekken 8"; info.gameId = GameId_T8;
        info.minorVersion = GameVersions::T8::RETAIL; info.movesetNamePrefix = "T8_";
        info.dataString = info.minorDataString = "t8-retail";
        Exporter exporter(memory, game, &info, directory);
        memory.Runtime(); const auto player = memory.player;
        Check(RetailT8Runtime::Players(memory, memory.mainModule.address + 0x959a320, memory.mainModule.address + 0x959a088)[0] == player,
            "Retail registry fixture does not resolve its sparse slots");
        Check(game.GetValue("registry_rva") == 0x959a320 && game.GetValue("motbin_offset") == 0x3b50,
            "Retail export regression loaded stale address settings");
        Check(exporter.GetCharacterAddresses().size() == 1 && exporter.GetCharacterAddress(1) == 0,
            "Retail extractor did not preserve empty player slots");
        Check(exporter.CanExtract(), "Retail extractor did not detect loaded root");
        const auto file = directory / ("T8_" + exporter.GetPlayerCharacterName(player) + ".tkmvst");
        uint8_t progress;
        for (auto codec : {ExtractSettings(0), ExtractSettings(ExtractSettings_CompressLZ4), ExtractSettings(ExtractSettings_CompressLZMA)}) {
            Check(exporter.Extract(player, codec | ExtractSettings_OVERWRITE_SAME_FILENAME, progress) == ExtractionErrcode_Successful &&
                progress == 100, "Retail extractor/codec failed");
            MovesetFile saved(file); saved.Validate();
            Check(saved.Header().gameId == GameId_T8 && saved.Header().minorVersion == GameVersions::T8::RETAIL &&
                saved.Header().GetGameSpecificFlags() == R::RuntimeAssetsFlag, "Retail snapshot misidentified");
            Check(R::Table(saved.Payload(), 12).size() == 0x448, "Retail round trip lost move data");
            auto assets = R::Assets(saved.Payload());
            Check(assets.size() == 1 && assets[0].instance == 0x12345678 && assets[0].type == 0 && assets[0].size == 128 &&
                !memcmp(saved.Payload().data() + assets[0].offset, memory.regions[memory.panm].data(), 128), "Retail PANM round trip changed bytes");
        }
        for(auto selected:{ExtractSettings(0),ExtractSettings(ExtractSettings_T8_BODY),ExtractSettings(ExtractSettings_T8_HANDS)}) {
            const auto settings=ExtractSettings_T8_PANM_SELECTION | selected | ExtractSettings_CompressLZ4 | ExtractSettings_OVERWRITE_SAME_FILENAME;
            if(!selected)memory.unavailable=memory.panm;
            Check(exporter.Extract(player,settings,progress)==ExtractionErrcode_Successful,"Production selective retail export failed");
            memory.unavailable=0;
            MovesetFile saved(file);saved.Validate();
            Check(R::Assets(saved.Payload()).size()==(selected==ExtractSettings_T8_BODY?1:0),"Production export ignored selected animation categories");
            // v4: every export carries the T8RX extension (and so a possibly empty animation directory).
            Check(saved.Header().GetGameSpecificFlags()==R::RuntimeAssetsFlag &&
                reinterpret_cast<const R::Header*>(saved.Payload().data())->version==4 && R::ReadExtension(saved.Payload()),
                "Selective export wrote incompatible snapshot identity flags");
        }
        MovesetFile before(file); const auto payloadBefore = std::vector<Byte>(before.Payload().begin(), before.Payload().end());
        memory.unavailable = memory.Pointer(12);
        Check(exporter.Extract(player, ExtractSettings_OVERWRITE_SAME_FILENAME, progress) == ExtractionErrcode_MovesetDataError &&
            progress < 100, "Unreadable retail source reported success");
        memory.unavailable = 0;
        MovesetFile after(file); Check(std::vector<Byte>(after.Payload().begin(), after.Payload().end()) == payloadBefore,
            "Failed retail capture replaced destination");
        auto lock = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        Check(lock != INVALID_HANDLE_VALUE, "Cannot lock retail destination");
        auto status = exporter.Extract(player, ExtractSettings_OVERWRITE_SAME_FILENAME, progress); CloseHandle(lock);
        Check(status == ExtractionErrcode_FileCreationErr, "Locked retail destination was replaced");
        Check(std::distance(std::filesystem::directory_iterator(directory), std::filesystem::directory_iterator{}) == 1,
            "Retail capture left temporary output");
    } catch (...) { std::filesystem::remove_all(directory); throw; }
    std::filesystem::remove_all(directory);
}

void RunRetailImportTests() {
    namespace I = RetailT8Import;
    {
        namespace P = RetailT8Resources;
        Memory m; m.Runtime();
        auto snapshot = R::CaptureComplete(m, Memory::Root, m.manager, m.container, 28);
        auto directory = std::filesystem::temp_directory_path() / ("TKM-T8-ANMBIN-" + std::to_string(GetCurrentProcessId()));
        Check(std::filesystem::create_directory(directory), "Cannot create ANMBIN test directory");
        auto cleanup = [&] { std::filesystem::remove(directory / "com.anmbin"); std::filesystem::remove(directory / "unit.anmbin"); std::filesystem::remove(directory); };
        auto save = [&](const char* name, const std::vector<Byte>& bytes) {
            std::ofstream file(directory / name, std::ios::binary); file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
            Check(bool(file), "Cannot write ANMBIN fixture");
        };
        auto put = [](auto& bytes, size_t at, auto value) { memcpy(bytes.data() + at, &value, sizeof(value)); };
        std::vector<Byte> empty(0x98), archive(0xd0 + 128);
        put(archive, 4, uint32_t(1)); put(archive, 0x38, uint64_t(0x98));
        put(archive, 0x98, uint64_t(0x12345678)); put(archive, 0xa0, uint64_t(0xd0));
        memcpy(archive.data() + 0xd0, m.regions.at(m.panm).data(), 128);
        try {
            save("com.anmbin", empty); save("unit.anmbin", archive);
            auto report = P::VerifyArchives(snapshot, directory, "unit");
            Check(report.blobs == 1 && report.bytes == 128, "ANMBIN fixture failed exact PANM verification");
            auto changed = archive; changed.back() ^= 1; save("unit.anmbin", changed);
            bool rejected = false; try { P::VerifyArchives(snapshot, directory, "unit"); } catch (const std::exception&) { rejected = true; }
            Check(rejected, "Changed ANMBIN PANM was accepted");
            auto shared = archive; put(shared, 0xa0, uint64_t(0));
            save("unit.anmbin", shared); save("com.anmbin", archive);
            Check(P::VerifyArchives(snapshot, directory, "unit").blobs == 1, "Zero-offset ANMBIN entry did not resolve shared data");
            for (int variant = 0; variant < 3; ++variant) {
                auto invalid = archive;
                if (variant == 0) put(invalid, 0xa0, UINT64_MAX);
                if (variant == 1) put(invalid, 4, uint32_t(100001));
                if (variant == 2) invalid.resize(12);
                save("unit.anmbin", invalid); rejected = false;
                try { P::VerifyArchives(snapshot, directory, "unit"); } catch (const std::exception&) { rejected = true; }
                Check(rejected, "Malformed ANMBIN metadata was accepted");
            }
            rejected = false; try { P::VerifyArchives(snapshot, directory, "../unit"); } catch (const std::exception&) { rejected = true; }
            Check(rejected, "ANMBIN code escaped its directory");
            // In-memory parser used for tkdata.bin archives: exact PANM bytes, unknown ids absent, conflicting duplicates rejected.
            {
                RetailT8GameData::Anmbin parsed(archive);
                const auto panm = parsed.Panm(0, 0x12345678);
                Check(panm && panm->size() == 128 && std::equal(panm->begin(), panm->end(), archive.begin() + 0xd0) &&
                    !parsed.Panm(0, 0x1234) && !parsed.Panm(1, 0x12345678), "ANMBIN parser returned the wrong PANM");
                std::vector<Byte> two(0x98 + 2 * 0x38 + 2 * 128);
                put(two, 4, uint32_t(2)); put(two, 0x38, uint64_t(0x98));
                put(two, 0x98, uint64_t(0x12345678)); put(two, 0xa0, uint64_t(0x108));
                put(two, 0xd0, uint64_t(0x12345678)); put(two, 0xd8, uint64_t(0x188));
                memcpy(two.data() + 0x108, archive.data() + 0xd0, 128); memcpy(two.data() + 0x188, archive.data() + 0xd0, 128);
                Check(RetailT8GameData::Anmbin(two).Panm(0, 0x12345678)->size() == 128, "Identical duplicate ANMBIN entries rejected");
                two.back() ^= 1;
                rejected = false; try { RetailT8GameData::Anmbin(two).Panm(0, 0x12345678); } catch (const std::exception&) { rejected = true; }
                Check(rejected, "Conflicting duplicate ANMBIN entries accepted");
            }
            // Disk extraction of one archive: every PANM written, a manifest, no partial folder; an interrupt leaves nothing.
            {
                RetailT8GameData::Anmbin parsed(archive);
                const auto out = directory / "disk";
                std::atomic<uint32_t> done{0}, total{0};
                Check(RetailT8Animations::ExtractArchive(parsed, out, nullptr, &done, &total) && done == 1 && total == 1 &&
                    std::filesystem::file_size(out / "body_12345678.panm") == 128 && std::filesystem::exists(out / "animations.json") &&
                    !std::filesystem::exists(directory / "disk.partial"), "Disk animation extraction wrote the wrong files");
                bool exists = false; try { RetailT8Animations::ExtractArchive(parsed, out); } catch (const std::exception&) { exists = true; }
                Check(exists, "Disk animation extraction overwrote an existing folder");
                std::atomic<bool> stop{true};
                Check(!RetailT8Animations::ExtractArchive(parsed, directory / "stopped", &stop) && !std::filesystem::exists(directory / "stopped") &&
                    !std::filesystem::exists(directory / "stopped.partial"), "Interrupted disk extraction left files");
                std::filesystem::remove_all(out);
            }
        } catch (...) { cleanup(); std::filesystem::remove_all(directory); throw; }
        cleanup();
    }
    {
        // Notes section of the retail container: written only when given, read back, kept by editor saves, unknown tags
        // skipped, corruption rejected; files without notes are byte-identical to before.
        Memory m; m.Runtime();
        const auto payload = R::CaptureComplete(m, Memory::Root, m.manager, m.container, 28);
        TKMovesetHeader outer{}; outer.characterId = 28; strcpy_s(outer.version_string, MOVESET_VERSION_STRING);
        const auto plain = R::Serialize(payload, outer);
        Check(R::ReadOuterSections(plain).empty() && R::Notes(plain).empty(), "A file without notes gained a section");
        const std::string text = "6 animations are not in this file (body 6).";
        const auto noted = R::Serialize(payload, outer, {R::NotesSection(text)});
        Check(R::Notes(noted) == text && R::ReadOuterSections(noted).size() == 1, "Notes did not round-trip");
        const auto& h = *reinterpret_cast<const TKMovesetHeader*>(noted.data());
        const auto notedPayload = std::span<const Byte>(noted).subspan(h.moveset_data_start);
        Check(std::equal(notedPayload.begin(), notedPayload.end(), payload.begin(), payload.end()), "Notes changed the payload");
        auto corrupt = noted; corrupt[((sizeof(TKMovesetHeader) + 7) & ~size_t(7)) + 16] ^= 1;
        bool rejected = false; try { R::ReadOuterSections(corrupt); } catch (const std::exception&) { rejected = true; }
        Check(rejected, "A corrupt header section was accepted");
        R::OuterSection unknown; unknown.tag = {'Z','Z','Z','Z'}; unknown.data = {1, 2, 3};
        const auto foreign = R::Serialize(payload, outer, {unknown});
        rejected = false; try { R::ReadOuterSections(foreign); } catch (const std::exception&) { rejected = true; }
        Check(rejected, "An unknown header section was accepted");
        // Names section: round trip, coexists with notes, payload untouched, truncation rejected.
        const R::NameMap names{{0x00000001u, "Kz_dash"}, {0xDEADBEEFu, "Kz_\xe6\x97\xa5_utf8"}, {0x7FFFFFFFu, ""}};
        const auto named = R::Serialize(payload, outer, {R::NotesSection(text), R::NamesSection(names)});
        Check(R::Names(named) == names && R::Notes(named) == text, "Names section did not round-trip");
        Check(R::Names(plain).empty(), "A file without a names section returned names");
        const auto& nh = *reinterpret_cast<const TKMovesetHeader*>(named.data());
        const auto namedPayload = std::span<const Byte>(named).subspan(nh.moveset_data_start);
        Check(std::equal(namedPayload.begin(), namedPayload.end(), payload.begin(), payload.end()), "Names changed the payload");
        auto section = R::NamesSection(names); section.data.pop_back();
        const auto truncated = R::Serialize(payload, outer, {section});
        rejected = false; try { R::Names(truncated); } catch (const std::exception&) { rejected = true; }
        Check(rejected, "A truncated names section was accepted");
        // Collected names: every move/animation hash the lookup knows, nothing else.
        const auto hashes = R::MoveHashes(payload);
        const auto collected = R::CollectNames(payload, [](uint32_t hash) -> const std::string* {
            static std::map<uint32_t, std::string> made; return &(made[hash] = "name_" + std::to_string(hash)); });
        for (const auto& move : hashes) {
            if (move.name) Check(collected.contains(*move.name), "A move name hash was not collected");
            if (move.animation) Check(collected.contains(*move.animation), "An animation hash was not collected");
        }
    }
    Memory memory;
    for (const auto& spec : R::Tables) {
        uint64_t address = 0; memcpy(&address, memory.regions.at(Memory::Root).data() + spec.pointerOffset, 8);
        memory.regions.at(address).assign(spec.stride, 0);
    }
    // Disjoint source allocations, first-record pointers (not NULL), external
    // strings, and pointer-shaped encrypted data that must never be scanned.
    memory.Put<uint64_t>(memory.Pointer(6) + 8, memory.Pointer(1));
    memory.Put<uint64_t>(memory.Pointer(6) + 16, memory.Pointer(8));
    memory.Put<uint64_t>(memory.Pointer(12) + 0x98, memory.Pointer(6));
    memory.Put<uint64_t>(memory.Pointer(12) + 0x110, memory.Pointer(2));
    memory.Put<uint64_t>(memory.Pointer(12), memory.Pointer(6));
    memory.Put<uint64_t>(memory.Pointer(12) + 0x40, 0x12345678);
    memory.Put<uint64_t>(memory.Pointer(12) + 0x50, 0xef00060012345678);
    memory.Put<uint64_t>(memory.Pointer(12) + 0xa0, UINT64_MAX); // inactive bank
    auto payload = R::Capture(memory, Memory::Root); const auto original = payload;
    TKMovesetHeader header; header.gameId = GameId_T8; header.minorVersion = GameVersions::T8::RETAIL;
    header.game_specific_flags = R::TablesOnlyFlag;
    auto plan = I::Prepare(header, payload);
    Check(plan.Ok() && payload == original && plan.dependencies.size() == 3, "Retail import preparation changed source or lost dependencies");
    const auto a = plan.RelocateTables(0x100000000), b = plan.RelocateTables(0x200000000);
    auto q = [](const auto& bytes, size_t field) { uint64_t value; memcpy(&value, bytes.data() + field, 8); return value; };
    Check(q(a, 0x230) == 0x100000000 + plan.offsets[12] &&
        q(a, plan.offsets[12] + 0x98) == 0x100000000 + plan.offsets[6] &&
        q(b, plan.offsets[6] + 8) == 0x200000000 + plan.offsets[1], "Retail graph relocation lost table/first-record pointers");
    Check(q(a, plan.offsets[12]) == memory.Pointer(6) && q(a, plan.offsets[12] + 0x50) == 0xef00060012345678 &&
        q(a, plan.offsets[12] + 0x40) == 0 && q(a, plan.offsets[12] + 0xa0) == 0 &&
        q(a, plan.offsets[12] + 0x138) == 0, "Retail relocation changed encrypted values/handles or retained external/inactive pointers");
    for (auto base : {0ULL, 3ULL, UINT64_MAX - 7}) {
        bool failed = false; try { plan.RelocateTables(base); } catch (const std::exception&) { failed = true; }
        Check(failed, "Invalid retail target address accepted");
    }
    for (auto bad : {memory.Pointer(1) + 1, memory.Pointer(1) + R::Tables[1].stride, UINT64_MAX, memory.Pointer(6)}) {
        memory.Put<uint64_t>(memory.Pointer(6) + 8, bad);
        auto rejected = I::Prepare(header, R::Capture(memory, Memory::Root));
        Check(!rejected.Ok() && rejected.tables.empty() && rejected.relocations.empty() &&
            rejected.issues[0].table == 6 && rejected.issues[0].field == 8, "Invalid typed retail pointer exposed a partial import buffer");
        bool failed = false; try { rejected.RelocateTables(0x100000000); } catch (const std::exception&) { failed = true; }
        Check(failed, "Failed retail graph was relocated");
    }
    memory.Put<uint64_t>(memory.Pointer(6) + 8, 0);
    memory.Put<uint64_t>(memory.Pointer(14) + 8, memory.Pointer(15));
    memory.Put<uint16_t>(memory.Pointer(14) + 2, 2);
    Check(!I::Prepare(header, R::Capture(memory, Memory::Root)).Ok(), "Out-of-bounds retail input sequence accepted");
    memory.Put<uint64_t>(memory.Pointer(14) + 8, 0);
    Check(!I::Prepare(header, R::Capture(memory, Memory::Root)).Ok(), "NULL nonempty retail input sequence accepted");
    memory.Put<uint16_t>(memory.Pointer(14) + 2, 0);
    memory.Runtime();
    auto complete = R::CaptureComplete(memory, Memory::Root, memory.manager, memory.container, 28);
    header.game_specific_flags = R::RuntimeAssetsFlag;
    auto runtimePlan = I::Prepare(header, complete);
    Check(runtimePlan.Ok() && runtimePlan.availableAnimations == 1 && runtimePlan.unavailableAnimations == 0 &&
        runtimePlan.tables.size() == plan.tables.size(), "Retail import failed to separate table graph from PANM assets");
    memory.Put<uint64_t>(memory.slotNode + 0x18, 0xdeadbeef);
    auto missing = I::Prepare(header, R::CaptureComplete(memory, Memory::Root, memory.manager, memory.container, 28));
    Check(missing.Ok() && missing.availableAnimations == 0 && missing.unavailableAnimations == 1,
        "Retail import lost unavailable animation slots");
    header.game_specific_flags = R::TablesOnlyFlag;
    for (int variant = 0; variant < 3; ++variant) {
        auto wrong = header;
        if (variant == 0) wrong.gameId = GameId_T7;
        if (variant == 1) wrong.minorVersion = GameVersions::T8::CLOSED_NETWORK_TEST;
        if (variant == 2) wrong.game_specific_flags = R::RuntimeAssetsFlag;
        bool failed = false; try { I::Prepare(wrong, payload); } catch (const std::exception&) { failed = true; }
        Check(failed, "Wrong retail import identity/flags accepted");
    }

    // Full production importer, including codec expansion, binding live external
    // lists, atomic publication, upload verification, rollback and stale guards.
    auto fixture = [](Memory& m, uint32_t resourceCharacter = 28) {
        for (size_t i = 0; i < R::Tables.size(); ++i) m.regions.at(m.Pointer(i)).assign(R::Tables[i].stride, 0);
        m.Put<uint64_t>(Memory::Root + 0x170, 0);
        m.Runtime(resourceCharacter);
        m.Put<uint32_t>(m.player + 0x16c, 28);
        m.Put<uint32_t>(m.Pointer(1), 220); m.Put<uint32_t>(m.Pointer(1) + 4, 28);
        m.Put<uint64_t>(m.player + 0x3e0, m.Pointer(12)); m.Put<uint32_t>(m.Pointer(12) + 0x120, 60);
        for (size_t i = 0; i < 5; ++i) m.Put<uint64_t>(m.player + 0x3b50 + i * 8, Memory::Root);
        // Runtime-encrypted fields with known values and valid checksums.
        for (auto [offset, value] : {std::pair<size_t, uint32_t>{0, 12345}, {0x20, 67890}}) {
            const uint64_t key = 0x3141592653589793ULL;
            const uint32_t low = value ^ (static_cast<uint32_t>(std::rotl(key, value & 31)) & 0xffffffe0U) ^ 0x1dU;
            uint32_t checksum = 0, part = low;
            for (int shift = 0; shift < 32; shift += 8) { checksum ^= part ^ static_cast<uint32_t>(std::rotl(key, shift + 8)); part >>= 8; }
            m.Put<uint64_t>(m.Pointer(12) + offset, low | (uint64_t(checksum ? checksum : 1) << 32));
            m.Put<uint64_t>(m.Pointer(12) + offset + 8, key);
        }
        m.Put<uint64_t>(m.Pointer(12) + 0x50, 0x12345678);
        auto external = m.Alloc(80);
        for (size_t i = 0; i < 2; ++i) {
            m.Put<uint64_t>(external + i * 40, i == 1 ? 0x8000 : 0x123);
            m.Put<uint64_t>(external + i * 40 + 8, m.Pointer(1));
            m.Put<uint64_t>(external + i * 40 + 16, m.Pointer(8));
        }
        m.Put<uint64_t>(m.Pointer(12) + 0x98, external);
        TKMovesetHeader h; h.characterId = 28; strcpy_s(h.version_string, MOVESET_VERSION_STRING);
        return R::Serialize(R::CaptureComplete(m, Memory::Root, m.manager, m.container, resourceCharacter), h);
    };
    // Import the exact file emitted by the strict record converter (Convert), saved plain and with editor metadata.
    // Native and converted table counts/row ordering intentionally differ. (A direct T7 import now uses the
    // self-contained converter, whose output needs the memory-only route; see the refusal test below.)
    for (int saved:{1,2}) {
        Memory m; fixture(m,8);
        m.Put<uint64_t>(m.Pointer(12)+0x20,RetailNames::Encode(RetailNames::Hash("a")));
        m.Put<uint64_t>(m.Pointer(12)+0x28,0);
        auto bytes=T7RetailConvertibleFixture();
        const auto& t7=*reinterpret_cast<const TKMovesetHeader*>(bytes.data());
        const auto& blocks=*reinterpret_cast<const StructsT7::TKMovesetHeaderBlocks*>(bytes.data()+t7.block_list);
        const auto converted=T7ToRetailT8::Convert(t7,blocks,std::span<const Byte>(bytes).subspan(t7.moveset_data_start));
        bytes=converted;
        if(saved==2) {
            const auto& h=*reinterpret_cast<const TKMovesetHeader*>(converted.data());
            const auto payload=std::span<const Byte>(converted).subspan(h.moveset_data_start);
            auto metadata=RetailEditorData::Read(h,payload);metadata.notes="Kept by every editor save.";
            bytes=RetailEditorData::Serialize(h,payload,metadata);
            // Notes and provenance both survive a second editor save (read, then written again).
            const auto& sh=*reinterpret_cast<const TKMovesetHeader*>(bytes.data());
            const auto again=RetailEditorData::Read(sh,std::span<const Byte>(bytes).subspan(sh.moveset_data_start));
            Check(again.stored && again.notes=="Kept by every editor save." && R::Notes(bytes)==again.notes,"Editor save lost its notes or provenance");
            bytes=RetailEditorData::Serialize(sh,std::span<const Byte>(bytes).subspan(sh.moveset_data_start),again);
            Check(R::Notes(bytes)=="Kept by every editor save.","Second editor save lost the notes");
        }
        const auto original=bytes;
        GameAddressesFile addresses; addresses.Reload(); GameData game; game.SetProcess(&m,&addresses);
        auto info=RetailInfo(); game.SetCurrentGame(&info); ImporterT8 importer(m,game,&info); uint8_t progress=0;
        const auto result=importer.Import(bytes.data(),bytes.size(),m.player,ImportSettings_ApplyInstantly,progress);
        Check(result==ImportationErrcode_Successful,importer.LastError().c_str());
        Check(bytes==original && progress==100 && m.readUInt64(m.player+0x3b50)==importer.lastLoaded.address,
            "Converted import mutated its source or failed publication");
        const auto& h=*reinterpret_cast<const TKMovesetHeader*>(converted.data());
        Check(I::AuditImage(std::span<const Byte>(converted).subspan(h.moveset_data_start),m,importer.lastLoaded.address,
            importer.lastLoaded.size,m,Memory::Root,h.characterId,28,true).bytes>0,"Converted import failed independent image audit");
        if(saved==1) {
            // QA 1.0 (LiveEditConvertedIdentity): a live edit of a converted document keeps the target's identity
            // codes in edited move rows (+0xd0.., row parity dependent), as the import wrote them.
            const auto root=importer.lastLoaded.address; auto edited=bytes;
            const auto start=static_cast<size_t>(reinterpret_cast<const TKMovesetHeader*>(edited.data())->moveset_data_start);
            const auto table=R::Table(std::span<const Byte>(edited).subspan(start),12);
            const auto at=start+static_cast<size_t>(table.data()-(edited.data()+start)), count=table.size()/0x448;
            std::vector<Byte> expected(table.begin(),table.end()),scratch(R::SourceHeaderSize);
            I::ApplyConvertedIdentity(scratch,expected,28);
            std::vector<size_t> rows{0}; if(count>4) rows.push_back(4);
            for(auto row:rows) edited[at+row*0x448+0x120]^=1;
            Check(memcmp(table.data()+0xd0,expected.data()+0xd0,4)!=0,"Converted fixture has no placeholder identity to replace");
            const auto sync=RetailLiveEdit::Sync(m,m.player,root,bytes,edited,importer.Offsets());
            Check(!sync.structural && sync.records==rows.size(),sync.reason.c_str());
            const auto moves=m.readUInt64(root+0x230);
            for(auto row:rows) {
                std::vector<Byte> published(0x20); Check(m.readBytes(moves+row*0x448+0xd0,published.data(),published.size()),"Cannot read live-edited row");
                Check(!memcmp(published.data(),expected.data()+row*0x448+0xd0,published.size()),"Live edit reverted a converted move to the placeholder identity");
            }
        }
        if(saved==1) {
            // Converted moves whose carrier is not loaded are bound to idle, not to the carrier's own animation.
            std::vector<Byte> payload(converted.begin()+h.moveset_data_start,converted.end());
            const auto moves=R::Table(payload,12); const auto count=moves.size()/0x448;
            const auto offset=static_cast<size_t>(moves.data()-payload.data());
            const auto idle=*reinterpret_cast<const uint16_t*>(payload.data()+reinterpret_cast<const R::Header*>(payload.data())->sourceHeaderOffset+0xaa);
            const size_t carrierRow=idle==0 && count>1 ? 1 : 0;
            const uint64_t handle=(1ULL<<32)|77; memcpy(payload.data()+offset+carrierRow*0x448+0x50,&handle,8);
            std::vector<size_t> rows(count); for(size_t i=0;i<count;++i) rows[i]=100+i;
            Check(I::FallBackMissingCarriers(rows,payload,{})==0 && rows[carrierRow]==100+carrierRow,"Carrier fallback changed rows without missing carriers");
            Check(I::FallBackMissingCarriers(rows,payload,{77})==1 && rows[carrierRow]==rows[idle],"Missing carrier was not bound to the idle animation");
        }
    }
    // Memory-only converted import: the file's embedded PANM is registered in the target's own map; nothing is
    // prepared or attached, the engine's node list is untouched, and a repeat import reuses the entry.
    for (bool dryRun:{true,false}) {
        namespace RT=RetailT8Runtime;
        Memory m; fixture(m);
        const auto map=RT::DomainMap(m,m.container,28);
        m.Put<uint8_t>(map+0x48,1);
        const auto head=m.readUInt64(map+0x10),native=m.readUInt64(head+8);
        m.Put<uint64_t>(head,native);m.Put<uint64_t>(native,head);
        auto source=T7RetailConvertibleFixture();
        const auto& t7=*reinterpret_cast<const TKMovesetHeader*>(source.data());
        const auto& blocks=*reinterpret_cast<const StructsT7::TKMovesetHeaderBlocks*>(source.data()+t7.block_list);
        const auto converted=T7ToRetailT8::Convert(t7,blocks,std::span<const Byte>(source).subspan(t7.moveset_data_start));
        const auto& h=*reinterpret_cast<const TKMovesetHeader*>(converted.data());
        std::vector<Byte> payload(converted.begin()+h.moveset_data_start,converted.end());
        auto align=[](size_t v){return (v+7)&~size_t(7);};
        auto header=*reinterpret_cast<R::Header*>(payload.data());
        const auto last=*reinterpret_cast<const R::Entry*>(payload.data()+header.directoryOffset+(R::Tables.size()-1)*sizeof(R::Entry));
        payload.resize(align(last.offset+last.count*last.stride));
        const auto moves=*reinterpret_cast<const R::Entry*>(payload.data()+header.directoryOffset+12*sizeof(R::Entry));
        constexpr uint64_t fileHandle=0xef001c000badf00dULL;
        for(size_t row=0;row<moves.count;++row)memcpy(payload.data()+moves.offset+row*0x448+0x50,&fileHandle,8);
        RetailPanm::Animation animation{3,{{"Top",std::vector<RetailPanm::Sample>(3)},{"Hip",std::vector<RetailPanm::Sample>(1)}}};
        const auto panm=RetailPanm::Encode(animation);
        R::AssetDirectory directory{{'T','8','A','N'},1,payload.size()+sizeof(R::AssetDirectory),0,0};
        directory.dataOffset=align(directory.entriesOffset+sizeof(R::AssetEntry));
        R::AssetEntry asset{T7AnimToRetailT8::CustomAssetAddress,directory.dataOffset,panm.size(),0x0badf00d,0};
        const auto at=payload.size();payload.resize(directory.dataOffset);
        memcpy(payload.data()+at,&directory,sizeof(directory));memcpy(payload.data()+directory.entriesOffset,&asset,sizeof(asset));
        payload.insert(payload.end(),panm.begin(),panm.end());
        header.version=2;header.size=payload.size();memcpy(payload.data(),&header,sizeof(header));
        const auto file=R::Serialize(payload,h);
        auto import=[&](GameProcess& target,ImporterT8& importer){
            uint8_t progress=0;
            const auto result=importer.Import(file.data(),file.size(),m.player,ImportSettings_ApplyInstantly,progress);
            Check(result==ImportationErrcode_Successful,importer.LastError().c_str());
            Check(importer.MemoryOnly() && importer.CustomAnimationsMissing()==0 &&
                importer.MemoryOnlyDomain()==28 && importer.MemoryOnlyFallbackMoves()==0,"Converted file did not take the memory-only route");
            const auto table=target.readUInt64(importer.lastLoaded.address+R::Tables[12].pointerOffset);
            const auto handle=target.readUInt64(table+0x50);
            Check(handle>>40==0xef001cULL && handle==importer.MemoryOnlyHandles().at(0),"Moves do not use the registered handle");
            const auto resource=RT::ResolveAnimation(target,m.container,28,static_cast<uint32_t>(handle));
            std::vector<Byte> copy(panm.size());
            Check(resource && target.readBytes(RT::Panm(target,m.container,28,static_cast<uint32_t>(handle),0,panm.size()),copy.data(),copy.size()) && copy==panm,
                "Registered animation does not resolve to the file's PANM");
            RetailT8Import::MemoryOnlyAudit expected{importer.MemoryOnlyContainer(),importer.MemoryOnlyDomain(),importer.MemoryOnlyStringRow(),importer.MemoryOnlyHandles()};
            expected.compiledVtable=importer.Engine().compiledVtable;
            const auto audit=I::AuditImage(payload,target,importer.lastLoaded.address,importer.lastLoaded.size,m,Memory::Root,h.characterId,28,true,nullptr,nullptr,&expected);
            Check(audit.animations==1 && audit.bytes>0,"Memory-only import failed its image audit");
            std::vector<gameAddr> list;for(auto n=target.readUInt64(head);n!=head;n=target.readUInt64(n))list.push_back(n);
            Check(list==std::vector<gameAddr>{native} && target.readUInt64(map+0x18)==1,"Memory-only import changed the engine's node list");
            return handle;
        };
        GameAddressesFile addresses; addresses.Reload(); GameData game; auto info=RetailInfo();
        if(dryRun){
            const auto before=m.regions;
            RetailImportShadow shadow(m);game.SetProcess(&shadow,&addresses);game.SetCurrentGame(&info);
            ImporterT8 importer(shadow,game,&info);
            import(shadow,importer);
            Check(importer.PublishedAnimations()==1 && shadow.writes>0 && m.regions==before,"Shadow memory-only import wrote to the game");
        }else{
            game.SetProcess(&m,&addresses);game.SetCurrentGame(&info);
            ImporterT8 importer(m,game,&info);
            const auto first=import(m,importer);
            Check(importer.PublishedAnimations()==1 && importer.ReusedAnimations()==0,"First memory-only import did not publish");
            m.Put<uint64_t>(m.player+0x3b50,Memory::Root); // the target's own moveset again
            for(size_t i=1;i<5;++i)m.Put<uint64_t>(m.player+0x3b50+i*8,Memory::Root);
            const auto second=import(m,importer);
            Check(second==first && importer.PublishedAnimations()==0 && importer.ReusedAnimations()==1,"Repeat import did not reuse its entry");
        }
    }
    // v4 native snapshots: captured strings and external cancel lists; v3 compatibility; memory-only import and audit.
    for (bool dryRun:{true,false}) {
        namespace RT=RetailT8Runtime;
        Memory m; fixture(m);
        const auto map=RT::DomainMap(m,m.container,28);
        m.Put<uint8_t>(map+0x48,1);
        const auto head=m.readUInt64(map+0x10),native=m.readUInt64(head+8);
        m.Put<uint64_t>(head,native);m.Put<uint64_t>(native,head);
        auto name=m.Alloc(32),animation=m.Alloc(32);
        memcpy(m.regions[name].data(),"Kz_lp_test",11);memcpy(m.regions[animation].data(),"Kz_lp_anim",11);
        m.Put<uint64_t>(m.Pointer(12)+0x40,name);m.Put<uint64_t>(m.Pointer(12)+0x48,animation);
        m.Put<uint64_t>(m.Pointer(12)+0x50,0xef001c0012345678ULL);
        { // a finished native resource, as in the live game (check L1): compiled object, control block, done request
            const auto resource=RT::ResolveAnimation(m,m.container,28,0x12345678);
            const auto object=m.Alloc(0xf0),ctrl=m.Alloc(0x20),request=m.Alloc(0x40);
            m.Put<uint64_t>(object,ShippedEngine(m.mainModule.address).compiledVtable);m.Put<uint32_t>(request+0x30,2);
            m.Put<uint64_t>(resource+0x190,object);m.Put<uint64_t>(resource+0x198,ctrl);m.Put<uint64_t>(resource+0x140,request);
        }
        auto payload=R::CaptureComplete(m,Memory::Root,m.manager,m.container,28);
        const auto data=R::ReadExtension(payload);
        Check(reinterpret_cast<const R::Header*>(payload.data())->version==4 && data && data->names.size()==1 &&
            data->names[0]==std::string("Kz_lp_test") && data->animations[0]==std::string("Kz_lp_anim") &&
            data->cancels.size()==1 && data->cancels.begin()->first==std::pair<uint32_t,uint32_t>(0,0x98) &&
            data->cancels.begin()->second.size()==80,"v4 capture lost move strings or the external cancel list");
        // Checksum, layout and trailing bytes are validated; v1-v3 views stay readable without the extension.
        {
            const auto block=R::ExtensionBytes(payload);
            const auto at=static_cast<size_t>(block.data()-payload.data());
            auto corrupt=payload;corrupt[at+sizeof(R::ExtensionHeader)]^=1;
            bool rejected=false;try{R::Validate(corrupt);}catch(const std::exception&){rejected=true;}
            Check(rejected,"Corrupt v4 extension accepted");
            std::vector<Byte> v3(payload.begin(),payload.begin()+at);
            while(v3.size()%8)v3.pop_back();
            auto header=*reinterpret_cast<R::Header*>(v3.data());header.version=3;header.size=v3.size();memcpy(v3.data(),&header,sizeof(header));
            R::Validate(v3);
            Check(!R::ReadExtension(v3) && R::ExtensionBytes(v3).empty() && R::Assets(v3).size()==R::Assets(payload).size(),
                "v3 view of a v4 snapshot changed");
            auto again=v3;R::AppendExtension(again,block);
            Check(again==payload,"Extension does not round-trip through AppendExtension");
            auto trailing=payload;trailing.resize(trailing.size()+8);
            rejected=false;try{R::Validate(trailing);}catch(const std::exception&){rejected=true;}
            Check(rejected,"Trailing bytes after a v4 extension accepted");
            // A T8RX block whose string pool starts past its end (size 72, two rows) and declares a huge pool: refused
            // before any string is read (it used to pass the layout check through unsigned wrap-around, then read far OOB).
            {
                auto hostile=v3;const auto start=hostile.size();
                R::ExtensionHeader x{{'T','8','R','X'},1,72,2,80,0x100000000ULL,0x100000050ULL,0,0,0};
                const uint32_t refs[2]={0x7fffff00u,UINT32_MAX};
                hostile.resize(start+72);memcpy(hostile.data()+start+sizeof(x),refs,sizeof(refs));
                x.crc32=Helpers::CalculateCrc32(hostile.data()+start+sizeof(x),sizeof(refs));memcpy(hostile.data()+start,&x,sizeof(x));
                auto hostileHeader=*reinterpret_cast<R::Header*>(hostile.data());hostileHeader.version=4;hostileHeader.size=hostile.size();
                memcpy(hostile.data(),&hostileHeader,sizeof(hostileHeader));
                rejected=false;try{R::Validate(hostile);}catch(const std::exception&){rejected=true;}
                Check(rejected,"T8RX extension with its string pool past its end accepted");
            }
        }
        // The live source's list changes after export: a memory-only import must use the captured one.
        const auto external=m.readUInt64(m.Pointer(12)+0x98);
        const auto capturedList=data->cancels.begin()->second;
        m.Put<uint64_t>(external,0x777);
        TKMovesetHeader outer{};outer.characterId=28;strcpy_s(outer.version_string,MOVESET_VERSION_STRING);
        const auto file=R::Serialize(payload,outer);
        auto run=[&](GameProcess& target,ImporterT8& importer){
            uint8_t progress=0;
            const auto result=importer.Import(file.data(),file.size(),m.player,ImportSettings_ApplyInstantly,progress);
            Check(result==ImportationErrcode_Successful,importer.LastError().c_str());
            Check(importer.MemoryOnly() && importer.Native() && importer.NativeBoundAnimations()==1 &&
                importer.PublishedAnimations()==0 && importer.MemoryOnlyFallbackMoves()==0,"v4 native snapshot did not take the memory-only route");
            const auto table=target.readUInt64(importer.lastLoaded.address+R::Tables[12].pointerOffset);
            Check(target.readUInt64(table+0x50)==0xef001c0012345678ULL,"Native handle not bound to the target's resident animation");
            char text[11]{};target.readBytes(target.readUInt64(table+0x40),text,sizeof(text));
            Check(!strcmp(text,"Kz_lp_test"),"Move name was not localised");
            const auto list=target.readUInt64(table+0x98);
            Check(list>=importer.lastLoaded.address && list<importer.lastLoaded.address+importer.lastLoaded.size &&
                target.readUInt64(list)==0x123,"External cancel list was not taken from the file");
            RetailT8Import::MemoryOnlyAudit expected{importer.MemoryOnlyContainer(),importer.MemoryOnlyDomain(),importer.MemoryOnlyStringRow(),
                importer.MemoryOnlyHandles(),importer.MemoryOnlyNames(),importer.MemoryOnlyAnimations(),&importer.Native()->cancels};
            expected.compiledVtable=importer.Engine().compiledVtable;
            const auto audit=I::AuditImage(payload,target,importer.lastLoaded.address,importer.lastLoaded.size,m,Memory::Root,28,28,false,nullptr,nullptr,&expected);
            Check(audit.animations==1 && audit.externalLists==1,"v4 native memory-only import failed its image audit");
        };
        (void)capturedList;
        GameAddressesFile addresses; addresses.Reload(); GameData game; auto info=RetailInfo();
        if(dryRun){
            const auto before=m.regions;
            RetailImportShadow shadow(m);game.SetProcess(&shadow,&addresses);game.SetCurrentGame(&info);
            ImporterT8 importer(shadow,game,&info);run(shadow,importer);
            Check(m.regions==before,"Shadow v4 native import wrote to the game");
            // A move whose animation is neither in the file nor in the game's archive: the import goes through and that
            // move plays the target's idle animation. It never keeps the unresolved handle.
            auto missing=payload;
            const auto moves=static_cast<size_t>(R::Table(missing,12).data()-missing.data());
            const uint64_t unknown=0xef001c0099999999ULL;memcpy(missing.data()+moves+0x50,&unknown,8);
            const auto missingFile=R::Serialize(missing,outer);
            RetailImportShadow lenient(m);game.SetProcess(&lenient,&addresses);
            ImporterT8 lenientImporter(lenient,game,&info);uint8_t progress=0;
            Check(lenientImporter.Import(missingFile.data(),missingFile.size(),m.player,ImportSettings_ApplyInstantly,progress)==ImportationErrcode_Successful &&
                lenientImporter.MemoryOnlyFallbackMoves()==1 && lenientImporter.UnavailableAnimations().count(0x99999999) &&
                lenientImporter.MemoryOnlyHandles().at(0)!=unknown && m.regions==before,
                ("An animation missing from the file and the archive did not fall back to idle: "+lenientImporter.LastError()).c_str());
        }else{
            game.SetProcess(&m,&addresses);game.SetCurrentGame(&info);
            ImporterT8 importer(m,game,&info);run(m,importer);
            // A v3 file (no extension) keeps the legacy route even with a populated map.
            m.Put<uint64_t>(external,0x123);
            for(size_t i=0;i<5;++i)m.Put<uint64_t>(m.player+0x3b50+i*8,Memory::Root);
            const auto block=R::ExtensionBytes(payload);
            std::vector<Byte> v3(payload.begin(),payload.begin()+(block.data()-payload.data()));
            while(v3.size()%8)v3.pop_back();
            auto header=*reinterpret_cast<R::Header*>(v3.data());header.version=3;header.size=v3.size();memcpy(v3.data(),&header,sizeof(header));
            const auto legacy=R::Serialize(v3,outer);uint8_t progress=0;
            Check(importer.Import(legacy.data(),legacy.size(),m.player,ImportSettings_ApplyInstantly,progress)==ImportationErrcode_Successful &&
                !importer.MemoryOnly() && !importer.Native(),"v3 snapshot left the legacy import route");
        }
    }
    for (bool missingMapping:{false,true}) {
        Memory m; fixture(m,8); const auto before=m.regions;
        auto bytes=T7RetailConvertibleFixture();
        if(missingMapping) {
            const auto& h=*reinterpret_cast<const TKMovesetHeader*>(bytes.data());
            const auto& blocks=*reinterpret_cast<const StructsT7::TKMovesetHeaderBlocks*>(bytes.data()+h.block_list);
            const auto& info=*reinterpret_cast<const StructsT7_gameAddr::MovesetInfo*>(bytes.data()+h.moveset_data_start);
            uint32_t unsupported=217;
            memcpy(bytes.data()+h.moveset_data_start+blocks.movesetBlock+info.table.requirement,&unsupported,4);
        }
        GameAddressesFile addresses; addresses.Reload(); GameData game; game.SetProcess(&m,&addresses);
        auto info=RetailInfo(); game.SetCurrentGame(&info); ImporterT8 importer(m,game,&info); uint8_t progress=0;
        // Direct T7 imports use the self-contained converter (no donor). This fixture target has no ready animation
        // map, so the memory-only route is out: the import falls back to the target's own moveset (never refused).
        (void)before;
        Check(importer.Import(bytes.data(),bytes.size(),m.player,ImportSettings_ApplyInstantly,progress)==ImportationErrcode_Successful &&
            importer.TargetFallback() && !importer.MemoryOnly() && m.readUInt64(m.player+0x3b50)!=Memory::Root,
            (std::string("T7 import without a ready animation map was not imported through the target fallback: ")+importer.LastError()).c_str());
    }
    auto run = [&](Memory& m, std::span<const Byte> bytes, ImportSettings settings = ImportSettings_ApplyInstantly) {
        GameAddressesFile addresses; addresses.Reload(); GameData game; game.SetProcess(&m, &addresses);
        auto info = RetailInfo(); game.SetCurrentGame(&info);
        ImporterT8 importer(m, game, &info); uint8_t progress = 0;
        auto code = importer.Import(bytes.data(), bytes.size(), m.player, settings, progress);
        if (code == ImportationErrcode_Successful) {
            auto root = importer.lastLoaded.address;
            Check(progress == 100 && root != Memory::Root && m.readUInt64(m.player + 0x3b50) == root,
                "Retail production importer failed publication");
            auto move = m.readUInt64(root + 0x230), cancel = m.readUInt64(move + 0x98);
            Check(m.readUInt64(cancel) == 0x123 && m.readUInt64(cancel + 40) == 0x8000 &&
                m.readUInt64(cancel + 8) == m.readUInt64(root + 0x180) &&
                m.readUInt64(cancel + 16) == m.readUInt64(root + 0x1f0), "External cancel list was not copied/relocated");
            Check(m.readUInt64(move + 0x50) == 0x12345678 && m.readUInt32(move + 0x120) == 60,
                "Runtime animation binding changed move data");
            if (settings & ImportSettings_ApplyInstantly)
                Check(m.readUInt64(m.player + 0x28f0) == move && m.readUInt32(m.player + 0x390) == 60 &&
                    m.readUInt64(m.player + 0x3b70) == root, "Retail instant import queued the wrong move/context");
            importer.CleanupUnusedMovesets(); Check(m.regions.contains(root), "Retail cleanup freed live resource dependencies");
        }
        return code;
    };
    {
        // QA 1.0 live-edit regressions (RetailLiveEdit::Sync bypasses Prepare's graph checks).
        Memory m; const auto file=fixture(m);
        Check(run(m,file)==ImportationErrcode_Successful,"Live-edit fixture import failed");
        const auto root=m.readUInt64(m.player+0x3b50), moves=m.readUInt64(root+0x230);
        const auto start=static_cast<size_t>(reinterpret_cast<const TKMovesetHeader*>(file.data())->moveset_data_start);
        auto offset=[&](size_t index) {
            const auto payload=std::span<const Byte>(file).subspan(start);
            return start+static_cast<size_t>(R::Table(payload,index).data()-payload.data());
        };
        auto put=[](std::vector<Byte>& bytes,size_t at,auto value){memcpy(bytes.data()+at,&value,sizeof(value));};
        const auto published=m.regions;
        // LiveEditRejectsChangedOutOfTablePointer: a new external reference used to be dropped silently (old kept).
        auto external=file; put(external,offset(12)+0x98,uint64_t(0x7777770000ULL));
        auto sync=RetailLiveEdit::Sync(m,m.player,root,file,external,FixtureOffsets);
        Check(sync.structural && m.regions==published,"Live edit silently kept a stale external reference");
        // LiveEditRejectsMisalignedPointer: a misaligned in-table pointer used to be relocated as is.
        auto misaligned=file; put(misaligned,offset(12)+0x110,m.Pointer(2)+1);
        sync=RetailLiveEdit::Sync(m,m.player,root,file,misaligned,FixtureOffsets);
        Check(sync.structural && m.regions==published,"Live edit wrote a misaligned table pointer");
        // LiveEditRejectsInputSequenceOverrun: count beyond the input table (Prepare rejects it on import).
        auto overrun=file; put(overrun,offset(14)+8,m.Pointer(15)); put(overrun,offset(14)+2,uint16_t(2));
        sync=RetailLiveEdit::Sync(m,m.player,root,file,overrun,FixtureOffsets);
        Check(sync.structural && m.regions==published,"Live edit wrote an input sequence beyond its table");
        // LiveEditIsAtomic: a failed write leaves no record of the edit applied.
        auto edited=file; put(edited,offset(12)+0x120,uint32_t(61)); put(edited,offset(9)+4,uint32_t(0x55));
        m.failWrite=m.writes+2; bool failed=false;
        try { RetailLiveEdit::Sync(m,m.player,root,file,edited,FixtureOffsets); } catch(const std::exception&) { failed=true; }
        Check(failed && m.readUInt32(moves+0x120)==60 && m.readUInt32(m.readUInt64(root+0x200)+4)==0,"Failed live edit was partially applied");
        m.failWrite=0;
        sync=RetailLiveEdit::Sync(m,m.player,root,file,edited,FixtureOffsets);
        Check(!sync.structural && sync.records==2 && m.readUInt32(moves+0x120)==61 && m.readUInt32(m.readUInt64(root+0x200)+4)==0x55,
            "Valid live edit was not applied");
    }
    {
        // Live edition of a T7 moveset imported into retail T8 (T7RetailLive): the T7 editor's live writers write into
        // the T7 shadow (pointers as addresses), the edited rows are converted alone and written into the published
        // moveset; an edit one row cannot carry stops with the reason and writes nothing.
        namespace S7 = StructsT7;
        Memory m; fixture(m, 8);
        const auto t7 = T7RetailConvertibleFixture();
        T7RetailLive live(t7);
        GameAddressesFile addresses; addresses.Reload(); GameData game; game.SetProcess(&m, &addresses);
        auto info = RetailInfo(); game.SetCurrentGame(&info);
        ImporterT8 importer(m, game, &info); uint8_t progress = 0;
        Check(importer.Import(live.Container().data(), live.Container().size(), m.player, ImportSettings_ApplyInstantly, progress) == ImportationErrcode_Successful,
            (std::string("T7 live edition container did not import: ") + importer.LastError()).c_str());
        Check(importer.GetCharacterAddress(0) == m.player, "Fixture player is not slot 0");
        const auto root = importer.lastLoaded.address;
        live.Bind(root, 0, &info);
        const auto& header = *reinterpret_cast<const TKMovesetHeader*>(t7.data());
        S7::TKMovesetHeaderBlocks blocks; memcpy(&blocks, t7.data() + header.block_list, sizeof(blocks));
        const auto& table = reinterpret_cast<const StructsT7_gameAddr::MovesetInfo*>(t7.data() + header.moveset_data_start)->table;
        const auto block = live.Shadow().Address() + blocks.movesetBlock;
        auto& shadow = live.Shadow();
        // A plain field: the T7 animation length is the retail move's animation_length (+0x120).
        const auto movesAddress = m.readUInt64(root + R::Tables[12].pointerOffset);
        const auto length = m.readUInt32(movesAddress + 0x120);
        shadow.writeUInt32(block + table.move + offsetof(S7::Move, anim_len), length + 7);
        live.Publish(m, game);
        Check(live.LastRecords() == 1 && m.readUInt32(movesAddress + 0x120) == length + 7, "Live T7 move edit was not written into the retail moveset");
        // A reference written as an address is stored as the T7 row index; the edited cancel row is converted and written.
        if (table.cancelCount && table.requirementCount) {
            shadow.writeUInt64(block + table.cancel + offsetof(S7::Cancel, requirements_addr), block + table.requirement);
            Check(shadow.Invalid().empty(), "A valid reference write was refused");
            uint64_t index; memcpy(&index, shadow.Row(6, 0).data() + offsetof(StructsT7_gameAddr::Cancel, requirements_addr), 8);
            Check(index == 0, "The shadow kept a reference as an address");
            live.Publish(m, game);
            const auto cancels = m.readUInt64(root + R::Tables[6].pointerOffset), requirements = m.readUInt64(root + R::Tables[1].pointerOffset);
            Check(m.readUInt64(cancels + 8) == requirements, "Live T7 cancel reference was not relocated into the retail moveset");
        }
        {
            // Apply keeps live edition bound: the applied moveset's rows that differ are published like live edits.
            auto applied = t7;
            auto* move = reinterpret_cast<StructsT7_gameAddr::Move*>(applied.data() + header.moveset_data_start + blocks.movesetBlock + table.move);
            move->anim_len = length + 11;
            shadow.SyncFrom(applied);
            live.Publish(m, game);
            Check(shadow.Invalid().empty() && live.LastRecords() == 1 && m.readUInt32(movesAddress + 0x120) == length + 11,
                "An applied T7 row was not published into the retail moveset");
            // A list that changed size is invalid for a single-row update.
            T7RetailLive resized(t7);
            auto grown = t7;
            reinterpret_cast<StructsT7_gameAddr::MovesetInfo*>(grown.data() + header.moveset_data_start)->table.entries[1].listCount += 1;
            resized.Shadow().SyncFrom(grown);
            Check(!resized.Shadow().Invalid().empty(), "A resized list was published row by row");
        }
        // A renamed move needs a new import: nothing is written.
        const auto published = m.regions;
        uint64_t name; shadow.readBytes(block + table.move + offsetof(StructsT7_gameAddr::Move, name_addr), &name, 8);
        shadow.writeUInt64(block + table.move + offsetof(StructsT7_gameAddr::Move, name_addr), live.Shadow().Address() + blocks.nameBlock + name + 1);
        bool stopped = false;
        try { live.Publish(m, game); } catch (const std::exception& error) { stopped = std::string(error.what()).find("import") != std::string::npos; }
        Check(stopped && m.regions == published, "A renamed move did not stop live edition, or wrote records");
        // An address outside every table marks the shadow invalid.
        T7RetailLive other(t7);
        other.Shadow().writeUInt64(block + table.move + offsetof(StructsT7_gameAddr::Move, cancel_addr), 0x1234);
        Check(!other.Shadow().Invalid().empty(), "A reference outside its table was accepted");
    }
    for(unsigned fail=0;fail<=7;++fail) {
        Memory m;fixture(m);m.Put<uint16_t>(Memory::Root+0xaa,0);const auto before=m.regions;
        GameAddressesFile addresses;addresses.Reload();GameData game;game.SetProcess(&m,&addresses);auto info=RetailInfo();game.SetCurrentGame(&info);
        auto state=RetailT8Preview::Read(m,game,info,0);
        Check(state.current==0 && state.count==1,"Current move preview did not follow actual move pointer");
        m.failWrite=fail;bool failed=false;
        try {Check(RetailT8Preview::Queue(m,game,info,0,Memory::Root,0x8001)==0,"Preview alias resolved to wrong move");}
        catch(const std::exception&){failed=true;}
        if(fail)Check(failed && m.regions==before,"Failed move preview did not roll back all writes");
        else Check(!failed && m.readUInt64(m.player+0x3b50)==Memory::Root && m.readUInt64(m.player+0x28f0)==m.Pointer(12) &&
            m.readUInt32(m.player+0x290c)==0x8001 && m.readUInt32(m.player+0x390)==60,"Move preview queued incorrect context/timer");
        Check(m.pauses==1 && m.resumes==1,"Move preview left process paused");
    }
    for(unsigned failure=0;failure<6;++failure) {
        Memory m;fixture(m);GameAddressesFile addresses;addresses.Reload();GameData game;game.SetProcess(&m,&addresses);auto info=RetailInfo();game.SetCurrentGame(&info);
        if(failure==3)m.changeAtPause=true;
        if(failure==4)m.changeRoundAtPause=true;
        if(failure==5)m.failPause=true;
        bool failed=false;try{RetailT8Preview::Queue(m,game,info,failure==0?2:0,failure==1?Memory::Root+8:Memory::Root,failure==2?UINT32_MAX:0);}
        catch(const std::exception&){failed=true;}
        Check(failed && m.writes==0,"Invalid/stale move preview wrote game memory");
    }
    {
        // Without memory-only registration, an export whose source is not loaded in the game binds to the target's own
        // moveset (target fallback): it is imported, never refused.
        Memory m;
        auto bytes = fixture(m);
        auto* outer = reinterpret_cast<TKMovesetHeader*>(bytes.data());
        auto* snapshot = reinterpret_cast<R::Header*>(bytes.data() + outer->moveset_data_start);
        snapshot->sourceAddress = 0x111000;
        auto target = m.Alloc(R::SourceHeaderSize);
        m.regions[target] = m.regions.at(Memory::Root);
        m.Put<uint32_t>(target + 4,20240101);
        m.Put<uint64_t>(m.player + 0x3b50,target);
        GameAddressesFile addresses; addresses.Reload(); GameData game; game.SetProcess(&m,&addresses);
        auto info = RetailInfo(); game.SetCurrentGame(&info);
        ImporterT8 importer(m,game,&info); uint8_t progress=0;
        outer->crc32=Helpers::CalculateCrc32(bytes.data()+outer->moveset_data_start,bytes.size()-outer->moveset_data_start);
        Check(importer.Import(bytes.data(),bytes.size(),m.player,ImportSettings_ApplyInstantly,progress)==ImportationErrcode_Successful &&
            importer.TargetFallback() && importer.BindingRoot()==target && m.readUInt64(m.player+0x3b50)!=target,
            (std::string("Import without a loaded source was not imported through the target fallback: ")+importer.LastError()).c_str());
    }
    {
        Memory m; fixture(m);
        const uint64_t handle = 0xef001c0012345678ULL;
        m.Put<uint64_t>(m.Pointer(12) + 0x50, handle);
        m.Put<uint32_t>(m.player + 0x168, 29); m.Put<uint32_t>(m.player + 0x16c, 29);
        m.Put<uint32_t>(m.Pointer(1) + 4, 29);
        TKMovesetHeader header; header.characterId = 29; strcpy_s(header.version_string, MOVESET_VERSION_STRING);
        auto bytes = R::Serialize(R::CaptureComplete(m, Memory::Root, m.manager, m.container, 29), header);
        RetailImportShadow shadow(m);
        GameAddressesFile addresses; addresses.Reload(); GameData game; game.SetProcess(&shadow, &addresses);
        auto info = RetailInfo(); game.SetCurrentGame(&info); ImporterT8 importer(shadow, game, &info); uint8_t progress = 0;
        Check(importer.Import(bytes.data(), bytes.size(), m.player, ImportSettings_ApplyInstantly, progress) == ImportationErrcode_Successful,
            "Re-exported cross-character moveset resolved resources using its visual identity");
        const auto moves = shadow.readUInt64(importer.lastLoaded.address + 0x230);
        Check(shadow.readUInt64(moves + 0x50) == handle && importer.CharacterFixes() == 0 && m.writes == 0,
            "Re-export changed animation handles or adapted an already-correct character requirement");
        Check(I::AuditImage(std::span<const Byte>(bytes).subspan(reinterpret_cast<const TKMovesetHeader*>(bytes.data())->moveset_data_start),
            shadow, importer.lastLoaded.address, importer.lastLoaded.size, m, Memory::Root).bytes > 0,
            "Re-exported cross-character moveset failed independent image audit");
    }
    {
        Memory m; auto bytes = fixture(m);
        m.Put<uint64_t>(m.player + 0x3b50, 0x777770000ULL);
        Check(run(m, bytes) == ImportationErrcode_Successful,
            "Unreadable optional source candidate poisoned a valid retained-root import");
    }
    for (bool optional : {false, true}) {
        Memory m; const auto value = m.Alloc(4); Byte byte = 0;
        GameProcess::WriteBatch transaction(m);
        Check(!(optional ? m.probeBytes(0xdeadbeef, &byte, 1) : m.readBytes(0xdeadbeef, &byte, 1)),
            "Unreadable probe unexpectedly succeeded");
        const bool staged = m.writeUInt32(value, 42);
        const auto result = transaction.Commit();
        Check(staged == optional && result.ok == optional && m.writes == (optional ? 1 : 0),
            "Optional/required read failure handling weakened transaction safety");
    }
    {
        Memory m; fixture(m);
        auto captured = m.regions.at(m.Pointer(12));
        captured.insert(captured.end(), m.regions.at(m.Pointer(12)).begin(), m.regions.at(m.Pointer(12)).end());
        auto loaded = captured;
        // A native runtime replacement changes one animation, while another
        // row still owns the original resource. Its move identity must stay put.
        memcpy(loaded.data() + 0x20, loaded.data(), 16);
        const auto bindings = I::BindAnimations(captured, loaded);
        Check(bindings == std::vector<size_t>({1, 1}), "Retail shared animation binding used replacement resource");
        {
            // A donor that does not play an animation binds that move to its idle row; a changed identity is only
            // counted. Neither refuses the import.
            auto missing = loaded; memcpy(missing.data() + 0x448 + 0x20, missing.data(), 16);
            I::BindStats stats;
            const auto rows = I::BindAnimations(captured, missing, false, nullptr, 1, &stats);
            // No donor row plays the captured animation any more: both captured rows fall back to the idle row (1).
            Check(stats.missingAnimations == 2 && rows == std::vector<size_t>({1, 1}), "Retail animation binding did not fall back for a missing resource");
            auto renamed = loaded; memcpy(renamed.data(), renamed.data() + 0x448 + 0x20, 16);
            stats = {};
            I::BindAnimations(captured, renamed, false, nullptr, 0, &stats);
            Check(stats.identityMismatches == 1, "Retail animation binding did not count a changed identity");
        }
        for (int failure = 0; failure < 2; ++failure) {
            auto invalid = loaded;
            if (failure == 0) invalid[0x20] ^= 1;
            if (failure == 1) invalid.pop_back();
            bool rejected = false;
            try { I::BindAnimations(captured, invalid); } catch (const std::exception&) { rejected = true; }
            Check(rejected, "Retail animation binding accepted corrupt records");
        }
    }
    for (auto codec : {TKMovesetCompressionType_None, TKMovesetCompressionType_LZ4, TKMovesetCompressionType_LZMA}) {
        Memory m; auto bytes = fixture(m); uint64_t size = 0;
        std::unique_ptr<Byte[]> encoded(CompressionUtils::RAW::Moveset::Compress(bytes.data(), bytes.size(), codec, size));
        Check(encoded && run(m, {encoded.get(), static_cast<size_t>(size)}) == ImportationErrcode_Successful,
            "Retail production import failed a codec");
        Check(m.pauses == 1 && m.resumes == 1, "Retail publication did not balance pause/resume");
    }
    {
        Memory m; auto bytes = fixture(m);
        namespace RT = RetailT8Runtime;
        auto q = [&](gameAddr at) { return m.readUInt64(at); };
        const auto node = RT::Lookup(m, q(m.container + 8), q(m.container + 0x18), q(m.container + 0x30), 28);
        const auto map = q(q(node + 0x18));
        auto job = m.Alloc(16), resource = m.Alloc(0x128), owner = m.Alloc(16), archive = m.Alloc(0x200);
        m.Put<uint64_t>(map, job); m.Put<uint64_t>(job + 8, resource);
        m.Put<uint64_t>(resource + 0x118, owner); m.Put<uint64_t>(owner, archive); m.Put<uint64_t>(owner + 8, 0x200);
        m.Put<uint32_t>(archive + 4, 1); m.Put<uint64_t>(archive + 0x38, archive + 0x98);
        m.Put<uint32_t>(archive + 0x98, 0x12345678); m.Put<uint64_t>(archive + 0xa0, archive + 0xd0);
        memcpy(m.regions[archive].data() + 0xd0, m.regions[m.panm].data(), 128);
        Check(!RT::ArchivePanm(m, map, 0x12345678, 4, 128), "Native ANMBIN rejected an empty category");
        m.Put<uint32_t>(m.panm + 4, 0); // Compiled cache has an obsolete raw source pointer.
        Check(RT::Panm(m, m.container, 28, 0x12345678, 0, 128) == archive + 0xd0 &&
            run(m, bytes) == ImportationErrcode_Successful, "Retail import could not validate the retained native ANMBIN source");
        for (int failure = 0; failure < 4; ++failure) {
            m.Put<uint32_t>(archive + 4, failure == 0 ? 100001 : 1);
            m.Put<uint64_t>(archive + 0x38, failure == 1 ? archive + 0x1f0 : archive + 0x98);
            m.Put<uint64_t>(archive + 0xa0, failure == 2 ? archive + 0x1f0 : archive + 0xd0);
            bool rejected = false;
            try { RT::ArchivePanm(m, map, 0x12345678, failure == 3 ? 6 : 0, 128); }
            catch (const std::exception&) { rejected = true; }
            Check(rejected, "Native ANMBIN lookup accepted invalid pool/category/blob extents");
        }
    }
    {
        Memory m; fixture(m);
        const auto original = m.regions.at(m.Pointer(12));
        m.regions[m.Pointer(12)].insert(m.regions[m.Pointer(12)].end(), original.begin(), original.end());
        m.Put<uint64_t>(Memory::Root + 0x238, 2);
        TKMovesetHeader header; header.characterId = 28; strcpy_s(header.version_string, MOVESET_VERSION_STRING);
        auto bytes = R::Serialize(R::CaptureComplete(m, Memory::Root, m.manager, m.container, 28), header);
        memcpy(m.regions[m.Pointer(12)].data() + 0x20, original.data(), 16);
        m.Put<uint64_t>(m.Pointer(12) + 0x50, 0xdeadbeef);
        GameAddressesFile addresses; addresses.Reload(); GameData game; game.SetProcess(&m, &addresses);
        auto info = RetailInfo(); game.SetCurrentGame(&info);
        RetailImportShadow shadow(m); game.SetProcess(&shadow, &addresses);
        ImporterT8 importer(shadow, game, &info); uint8_t progress = 0;
        Check(importer.Import(bytes.data(), bytes.size(), m.player, ImportSettings_ApplyInstantly, progress) == ImportationErrcode_Successful,
            "Retail import rejected a captured animation retained in another native move");
        auto importedMoves = shadow.readUInt64(importer.lastLoaded.address + 0x230);
        Check(shadow.readUInt64(importedMoves + 0x50) == 0x12345678 && m.writes == 0,
            "Retail import bound runtime replacement instead of captured animation");
        const auto payload = std::span<const Byte>(bytes).subspan(reinterpret_cast<const TKMovesetHeader*>(bytes.data())->moveset_data_start);
        Check(I::AuditImage(payload, shadow, importer.lastLoaded.address, importer.lastLoaded.size, m, Memory::Root).bytes > 0,
            "Retail image audit rejected animation identity binding");
        shadow.writeUInt64(importedMoves + 0x50, 0xdeadbeef);
        bool rejected = false;
        try { I::AuditImage(payload, shadow, importer.lastLoaded.address, importer.lastLoaded.size, m, Memory::Root); }
        catch (const std::exception&) { rejected = true; }
        Check(rejected, "Retail image audit accepted the wrong substituted animation handle");
    }
    {
        Memory m;auto bytes=fixture(m);auto info=RetailInfo();GameAddressesFile addresses;addresses.Reload();GameData game;game.SetProcess(&m,&addresses);game.SetCurrentGame(&info);
        EditorT8Retail editor(m,game,&info);auto copy=static_cast<Byte*>(malloc(bytes.size()));memcpy(copy,bytes.data(),bytes.size());Check(editor.LoadMoveset(copy,bytes.size()),"Native editor fixture failed");
        auto move=editor.CreateNew(EditorT8Retail::Type(12));editor.RenameMove(move,"CUSTOM_NativeImport");editor.CreateNew(EditorT8Retail::Type(1));editor.CreateNew(EditorT8Retail::Type(6));
        editor.EditRows(12,0,{int(move),0},{});bytes=editor.BuildSaveSnapshot();const auto& h=*reinterpret_cast<const TKMovesetHeader*>(bytes.data());auto payload=std::span<const Byte>(bytes).subspan(h.moveset_data_start);auto metadata=RetailEditorData::Read(h,payload);
        RetailImportShadow shadow(m);game.SetProcess(&shadow,&addresses);ImporterT8 importer(shadow,game,&info);uint8_t progress=0;
        Check(importer.Import(bytes.data(),bytes.size(),m.player,ImportSettings_ApplyInstantly,progress)==ImportationErrcode_Successful,"Structurally edited native import failed");
        Check(I::AuditImage(payload,shadow,importer.lastLoaded.address,importer.lastLoaded.size,m,Memory::Root,28,28,false,&metadata).bytes>0 && m.writes==0,"Structural native image audit failed");
        auto name=shadow.readUInt64(shadow.readUInt64(importer.lastLoaded.address+0x230)+0x40);std::array<char,20> text{};Check(shadow.readBytes(name,text.data(),text.size()) && std::string(text.data())=="CUSTOM_NativeImport","Custom name did not relocate into allocation");
    }
    for (size_t fail = 1; fail <= 9; ++fail) {
        Memory m; auto bytes = fixture(m); const auto before = m.regions; m.failWrite = fail;
        Check(run(m, bytes) == ImportationErrcode_GameWriteError && m.regions == before && m.allocatedMemory.empty(),
            "Retail failed upload/publication was not fully rolled back");
        Check(m.pauses == 1 && m.resumes == 1, "Retail write failure left process suspended");
    }
    for (int failure = 0; failure < 11; ++failure) {
        Memory m; auto bytes = fixture(m);
        if (failure == 0) m.failPause = true;
        if (failure == 1) m.changeAtPause = true;
        if (failure == 2) m.corruptUpload = true;
        if (failure == 10) m.changeRoundAtPause = true; // same registry/key, new round object
        if (failure == 3) m.Put<uint64_t>(m.Pointer(12), 0); // invalid checksum
        if (failure == 4 || failure == 6) continue; // 4: a changed captured PANM is not unsafe (checked below); 6: no build check anymore
        if (failure == 5) m.Put<uint32_t>(m.player + 4, 2); // unavailable target slot
        const auto external = m.readUInt64(m.Pointer(12) + 0x98);
        if (failure == 7) m.Put<uint64_t>(external + 40, 0); // unterminated external list
        if (failure == 8) m.Put<uint64_t>(external + 8, 0xdeadbeef); // external dependency
        if (failure == 9) { // invalid imported idle alias
            const auto start = reinterpret_cast<const TKMovesetHeader*>(bytes.data())->moveset_data_start;
            const uint16_t invalid = UINT16_MAX;
            memcpy(bytes.data() + start + sizeof(R::Header) + 0xaa, &invalid, sizeof(invalid));
        }
        Check(run(m, bytes) != ImportationErrcode_Successful && m.allocatedMemory.empty(), "Unsafe retail state was imported");
        Check(m.readUInt64(m.player + 0x3b50) == Memory::Root + (failure == 1 ? 8 : 0), "Rejected retail import changed player root");
        Check(failure == 2 || m.writes == 0, "Rejected retail import wrote game state");
    }
    {
        // The game's copy of an animation differs from the file's (an edited animation): the import goes through on
        // the donor route, the game's copy plays and the difference is counted.
        Memory m; auto bytes = fixture(m);
        m.Put<uint32_t>(m.panm + 96, 123);
        GameAddressesFile addresses; addresses.Reload(); GameData game; game.SetProcess(&m, &addresses);
        auto info = RetailInfo(); game.SetCurrentGame(&info);
        ImporterT8 importer(m, game, &info); uint8_t progress = 0;
        Check(importer.Import(bytes.data(), bytes.size(), m.player, ImportSettings_ApplyInstantly, progress) == ImportationErrcode_Successful &&
            importer.EditedAnimationsKept() == 1, ("Edited animation stopped the import: " + importer.LastError()).c_str());
    }
    {
        Memory m; auto bytes = fixture(m); m.failWrite = 3; m.failRollback = true;
        Check(run(m, bytes) == ImportationErrcode_GameRollbackError && !m.allocatedMemory.empty(),
            "Retail incomplete rollback released potentially referenced allocation");
        Check(m.resumes == 1, "Retail incomplete rollback left process suspended");
    }
    {
        Memory m; auto bytes = fixture(m); const auto before = m.regions;
        RetailImportShadow shadow(m);
        GameAddressesFile addresses; addresses.Reload(); GameData game; game.SetProcess(&shadow, &addresses);
        auto info = RetailInfo(); game.SetCurrentGame(&info);
        ImporterT8 importer(shadow, game, &info); uint8_t progress = 0;
        Check(importer.Import(bytes.data(), bytes.size(), m.player, ImportSettings_ApplyInstantly, progress) == ImportationErrcode_Successful &&
            shadow.writes == 9 && m.writes == 0 && m.regions == before && m.pauses == 0,
            "Read-only retail shadow modified its source or failed production import");
        auto payload = std::span<const Byte>(bytes).subspan(reinterpret_cast<const TKMovesetHeader*>(bytes.data())->moveset_data_start);
        auto audit = I::AuditImage(payload, shadow, importer.lastLoaded.address, importer.lastLoaded.size, m, Memory::Root);
        Check(audit.externalLists == 1 && audit.bytes > R::SourceHeaderSize, "Retail shadow image audit omitted data");
        shadow.writeUInt32(shadow.readUInt64(importer.lastLoaded.address + 0x230) + 0x120, 61);
        bool rejected = false;
        try { I::AuditImage(payload, shadow, importer.lastLoaded.address, importer.lastLoaded.size, m, Memory::Root); }
        catch (const std::exception&) { rejected = true; }
        Check(rejected && m.regions == before, "Retail audit missed altered uploaded bytes or wrote source");
    }
    {
        Memory m; auto bytes = fixture(m);
        Check(run(m, bytes, 0) == ImportationErrcode_Successful && m.writes == 2 &&
            m.readUInt64(m.player + 0x3b58) == Memory::Root && !m.readUInt64(m.player + 0x28f0),
            "Deferred retail import changed current move/context");
    }
    {
        // Two actual registry entries: donor P1 and a different P2 layout.
        Memory m; auto bytes = fixture(m); auto donor = m.player;
        auto entry = m.Alloc(0x4000); auto target = entry + 0x90;
        auto targetRoot = m.Alloc(R::SourceHeaderSize);
        auto targetMoves = m.Alloc(2 * 0x448);
        m.regions[targetRoot] = m.regions.at(Memory::Root);
        m.Put<uint64_t>(targetRoot + 0x230, targetMoves); m.Put<uint64_t>(targetRoot + 0x238, 2);
        m.Put<uint32_t>(targetMoves + 0x120, 60);
        m.Put<uint64_t>(m.playerVector + 8, entry); m.Put<uint32_t>(target + 4, 1);
        m.Put<uint32_t>(target + 0x168, 29); m.Put<uint32_t>(target + 0x16c, 29);
        m.Put<uint64_t>(target + 0x3e0, targetMoves);
        for (size_t i = 0; i < 5; ++i) m.Put<uint64_t>(target + 0x3b50 + i * 8, targetRoot);
        const auto donorBytes = m.regions.at(donor - 0x90);
        m.player = target;
        const auto beforeFailure = m.regions;
        m.failWrite = 3;
        Check(run(m, bytes) == ImportationErrcode_GameWriteError && m.regions == beforeFailure && m.allocatedMemory.empty(),
            "Cross-character rollback changed donor or target state");
        m.failWrite = 0; m.writes = 0;
        Check(run(m, bytes) == ImportationErrcode_Successful, "Cross-character import failed with a loaded matching donor");
        auto imported = m.readUInt64(target + 0x3b50);
        Check(m.readUInt32(m.readUInt64(imported + 0x180) + 4) == 29 &&
            m.readUInt32(target + 0x168) == 29 && m.readUInt32(target + 0x16c) == 29 &&
            m.regions.at(donor - 0x90) == donorBytes && m.readUInt64(donor + 0x3b50) == Memory::Root,
            "Cross-character import changed donor/visual identity or missed self requirement");
        auto payload = std::span<const Byte>(bytes).subspan(reinterpret_cast<const TKMovesetHeader*>(bytes.data())->moveset_data_start);
        Check(I::AuditImage(payload, m, imported, m.allocatedMemory.back().second, m, Memory::Root, 28, 29).externalLists == 1,
            "Cross-character image audit failed");
        // Replaced active roots must not prevent restoration via a fully checked
        // still-resident native capture root. No stale address is accepted alone.
        m.Put<uint64_t>(donor + 0x3b50, targetRoot);
        for (size_t i = 0; i < 5; ++i) m.Put<uint64_t>(target + 0x3b50 + i * 8, targetRoot);
        Check(run(m, bytes) == ImportationErrcode_Successful && m.readUInt64(donor + 0x3b50) == targetRoot,
            "Cross-character import could not bind a validated retained source root");
        GameAddressesFile addresses; addresses.Reload(); GameData game; game.SetProcess(&m, &addresses);
        auto info = RetailInfo(); game.SetCurrentGame(&info); ExtractorT8 exporter(m, game, &info);
        auto hidden = m.Alloc(0x4000);
        m.Put<uint64_t>(m.playerVector + 16, hidden); m.Put<uint32_t>(hidden + 0x90 + 4, 2);
        Check(exporter.GetCharacterAddresses().size() == 2 && !exporter.GetCharacterAddress(2) && !exporter.GetCharacterAddress(3),
            "Retail exporter exposed more than P1/P2");
    }
    {
        // Swapping source/target constants preserves both equal and not-equal
        // predicates. Opponent conditions and unrelated parameters stay intact.
        std::vector<Byte> records(8 * 20);
        const uint32_t ids[] = {220, 221, 224, 225, 222, 223, 226, 227};
        for (size_t i = 0; i < 8; ++i) {
            memcpy(records.data() + i * 20, &ids[i], 4);
            uint32_t value = i % 2 ? 8 : 6; memcpy(records.data() + i * 20 + 4, &value, 4);
            uint32_t extra = 6; memcpy(records.data() + i * 20 + 8, &extra, 4);
        }
        const auto before = records;
        Check(I::AdaptCharacterRequirements(records, 6, 8) == 4, "Retail identity fix count differs");
        for (size_t i = 0; i < 8; ++i) {
            uint32_t value; memcpy(&value, records.data() + i * 20 + 4, 4);
            Check(value == (i < 4 ? (i % 2 ? 6 : 8) : (i % 2 ? 8 : 6)) &&
                !memcmp(records.data() + i * 20 + 8, before.data() + i * 20 + 8, 12),
                "Retail identity fix changed wrong words/opponent predicates");
        }
        Check(I::AdaptCharacterRequirements(records, 8, 6) == 4 && records == before, "Retail identity adaptation is not reversible");
        {
            // A target whose identity fields differ: primary checks (220/221) take +0x168 (8), secondary checks
            // (224/225) take +0x16c (9). Rows: 220=6, 221=8, 224=6, 225=8.
            auto split = before;
            I::AdaptCharacterRequirements(split, 6, 8, 9);
            uint32_t v[4]; for (size_t i = 0; i < 4; ++i) memcpy(&v[i], split.data() + i * 20 + 4, 4);
            Check(v[0] == 8 && v[1] == 6 && v[2] == 9 && v[3] == 8, "Split target identities were not applied per field");
            auto same = before;
            Check(I::AdaptCharacterRequirements(same, 6, 6, 9) == 1, "Secondary checks were skipped when only +0x16c differs");
        }
        // Four-word list forms, with distinct sentinels for primary/secondary.
        const uint32_t listIds[] = {499,500,503,504,501,502,505,506};
        for (size_t i=0;i<8;++i) {
            memcpy(records.data()+i*20,&listIds[i],4);
            uint32_t values[]={6,8,(i<2 || i==4 || i==5)?125U:255U,99};
            memcpy(records.data()+i*20+4,values,16);
        }
        const auto lists=records;
        Check(I::AdaptCharacterRequirements(records,6,8)==8,"Four-word identity lists were not adapted");
        for(size_t i=0;i<8;++i) {
            uint32_t values[4];memcpy(values,records.data()+i*20+4,16);
            Check(values[0]==(i<4?8U:6U) && values[1]==(i<4?6U:8U) &&
                !memcmp(records.data()+i*20+12,lists.data()+i*20+12,8),
                "List adaptation changed an unused selector, unrelated word or opponent predicate");
        }
        Check(I::AdaptCharacterRequirements(records,8,6)==8 && records==lists,"Direct list adaptation was not reversible");
        const uint32_t scalarIds[]={220,221,224,225};
        for(size_t i=0;i<4;++i) {
            memcpy(records.data()+i*20,&scalarIds[i],4);
            uint32_t value=0xabcd0006;memcpy(records.data()+i*20+4,&value,4);
        }
        I::AdaptCharacterRequirements(records,6,8);
        for(size_t i=0;i<4;++i) {
            uint32_t value;memcpy(&value,records.data()+i*20+4,4);
            Check(value==0xabcd0008,"Scalar identity adaptation lost the upper parameter word");
        }
        // Native legacy selectors: 0x81=23, 0x86=42. Canonical direct IDs
        // preserve their predicate meaning after cross-character adaptation.
        records.assign(40,0);
        const uint32_t legacy[]={499,0x81,0x86,125,0x84,503,23,42,255,0x81};
        memcpy(records.data(),legacy,sizeof(legacy));
        Check(I::AdaptCharacterRequirements(records,23,42)==4,"Legacy identity selector adaptation count differs");
        uint32_t adapted[10];memcpy(adapted,records.data(),40);
        Check(adapted[1]==42 && adapted[2]==23 && adapted[3]==125 && adapted[4]==0x84 &&
            adapted[6]==42 && adapted[7]==23 && adapted[8]==255 && adapted[9]==0x81,
            "Legacy primary selectors were confused with secondary IDs or unused selectors");
        // A non-roster identity (1003) cannot be written into these fields: the import goes on, nothing names 1003,
        // and a record naming the target keeps its value.
        records.assign(40,0);
        const uint32_t wide[]={220,6,0,0,0,224,6,0,0,0};
        memcpy(records.data(),wide,sizeof(wide));
        const auto unchanged=records;
        bool threw=false;
        try { Check(I::AdaptCharacterRequirements(records,1003,6)==0 && records==unchanged,"Unrepresentable identity changed records"); }
        catch (const std::exception&) { threw=true; }
        Check(!threw,"Unrepresentable character identity stopped the import");
    }
}

void ExportLiveRetail(const std::filesystem::path& directory) {
    Check(std::filesystem::create_directory(directory), "Live export directory already exists");
    GameProcess process;
    Check(process.Attach("Polaris-Win64-Shipping.exe", 0), "Cannot attach read-only to retail T8");
    GameAddressesFile addresses; addresses.Reload(); GameData game; game.SetProcess(&process, &addresses);
    GameInfo info{}; info.name = "Tekken 8"; info.gameId = GameId_T8;
    info.minorVersion = GameVersions::T8::RETAIL; info.movesetNamePrefix = "T8_";
    info.dataString = info.minorDataString = "t8-retail"; info.ptrSize = 8; info.characterCount = 2;
    game.SetCurrentGame(&info);
    Exporter exporter(process, game, &info, directory);
    size_t exported = 0;
    for (uint8_t slot = 0; slot < 2; ++slot) {
        auto player = exporter.GetCharacterAddress(slot); if (!player) continue;
        std::cout << "Exporting slot " << unsigned(slot) << ": " << exporter.GetPlayerCharacterName(player) << std::endl;
        uint8_t progress;
        const auto status = exporter.Extract(player, ExtractSettings_CompressLZ4 | ExtractSettings_OVERWRITE_SAME_FILENAME, progress);
        if (status != ExtractionErrcode_Successful || progress != 100)
            throw std::runtime_error("Live retail export failed (" + std::to_string(status) + "): " + exporter.LastError());
        ++exported;
    }
    Check(exported > 0, "No retail players detected");
    for (auto& entry : std::filesystem::directory_iterator(directory)) {
        MovesetFile file(entry.path()); file.Validate();
        Check(file.Header().crc32 == Helpers::CalculateCrc32(file.Payload().data(), file.Payload().size()), "Live retail export CRC differs");
        auto assets = R::Assets(file.Payload());
        std::array<size_t, 6> types{}; size_t unavailable = 0;
        for (auto& asset : assets) { if (asset.size) ++types[asset.type]; else ++unavailable; }
        std::cout << entry.path().filename().string() << ": " << R::Table(file.Payload(), 12).size() / 0x448 << " moves, "
            << assets.size() - unavailable << " PANMs, " << unavailable << " unavailable slots; types";
        for (auto count : types) std::cout << ' ' << count;
        std::cout << std::endl;
    }
}

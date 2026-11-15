#include "T7ToRetailT8.hpp"
#include "RetailT8Aliases.hpp"
#include "GameIDs.hpp"
#include "RetailNames.hpp"
#include "MovesetFileConverter.hpp"
#include "Importer_t8/RetailImport.hpp"
#include <nlohmann/json.hpp>
#include <cstring>

std::vector<Byte> SemanticFixtureWithPropertyList(size_t rows);
namespace {
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
struct Fixture {
    TKMovesetHeader header;
    StructsT7::TKMovesetHeaderBlocks blocks{};
    std::vector<Byte> payload;
    Fixture() {
        namespace S = StructsT7;
        header.gameId = GameId_T7; header.characterId = 9;
        blocks.tableBlock = offsetof(S::MovesetInfo,table);
        blocks.motalistsBlock = offsetof(S::MovesetInfo,motas);
        blocks.nameBlock = blocks.movesetBlock = sizeof(S::MovesetInfo);
        payload.resize(blocks.movesetBlock);
        memset(payload.data() + blocks.motalistsBlock,0xff,sizeof(S::MotaList));
        S::Requirement requirements[] = {{0,0},{881,0}};
        S::ExtraMoveProperty properties[] = {{17,0x8001,0},{0,0,0}};
        StructsT7_gameAddr::OtherMoveProperty guarded[] = {{0,0x8001,0},{1,881,0}};
        StructsT7_gameAddr::Cancel cancel{};
        cancel.command = 0x8000; cancel.requirements_addr = 0; cancel.extradata_addr = UINT64_MAX;
        Add(1,requirements,sizeof(requirements),2);
        Add(9,properties,sizeof(properties),2);
        Add(10,guarded,sizeof(guarded),2);
        Add(11,guarded,sizeof(guarded),2);
        Add(6,&cancel,sizeof(cancel),1);
        blocks.animationBlock = blocks.motaBlock = blocks.movelistBlock = payload.size();
    }
    void Add(size_t table, const void* data, size_t bytes, uint64_t count) {
        const auto offset = payload.size();
        payload.resize(offset + bytes);
        memcpy(payload.data() + offset,data,bytes);
        auto info = reinterpret_cast<StructsT7_gameAddr::MovesetInfo*>(payload.data());
        info->table.entries[table].listAddr = offset - blocks.movesetBlock;
        info->table.entries[table].listCount = count;
    }
    template<class T> T* Rows(size_t table) {
        const auto info = reinterpret_cast<const StructsT7_gameAddr::MovesetInfo*>(payload.data());
        return reinterpret_cast<T*>(payload.data() + blocks.movesetBlock + info->table.entries[table].listAddr);
    }
    auto Build() const { return T7ToRetailT8::Build(header,blocks,payload); }
};
}

std::vector<Byte> SemanticFixture();
std::vector<Byte> SemanticBodyFixture();
std::vector<Byte> T7RetailConvertibleFixture() {
    // A real 23-bone T7 body animation: the converter refuses any move it cannot animate (no idle fallback).
    auto bytes=SemanticBodyFixture();
    auto& header=*reinterpret_cast<TKMovesetHeader*>(bytes.data());
    header.characterId=9; strcpy_s(header.version_string,"1.0");
    const auto& blocks=*reinterpret_cast<const StructsT7::TKMovesetHeaderBlocks*>(bytes.data()+header.block_list);
    auto& info=*reinterpret_cast<StructsT7_gameAddr::MovesetInfo*>(bytes.data()+header.moveset_data_start);
    for(size_t table:{size_t(3),size_t(17),size_t(18)}) info.table.entries[table].listCount=0;
    return bytes;
}

void RunT7ToRetailT8Tests() {
    for(auto [name,hash]:{std::pair<const char*,uint32_t>{"",3696677242U},{"a",1016544589U},
        {"m",1700518541U},{"1234",1527950834U},{"12345",2946287185U},{"123456789012",3307383567U},
        {"1234567890123",297541912U},{"123456789012345678901234",143204817U},
        {"1234567890123456789012345",1623903661U},{"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ",1151599707U}})
        Check(RetailNames::Hash(name)==hash,"Retail name hash differs from reference vectors");
    Check(CanConvertMoveset(GameId_T7,GameId_T8,GameVersions::T8::RETAIL) &&
        !CanConvertMoveset(GameId_T7,GameId_T8,GameVersions::T8::CLOSED_NETWORK_TEST) &&
        CanConvertMoveset(GameId_T8,GameId_T7,0) && CanConvertMoveset(GameId_T5,GameId_T7,0),
        "Conversion target selection broke retail or legacy routing");
    {
        namespace R=RetailT8Snapshot;
        auto source=T7RetailConvertibleFixture(); const auto original=source;
        const auto& header=*reinterpret_cast<const TKMovesetHeader*>(source.data());
        const auto& blocks=*reinterpret_cast<const StructsT7::TKMovesetHeaderBlocks*>(source.data()+header.block_list);
        auto converted=T7ToRetailT8::Convert(header,blocks,std::span<const Byte>(source).subspan(header.moveset_data_start));
        const auto& result=*reinterpret_cast<const TKMovesetHeader*>(converted.data());
        Check(result.gameId==GameId_T8 && result.minorVersion==GameVersions::T8::RETAIL &&
            result.characterId==8 && result.GetConversionOrigin()==GameVersions::T8::FROM_T7,"Converted retail metadata is wrong");
        auto payload=std::span<const Byte>(converted).subspan(result.moveset_data_start);
        R::Validate(payload);
        auto image=RetailT8Import::Prepare(result,payload);
        Check(image.Ok() && image.counts[12]==1 && image.counts[1]==3,"Converted retail tables failed independent graph preparation");
        auto relocated=image.RelocateTables(0x500000000);
        uint64_t reference; memcpy(&reference,relocated.data()+image.offsets[12]+0x98,8);
        Check(reference==0x500000000+image.offsets[6],"Converted first cancel index became NULL");
        Check(source==original,"Binary conversion modified the T7 source");
        auto move=R::Table(payload,12);
        std::vector<Byte> loaded(move.begin(),move.end());
        loaded.insert(loaded.end(),move.begin(),move.end());
        Check(RetailT8Import::BindAnimations(move,loaded,true)==std::vector<size_t>{0},"Converted binding required equal native table counts");
        RetailT8Import::BindStats stats;
        RetailT8Import::BindAnimations(move,loaded,false,nullptr,0,&stats);
        Check(stats.missingAnimations==0,"Native binding failed on a loaded moveset with more moves");
        uint64_t different=RetailNames::Encode(42);
        memcpy(loaded.data()+0x20,&different,8); memcpy(loaded.data()+0x448+0x20,&different,8);
        stats={};
        Check(RetailT8Import::BindAnimations(move,loaded,true,nullptr,1,&stats)==std::vector<size_t>{1} && stats.missingAnimations==1,
            "Missing converted animation binding did not fall back to the idle row");
    }
    {
        // A body animation with fewer than 23 channels is refused from its layout, before the decode (whose 0x64 cost
        // grows with frames squared); the conversion goes on and the move falls back.
        auto source=T7RetailConvertibleFixture();
        const auto& header=*reinterpret_cast<const TKMovesetHeader*>(source.data());
        const auto& blocks=*reinterpret_cast<const StructsT7::TKMovesetHeaderBlocks*>(source.data()+header.block_list);
        const uint16_t bones=22; memcpy(source.data()+header.moveset_data_start+blocks.animationBlock+2,&bones,2);
        T7ToRetailT8::ConversionReport report;
        T7ToRetailT8::ConvertStandalone(header,blocks,std::span<const Byte>(source).subspan(header.moveset_data_start),&report);
        Check(report.animationsConversionFailed==1 && report.animationsFallback==1,"A 22-channel T7 animation was not refused");
    }
    {
        // Self-contained converter: projectiles are converted row for row (T7 0xa8 -> retail 0xe0).
        namespace R=RetailT8Snapshot;
        auto source=SemanticFixture();
        const auto& header=*reinterpret_cast<const TKMovesetHeader*>(source.data());
        const auto& blocks=*reinterpret_cast<const StructsT7::TKMovesetHeaderBlocks*>(source.data()+header.block_list);
        const auto t7=std::span<const Byte>(source).subspan(header.moveset_data_start);
        const auto& info=*reinterpret_cast<const StructsT7_gameAddr::MovesetInfo*>(t7.data()+blocks.movesetInfoBlock);
        const auto count=info.table.entries[3].listCount;
        Check(count>0,"Semantic fixture lost its projectile rows");
        auto* rows=t7.data()+blocks.movesetBlock+info.table.entries[3].listAddr;
        T7ToRetailT8::ConversionReport report;
        auto converted=T7ToRetailT8::ConvertStandalone(header,blocks,t7,&report,false);
        const auto& result=*reinterpret_cast<const TKMovesetHeader*>(converted.data());
        auto payload=std::span<const Byte>(converted).subspan(result.moveset_data_start);
        auto table=R::Table(payload,3);
        Check(table.size()==count*0xe0,"Converted projectile table lost rows");
        for(uint64_t row=0;row<count;++row) {
            const auto* in=rows+row*sizeof(StructsT7::Projectile); const auto* out=table.data()+row*0xe0;
            uint32_t a,b; memcpy(&a,in+0x38,4); memcpy(&b,out+0x3c,4);
            Check(a==b,"Projectile hit level not moved to 0x3c");
            memcpy(&a,in+0x84,4); memcpy(&b,out+0xbc,4);
            Check(a==b,"Projectile 0x84 not moved to 0xbc");
            memcpy(&b,out+0xa8,4); Check(b==1,"Projectile 0xa8 constant missing");
        }
        auto image=RetailT8Import::Prepare(result,payload);
        Check(image.Ok() && image.counts[3]==count,"Converted projectile references failed graph preparation");
    }
    {
        // QA 1.0 (StandalonePropertyListWalkBounded): an extra-property walk stops at ListLimit (4096 rows) like
        // buildClips; a longer list is refused instead of costing O(moves x list length).
        for (size_t rows : {size_t(4096), size_t(5000)}) {
            auto source=SemanticFixtureWithPropertyList(rows);
            const auto& header=*reinterpret_cast<const TKMovesetHeader*>(source.data());
            const auto& blocks=*reinterpret_cast<const StructsT7::TKMovesetHeaderBlocks*>(source.data()+header.block_list);
            const auto t7=std::span<const Byte>(source).subspan(header.moveset_data_start);
            std::string error;
            try { T7ToRetailT8::ConvertStandalone(header,blocks,t7,nullptr,false); } catch(const std::exception& e) { error=e.what(); }
            if (rows==4096) Check(error.empty(),("A 4096-row property list must still convert: "+error).c_str());
            else Check(error.find("Unterminated T7 property list")!=std::string::npos,("Overlong property list was walked: "+error).c_str());
        }
    }
    {
        // Property id 0 ends a T7 extra-property list whatever its frame (owner decision, 1.0): a {frame 5, id 0}
        // first row is the whole list, and the retail end row is written as {0, 0}. The rows after it are not walked.
        namespace R=RetailT8Snapshot;
        auto source=SemanticFixtureWithPropertyList(3);
        const auto& header=*reinterpret_cast<const TKMovesetHeader*>(source.data());
        const auto& blocks=*reinterpret_cast<const StructsT7::TKMovesetHeaderBlocks*>(source.data()+header.block_list);
        auto* t7bytes=source.data()+header.moveset_data_start;
        const auto& info=*reinterpret_cast<const StructsT7_gameAddr::MovesetInfo*>(t7bytes+blocks.movesetInfoBlock);
        auto* rows=reinterpret_cast<StructsT7::ExtraMoveProperty*>(t7bytes+blocks.movesetBlock+info.table.entries[9].listAddr);
        rows[0].starting_frame=5; rows[0].id=0; rows[0].value_unsigned=0;
        rows[1].starting_frame=1; rows[1].id=0x8001; rows[1].value_unsigned=0;
        const auto t7=std::span<const Byte>(source).subspan(header.moveset_data_start);
        auto converted=T7ToRetailT8::ConvertStandalone(header,blocks,t7,nullptr,false);
        const auto& result=*reinterpret_cast<const TKMovesetHeader*>(converted.data());
        auto payload=std::span<const Byte>(converted).subspan(result.moveset_data_start);
        auto table=R::Table(payload,9);
        Check(table.size()==R::Tables[9].stride,"An id-0 row with a frame did not end the property list");
        uint32_t frame=1,id=1; memcpy(&frame,table.data(),4); memcpy(&id,table.data()+16,4);
        Check(frame==0 && id==0,"Converted property list end is not a {0, 0} row");
    }
    const auto* strong = RetailT8::FindAlias(false,225);
    Check(strong && strong->target == 228,"Existing requirement alignment was lost");
    const auto* camera = RetailT8::FindAlias(true,0x83a2);
    Check(camera && camera->target == 0x84eb,"Handler-reviewed camera selection was lost");
    const auto* flag = RetailT8::FindAlias(true,0x835f);
    Check(flag && flag->target == 0x84a8,"Resolved historical alias conflict was lost");
    Fixture source;
    const auto before = source.payload;
    auto plan = source.Build();
    Check(plan.RecordsConverted() && !plan.Loadable(),"Record conversion incorrectly implies a loadable image");
    Check(source.payload == before,"Conversion modified the input payload");
    Check(plan.requirements.size() == 2 && plan.requirements[1].id == 1100,"Requirement terminator lost");
    Check(plan.properties.size() == 2 && plan.properties[0].frame == 17,"Property frame lost");
    Check(plan.startProperties.size() == 2 && plan.endProperties.size() == 2,"Guarded properties missing");
    Check(plan.startProperties[0].requirement == 0 && plan.startProperties[1].id == 1100 &&
        plan.startProperties[1].requirement == 1,"Guarded property references or terminator changed");
    bool first = false, null = false;
    for (const auto& ref : plan.references) if (ref.table == 6) {
        if (ref.targetTable == 1) first = ref.index == 0;
        if (ref.targetTable == 8) null = ref.index == UINT64_MAX;
    }
    Check(first && null,"First row and null references were conflated");
    auto report = T7ToRetailT8::Report(plan);
    Check(report["loadable_moveset"] == false && report["tables"].size() == 20 &&
        report["target_build"] == 26155785,"Conversion report lost its format/build constraints");
    source.Rows<StructsT7_gameAddr::OtherMoveProperty>(11)[0].value = 1;
    plan = source.Build();
    Check(plan.issueCount == 1 && plan.issues[0].table == 11 && plan.issues[0].row == 0,
        "End-property unsupported parameter was not diagnosed");
    Check(plan.requirements.empty() && plan.properties.empty() && plan.startProperties.empty() && plan.endProperties.empty(),
        "Partial converted arrays escaped a failed mapping");
    Check(plan.ImportBlocker().find("end_properties[0]") != std::string::npos,"Live preflight lost the first blocker");
    Fixture nullable;
    nullable.Rows<StructsT7_gameAddr::OtherMoveProperty>(10)[0].requirements_addr = UINT64_MAX;
    plan = nullable.Build();
    Check(plan.startProperties[0].requirement == UINT64_MAX,"Null guarded requirement changed");
    nullable.Rows<StructsT7::Requirement>(1)[0].condition = 225;
    plan = nullable.Build();
    Check(!plan.RecordsConverted() && plan.issues[0].table == 1,"Unknown requirement silently accepted");
    Check(plan.idAlignments[0].target == 228 && !plan.idAlignments[0].parametersConverted,
        "Known ID alignment was confused with an implemented parameter transform");
    Fixture invalidReference;
    invalidReference.Rows<StructsT7_gameAddr::Cancel>(6)->requirements_addr = 999;
    bool rejected = false;
    try { invalidReference.Build(); } catch (const std::exception&) { rejected = true; }
    Check(rejected,"Invalid source reference accepted");
    Fixture wrongGame; wrongGame.header.gameId = GameId_T8;
    rejected = false;
    try { wrongGame.Build(); } catch (const std::exception&) { rejected = true; }
    Check(rejected,"Retail/CNT input accepted as T7");
}

#include "T7ToRetailT8.hpp"
#include "RetailT8Aliases.hpp"
#include "GameIDs.hpp"
#include "RetailNames.hpp"
#include "MovesetFileConverter.hpp"
#include "Importer_t8/RetailImport.hpp"
#include <nlohmann/json.hpp>
#include <cstring>
#include <set>
#include <string>

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
std::vector<Byte> SemanticCameraFixture();
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

// Edits rows of every T7 table with changes a single-row update carries, applies each with
// LiveConversion::UpdateRow, and checks the result against a full conversion of the edited moveset (byte for byte,
// animations included). Rows whose edit needs a full conversion (UpdateRow gives a reason) are put back. Returns the
// number of rows edited. Also used on real files: MovesetRegressionTests --check-t7-live FILE...
size_t CheckT7LiveConversion(std::span<const Byte> file) {
    namespace S = StructsT7;
    namespace G = StructsT7_gameAddr;
    const auto& header = *reinterpret_cast<const TKMovesetHeader*>(file.data());
    StructsT7::TKMovesetHeaderBlocks blocks; memcpy(&blocks, file.data() + header.block_list, sizeof(blocks));
    std::vector<Byte> payload(file.begin() + header.moveset_data_start, file.end());
    const auto profile = ImportProfile::Read(header);
    T7ToRetailT8::LiveConversion live(header, blocks, payload, profile);
    Check(live.File() == T7ToRetailT8::ConvertStandalone(header, blocks, payload, nullptr, true, profile),
        "Live conversion differs from the full conversion");
    const auto& info = *reinterpret_cast<const G::MovesetInfo*>(payload.data() + blocks.movesetInfoBlock);
    constexpr size_t strides[19] = {
        sizeof(S::Reactions), sizeof(S::Requirement), sizeof(S::HitCondition), sizeof(S::Projectile),
        sizeof(S::Pushback), sizeof(S::PushbackExtradata), sizeof(S::Cancel), sizeof(S::Cancel),
        sizeof(S::CancelExtradata), sizeof(S::ExtraMoveProperty), sizeof(S::OtherMoveProperty),
        sizeof(S::OtherMoveProperty), sizeof(S::Move), sizeof(S::Voiceclip), sizeof(S::InputSequence),
        sizeof(S::Input), sizeof(S::UnknownParryRelated), sizeof(S::CameraData), sizeof(S::ThrowCamera)};
    auto edited = payload;
    size_t edits = 0;
    for (size_t table = 0; table < 19; ++table) {
        const auto count = info.table.entries[table].listCount;
        if (!count) continue;
        const std::set<uint64_t> rows{0, count / 2, count - 1};
        for (auto row : rows) {
            Byte* r = edited.data() + blocks.movesetBlock + info.table.entries[table].listAddr + row * strides[table];
            const std::vector<Byte> original(r, r + strides[table]);
            auto add = [&](size_t offset, auto delta) {
                decltype(delta) v; memcpy(&v, r + offset, sizeof(v)); v = decltype(delta)(v + delta); memcpy(r + offset, &v, sizeof(v));
            };
            auto flip = [&](size_t offset) { r[offset] ^= 1; };
            switch (table) {
            case 0: flip(offsetof(S::Reactions, front_direction)); break;
            case 1: add(offsetof(S::Requirement, param_unsigned), uint32_t(1)); break;
            case 2: add(offsetof(S::HitCondition, damage), uint32_t(1)); break;
            case 3: add(0x80, uint32_t(1)); flip(0x38); break;
            case 4: add(offsetof(S::Pushback, duration), uint16_t(1)); break;
            case 5: add(offsetof(S::PushbackExtradata, horizontal_offset), uint16_t(1)); break;
            case 6: case 7: {
                add(offsetof(S::Cancel, detection_end), uint32_t(1));
                uint64_t requirement; memcpy(&requirement, r + offsetof(G::Cancel, requirements_addr), 8);
                if (requirement != UINT64_MAX && requirement != 0) { requirement = 0; memcpy(r + offsetof(G::Cancel, requirements_addr), &requirement, 8); }
                break;
            }
            case 8: flip(offsetof(S::CancelExtradata, value)); break;
            case 9: add(offsetof(S::ExtraMoveProperty, value_unsigned), uint32_t(1)); break;
            case 10: case 11: add(offsetof(S::OtherMoveProperty, value), uint32_t(1)); break;
            case 12:
                flip(offsetof(S::Move, hitlevel)); add(offsetof(S::Move, first_active_frame), uint32_t(1));
                add(offsetof(S::Move, distance), uint16_t(3));
                break;
            case 13: { uint32_t v; memcpy(&v, r, 4); if (v != UINT32_MAX) flip(0); break; }
            case 14: add(offsetof(S::InputSequence, input_window_frames), uint16_t(1)); break;
            case 15: flip(offsetof(S::Input, direction)); break;
            case 16: flip(0); break;
            case 17: flip(offsetof(S::CameraData, _0x0_int)); break;
            case 18: flip(offsetof(S::ThrowCamera, side)); break;
            }
            if (!memcmp(r, original.data(), original.size())) continue;
            if (live.UpdateRow(table, row, {r, strides[table]})) memcpy(r, original.data(), original.size());
            else ++edits;
        }
    }
    auto full = T7ToRetailT8::ConvertStandalone(header, blocks, edited, nullptr, true, profile);
    // The live file keeps the CRC of its first conversion (it is never imported once edited).
    auto& fullHeader = *reinterpret_cast<TKMovesetHeader*>(full.data());
    const auto& liveHeader = *reinterpret_cast<const TKMovesetHeader*>(live.File().data());
    fullHeader.crc32 = liveHeader.crc32; fullHeader.orig_crc32 = liveHeader.orig_crc32;
    if (live.File() != full) {
        size_t at = 0;
        while (at < full.size() && at < live.File().size() && full[at] == live.File()[at]) ++at;
        throw std::runtime_error("Live row conversion differs from the full conversion of the edited moveset at byte " + std::to_string(at));
    }
    return edits;
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
    {
        // T7 camera on either side (33845) -> the retail side pair 0x868C + 0x868D with the converted camera's index; the
        // list's drama camera row (33443 -> 0x8313) is left out, since in retail the drama camera overrides it (2026-11-18).
        namespace R=RetailT8Snapshot;
        auto source=SemanticCameraFixture();
        auto& header=*reinterpret_cast<TKMovesetHeader*>(source.data());
        header.characterId=9; strcpy_s(header.version_string,"1.0");
        const auto& blocks=*reinterpret_cast<const StructsT7::TKMovesetHeaderBlocks*>(source.data()+header.block_list);
        auto& info=*reinterpret_cast<StructsT7_gameAddr::MovesetInfo*>(source.data()+header.moveset_data_start);
        for(size_t table:{size_t(3),size_t(17),size_t(18)}) info.table.entries[table].listCount=0;
        const auto t7=std::span<const Byte>(source).subspan(header.moveset_data_start);
        T7ToRetailT8::ConversionReport report;
        auto converted=T7ToRetailT8::ConvertStandalone(header,blocks,t7,&report);
        const auto& result=*reinterpret_cast<const TKMovesetHeader*>(converted.data());
        auto table=R::Table(std::span<const Byte>(converted).subspan(result.moveset_data_start),9);
        std::vector<std::pair<uint32_t,uint32_t>> rows;
        for(size_t at=0;at+R::Tables[9].stride<=table.size();at+=R::Tables[9].stride) {
            uint32_t id,value; memcpy(&id,table.data()+at+16,4); memcpy(&value,table.data()+at+20,4); rows.push_back({id,value});
        }
        Check(report.cameraAnimationsConverted==1 && report.cameraAnimationProperties==1,"Camera clip or row not converted");
        Check(rows.size()==3 && rows[0]==std::pair<uint32_t,uint32_t>{0x868C,0x2000} && rows[1]==std::pair<uint32_t,uint32_t>{0x868D,0x2000} &&
            rows[2].first==0,"Either-side camera is not the 0x868C/0x868D pair");
        for(const auto& row:rows) Check(row.first!=0x8313,"Drama camera row kept beside a camera animation");
        Check(CheckT7LiveConversion(source)>0,"Camera fixture live conversion");
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
    {
        // Live edition of a T7 moveset in retail T8: one converted row gives the bytes of a full conversion.
        Check(CheckT7LiveConversion(T7RetailConvertibleFixture()) > 0, "Live conversion edited no row of the convertible fixture");
        Check(CheckT7LiveConversion(SemanticFixture()) > 0, "Live conversion edited no row of the semantic fixture");
        // Edits a single row cannot carry give the reason (a new import is needed) and change nothing.
        const auto file = T7RetailConvertibleFixture();
        const auto& header = *reinterpret_cast<const TKMovesetHeader*>(file.data());
        StructsT7::TKMovesetHeaderBlocks blocks; memcpy(&blocks, file.data() + header.block_list, sizeof(blocks));
        const auto payload = std::span<const Byte>(file).subspan(header.moveset_data_start);
        T7ToRetailT8::LiveConversion live(header, blocks, payload, ImportProfile::Read(header));
        const auto before = live.File();
        const auto& info = *reinterpret_cast<const StructsT7_gameAddr::MovesetInfo*>(payload.data());
        auto row = [&](size_t table, uint64_t index, size_t stride) {
            auto at = payload.data() + blocks.movesetBlock + info.table.entries[table].listAddr + index * stride;
            return std::vector<Byte>(at, at + stride);
        };
        auto move = row(12, 0, sizeof(StructsT7::Move));
        reinterpret_cast<StructsT7_gameAddr::Move*>(move.data())->name_addr += 1;
        Check(live.UpdateRow(12, 0, move).has_value() && live.File() == before, "A renamed move was converted alone");
        move = row(12, 0, sizeof(StructsT7::Move));
        reinterpret_cast<StructsT7_gameAddr::Move*>(move.data())->anim_addr += 8;
        Check(live.UpdateRow(12, 0, move).has_value() && live.File() == before, "A move animation change was converted alone");
        move = row(12, 0, sizeof(StructsT7::Move));
        auto& head = reinterpret_cast<StructsT7_gameAddr::Move*>(move.data())->extra_move_property_addr;
        head = head == UINT64_MAX ? info.table.extraMovePropertyCount + 5 : UINT64_MAX - 2;
        Check(live.UpdateRow(12, 0, move).has_value() && live.File() == before, "A move's new property list was converted alone");
        for (uint64_t index = 0; index < info.table.extraMovePropertyCount; ++index) {
            auto property = row(9, index, sizeof(StructsT7::ExtraMoveProperty));
            auto& id = reinterpret_cast<StructsT7::ExtraMoveProperty*>(property.data())->id;
            if (id != 0) continue;
            id = 0x8001; // the list end moves
            Check(live.UpdateRow(9, index, property).has_value() && live.File() == before, "A moved property list end was converted alone");
            break;
        }
        Check(live.UpdateRow(19, 0, move).has_value() && live.UpdateRow(12, 1u << 20, move).has_value(), "An out-of-range row was accepted");
    }
}

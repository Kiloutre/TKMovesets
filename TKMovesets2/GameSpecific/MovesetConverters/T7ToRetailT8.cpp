#include "T7ToRetailT8.hpp"
#include "RetailT8Aliases.hpp"
#include "Importer_t7/Importer_t7_Utils.hpp"
#include "Extractor_t8/RetailSnapshot.hpp"
#include "GameIDs.hpp"
#include "RetailNames.hpp"
#include "Importer_t8/RetailImport.hpp"
#include "constants.h"
#include "Structs_t8.h"
#include <nlohmann/json.hpp>
#include <cstring>

namespace T7ToRetailT8 {
namespace {
namespace S = StructsT7;
namespace G = StructsT7_gameAddr;
namespace R = RetailT8Snapshot;
namespace S8 = StructsT8;
// Retail field offset (Structs_t8.h).
#define AT(record, member) uint32_t(offsetof(S8::record, member))
constexpr uint32_t ReactionPushback(int i) { return AT(Reactions, pushbacks) + i * uint32_t(sizeof(gameAddr)); }
constexpr uint64_t Budget = 256ULL << 20;
constexpr uint32_t Strides[] = {
    sizeof(S::Reactions), sizeof(S::Requirement), sizeof(S::HitCondition), sizeof(S::Projectile),
    sizeof(S::Pushback), sizeof(S::PushbackExtradata), sizeof(S::Cancel), sizeof(S::Cancel),
    sizeof(S::CancelExtradata), sizeof(S::ExtraMoveProperty), sizeof(S::OtherMoveProperty),
    sizeof(S::OtherMoveProperty), sizeof(S::Move), sizeof(S::Voiceclip), sizeof(S::InputSequence),
    sizeof(S::Input), sizeof(S::UnknownParryRelated), sizeof(S::CameraData), sizeof(S::ThrowCamera)
};
struct Field { size_t table; uint32_t source, target; size_t destination; };
// Explicit source -> retail field locations. Values remain indices in the
// intermediate, so index zero cannot be confused with a null runtime pointer.
constexpr Field Fields[] = {
    {0,0,ReactionPushback(0),4},{0,8,ReactionPushback(1),4},{0,16,ReactionPushback(2),4},{0,24,ReactionPushback(3),4},
    {0,32,ReactionPushback(4),4},{0,40,ReactionPushback(5),4},{0,48,ReactionPushback(6),4},
    {2,0,AT(HitCondition,requirements),1},{2,16,AT(HitCondition,reactions),0},
    {3,0x60,AT(Projectile,hit_condition),2},{3,0x68,AT(Projectile,cancel),6}, {4,8,AT(Pushback,extra),5},
    {6,8,AT(Cancel,requirements),1},{6,16,AT(Cancel,extra),8},{7,8,AT(Cancel,requirements),1},{7,16,AT(Cancel,extra),8},
    {10,0,AT(OtherMoveProperty,requirements),1},{11,0,AT(OtherMoveProperty,requirements),1},
    {12,0x20,AT(Move,cancels),6},{12,0x28,AT(Move,cancels_2),6},{12,0x38,AT(Move,cancels_3),6},{12,0x48,AT(Move,cancels_4),6},
    {12,offsetof(G::Move,hit_condition_addr),AT(Move,hit_conditions),2},
    {12,offsetof(G::Move,voicelip_addr),AT(Move,voiceclip),13},
    {12,offsetof(G::Move,extra_move_property_addr),AT(Move,extra_properties),9},
    {12,offsetof(G::Move,move_start_extraprop_addr),AT(Move,start_properties),10},
    {12,offsetof(G::Move,move_end_extraprop_addr),AT(Move,end_properties),11},
    {14,8,AT(InputSequence,inputs),15},{18,8,AT(ThrowCamera,camera),17}
};
template<class T> T Read(std::span<const Byte> bytes, uint64_t offset) {
    if (offset > bytes.size() || sizeof(T) > bytes.size() - offset)
        throw std::runtime_error("Truncated T7 conversion field");
    T result; memcpy(&result, bytes.data() + offset, sizeof(T)); return result;
}
}

Plan Build(const TKMovesetHeader& header, const S::TKMovesetHeaderBlocks& blocks,
    std::span<const Byte> payload) {
    if (header.gameId != GameId_T7) throw std::invalid_argument("T7 to retail T8 conversion requires T7 input");
    std::string reason;
    if (!T7ImportUtils::ValidateT7Semantics(blocks, payload.data(), payload.size(), &reason))
        throw std::runtime_error(reason);
    const auto info = Read<G::MovesetInfo>(payload, blocks.movesetInfoBlock);
    Plan plan;
    plan.character = header.characterId;
    plan.animationBytes = blocks.motaBlock - blocks.animationBlock;
    plan.motaBytes = blocks.movelistBlock - blocks.motaBlock;
    plan.movelistBytes = payload.size() - blocks.movelistBlock;
    uint64_t budget = Budget;
    auto reserve = [&](uint64_t count, uint64_t stride) {
        if (count > budget / stride) throw std::length_error("T7 to retail T8 plan exceeds allocation budget");
        budget -= count * stride;
    };
    auto records = payload.subspan(blocks.movesetBlock, blocks.animationBlock - blocks.movesetBlock);
    for (size_t i = 0; i < plan.tables.size(); ++i) {
        auto& table = plan.tables[i]; table.targetStride = R::Tables[i].stride;
        if (i < std::size(Strides)) {
            table.count = info.table.entries[i].listCount;
            table.sourceOffset = info.table.entries[i].listAddr;
            table.sourceStride = Strides[i];
        }
    }
    for (const auto& field : Fields) {
        const auto& table = plan.tables[field.table]; reserve(table.count, sizeof(Reference));
        for (uint64_t row = 0; row < table.count; ++row)
            plan.references.push_back({field.table, row, field.source, field.target, field.destination,
                Read<uint64_t>(records, table.sourceOffset + row * table.sourceStride + field.source)});
    }
    reserve(plan.tables[12].count, sizeof(MoveResource));
    for (uint64_t row = 0; row < plan.tables[12].count; ++row) {
        const auto move = Read<G::Move>(records, info.table.move + row * sizeof(G::Move));
        auto name = [&](uint64_t offset) {
            const auto text = reinterpret_cast<const char*>(payload.data() + blocks.nameBlock + offset);
            // The semantic validator above checked the complete string extent.
            const auto length = strlen(text); reserve(length + 1, 1); return std::string(text, length);
        };
        plan.moves.push_back({row, move.anim_addr, name(move.name_addr), name(move.anim_name_addr)});
    }
    auto reject = [&](size_t table, uint64_t row, uint32_t id, uint32_t parameter, std::string message) {
        ++plan.issueCount;
        if (plan.issues.size() < 256) plan.issues.push_back({table,row,id,parameter,std::move(message)});
    };
    reserve(info.table.requirementCount, sizeof(RetailT8::Requirement));
    reserve(info.table.extraMovePropertyCount, sizeof(RetailT8::Property));
    reserve(info.table.moveBeginningPropCount, sizeof(GuardedProperty));
    reserve(info.table.moveEndingPropCount, sizeof(GuardedProperty));
    for (size_t table : {size_t(1),size_t(9),size_t(10),size_t(11)})
        reserve(plan.tables[table].count,sizeof(IdAlignment));
    auto convert = [&](size_t table, uint64_t row, bool property, uint32_t id, uint32_t value, uint32_t& target) {
        const bool mapped = RetailT8::ConvertId(property,id,value,target,reason);
        const auto* evidence = RetailT8::FindAlias(property,id);
        plan.idAlignments.push_back({table,row,id,mapped ? target : evidence ? evidence->target : UINT32_MAX,mapped,
            mapped ? "explicit_parameter_rule" : evidence ? evidence->confidence : "unmapped",
            mapped ? "record_rules" : evidence ? evidence->provenance : "none"});
        if (!mapped && evidence && evidence->target != UINT32_MAX)
            reason += "; target ID " + std::to_string(evidence->target) + " has " + evidence->confidence + " evidence; parameter conversion is not implemented";
        return mapped;
    };
    for (uint64_t row = 0; row < info.table.requirementCount; ++row) {
        const auto value = Read<S::Requirement>(records, info.table.requirement + row * sizeof(S::Requirement));
        RetailT8::Requirement target{};
        if (convert(1,row,false,value.condition,value.param_unsigned,target.id)) {
            target.parameters[0] = value.param_unsigned; plan.requirements.push_back(target);
        } else reject(1,row,value.condition,value.param_unsigned,reason);
    }
    for (uint64_t row = 0; row < info.table.extraMovePropertyCount; ++row) {
        const auto value = Read<S::ExtraMoveProperty>(records, info.table.extraMoveProperty + row * sizeof(S::ExtraMoveProperty));
        RetailT8::Property target{value.starting_frame,0};
        if (convert(9,row,true,value.id,value.value_unsigned,target.id)) {
            // Property id 0 ends a T7 list whatever its frame; a frame-0 row with an id is an ordinary property.
            if (value.id == 0) target.frame = 0;
            target.parameters[0] = value.value_unsigned; plan.properties.push_back(target);
        } else reject(9,row,value.id,value.value_unsigned,reason);
    }
    for (size_t table : {size_t(10),size_t(11)}) {
        auto& output = table == 10 ? plan.startProperties : plan.endProperties;
        for (uint64_t row = 0; row < plan.tables[table].count; ++row) {
            const auto value = Read<G::OtherMoveProperty>(records,
                plan.tables[table].sourceOffset + row * sizeof(G::OtherMoveProperty));
            GuardedProperty target{value.requirements_addr,0};
            // 881 is the start/end list terminator in the requirement namespace.
            // Other entries use the property namespace, including ID zero (not a terminator).
            if (convert(table,row,value.extraprop != 881,value.extraprop,value.value,target.id)) {
                target.parameters[0] = value.value; output.push_back(target);
            } else reject(table,row,value.extraprop,value.value,reason);
        }
    }
    if (!plan.RecordsConverted()) {
        plan.requirements.clear(); plan.properties.clear();
        plan.startProperties.clear(); plan.endProperties.clear();
    }
    return plan;
}

std::string Plan::ImportBlocker() const {
    auto message = "T7 to retail T8 import is not loadable: " + std::to_string(issueCount) + " unsupported requirement/property records";
    if (!issues.empty()) {
        const auto& issue = issues.front();
        message += "; first: " + std::string(R::Tables[issue.table].name) + "[" + std::to_string(issue.row) +
            "] ID " + std::to_string(issue.id) + ", parameter " + std::to_string(issue.parameter) + " (" + issue.reason + ")";
    }
    return message + "; conversion requires supported record mappings and native T8 animation bindings";
}

std::vector<Byte> Convert(const TKMovesetHeader& header, const S::TKMovesetHeaderBlocks& blocks,
    std::span<const Byte> payload) {
    const auto plan=Build(header,blocks,payload);
    if(!plan.RecordsConverted()) throw std::runtime_error(plan.ImportBlocker());
    if(plan.moves.empty()) throw std::runtime_error("T7 conversion requires at least one move");
    // Resource IDs in these tables are game-specific. Never silently reuse T7
    // VFX, audio or camera identifiers as retail IDs.
    for(size_t table:{size_t(3),size_t(17),size_t(18)})
        if(plan.tables[table].count) throw std::runtime_error(std::string("T7 conversion has no retail resource mapping for ")+R::Tables[table].name);
    const auto records=payload.subspan(blocks.movesetBlock,blocks.animationBlock-blocks.movesetBlock);
    for(uint64_t row=0;row<plan.tables[13].count;++row)
        if(Read<uint32_t>(records,plan.tables[13].sourceOffset+row*4)!=UINT32_MAX)
            throw std::runtime_error("T7 voiceclip IDs require a retail audio mapping");
    // T7 -> retail character IDs, for the base roster whose runtime identities
    // are established. Characters without a counterpart are rejected explicitly.
    constexpr uint32_t Characters[]={0,1,2,3,4,5,6,7,35,8,9,10,11,12,13,14,15,16,17,18,
        19,UINT32_MAX,20,UINT32_MAX,UINT32_MAX,UINT32_MAX,21,42,UINT32_MAX,23,24,33,
        UINT32_MAX,UINT32_MAX,22};
    if(header.characterId>=std::size(Characters)||Characters[header.characterId]==UINT32_MAX)
        throw std::runtime_error("T7 character has no supported retail T8 identity mapping");
    const auto character=Characters[header.characterId];
    const auto info=Read<G::MovesetInfo>(payload,blocks.movesetInfoBlock);
    constexpr uint64_t Base=0x10000000; // virtual address space, never a process pointer
    auto align=[](size_t value){return (value+7)&~size_t(7);};
    R::Header snapshot{{'T','8','R','S'},1,Base,sizeof(R::Header),align(sizeof(R::Header)+R::SourceHeaderSize),0};
    std::vector<Byte> output(snapshot.directoryOffset+R::Tables.size()*sizeof(R::Entry));
    auto put=[&]<class T>(size_t at,T value){memcpy(output.data()+at,&value,sizeof(value));};
    std::array<R::Entry,20> entries{};
    for(size_t table=0;table<entries.size();++table) {
        auto count=plan.tables[table].count;
        if(table==1 && !plan.properties.empty()) ++count; // unconditional property guard
        const auto start=align(output.size());
        const auto stride=R::Tables[table].stride;
        if(start>Budget || count>(Budget-start)/stride) throw std::length_error("Converted T8 tables exceed allocation budget");
        output.resize(start+count*stride);
        entries[table]={count?Base+start:0,count,start,stride,0};
        put(snapshot.directoryOffset+table*sizeof(R::Entry),entries[table]);
        put(snapshot.sourceHeaderOffset+R::Tables[table].pointerOffset,entries[table].address);
        put(snapshot.sourceHeaderOffset+R::Tables[table].countOffset,count);
    }
    auto field=[&](size_t table,uint64_t row,size_t offset=0){return entries[table].offset+row*entries[table].stride+offset;};
    using H=S8::MovesetHeader;
    put(snapshot.sourceHeaderOffset+offsetof(H,skip_animation_lookup),uint32_t(0x10000));
    put(snapshot.sourceHeaderOffset+offsetof(H,compile_date),uint32_t(20240101)); // format epoch; no captured compilation date
    // Nonzero dependencies request rebinding to the native resource owner.
    for(size_t at=offsetof(H,strings);at<offsetof(H,orig_aliases);at+=sizeof(gameAddr)) put(snapshot.sourceHeaderOffset+at,Base);
    put(snapshot.sourceHeaderOffset+offsetof(H,_0x170),Base);
    for(size_t at=offsetof(H,runtime_slots);at<sizeof(H);at+=sizeof(gameAddr)) put(snapshot.sourceHeaderOffset+at,Base);
    for(size_t i=0;i<std::size(H{}.orig_aliases);++i) {
        put(snapshot.sourceHeaderOffset+offsetof(H,orig_aliases)+i*2,i<56?info.orig_aliases[i]:info.orig_aliases[1]);
        put(snapshot.sourceHeaderOffset+offsetof(H,current_aliases)+i*2,i<56?info.current_aliases[i]:info.current_aliases[1]);
    }
    // Retail holds 32 unknown aliases (0x120..0x15F); T7 has 36. Copy only what fits, so the identity words at
    // 0x160..0x167 keep their values instead of T7's last four entries.
    static_assert(sizeof(H{}.unknown_aliases)<=sizeof(info.unknown_aliases));
    memcpy(output.data()+snapshot.sourceHeaderOffset+offsetof(H,unknown_aliases),info.unknown_aliases,sizeof(H{}.unknown_aliases));
    // Tables with unchanged scalar layouts; their references are rewritten below.
    for(size_t table:{size_t(0),size_t(2),size_t(4),size_t(5),size_t(6),size_t(7),size_t(8),size_t(14),size_t(15),size_t(16)}) {
        const auto& source=plan.tables[table];
        for(uint64_t row=0;row<source.count;++row)
            memcpy(output.data()+field(table,row),records.data()+source.sourceOffset+row*source.sourceStride,source.sourceStride);
    }
    for(size_t row=0;row<plan.requirements.size();++row) put(field(1,row),plan.requirements[row]);
    const auto guard=entries[1].address+plan.requirements.size()*entries[1].stride;
    if(!plan.properties.empty()) put(field(1,plan.requirements.size()),RetailT8::Requirement{1100});
    for(size_t row=0;row<plan.properties.size();++row) {
        const auto& value=plan.properties[row];
        put(field(9,row,AT(ExtraMoveProperty,frame)),value.frame); put(field(9,row,AT(ExtraMoveProperty,requirements)),value.frame?guard:uint64_t(0));
        put(field(9,row,AT(ExtraMoveProperty,id)),value.id);
        memcpy(output.data()+field(9,row,AT(ExtraMoveProperty,params)),value.parameters.data(),16);
    }
    for(size_t table:{size_t(10),size_t(11)}) {
        const auto& values=table==10?plan.startProperties:plan.endProperties;
        for(size_t row=0;row<values.size();++row) {
            put(field(table,row,AT(OtherMoveProperty,id)),values[row].id);
            memcpy(output.data()+field(table,row,AT(OtherMoveProperty,params)),values[row].parameters.data(),16);
        }
    }
    for(size_t row=0;row<plan.moves.size();++row) {
        const auto move=Read<G::Move>(records,plan.tables[12].sourceOffset+row*sizeof(G::Move));
        const auto at=field(12,row);
        using M=S8::Move;
        for(auto [offset,value]:{std::pair<size_t,uint32_t>{offsetof(M,name_key),RetailNames::Hash(plan.moves[row].name)},
            {offsetof(M,anim_key),RetailNames::Hash(plan.moves[row].animation)},{offsetof(M,vuln),move.vuln},{offsetof(M,hit_level),move.hitlevel},
            {offsetof(M,character_id),character},{offsetof(M,move_key),move.moveId_val1}}) put(at+offset,RetailNames::Encode(value));
        put(at+offsetof(M,name_addr),Base); put(at+offsetof(M,anim_name_addr),Base); put(at+offsetof(M,anim_addr),uint64_t(1));
        put(at+offsetof(M,cancel_gate_2),move._0x30_int__0x28_related); put(at+offsetof(M,cancel_gate_3),move._0x40_int__0x38_related);
        put(at+offsetof(M,cancel_gate_4),move._0x50_int__0x48_related); put(at+offsetof(M,transition),move.transition); put(at+offsetof(M,end_rotation),move._0x56_short);
        put(at+offsetof(M,animation_length),move.anim_len); put(at+offsetof(M,airborne_start),move.airborne_start); put(at+offsetof(M,airborne_end),move.airborne_end); put(at+offsetof(M,ground_fall),move.ground_fall);
        put(at+offsetof(M,_0x150),move._0x98_int); put(at+offsetof(M,active_start),move.first_active_frame); put(at+offsetof(M,active_end),move.last_active_frame);
        constexpr size_t Hitbox0=offsetof(M,hitboxes);
        put(at+Hitbox0+offsetof(S8::Hitbox,start),move.first_active_frame); put(at+Hitbox0+offsetof(S8::Hitbox,last),move.last_active_frame);
        put(at+Hitbox0+offsetof(S8::Hitbox,location),move.hitbox_location);
        put(at+offsetof(M,collision),move._0xA8_short); put(at+offsetof(M,distance),move.distance);
    }
    for(uint64_t row=0;row<entries[13].count;++row) put(field(13,row),UINT32_MAX);
    for(const auto& ref:plan.references) {
        const auto& target=entries[ref.targetTable];
        put(field(ref.table,ref.row,ref.targetField),ref.index==UINT64_MAX?0:target.address+ref.index*target.stride);
    }
    snapshot.size=output.size(); put(0,snapshot);
    TKMovesetHeader outer=header;
    strcpy_s(outer.version_string,MOVESET_VERSION_STRING);
    strcpy_s(outer.orig_character_name,header.GetOrigCharacterName());
    outer.characterId=character; outer.conversion_origin=GameVersions::T8::FROM_T7; strcpy_s(outer.origin,"Tekken 8");
    auto file=R::Serialize(output,outer);
    auto& emitted=*reinterpret_cast<const TKMovesetHeader*>(file.data());
    if(!RetailT8Import::Prepare(emitted,{file.data()+emitted.moveset_data_start,file.size()-emitted.moveset_data_start}).Ok())
        throw std::runtime_error("Converted T8 reference graph is invalid");
    return file;
}

nlohmann::json Report(const Plan& plan) {
    using nlohmann::json;
    // JSON objects are much larger than the native intermediate. Bound report
    // expansion separately; callers have not installed an output file yet.
    constexpr uint64_t ReportRows = 262144;
    const uint64_t rows = plan.references.size() + plan.moves.size() + plan.idAlignments.size() +
        plan.requirements.size() + plan.properties.size() + plan.startProperties.size() + plan.endProperties.size();
    if (rows > ReportRows) throw std::length_error("T7 to retail T8 JSON plan exceeds report row budget");
    json report{{"format","t7-to-retail-t8-plan-v1"},{"ok",plan.RecordsConverted()},
        {"loadable_moveset",plan.Loadable()},{"source_character",plan.character},
        {"target_build",26155785},{"target_pe_timestamp",0x6ADB1AD5},
        {"reference_encoding","source row index; null = 18446744073709551615"},
        {"issues_total",plan.issueCount},{"issues",json::array()}, {"tables",json::array()},
        {"id_alignments",json::array()},
        {"references",json::array()},{"moves",json::array()}, {"requirements",json::array()},
        {"properties",json::array()},{"start_properties",json::array()},{"end_properties",json::array()},
        {"source_assets",{{"animation_bytes",plan.animationBytes},{"mota_bytes",plan.motaBytes},{"movelist_bytes",plan.movelistBytes}}},
        {"import_blocker",plan.ImportBlocker()}};
    for (size_t i = 0; i < plan.tables.size(); ++i) {
        const auto& table = plan.tables[i];
        report["tables"].push_back({{"name",R::Tables[i].name},{"count",table.count},
            {"source_offset",table.sourceOffset},{"source_stride",table.sourceStride},{"target_stride",table.targetStride}});
    }
    for (const auto& ref : plan.references) report["references"].push_back({{"table",ref.table},{"row",ref.row},
        {"source_field",ref.sourceField},{"target_field",ref.targetField},{"target_table",ref.targetTable},{"index",ref.index}});
    for (const auto& move : plan.moves) report["moves"].push_back({{"row",move.row},{"name",move.name},
        {"animation",move.animation},{"source_animation_offset",move.animationOffset}});
    for (const auto& issue : plan.issues) report["issues"].push_back({{"table",R::Tables[issue.table].name},
        {"row",issue.row},{"id",issue.id},{"parameter",issue.parameter},{"reason",issue.reason}});
    uint64_t selected = 0, converted = 0;
    for (const auto& row : plan.idAlignments) {
        selected += row.target != UINT32_MAX; converted += row.parametersConverted;
        report["id_alignments"].push_back({{"table",R::Tables[row.table].name},{"row",row.row},
            {"source_id",row.source},{"target_id",row.target == UINT32_MAX ? json(nullptr) : json(row.target)},
            {"confidence",row.confidence},{"provenance",row.provenance},{"parameters_converted",row.parametersConverted}});
    }
    report["id_alignment_summary"] = {{"records_total",plan.idAlignments.size()},
        {"selected_target_ids",selected},{"parameters_converted",converted},
        {"unresolved_target_ids",plan.idAlignments.size() - selected}};
    for (const auto& row : plan.requirements) report["requirements"].push_back({{"id",row.id},{"parameters",row.parameters}});
    for (const auto& row : plan.properties) report["properties"].push_back({{"frame",row.frame},{"id",row.id},
        {"guard","unconditional"},{"parameters",row.parameters}});
    for (auto [name, rows] : {std::pair{"start_properties",&plan.startProperties},std::pair{"end_properties",&plan.endProperties}})
        for (const auto& row : *rows) report[name].push_back({{"requirement",row.requirement},{"id",row.id},{"parameters",row.parameters}});
    return report;
}
}
#undef AT

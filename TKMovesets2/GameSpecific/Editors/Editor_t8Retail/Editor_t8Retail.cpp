#include "AppTheme.hpp"
#include "Editor_t8Retail.hpp"
#include "RetailCommands.hpp"
#include "GameIDs.hpp"
#include "Compression.hpp"
#include "constants.h"
#include "Structs_t8.h"
#include <bit>
#include <charconv>
#include <cmath>
#include <limits>

namespace R = RetailT8Snapshot;
namespace S8 = StructsT8;
// The snapshot table strides are the retail record sizes.
static_assert(R::Tables[0].stride==sizeof(S8::Reactions) && R::Tables[1].stride==sizeof(S8::Requirement) &&
    R::Tables[2].stride==sizeof(S8::HitCondition) && R::Tables[3].stride==sizeof(S8::Projectile) &&
    R::Tables[4].stride==sizeof(S8::Pushback) && R::Tables[5].stride==sizeof(S8::PushbackExtra) &&
    R::Tables[6].stride==sizeof(S8::Cancel) && R::Tables[7].stride==sizeof(S8::Cancel) &&
    R::Tables[8].stride==sizeof(S8::CancelExtra) && R::Tables[9].stride==sizeof(S8::ExtraMoveProperty) &&
    R::Tables[10].stride==sizeof(S8::OtherMoveProperty) && R::Tables[11].stride==sizeof(S8::OtherMoveProperty) &&
    R::Tables[12].stride==sizeof(S8::Move) && R::Tables[13].stride==sizeof(S8::Voiceclip) &&
    R::Tables[14].stride==sizeof(S8::InputSequence) && R::Tables[15].stride==sizeof(S8::Input) &&
    R::Tables[16].stride==sizeof(S8::Parry) && R::Tables[17].stride==sizeof(S8::CameraData) &&
    R::Tables[18].stride==sizeof(S8::ThrowCamera) && R::Tables[19].stride==sizeof(S8::Dialogue));
static_assert(R::SourceHeaderSize==sizeof(S8::MovesetHeader));
namespace {
constexpr uint32_t At(size_t offset) { return static_cast<uint32_t>(offset); }
template<class T> T Read(std::span<const Byte> data, size_t offset) {
    if (offset > data.size() || sizeof(T) > data.size() - offset) throw std::out_of_range("Retail field outside record");
    T value; memcpy(&value, data.data() + offset, sizeof(value)); return value;
}
uint64_t Number(std::string_view text, uint64_t maximum) {
    int base = 10;
    if (text.starts_with("0x") || text.starts_with("0X")) { text.remove_prefix(2); base = 16; }
    uint64_t value = 0; auto result = std::from_chars(text.data(), text.data() + text.size(), value, base);
    if (text.empty() || result.ec != std::errc{} || result.ptr != text.data() + text.size() || value > maximum)
        throw std::invalid_argument("Value is outside this field's range");
    return value;
}
[[noreturn]] void Fixed() { throw std::runtime_error("Retail table sizes and animation resources are fixed in this editor."); }
size_t Width(EditorT8Retail::Kind kind) {
    using K = EditorT8Retail::Kind;
    return (kind == K::U16 || kind == K::S16) ? 2 : (kind == K::U64 || kind == K::Reference || kind == K::Encoded32) ? 8 : 4;
}
}
EditorWindowType EditorT8Retail::Type(size_t table) {
    if (table >= R::Tables.size()) throw std::out_of_range("Unknown retail table");
    return 0x80000000U | static_cast<uint32_t>(table);
}
size_t EditorT8Retail::TableIndex(EditorWindowType type) {
    const auto table = type & 0x7fffffffU;
    if (!(type & 0x80000000U) || table >= R::Tables.size()) throw std::out_of_range("Unknown retail form");
    return table;
}
std::optional<uint32_t> EditorT8Retail::Decode(uint64_t value, uint64_t key) {
    uint32_t check = 0, x = static_cast<uint32_t>(value);
    for (int shift = 0; shift < 32; shift += 8, x >>= 8) check ^= x ^ static_cast<uint32_t>(std::rotl(key, shift + 8));
    if (!check) check = 1;
    if (static_cast<uint32_t>(value >> 32) != check) return {};
    auto s = value ^ 0x1dULL;
    return static_cast<uint32_t>((std::rotl(key, static_cast<int>(s & 31)) & ~31ULL) ^ s);
}
uint64_t EditorT8Retail::Encode(uint32_t value, uint64_t key) {
    const auto low = value ^ static_cast<uint32_t>(std::rotl(key, value & 31) & ~31ULL) ^ 0x1dU;
    uint32_t check = 0, x = low;
    for (int shift = 0; shift < 32; shift += 8, x >>= 8) check ^= x ^ static_cast<uint32_t>(std::rotl(key, shift + 8));
    return low | (uint64_t(check ? check : 1) << 32);
}
std::vector<EditorT8Retail::Field> EditorT8Retail::BuildSchema(size_t table) {
    Type(table); using K = Kind; std::vector<Field> f;
    auto add = [&](const char* name, uint32_t offset, K kind = K::U32, int target = -1, bool ro = false) { f.push_back({name,offset,kind,target,ro}); };
    auto ref = [&](const char* name, uint32_t offset, int target) { add(name, offset, K::Reference, target); };
    auto params = [&](uint32_t offset, int count) { for (int i=0;i<count;++i) f.push_back({"parameter_"+std::to_string(i+1),offset+4*i,K::U32}); };
    // Offsets come from the retail record structs (Structs_t8.h).
    switch (table) {
    case 0: {
        // Layout: RE project findings log 2026-11-14 "Reaction move ids and hitbox layout" (reaction picker 0x1418FE330
        // reads move ids from 0x50..0x6A only; 0x44..0x4E and 0x6C..0x6F are 0 in every retail file).
        using T = S8::Reactions;
        for (int i=0;i<7;++i) f.push_back({std::string(std::array{"front","backturned","left_side","right_side","front_counterhit","downed","block"}[i])+"_pushback",At(offsetof(T,pushbacks)+i*sizeof(gameAddr)),K::Reference,4});
        for(int i=0;i<6;++i)f.push_back({std::string(std::array{"front","back","left_side","right_side","front_counterhit","downed"}[i])+"_direction",At(offsetof(T,direction)+i*sizeof(uint16_t)),K::U16});
        for(int i=0;i<4;++i)f.push_back({std::string(std::array{"front","back","left_side","right_side"}[i])+"_rotation",At(offsetof(T,rotation)+i*sizeof(uint16_t)),K::U16});
        add("vertical_pushback",At(offsetof(T,vertical_pushback)),K::S16); add("downed_rotation",At(offsetof(T,downed_rotation)),K::U16);
        for(int i=0;i<14;++i)f.push_back({std::string(std::array{"standing","crouch","counterhit","crouch_counterhit","left_side","crouch_left_side","right_side","crouch_right_side","backturned","crouch_backturned","block","crouch_block","wallslump","downed"}[i])+"_moveid",At(offsetof(T,moveid)+i*sizeof(uint16_t)),K::U16,12});
        add("unknown_0x6C",At(offsetof(T,_0x6C)));
        break;
    }
    case 1: add("condition",At(offsetof(S8::Requirement,condition))); params(At(offsetof(S8::Requirement,params)),4); break;
    case 2: { using T = S8::HitCondition;
        ref("requirements",At(offsetof(T,requirements)),1); add("damage",At(offsetof(T,damage))); add("unknown_0xC",At(offsetof(T,_0xC))); ref("reactions",At(offsetof(T,reactions)),0); break; }
    case 3: { using T = S8::Projectile;
        ref("hit_conditions",At(offsetof(T,hit_condition)),2);ref("cancels",At(offsetof(T,cancel)),6);
        for(uint32_t offset=0;offset<sizeof(T);offset+=4)if(offset<offsetof(T,hit_condition) || offset>=offsetof(T,cancel)+sizeof(gameAddr))f.push_back({"unknown_"+std::to_string(offset),offset,K::U32});
        break; }
    case 4: { using T = S8::Pushback;
        add("duration",At(offsetof(T,duration)),K::U16); add("displacement",At(offsetof(T,displacement)),K::U16); add("extra_count",At(offsetof(T,extra_count))); ref("extra",At(offsetof(T,extra)),5); break; }
    case 5: add("value",At(offsetof(S8::PushbackExtra,value)),K::U16); break;
    case 6: case 7: { using T = S8::Cancel;
        add("command",At(offsetof(T,command)),K::U64); ref("requirements",At(offsetof(T,requirements)),1); ref("extra",At(offsetof(T,extra)),8);
        add("window_start",At(offsetof(T,window_start))); add("window_end",At(offsetof(T,window_end))); add("starting_frame",At(offsetof(T,starting_frame)));
        add("move_id",At(offsetof(T,move_id)),K::U16); add("option",At(offsetof(T,option)),K::U16); break; }
    case 8: add("value",At(offsetof(S8::CancelExtra,value))); break;
    case 9: { using T = S8::ExtraMoveProperty;
        add("frame",At(offsetof(T,frame))); ref("requirements",At(offsetof(T,requirements)),1); add("property",At(offsetof(T,id))); params(At(offsetof(T,params)),5); break; }
    case 10: case 11: { using T = S8::OtherMoveProperty;
        ref("requirements",At(offsetof(T,requirements)),1); add("property",At(offsetof(T,id))); params(At(offsetof(T,params)),5); break; }
    case 12: { using T = S8::Move;
        add("name_hash",At(offsetof(T,name_key)),K::Encoded32,-1,true); add("animation_hash",At(offsetof(T,anim_key)),K::Encoded32,-1,true);
        add("vulnerability",At(offsetof(T,vuln)),K::Encoded32); add("hit_level",At(offsetof(T,hit_level)),K::Encoded32);
        ref("cancels",At(offsetof(T,cancels)),6); ref("cancels_2",At(offsetof(T,cancels_2)),6); add("cancel_gate_2",At(offsetof(T,cancel_gate_2)),K::S32);
        ref("cancels_3",At(offsetof(T,cancels_3)),6); add("cancel_gate_3",At(offsetof(T,cancel_gate_3)),K::S32); ref("cancels_4",At(offsetof(T,cancels_4)),6); add("cancel_gate_4",At(offsetof(T,cancel_gate_4)),K::S32);
        add("transition",At(offsetof(T,transition)),K::U16); add("end_rotation",At(offsetof(T,end_rotation)),K::U16);add("character_id",At(offsetof(T,character_id)),K::Encoded32);add("move_key",At(offsetof(T,move_key)),K::Encoded32);add("unknown_0x150",At(offsetof(T,_0x150)));add("unknown_0x154",At(offsetof(T,_0x154)));
        ref("hit_conditions",At(offsetof(T,hit_conditions)),2); add("damage_override",At(offsetof(T,damage_override)),K::S16);
        add("animation_length_adjuster",At(offsetof(T,animation_length_adjuster))); add("animation_length",At(offsetof(T,animation_length))); add("airborne_start",At(offsetof(T,airborne_start))); add("airborne_end",At(offsetof(T,airborne_end))); add("ground_fall",At(offsetof(T,ground_fall)));
        ref("voiceclips",At(offsetof(T,voiceclip)),13); ref("properties",At(offsetof(T,extra_properties)),9); ref("start_properties",At(offsetof(T,start_properties)),10); ref("end_properties",At(offsetof(T,end_properties)),11);
        // Active window and hitboxes: hit-line builder 0x1418C3EE0, live test 0x141913960 (RE findings log 2026-11-14).
        add("active_start",At(offsetof(T,active_start))); add("active_end",At(offsetof(T,active_end)));
        using H = S8::Hitbox;
        for (int i=0;i<8;++i) {
            auto prefix="hitbox_"+std::to_string(i)+"_";const uint32_t at=At(offsetof(T,hitboxes)+i*sizeof(H));
            f.push_back({prefix+"start",at+At(offsetof(H,start)),K::U32}); f.push_back({prefix+"last",at+At(offsetof(H,last)),K::U32}); f.push_back({prefix+"location",at+At(offsetof(H,location)),K::U32});
            f.push_back({prefix+"radius",at+At(offsetof(H,radius)),K::F32}); f.push_back({prefix+"vertical_height",at+At(offsetof(H,vertical_height)),K::F32});
            for (int j=0;j<3;++j) f.push_back({prefix+"offset_a_"+std::to_string(j),at+At(offsetof(H,offset_a)+j*sizeof(float)),K::F32});
            for (int j=0;j<3;++j) f.push_back({prefix+"offset_b_"+std::to_string(j),at+At(offsetof(H,offset_b)+j*sizeof(float)),K::F32});
            f.push_back({prefix+"starts_off",at+At(offsetof(H,starts_off)),K::U32});
        }
        add("collision",At(offsetof(T,collision)),K::U16); add("distance",At(offsetof(T,distance)),K::U16); break; }
    case 13: params(At(offsetof(S8::Voiceclip,category)),3); break;
    case 14: { using T = S8::InputSequence;
        add("window",At(offsetof(T,window)),K::U16); add("input_count",At(offsetof(T,input_count)),K::U16); add("unknown",At(offsetof(T,_0x4))); ref("inputs",At(offsetof(T,inputs)),15); break; }
    case 15: add("direction",At(offsetof(S8::Input,direction))); add("buttons",At(offsetof(S8::Input,buttons))); break;
    case 16: add("key",At(offsetof(S8::Parry,value))); break;
    case 17: { using T = S8::CameraData;
        add("probability",At(offsetof(T,probability))); add("type",At(offsetof(T,type)),K::U16); add("left_camera",At(offsetof(T,left_camera)),K::U16); add("right_camera",At(offsetof(T,right_camera)),K::U16); add("rotation",At(offsetof(T,rotation)),K::U16); break; }
    case 18: add("side",At(offsetof(S8::ThrowCamera,side)),K::U64); ref("camera",At(offsetof(S8::ThrowCamera,camera)),17); break;
    case 19: { using T = S8::Dialogue;
        add("type",At(offsetof(T,type)),K::U16); add("id",At(offsetof(T,id)),K::U16); ref("requirements",At(offsetof(T,requirements)),1); add("voice_key",At(offsetof(T,voice_key))); add("facial_animation",At(offsetof(T,facial_animation)),K::S32); break; }
    }
    return f;
}
void EditorT8Retail::LoadMovesetPtr(Byte* data, uint64_t size) {
    if (!data || size < sizeof(TKMovesetHeader)) throw std::invalid_argument("Missing retail container");
    auto* outer = reinterpret_cast<TKMovesetHeader*>(data);
    if (!outer->ValidateHeader(size) || outer->header_size < offsetof(TKMovesetHeader,game_specific_flags)+sizeof(uint64_t) || outer->isCompressed() || outer->gameId != GameId_T8 || outer->minorVersion != GameVersions::T8::RETAIL ||
        outer->block_list_size != 1 || outer->moveset_data_start > size || outer->block_list > outer->moveset_data_start ||
        outer->moveset_data_start - outer->block_list < 8 || Read<uint64_t>({data,size},outer->block_list) != 0)
        throw std::invalid_argument("This editor requires a retail T8RS container");
    auto payload = std::span<const Byte>(data + outer->moveset_data_start, size - outer->moveset_data_start);
    R::Validate(payload); auto header=Read<R::Header>(payload,0);
    if (outer->game_specific_flags != (header.version == 1 ? R::TablesOnlyFlag : R::RuntimeAssetsFlag)) throw std::invalid_argument("Retail profile flags differ");
    std::array<R::Entry,20> entries;
    for (size_t i=0;i<entries.size();++i) entries[i]=Read<R::Entry>(payload,header.directoryOffset+i*sizeof(R::Entry));
    auto metadata=std::make_shared<const RetailEditorData::Metadata>(RetailEditorData::Read(*outer,payload));
    m_metadata=std::move(metadata); m_entries=entries; m_moveset=data; m_movesetSize=size; m_header=outer; m_insertedRows.clear();
    m_movesetData=data+outer->moveset_data_start; m_movesetDataSize=payload.size(); live_loadedMoveset=0;
}
bool EditorT8Retail::LoadMoveset(Byte* data, uint64_t size) {
    if (data==m_moveset) return false;
    std::unique_ptr<Byte,decltype(&free)> owner(data,&free);
    if (!CompressionUtils::RAW::Moveset::ExpandFileBuffer(data,size)) return false;
    owner.release(); owner.reset(data);
    auto* previous=m_moveset; const auto previousSize=m_movesetSize;
    try {
        LoadMovesetPtr(data,size);
        if (CalculateCRC32()!=m_header->crc32) throw std::invalid_argument("Retail payload CRC differs");
        owner.release(); free(previous); return true;
    } catch (...) {
        if (previous) LoadMovesetPtr(previous,previousSize);
        else { m_moveset=nullptr; m_movesetSize=0; m_header=nullptr; m_movesetData=nullptr; m_movesetDataSize=0; m_entries={}; }
        return false;
    }
}

uint64_t EditorT8Retail::Count(size_t table) const { Type(table); return m_movesetData ? m_entries[table].count : 0; }
uint32_t EditorT8Retail::MoveFingerprint(uint32_t row) const {
    auto record=Record(12,row);std::vector<Byte> bytes(record.begin(),record.end());
    for(const auto& field:Schema(12))if(field.kind==Kind::Reference){auto pointer=Read<uint64_t>(record,field.offset);const auto& entry=m_entries[field.target];if(pointer && pointer>=entry.address && pointer-entry.address<entry.count*entry.stride && (pointer-entry.address)%entry.stride==0){auto id=(pointer-entry.address)/entry.stride;memcpy(bytes.data()+field.offset,&id,8);}}
    return Helpers::CalculateCrc32(bytes.data(),bytes.size());
}
uint32_t EditorT8Retail::ResolveMove(uint32_t id) const {
    if(id>=0x8000 && id<0x8000+60) {
        const auto header=Read<R::Header>({m_movesetData,static_cast<size_t>(m_movesetDataSize)},0);
        id=Read<uint16_t>({m_movesetData,static_cast<size_t>(m_movesetDataSize)},header.sourceHeaderOffset+0xa8+2*(id-0x8000));
    }
    if(id>=Count(12))throw std::out_of_range("Move ID or alias is outside this moveset");
    return id;
}
std::vector<uint32_t> EditorT8Retail::MoveAliases(uint32_t row) const {
    std::vector<uint32_t> result;
    for(uint32_t alias=0x8000;alias<0x8000+60;++alias)try{if(ResolveMove(alias)==row)result.push_back(alias);}catch(const std::out_of_range&){}
    return result;
}
bool EditorT8Retail::MatchesMoveNames(std::span<const Byte> loaded) const {
    if(loaded.size()!=Count(12)*0x448)return false;
    for(uint32_t row=0;row<Count(12);++row) {
        auto hash=Decode(Read<uint64_t>(loaded,row*0x448),Read<uint64_t>(loaded,row*0x448+8));
        if(!hash || hash!=MoveNameHash(row))return false;
    }
    return true;
}
std::string EditorT8Retail::InputSequenceText(uint32_t sequence) const {
    // Port of EditorT7::GetInputSequenceString: input_sequences {u16 window; u16 count; u32; u64 inputs}.
    auto record=Record(14,sequence);const auto count=Read<uint16_t>(record,2);const auto pointer=Read<uint64_t>(record,8);
    if(count==0 || count>MAX_INPUT_SEQUENCE_SHORT_LEN)return std::to_string(count)+" inputs";
    const auto& inputs=m_entries[15];
    if(pointer<inputs.address || (pointer-inputs.address)%inputs.stride || (pointer-inputs.address)/inputs.stride+count>inputs.count)return "Invalid inputs";
    std::string result;const auto first=uint32_t((pointer-inputs.address)/inputs.stride);
    for(uint32_t i=0;i<count;++i){if(i)result+=", ";result+=RetailT8Commands::CommandString(Read<uint64_t>(Record(15,first+i),0));}
    return result;
}
std::optional<uint32_t> EditorT8Retail::ReferenceIndex(size_t table,uint32_t row,const Field& field) const {
    if(field.kind!=Kind::Reference || field.target<0)return std::nullopt;
    auto record=Record(table,row);const auto pointer=Read<uint64_t>(record,field.offset);const auto& target=m_entries[field.target];
    if(!pointer || !target.count || pointer<target.address || pointer-target.address>=target.count*target.stride || (pointer-target.address)%target.stride)return std::nullopt;
    return uint32_t((pointer-target.address)/target.stride);
}
std::vector<EditorT8Retail::Reference> EditorT8Retail::ReferencesTo(size_t table,uint32_t row) const {
    Record(table,row);std::vector<Reference> result;const auto target=m_entries[table].address+row*m_entries[table].stride;
    for(size_t source=0;source<20;++source)for(uint32_t id=0;id<Count(source);++id){auto record=Record(source,id);
        for(const auto& field:Schema(source)){
            bool matches=false;
            if(field.kind==Kind::Reference && field.target==table)matches=Read<uint64_t>(record,field.offset)==target;
            if((source==6 || source==7) && field.name=="move_id"){
                const bool group=Read<uint64_t>(record,0)==RetailT8Commands::GroupCancel;
                if(table==7 && group)matches=Read<uint16_t>(record,36)==row;
                if(table==12 && !group)try{matches=ResolveMove(Read<uint16_t>(record,36))==row;}catch(const std::out_of_range&){}
            }
            if(table==12 && ((source==12 && field.name=="transition") || (source==0 && field.name.ends_with("_moveid"))))try{matches=ResolveMove(Read<uint16_t>(record,field.offset))==row;}catch(const std::out_of_range&){}
            if(table==14 && (source==6 || source==7) && field.name=="command"){
                const auto command=Read<uint64_t>(record,0);matches=RetailT8Commands::IsInputSequence(command) && RetailT8Commands::InputSequenceId(command)==row;
            }
            if((table==3 || table==18) && (source==9 || source==10 || source==11) && field.name=="parameter_1"){
                const auto property=Read<uint32_t>(record,source==9?16:8);matches=Read<uint32_t>(record,field.offset)==row && ((table==3 && property==33403) || (table==18 && property==34447));
            }
            if(matches)result.push_back({source,id,field.name});
        }
    }
    return result;
}
std::span<const Byte> EditorT8Retail::Record(size_t table, uint32_t row) const {
    Type(table); const auto& e=m_entries[table]; if (!m_movesetData || row>=e.count) throw std::out_of_range("Retail row outside table");
    return {m_movesetData+e.offset+row*e.stride,e.stride};
}
std::optional<uint32_t> EditorT8Retail::MoveNameHash(uint32_t row) const { auto r=Record(12,row); return Decode(Read<uint64_t>(r,0),Read<uint64_t>(r,8)); }
std::string EditorT8Retail::FieldText(const Field& field,EditorInput* input) const {
    auto text=InputString(input);
    if(!(input->flags&EditorInput_DataChangeable))return std::string(text);
    const auto width=(field.kind==Kind::U16||field.kind==Kind::S16)?16:field.kind==Kind::U64?64:32;
    const uint64_t maximum=width==64?UINT64_MAX:(uint64_t(1)<<width)-1;
    uint64_t bits=0;
    if(input->flags&EditorInput_Float){
        float value;auto result=std::from_chars(text.data(),text.data()+text.size(),value);
        if(text.empty()||result.ec!=std::errc{}||result.ptr!=text.data()+text.size()||!std::isfinite(value))throw std::invalid_argument("Invalid finite float");bits=std::bit_cast<uint32_t>(value);
    }else if(input->flags&EditorInput_Hex){auto normalized=std::string(text);if(!text.starts_with("0x")&&!text.starts_with("0X"))normalized="0x"+normalized;bits=Number(normalized,maximum);}
    else if(input->flags&EditorInput_Signed){
        int64_t value;auto result=std::from_chars(text.data(),text.data()+text.size(),value);
        if(text.empty()||result.ec!=std::errc{}||result.ptr!=text.data()+text.size() || (width<64 && (value<-(int64_t(1)<<(width-1)) || value>(int64_t(1)<<(width-1))-1)))throw std::invalid_argument("Signed value is outside the field range");bits=uint64_t(value)&maximum;
    }else bits=Number(text,maximum);
    if(field.kind==Kind::S32)return std::to_string(int32_t(bits));
    if(field.kind==Kind::S16)return std::to_string(int16_t(bits));
    if(field.kind==Kind::F32){auto value=std::bit_cast<float>(uint32_t(bits));if(!std::isfinite(value))throw std::invalid_argument("Non-finite float bits");char buffer[64];auto result=std::to_chars(buffer,buffer+sizeof(buffer),value);return {buffer,result.ptr};}
    return std::to_string(bits);
}
uint64_t EditorT8Retail::Parse(const Field& f, std::string_view text, std::span<const Byte> row) const {
    using K=Kind; const auto old = Width(f.kind)==2 ? Read<uint16_t>(row,f.offset) : Width(f.kind)==4 ? Read<uint32_t>(row,f.offset) : Read<uint64_t>(row,f.offset);
    if (f.kind==K::Reference) {
        const auto& target=m_entries[f.target];
        const bool mapped=old && target.count && old>=target.address && old-target.address<target.count*target.stride && (old-target.address)%target.stride==0;
        if (old && !mapped) { if (Number(text,UINT64_MAX)!=old) throw std::invalid_argument("External references are retained read-only"); return old; }
        if (text=="-1") return 0;
        auto index=Number(text,UINT32_MAX); if (index>=target.count) throw std::out_of_range("Reference index exceeds target table");
        return target.address+index*target.stride;
    }
    uint64_t value;
    if (f.kind==K::S32) {
        int32_t v=0; auto parsed=std::from_chars(text.data(),text.data()+text.size(),v);
        if (text.empty() || parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size()) throw std::invalid_argument("Invalid signed 32-bit value");
        value=static_cast<uint32_t>(v);
    } else if (f.kind==K::S16) {
        int16_t v=0; auto parsed=std::from_chars(text.data(),text.data()+text.size(),v);
        if (text.empty() || parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size()) throw std::invalid_argument("Invalid signed 16-bit value");
        value=static_cast<uint16_t>(v);
    } else if (f.kind==K::F32 && std::isfinite(std::bit_cast<float>(static_cast<uint32_t>(old)))) {
        float v=0; auto parsed=std::from_chars(text.data(),text.data()+text.size(),v);
        if (text.empty() || parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || !std::isfinite(v)) throw std::invalid_argument("Invalid finite float");
        value=std::bit_cast<uint32_t>(v);
    } else value=Number(text,f.kind==K::U16 ? UINT16_MAX : f.kind==K::U64 ? UINT64_MAX : UINT32_MAX);
    if (f.kind==K::Encoded32) {
        auto decoded=Decode(old,Read<uint64_t>(row,f.offset+8));
        if (!decoded || (f.readOnly && value!=*decoded)) throw std::invalid_argument("This encoded identity is read-only");
        return value==*decoded ? old : Encode(static_cast<uint32_t>(value),Read<uint64_t>(row,f.offset+8));
    }
    if ((f.readOnly || (f.kind==K::F32 && !std::isfinite(std::bit_cast<float>(static_cast<uint32_t>(old))))) && value!=old)
        throw std::invalid_argument("This field is read-only");
    return value;
}
InputMap EditorT8Retail::RecordFields(size_t table, uint32_t id, VectorSet<std::string>& order) {
    auto row=Record(table,id); InputMap result;
    for (auto f:Schema(table)) {
        using K=Kind; EditorInput* input=nullptr;
        auto integer = [&](uint64_t value, uint32_t flags) { return EditorUtils::CreateField(f.name,order,result,0,flags,value); };
        if (f.kind==K::Encoded32) {
            auto value=Decode(Read<uint64_t>(row,f.offset),Read<uint64_t>(row,f.offset+8));
            if (!value) continue;
            input=integer(*value,f.name=="hit_level" ? EditorInput_H32 : EditorInput_U32);
        } else if (f.kind==K::Reference) {
            auto value=Read<uint64_t>(row,f.offset); const auto& t=m_entries[f.target];
            if (!value || (t.count && value>=t.address && value-t.address<t.count*t.stride && (value-t.address)%t.stride==0)) {
                auto index=value ? static_cast<int64_t>((value-t.address)/t.stride) : -1;
                input=EditorUtils::CreateField(f.name,order,result,0,EditorInput_S64|EditorInput_Clickable,index);
            } else { input=integer(value,EditorInput_H64); f.readOnly=true; }
        } else if (f.kind==K::F32) {
            auto bits=Read<uint32_t>(row,f.offset); auto value=std::bit_cast<float>(bits);
            if (std::isfinite(value)) {
                input=EditorUtils::CreateField(f.name,order,result,0,EditorInput_Float,value);
                sprintf_s(input->buffer,input->bufsize,"%.9g",value);
                input->imguiInputFlags=(input->imguiInputFlags & ~ImGuiInputTextFlags_CharsDecimal) | ImGuiInputTextFlags_CharsScientific;
            } else { input=integer(bits,EditorInput_H64); f.readOnly=true; }
        } else if (f.kind==K::S32) input=EditorUtils::CreateField(f.name,order,result,0,EditorInput_S32,Read<int32_t>(row,f.offset));
        else if (f.kind==K::S16) input=EditorUtils::CreateField(f.name,order,result,0,EditorInput_S16,Read<int16_t>(row,f.offset));
        else input=integer(f.kind==K::U16 ? Read<uint16_t>(row,f.offset) : f.kind==K::U64 ? Read<uint64_t>(row,f.offset) : Read<uint32_t>(row,f.offset),f.kind==K::U64 ? EditorInput_H64 : f.name.ends_with("_location") ? EditorInput_H32 : f.kind==K::U16 ? EditorInput_U16 : EditorInput_U32);
        // Ctrl+B cycles integer bases only: a float's bits shown as an integer would be written back as a float value.
        if(f.kind!=K::Reference && f.kind!=K::F32 && !f.readOnly)input->flags|=EditorInput_DataChangeable;
        input->fullName=f.name; input->displayName=f.name;
        if(f.name.starts_with("unknown_"))input->category=5;
        if (table==12) {
            if (f.name.starts_with("hitbox_")) input->category=7+2*(f.name[7]-'0');
            else if (f.name.starts_with("cancel_gate_") || f.name=="cancels_2" || f.name=="cancels_3" || f.name=="cancels_4" ||
                f.name=="end_rotation" || f.name=="animation_length_adjuster" || f.name=="damage_override" || f.name=="collision") input->category=3;
            else if (f.kind==K::Reference) input->category=2;
        } else if (f.kind==K::Reference) input->category=2;
        else if (f.name=="unknown") input->category=5;
        if (!input->color || f.kind==K::S32 || f.kind==K::S16) input->color=AppTheme::Input;
        if (f.kind==K::Reference) input->color=FORM_INPUT_REF;
        input->hoverDescription="Retail record offset "+std::to_string(f.offset);
        if (f.readOnly) input->imguiInputFlags|=ImGuiInputTextFlags_ReadOnly;
    }
    return result;
}
bool EditorT8Retail::ValidateField(EditorWindowType type, EditorInput* input) {
    try {
        auto text=InputString(input);
        for (const auto& f:Schema(TableIndex(type))) if (f.name==input->name.str()) {
            if (input->imguiInputFlags & ImGuiInputTextFlags_ReadOnly) return !text.empty();
            auto canonical=FieldText(f,input);text=canonical;
            if (f.kind==Kind::Reference) return text=="-1" || Number(text,UINT32_MAX)<Count(f.target);
            // Use the engine parser with a neutral scalar record. Encoded identities
            // require their real key at Apply; their displayed value is uint32.
            if (f.kind==Kind::Encoded32) { Number(text,UINT32_MAX); return true; }
            std::array<Byte,0x448> row{}; Parse(f,text,row); return true;
        }
        return false;
    } catch (...) { return false; }
}
void EditorT8Retail::SaveRecord(size_t table, uint32_t id, InputMap& fields) {
    auto row=Record(table,id); const auto& schema=Schema(table);
    struct Patch { uint32_t offset; size_t width; uint64_t value; }; std::vector<Patch> patches;
    size_t expected=0;
    for (const auto& f:schema) {
        if (f.kind==Kind::Encoded32 && !Decode(Read<uint64_t>(row,f.offset),Read<uint64_t>(row,f.offset+8))) continue;
        ++expected; auto input=fields.at(f.name);
        try { patches.push_back({f.offset,Width(f.kind),Parse(f,FieldText(f,input),row)}); }
        catch (const std::exception& error) { throw std::invalid_argument(f.name+": "+error.what()); }
    }
    if (fields.size()!=expected) throw std::invalid_argument("Retail field schema differs");
    // Terminators are retained when applying a list, including after resizing. Extra properties end on property id 0
    // (+16), whatever the frame. Rows just inserted by EditRows are zero-filled, so an inserted extra property row reads
    // as a terminator until its fields are saved: it is not one.
    const bool inserted=m_insertedRows.contains(std::pair<size_t,uint32_t>(table,id));
    for (const auto& p:patches) {
        const bool terminal = !inserted && ((table==1 && p.offset==0 && Read<uint32_t>(row,0)==1100) ||
            ((table==6 || table==7) && p.offset==0 && (Read<uint64_t>(row,0)==0x8000 || (table==7 && Read<uint64_t>(row,0)==RetailT8Commands::GroupEnd))) ||
            (table==9 && p.offset==16 && Read<uint32_t>(row,16)==0) ||
            ((table==10 || table==11) && p.offset==8 && Read<uint32_t>(row,8)==1100) || (table==13 && p.offset==0 && Read<uint32_t>(row,0)==UINT32_MAX));
        if (terminal && p.value!=(Width(Kind::U64)==p.width ? Read<uint64_t>(row,p.offset) : Read<uint32_t>(row,p.offset)))
            throw std::invalid_argument("The existing list terminator must be retained");
    }
    if (table==14) {
        uint64_t count=0,pointer=0;
        for (const auto& p:patches) { if (p.offset==2) count=p.value; if (p.offset==8) pointer=p.value; }
        const auto& t=m_entries[15];
        if ((count!=Read<uint16_t>(row,2) || pointer!=Read<uint64_t>(row,8)) && count && (!pointer || pointer<t.address || (pointer-t.address)%t.stride || (pointer-t.address)/t.stride>=t.count || count>t.count-(pointer-t.address)/t.stride))
            throw std::invalid_argument("Input sequence exceeds the input table");
    }
    uint64_t proposedCommand=(table==6 || table==7)?Read<uint64_t>(row,0):0;
    for(const auto& patch:patches)if((table==6 || table==7) && patch.offset==0)proposedCommand=patch.value;
    if (table==0 || table==6 || table==7 || table==12) {
        for (const auto& p:patches) {
            // Reaction move ids: 0x50..0x6A only (0x4E is downed_rotation, not a move id).
            constexpr uint32_t reactionIds=At(offsetof(StructsT8::Reactions,moveid));
            const bool moveId=(table==0 && p.offset>=reactionIds && p.offset<reactionIds+At(sizeof(StructsT8::Reactions::moveid))) || (table==12 && p.offset==0xcc) || ((table==6 || table==7) && p.offset==36);
            if((table==6 || table==7) && p.offset==36 && proposedCommand==RetailT8Commands::GroupCancel){
                if(p.value>=Count(7))throw std::out_of_range("Grouped cancel reference exceeds table");continue;
            }
            if (moveId && p.value!=Read<uint16_t>(row,p.offset) && p.value>=Count(12) &&
                !(p.value>=0x8000 && p.value<0x8000+60) && p.value!=0xffff)
                throw std::out_of_range("Move reference exceeds the moves/aliases range");
        }
    }
    if (table==12) for (uint32_t offset:{0xa0U,0xb0U,0xc0U}) {
        uint64_t pointer=0,gate=0;
        for (const auto& p:patches) { if (p.offset==offset) pointer=p.value; if (p.offset==offset+8) gate=p.value; }
        const auto& t=m_entries[6];
        if ((pointer!=Read<uint64_t>(row,offset) || static_cast<uint32_t>(gate)!=Read<uint32_t>(row,offset+8)) && static_cast<int32_t>(gate)>0 && pointer && (!t.count || pointer<t.address || pointer-t.address>=t.count*t.stride || (pointer-t.address)%t.stride))
            throw std::invalid_argument("Cannot activate an unresolved optional cancel bank");
    }
    // All checks and allocations precede the first write. Fixed-width field
    // patches cannot change container extents or animation resources.
    auto apply=[&] {
        auto target=m_movesetData+m_entries[table].offset+id*m_entries[table].stride;
        for (const auto& p:patches) memcpy(target+p.offset,&p.value,p.width);
        // Encrypted move values keep a plain copy in slot row % 8 - 4 when row % 8 >= 4 (SetPlainCopy); the
        // game cross-checks it (a stale hit-level copy makes the attack never connect).
        if (table==12) for (const auto& p:patches) {
            if (p.offset!=0x58 && p.offset!=0x78 && p.offset!=0xd0 && p.offset!=0xf0) continue;
            uint64_t key; memcpy(&key,target+p.offset+8,8);
            if (auto plain=Decode(p.value,key)) SetPlainCopy(target+p.offset,id,*plain);
        }
    };
    if (m_activeBatch) Mutate(apply); else apply();
    m_insertedRows.erase(std::pair<size_t,uint32_t>(table,id));
}
void EditorT8Retail::SetPlainCopy(Byte* field,uint32_t row,uint32_t value) {
    // Native layout (all 64,293 native moves, T7ToRetailT8Standalone SetEncodedField): four u32 slots after value/key
    // hold 0x765 + row, except slot row % 8 - 4, which holds the plain value when row % 8 >= 4. Other rows have no copy.
    if (row%8>=4) memcpy(field+0x10+(row%8-4)*4,&value,sizeof(value));
}
uint32_t EditorT8Retail::CalculateCRC32() { return Helpers::CalculateCrc32(m_movesetData,m_movesetDataSize); }

void EditorT8Retail::RestoreMovesetPtr(Byte* data,uint64_t size) noexcept {
    m_moveset=data; m_movesetSize=size; m_header=reinterpret_cast<TKMovesetHeader*>(data);
    m_movesetData=data+m_header->moveset_data_start; m_movesetDataSize=size-m_header->moveset_data_start;
    R::Header header; memcpy(&header,m_movesetData,sizeof(header));
    memcpy(m_entries.data(),m_movesetData+header.directoryOffset,sizeof(m_entries));
    m_insertedRows.clear();
}

const std::vector<EditorT8Retail::Field>& EditorT8Retail::Schema(size_t table) {
    Type(table);
    static const auto schemas=[] { std::array<std::vector<Field>,20> result; for(size_t i=0;i<result.size();++i) result[i]=BuildSchema(i); return result; }();
    return schemas[table];
}

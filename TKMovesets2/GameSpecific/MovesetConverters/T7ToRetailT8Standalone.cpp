// Self-contained T7 -> retail T8 conversion (ConvertStandalone).
//
// Every record is built from the T7 moveset plus fixed retail tables shipped with
// the tool: move-record field defaults measured over native exports, the
// encrypted-field layout, the alias catalog, pooled cross-character votes and the
// built-in body skeleton for animations. No other moveset is read, so the output is
// the same for every import target. Requirement/property IDs are mapped by the
// static aliases, then the cross-character votes, then the rest of the alias
// catalog. See docs/conversion.md.
#include "T7ToRetailT8.hpp"
#include "ConversionRules.hpp"
#include "RetailT8Aliases.hpp"
#include "Importer_t7/Importer_t7_Utils.hpp"
#include "Extractor_t8/RetailSnapshot.hpp"
#include "GameIDs.hpp"
#include "RetailNames.hpp"
#include "Importer_t8/RetailImport.hpp"
#include "constants.h"
#include "T7AnimToRetailT8.hpp"
#include "RetailPanm.hpp"
#include "AnimationReader.hpp"
#include "MovesetFile.hpp"
#include "MovesetFolders.hpp"
#include "Helpers.hpp"
#include "Structs_t8.h"
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <nlohmann/json.hpp>
#include <bit>
#include <cstring>
#include <map>
#include <optional>
#include <unordered_map>

namespace T7ToRetailT8 {
namespace {
namespace S = StructsT7;
namespace G = StructsT7_gameAddr;
namespace R = RetailT8Snapshot;
namespace S8 = StructsT8;
// Retail field offset (Structs_t8.h). Offsets read from T7 rows (Get on a T7 row) stay T7 layout literals.
#define AT(record, member) uint32_t(offsetof(S8::record, member))
constexpr uint64_t Null7 = UINT64_MAX;
constexpr uint64_t Base = 0x10000000; // file-local virtual address space
constexpr uint64_t ListLimit = 4096;

template<class T> T Get(std::span<const Byte> bytes, uint64_t offset) {
    if (offset > bytes.size() || sizeof(T) > bytes.size() - offset)
        throw std::runtime_error("Truncated conversion field");
    T result; memcpy(&result, bytes.data() + offset, sizeof(T)); return result;
}
template<class T> void Set(std::vector<Byte>& bytes, uint64_t offset, T value) {
    if (offset > bytes.size() || sizeof(T) > bytes.size() - offset)
        throw std::runtime_error("Conversion write outside output");
    memcpy(bytes.data() + offset, &value, sizeof(T));
}
// Retail encrypted 32-bit value (value qword + key qword), see EditorT8Retail.
std::optional<uint32_t> Decode(uint64_t value, uint64_t key) {
    uint32_t check = 0, x = static_cast<uint32_t>(value);
    for (int shift = 0; shift < 32; shift += 8, x >>= 8) check ^= x ^ static_cast<uint32_t>(std::rotl(key, shift + 8));
    if (!check) check = 1;
    if (static_cast<uint32_t>(value >> 32) != check) return {};
    auto s = value ^ 0x1dULL;
    return static_cast<uint32_t>((std::rotl(key, static_cast<int>(s & 31)) & ~31ULL) ^ s);
}
// Encrypted move fields are 0x20-byte blocks: value, key, then four u32 slots where one slot keeps
// a plain copy of the value and the others a filler constant. The game cross-checks the copy (a
// mismatched hit level makes attacks never connect), so re-encoding must update the copy too.
void SetEncoded(std::vector<Byte>& out, uint64_t at, uint32_t value);
uint64_t Encode(uint32_t value, uint64_t key) {
    const auto low = value ^ static_cast<uint32_t>(std::rotl(key, value & 31) & ~31ULL) ^ 0x1dU;
    uint32_t check = 0, x = low;
    for (int shift = 0; shift < 32; shift += 8, x >>= 8) check ^= x ^ static_cast<uint32_t>(std::rotl(key, shift + 8));
    return low | (uint64_t(check ? check : 1) << 32);
}
void SetEncoded(std::vector<Byte>& out, uint64_t at, uint32_t value) {
    uint64_t key; memcpy(&key, out.data() + at + AT(EncryptedValue, key), 8);
    const auto encoded = Encode(value, key); memcpy(out.data() + at + AT(EncryptedValue, value), &encoded, 8);
    uint32_t slots[4]; memcpy(slots, out.data() + at + AT(EncryptedValue, check_slots), sizeof(slots));
    int copy = -1;  // the slot that differs from the three filler slots
    for (int k = 0; k < 4; ++k) {
        int same = 0;
        for (int m = 0; m < 4; ++m) same += m != k && slots[m] == slots[k];
        if (same == 0) { copy = k; break; }
    }
    if (copy < 0) return; // template value equals the filler: no detectable copy slot
    slots[copy] = value; memcpy(out.data() + at + AT(EncryptedValue, check_slots), slots, sizeof(slots));
}

// Rules from t7_t8_conversion_rules.ini (built-in defaults: Rules/t7_t8_conversion_rules.ini; docs/conversion-rules.md).
const ConversionRules::Rules& Rules() { return ConversionRules::Current(); }
// T7 character IDs (from the T7 extracts' headers) -> retail T8 IDs (map:t7_t8_character).
uint32_t CharacterT7ToT8(uint32_t id) { return Rules().Map("t7_t8_character").Map(ConversionRules::ToT8, id).value_or(UINT32_MAX); }

// Special cancel commands were renumbered in retail T8 (map:t7_t8_cmd): 0x8003.. moved by two, group cancel 0x800b ->
// 0x8012, group end 0x800c -> 0x8013 and the input sequence base 0x800d -> 0x8014. T7 0x410/0x440 walk inputs lost
// bit 0x400 (map:t7_t8_walk). Evidence: cancel lists of the 1,724 Kazuya moves shared by both games.
uint64_t Command(uint64_t command) {
    if (command >> 32) {
        // Button bits: T7 Rage Art is bit 4; retail moved Rage Art to bit 6 and uses bit 4 for Heat (retail cancels
        // with bit 4 lead to *_HeatSmash / *_EX moves, bit 6 to *_RageArts00: RE project findings log 2026-11-14).
        constexpr uint64_t T7RageArt = 1ULL << (32 + 4), RetailRageArt = 1ULL << (32 + 6);
        return (command & T7RageArt) ? (command & ~T7RageArt) | RetailRageArt : command;
    }
    const auto& map = Rules().Map(command >= 0x8000 ? "t7_t8_cmd" : "t7_t8_walk");
    return map.Map(ConversionRules::ToT8, static_cast<uint32_t>(command)).value_or(static_cast<uint32_t>(command));
}
// T8 inserted three move-alias slots at index 30 and one at 58, 60 vs 56 (map:t7_t8_alias_slot, which covers 0..55).
uint32_t AliasSlot(uint32_t slot) { return Rules().Map("t7_t8_alias_slot").Map(ConversionRules::ToT8, slot).value_or(slot); }
uint16_t MoveId(uint16_t id) {
    if (id >= 0x8000 && id < 0x8000 + 56) return static_cast<uint16_t>(0x8000 + AliasSlot(id - 0x8000));
    return id;
}

constexpr uint32_t Strides[] = {
    sizeof(S::Reactions), sizeof(S::Requirement), sizeof(S::HitCondition), sizeof(S::Projectile),
    sizeof(S::Pushback), sizeof(S::PushbackExtradata), sizeof(S::Cancel), sizeof(S::Cancel),
    sizeof(S::CancelExtradata), sizeof(S::ExtraMoveProperty), sizeof(S::OtherMoveProperty),
    sizeof(S::OtherMoveProperty), sizeof(S::Move), sizeof(S::Voiceclip), sizeof(S::InputSequence),
    sizeof(S::Input), sizeof(S::UnknownParryRelated), sizeof(S::CameraData), sizeof(S::ThrowCamera)
};

struct T7View {
    std::span<const Byte> payload, records;
    std::array<uint64_t, 19> count{}, offset{};
    uint64_t names = 0;
    std::span<const Byte> Row(size_t table, uint64_t row) const {
        if (row >= count[table]) throw std::runtime_error("T7 reference outside " + std::string(R::Tables[table].name));
        return records.subspan(offset[table] + row * Strides[table], Strides[table]);
    }
    std::string Name(uint64_t at) const {
        if (names + at >= payload.size()) throw std::runtime_error("T7 name outside name block");
        const auto* text = reinterpret_cast<const char*>(payload.data() + names + at);
        return std::string(text, strnlen(text, payload.size() - names - at));
    }
};

// Pooled votes from every character exported from both games (tools/generate_t7_t8_cross_votes.py).
// Used after the static aliases and before the rest of the alias catalog.
struct CrossVote { bool property; uint32_t source, target, votes, total; };
constexpr CrossVote CrossVotes[] = {
#include "T7ToRetailT8Votes.inc"
};
std::optional<uint32_t> CrossVoteFor(bool property, uint32_t id) {
    const auto found = std::lower_bound(std::begin(CrossVotes), std::end(CrossVotes), std::pair{property, id},
        [](const CrossVote& row, auto key) { return std::pair{row.property, row.source} < key; });
    if (found == std::end(CrossVotes) || found->property != property || found->source != id) return {};
    return found->target;
}
// T7 voiceclip u32 = category << 24 | id; retail T8 = {category, 0, id}. Categories keep their IDs (0/1/6/7 on every
// paired Kazuya/Jin/Paul/King move) except those in map:t7_to_t8_voice_fixed_id (category 2 always becomes 39302).
std::array<uint32_t, 3> Voiceclip(uint32_t value) {
    if (value == UINT32_MAX) return {UINT32_MAX, UINT32_MAX, UINT32_MAX};
    const uint32_t category = value >> 24;
    const auto fixed = Rules().Map("t7_to_t8_voice_fixed_id").Map(ConversionRules::ToT8, category);
    return {category, 0, fixed ? *fixed : value & 0xffffffU};
}
// Retail properties that play a hand, face, wing or camera animation by id (list:t7_t8_drop_t8_props). The handlers match
// T7's, but the id indexes the target character's assets in retail, and converted movesets carry no such assets: on
// Akuma (P1 Jin) the idle's hand animations 0x40FC/0x40FD skipped the idle to its last frame and the moveset cycled
// through its Co_Dummy moves with no input (2026-11-14). Kept dropped, as before the handler-review mapping.
bool PlaysUnconvertedAsset(uint32_t retail) { return Rules().InList("t7_t8_drop_t8_props", retail); }
// Properties whose parameter encoding changed (t7_to_t8_prop transforms; docs/conversion.md,
// "Handler-review mapping pass"). Returns the retail id and its five parameters, or nothing; `drop` is set when a
// transform recognized the id and asked for the row to be dropped (miss = drop), so no other mapping may keep it.
struct ConvertedProperty { uint32_t id; std::array<uint32_t, 5> params; };
std::optional<ConvertedProperty> ParamProperty(uint32_t id, uint32_t value, bool& drop, const ConversionRules::Settings& settings) {
    drop = false;
    uint32_t target; std::array<uint32_t, 5> p{};
    switch (Rules().ToT8(id, value, target, p, settings)) {
    case ConversionRules::Rules::Applied::Mapped: return ConvertedProperty{target, p};
    case ConversionRules::Rules::Applied::Drop: drop = true; return {};
    default: break;
    }
    // Current-alias overwrite and reset blocks (map:t7_t8_prop_alias_block, 33 each = T7 0x83C4.. / 0x83E5.. -> retail
    // 0x850D.. / 0x852E..): the index is the alias slot, which retail renumbered (AliasSlot), and the parameter is an
    // own move id (MoveId).
    const auto size = Rules().Value("t7_t8_prop_alias_block_size");
    for (const auto& block : Rules().Map("t7_t8_prop_alias_block").lines) {
        if (!block.forward[ConversionRules::ToT8] || id < block.first[0] || id >= block.first[0] + size) continue;
        const auto slot = AliasSlot(id - block.first[0]);
        if (slot >= size) return {};
        return ConvertedProperty{block.first[1] + slot, {(value & 0xffff0000U) | MoveId(static_cast<uint16_t>(value)), 0, 0, 0, 0}};
    }
    return {};
}

// ---- Built-in retail knowledge for the self-contained converter --------------------------------------
// Placeholder identity of converted movesets (no retail character uses it). The importer swaps it for the
// target's identity, as it does for any cross-character import.
// Header compile date shared by every native export of build 26155785 (27 measured).
constexpr uint32_t RetailHeaderCompileDate = 0x0134d94f;
// Character identity code: n << 16 | (0x10000 - n). Moves carry code(id - 1) at +0xd0 (encrypted); the header
// carries code(id - 1) at +0x160 and code(id) at +0x164 (all 64,293 native moves of 27 characters).
constexpr uint32_t IdentityCode(uint32_t n) { return (n << 16) | ((0x10000 - n) & 0xffff); }
// Converted animations are embedded under instances from here on; the memory-only import gives each its own entry.
constexpr uint32_t FirstAnimationInstance = 0x00010000;
constexpr uint64_t AnimationHandle(uint32_t instance) {
    return 0xef00000000000000ULL | (uint64_t(ConvertedCharacter & 0xff) << 40) | instance;
}
// Move fields T7 has no counterpart for (field:t7_to_t8_move): the most common value over 64,293 native moves (each >= 96.9%,
// most 100%). Every other field the converter does not write is 0 in native moves as well.
// Encrypted move field (0x20 bytes): {value, key}, then four u32 slots holding 0x765 + row, except that rows with
// row % 8 >= 4 keep a plain copy of the value in slot row % 8 - 4 (all 64,293 native moves; the game cross-checks
// the copy). Keys are random in native files; here they are a fixed function of row and field.
void SetEncodedField(std::vector<Byte>& out, uint64_t record, uint64_t row, uint32_t offset, uint32_t value) {
    uint64_t key = (row << 8 | offset) * 0x9e3779b97f4a7c15ULL + 0x2545f4914f6cdd1dULL;
    key ^= key >> 31; key *= 0xbf58476d1ce4e5b9ULL; key ^= key >> 29;
    const auto encoded = Encode(value, key);
    memcpy(out.data() + record + offset + AT(EncryptedValue, value), &encoded, 8); memcpy(out.data() + record + offset + AT(EncryptedValue, key), &key, 8);
    uint32_t slots[4]; for (auto& s : slots) s = static_cast<uint32_t>(0x765 + row);
    if (row % 8 >= 4) slots[row % 8 - 4] = value;
    memcpy(out.data() + record + offset + AT(EncryptedValue, check_slots), slots, sizeof(slots));
}
}

void EncodeRetailField(std::vector<Byte>& out, uint64_t record, uint64_t row, uint32_t offset, uint32_t value) {
    SetEncodedField(out, record, row, offset, value);
}

nlohmann::json ConversionReportJson(const ConversionReport& r) {
    nlohmann::json unresolved = nlohmann::json::array(), dropped = nlohmann::json::array();
    for (auto [id, n] : r.unresolvedRequirementIds) unresolved.push_back({{"id", id}, {"rows", n}});
    for (auto [id, n] : r.droppedPropertyIds) dropped.push_back({{"id", id}, {"rows", n}});
    return {{"moves", r.moves}, {"paired_moves", r.pairedMoves}, {"source_character", r.sourceCharacter},
        {"converted_character", r.convertedCharacter},
        {"animations", {{"by_move_name", r.animationsByName}, {"by_animation_name", r.animationsByMap}, {"converted_from_t7", r.animationsConverted},
            {"fallback_idle", r.animationsFallback}, {"carriers_available", r.carriersAvailable}, {"unconverted_no_carrier", r.animationsUnconverted},
            {"length_rejected", r.animationsLengthRejected}, {"synthetic_instances", r.animationsSynthetic},
            {"conversion_failed", r.animationsConversionFailed}, {"embedded_native", r.embeddedNativeAnimations}, {"custom_animation_bytes", r.customAnimationBytes}, {"template_bytes", r.animationTemplateBytes}, {"template_is_idle", r.idleTemplate}, {"converted_moves", r.convertedMoves}}},
        {"walk_cancels_retimed", r.walkCancelsRetimed}, {"packed_hitboxes", r.packedHitboxes},
        {"voiceclips", {{"converted", r.voiceclipsConverted}, {"dropped", r.voiceclipsDropped}}},
        {"requirements", {{"voted", r.requirementsVoted}, {"cross_character_votes", r.requirementsCrossVoted}, {"catalog", r.requirementsCatalog}, {"unresolved", r.requirementsUnresolved}, {"unresolved_assumed_true", r.requirementsAssumedTrue}, {"unresolved_properties_dropped", r.requirementPropertiesDropped},
            {"voted_ids", r.votedRequirementIds}, {"unresolved_ids", unresolved}}},
        {"properties", {{"voted", r.propertiesVoted}, {"cross_character_votes", r.propertiesCrossVoted}, {"catalog", r.propertiesCatalog}, {"dropped", r.propertiesDropped},
            {"voted_ids", r.votedPropertyIds}, {"dropped_ids", dropped}}},
        {"projectile_spawns", r.projectileSpawns}, {"projectile_spawns_dropped", r.projectilesDropped}, {"throw_cameras_dropped", r.throwsDropped}};
}

std::vector<Byte> ConvertStandalone(const TKMovesetHeader& header, const S::TKMovesetHeaderBlocks& blocks,
    std::span<const Byte> payload, ConversionReport* report, bool convertAnimations, const ImportProfile::Profile& profile) {
    const auto settings = ImportProfile::ConversionSettings(profile);
    // Self-contained T7 -> retail T8: every record is built from the T7 moveset plus the fixed retail knowledge below
    // (field defaults measured over 27 native exports, the encrypted-field layout, the alias catalog and pooled
    // cross-character votes, the built-in body skeleton). No other moveset is read, so the output is the same for
    // every import target; the importer only swaps the placeholder identity for the target's.
    if (header.gameId != GameId_T7) throw std::invalid_argument("T8 conversion requires T7 input");
    std::string reason;
    if (!T7ImportUtils::ValidateT7Semantics(blocks, payload.data(), payload.size(), &reason))
        throw std::runtime_error(reason);
    ConversionReport local; auto& rep = report ? *report : local; rep = {};
    const auto& rules = Rules();

    // ---- Views ---------------------------------------------------------
    T7View t7{payload, payload.subspan(blocks.movesetBlock, blocks.animationBlock - blocks.movesetBlock)};
    const auto info = Get<G::MovesetInfo>(payload, blocks.movesetInfoBlock);
    for (size_t i = 0; i < 19; ++i) { t7.count[i] = info.table.entries[i].listCount; t7.offset[i] = info.table.entries[i].listAddr; }
    t7.names = blocks.nameBlock;
    const auto moveCount = t7.count[12];
    if (!moveCount) throw std::runtime_error("T7 moveset has no moves");
    std::vector<G::Move> moves(moveCount);
    std::vector<std::string> moveNames(moveCount), animNames(moveCount);
    for (uint64_t row = 0; row < moveCount; ++row) {
        moves[row] = Get<G::Move>(t7.Row(12, row), 0);
        moveNames[row] = t7.Name(moves[row].name_addr);
        animNames[row] = t7.Name(moves[row].anim_name_addr);
    }

    // ---- ID resolution ---------------------------------------------------
    std::map<uint32_t, uint64_t> unresolvedRequirements, unresolvedProperties;
    // Static catalog rows (status "static": evidence-backed fixes from the research repo's
    // docs/t7/t7_t8_overrides.csv, e.g. T7 615 "Screw available" -> 724 "Tornado Available") win over the
    // pooled votes, which can be consistently wrong: T7 [615], default pairs with T8 [723 juggle], default.
    auto staticAlias = [](bool property, uint32_t id) -> std::optional<uint32_t> {
        const auto* alias = RetailT8::FindAlias(property, id);
        if (alias && alias->target != UINT32_MAX && std::strcmp(alias->confidence, "static") == 0) return alias->target;
        return {};
    };
    auto mapRequirement = [&](uint32_t id) -> std::optional<uint32_t> {
        // map:t7_t8_req first: the terminator 881 -> 1100, story checks 558/559 -> 667/668 (T8 Kazuya uses them exactly where T7
        // uses 558/559) and any override; "none" leaves the id unresolved.
        uint32_t fixedTarget;
        switch (rules.Map("t7_t8_req").Find(ConversionRules::ToT8, id, fixedTarget)) {
        case ConversionRules::IdMap::Result::Mapped: return fixedTarget;
        case ConversionRules::IdMap::Result::None: ++rep.requirementsUnresolved; ++unresolvedRequirements[id]; return {};
        default: break;
        }
        std::optional<uint32_t> found;
        if (auto fixed = staticAlias(false, id)) { ++rep.requirementsCatalog; found = fixed; }
        else if (auto cross = CrossVoteFor(false, id)) { ++rep.requirementsCrossVoted; found = cross; }
        else if (auto* alias = RetailT8::FindAlias(false, id); alias && alias->target != UINT32_MAX) { ++rep.requirementsCatalog; found = alias->target; }
        if (found && !(id >= 0x8000 && PlaysUnconvertedAsset(*found))) return found;
        ++rep.requirementsUnresolved; ++unresolvedRequirements[id]; return {};
    };
    auto mapProperty = [&](uint32_t id) -> std::optional<uint32_t> {
        if (id == 0) return 0u;
        std::optional<uint32_t> found;
        uint32_t fixedTarget;
        if (const auto result = rules.Map("t7_t8_prop").Find(ConversionRules::ToT8, id, fixedTarget); result != ConversionRules::IdMap::Result::NoRule) {
            if (result == ConversionRules::IdMap::Result::Mapped && !PlaysUnconvertedAsset(fixedTarget)) { ++rep.propertiesCatalog; return fixedTarget; }
            ++rep.propertiesDropped; ++unresolvedProperties[id]; return {};
        }
        if (auto fixed = staticAlias(true, id)) { ++rep.propertiesCatalog; found = fixed; }
        else if (auto cross = CrossVoteFor(true, id)) { ++rep.propertiesCrossVoted; found = cross; }
        else if (auto* alias = RetailT8::FindAlias(true, id); alias && alias->target != UINT32_MAX) { ++rep.propertiesCatalog; found = alias->target; }
        if (found && !PlaysUnconvertedAsset(*found)) return found;
        ++rep.propertiesDropped; ++unresolvedProperties[id]; return {};
    };

    // ---- Output layout ---------------------------------------------------
    std::array<std::vector<std::vector<Byte>>, 3> builtLists; // 9, 10, 11 rows (T8 layout, pointers as T7 indices + tags)
    std::array<std::map<uint64_t, uint64_t>, 3> listStart;      // T7 head row -> output row
    std::array<uint64_t, 3> listRows{};
    struct Pending { size_t table; uint64_t row; uint32_t field; size_t target; uint64_t index; };
    std::vector<Pending> pending; // references resolved after layout, index into target table
    constexpr uint64_t Guard = UINT64_MAX - 1;   // marker: unconditional requirement guard
    std::vector<std::array<uint32_t, 5>> requirementRows(t7.count[1]);
    // Requirement fallbacks (t7_t8_conversion_rules.ini): never-true identity check, T7's highest evaluator case.
    const std::array<uint32_t, 5> neverTrue{rules.Value("t7_to_t8_req_never_true"), rules.Value("t7_to_t8_req_never_true_param"), 0, 0, 0};
    const auto highestCase = rules.Value("t7_to_t8_req_highest_case");
    const bool assumeZeroTrue = rules.Value("t7_t8_req_unknown_zero_params_true") != 0;
    const auto [t7Projectile, t8Projectile] = rules.Pair("t7_t8_projectile_prop");
    const auto [t7Throw, t8Throw] = rules.Pair("t7_t8_throw_prop");
    for (uint64_t row = 0; row < t7.count[1]; ++row) {
        auto r = t7.Row(1, row);
        const auto id = Get<uint32_t>(r, 0), param = Get<uint32_t>(r, 4);
        std::array<uint32_t, 5> out{};
        bool drop = false;
        if (auto profiled = ImportProfile::RequirementOverride(profile, ImportProfile::Game::T7, id, {param, 0, 0, 0})) {
            // Decided by the moveset's import profile (story, modes, items, stage...): before the rules.
            out = *profiled ? std::array<uint32_t, 5>{} : neverTrue; ++rep.requirementsCatalog;
        } else if (auto holds = rules.RequirementHolds(ConversionRules::ToT8, id, param, {}, settings)) {
            // t7_to_t8_req condition: decided for normal play at conversion (e.g. story progress checks).
            out = *holds ? std::array<uint32_t, 5>{} : neverTrue; ++rep.requirementsCatalog;
        } else if (auto converted = ParamProperty(id, param, drop, settings)) {
            // Retail requirements hold four parameters (S8::Requirement::params[4]).
            out = {converted->id, converted->params[0], converted->params[1], converted->params[2], converted->params[3]}; ++rep.requirementsCatalog;
        } else if (auto mapped = drop ? std::optional<uint32_t>() : mapRequirement(id)) {
            out[0] = *mapped; out[1] = param;
            // Character identity checks (T7 217..224 -> T8 220..227) use T7 character IDs. The moveset's own
            // character becomes the placeholder identity, which the importer swaps for the target's.
            if (*mapped >= 220 && *mapped <= 227 && (param & 0xffff) < 0x81) {
                const auto t7Character = param & 0xffff;
                auto c = t7Character == header.characterId ? ConvertedCharacter : CharacterT7ToT8(t7Character);
                out[1] = (param & 0xffff0000U) | (c == UINT32_MAX ? 0xfeU : c);
            }
        } else if (id >= 0x8000) {
            // IDs from 0x8000 up are property calls placed in a requirement list (a side effect run when the list
            // is evaluated), not conditions. Without a retail equivalent the side effect is dropped and the list
            // keeps its real conditions. Making it never-true vetoed every list that used one, e.g. Akuma's
            // buffered specials: jab's frame-12 cancel to Ry_shoryu needs [49, 346 == 3, 33224(3), 33521(-2)],
            // and 33521 has no retail ID, so b1 -> DP, 1 -> DP and 2 -> qcf+2 never fired.
            out = {0, 0, 0, 0, 0}; ++rep.requirementPropertiesDropped;
        } else if (id > highestCase || rules.InList("t7_to_t8_req_known_false", id)) {
            // No case in T7's evaluator (check_condition_v2.c; highest case 875): the default returns false, so the
            // list never passed in T7. Keep it never-true instead of assuming a default state (seen: 418, 739, 901, 5000).
            out = neverTrue;
        } else if (assumeZeroTrue && !drop && param == 0 && !rules.InList("t7_to_t8_req_default_false", id)) {
            // Unknown "flag == 0" checks hold in normal play (default state).
            out = {0, 0, 0, 0, 0}; ++rep.requirementsAssumedTrue;
        } else {
            // Unknown predicate: never true (primary identity selector 0x88 is an
            // unimplemented slot in build 26155785, see AdaptCharacterRequirements).
            // Game mode (563), customization (560) and assist (636) checks stay false.
            out = neverTrue;
        }
        requirementRows[row] = out;
    }
    const uint64_t guardRow = t7.count[1]; // [1100]

    auto buildList = [&](size_t slot, uint64_t head) -> uint64_t {
        if (head == Null7) return Null7;
        if (auto found = listStart[slot].find(head); found != listStart[slot].end()) return found->second;
        const size_t table = 9 + slot;
        const auto start = listRows[slot];
        auto& rows = builtLists[slot];
        // Bounded like buildClips: rows with unmapped IDs are skipped before the output budget below, and lists
        // are cached by head only, so an unbounded walk costs O(moves x list length) on a crafted file.
        for (uint64_t row = head; row < t7.count[table] && row - head < ListLimit; ++row) {
            auto r = t7.Row(table, row);
            bool last = false;
            std::vector<Byte> out(R::Tables[table].stride, 0);
            uint64_t requirement = Guard;
            if (table == 9) {
                const auto frame = Get<uint32_t>(r, 0), id = Get<uint32_t>(r, 4); auto value = Get<uint32_t>(r, 8);
                last = id == 0; // Property id 0 ends a T7 list, whatever the frame (a frame-0 row with an id does not)
                if (!last && ImportProfile::StripsProperty(profile, ImportProfile::Game::T7, id)) { ++rep.propertiesDropped; continue; }
                std::optional<uint32_t> mapped;
                // Projectile spawn: retail 0x827B takes the projectile row (table converted row for row below);
                // the spawn bone/offsets 33292..33295 map to 0x827C..0x827F through the catalog. Bone indices
                // are the same in both games (retail 0x14195B770: 0 Trans, 9 R_Hand, 13 L_Hand).
                if (id == t7Projectile) { if (value < t7.count[3]) { mapped = t8Projectile; ++rep.projectileSpawns; } else ++rep.projectilesDropped; }
                else if (id == t7Throw) {
                    // Throw: the value indexes the throw table, which is converted from T7 row for row.
                    if (value < t7.count[18]) mapped = t8Throw; else ++rep.throwsDropped;
                }
                bool drop = false;
                const auto converted = last ? std::optional<ConvertedProperty>() : ParamProperty(id, value, drop, settings);
                if (converted) { mapped = converted->id; ++rep.propertiesCatalog; }
                else if (drop && id != t7Projectile && id != t7Throw) { ++rep.propertiesDropped; }
                else if (!mapped && id != t7Projectile && id != t7Throw) mapped = mapProperty(id);
                if (!last && !mapped) continue;
                // The retail end row is written as {frame 0, id 0} even when the T7 terminator kept a frame.
                Set<uint32_t>(out, AT(ExtraMoveProperty, frame), last ? 0 : frame); Set<uint32_t>(out, AT(ExtraMoveProperty, id), last ? 0 : *mapped);
                Set<uint32_t>(out, AT(ExtraMoveProperty, params), last ? 0 : value);
                if (converted) for (size_t i = 0; i < converted->params.size(); ++i) Set<uint32_t>(out, AT(ExtraMoveProperty, params) + i * 4, converted->params[i]);
            } else {
                const auto id = Get<uint32_t>(r, 8), value = Get<uint32_t>(r, 12);
                requirement = Get<uint64_t>(r, 0);
                last = id == 881;
                if (!last && ImportProfile::StripsProperty(profile, ImportProfile::Game::T7, id)) { ++rep.propertiesDropped; continue; }
                bool drop = false;
                const auto converted = last ? std::optional<ConvertedProperty>() : ParamProperty(id, value, drop, settings);
                std::optional<uint32_t> mapped = last ? std::optional<uint32_t>(1100u) : converted ? converted->id : drop ? std::nullopt : mapProperty(id);
                if (converted) ++rep.propertiesCatalog;
                else if (drop) ++rep.propertiesDropped;
                if (!mapped) continue;
                Set<uint32_t>(out, AT(OtherMoveProperty, id), *mapped); Set<uint32_t>(out, AT(OtherMoveProperty, params), last ? 0 : value);
                if (converted) for (size_t i = 0; i < converted->params.size(); ++i) Set<uint32_t>(out, AT(OtherMoveProperty, params) + i * 4, converted->params[i]);
                if (requirement == Null7) requirement = Guard;
            }
            const auto at = listRows[slot]++;
            pending.push_back({table, at, table == 9 ? AT(ExtraMoveProperty, requirements) : AT(OtherMoveProperty, requirements), 1, requirement});
            rows.push_back(std::move(out));
            if (last) { listStart[slot][head] = start; return start; }
            if (rows.size() > (64ULL << 20) / R::Tables[table].stride) throw std::length_error("Converted property lists exceed budget");
        }
        throw std::runtime_error("Unterminated T7 property list");
    };
    std::vector<std::array<uint64_t, 3>> moveLists(moveCount);
    for (uint64_t row = 0; row < moveCount; ++row)
        moveLists[row] = {buildList(0, moves[row].extra_move_property_addr), buildList(1, moves[row].move_start_extraprop_addr),
                          buildList(2, moves[row].move_end_extraprop_addr)};

    // Voiceclips: every move's T7 list converted (see Voiceclip for the ID rule).
    std::vector<std::array<uint32_t, 3>> clips;
    std::map<uint64_t, uint64_t> clipStart; // T7 head row -> output row
    auto buildClips = [&](uint64_t head) -> uint64_t {
        if (head == Null7 || head >= t7.count[13]) return Null7;
        if (auto found = clipStart.find(head); found != clipStart.end()) return found->second;
        std::vector<std::array<uint32_t, 3>> list;
        bool terminated = false;
        for (uint64_t row = head; row < t7.count[13] && row - head < ListLimit; ++row) {
            const auto value = Get<uint32_t>(t7.Row(13, row), 0);
            if (value == UINT32_MAX) { terminated = true; break; }
            list.push_back(Voiceclip(value)); ++rep.voiceclipsConverted;
        }
        if (!terminated || list.empty()) return clipStart[head] = Null7;
        const auto start = clips.size();
        clips.insert(clips.end(), list.begin(), list.end());
        clips.push_back(Voiceclip(UINT32_MAX));
        return clipStart[head] = start;
    };
    std::vector<uint64_t> moveClips(moveCount, Null7);
    for (uint64_t row = 0; row < moveCount; ++row) moveClips[row] = buildClips(moves[row].voicelip_addr);

    std::array<uint64_t, 20> counts{};
    for (size_t i = 0; i < 19; ++i) counts[i] = t7.count[i];
    counts[1] = t7.count[1] + 1;
    counts[9] = listRows[0]; counts[10] = listRows[1]; counts[11] = listRows[2];
    counts[13] = clips.size();
    counts[19] = 0;                      // dialogues (pre-fight interactions)

    auto align = [](size_t value) { return (value + 7) & ~size_t(7); };
    R::Header snapshot{{'T','8','R','S'}, 1, Base, sizeof(R::Header), align(sizeof(R::Header) + R::SourceHeaderSize), 0};
    std::vector<Byte> output(snapshot.directoryOffset + R::Tables.size() * sizeof(R::Entry));
    std::array<R::Entry, 20> entries{};
    for (size_t table = 0; table < 20; ++table) {
        const uint64_t start = align(output.size()), stride = R::Tables[table].stride;
        if (counts[table] > (512ULL << 20) / stride) throw std::length_error("Converted T8 tables exceed allocation budget");
        output.resize(start + counts[table] * stride);
        entries[table] = {counts[table] ? Base + start : 0, counts[table], start, static_cast<uint32_t>(stride), 0};
    }
    auto field = [&](size_t table, uint64_t row, size_t offset = 0) { return entries[table].offset + row * entries[table].stride + offset; };
    auto address = [&](size_t table, uint64_t row) { return entries[table].address + row * entries[table].stride; };
    auto ref = [&](size_t table, uint64_t row, uint32_t offset, size_t target, uint64_t index) {
        if (index == Null7) { Set<uint64_t>(output, field(table, row, offset), 0); return; }
        if (index == Guard) index = guardRow;
        if (index >= counts[target]) throw std::runtime_error(std::string("Converted reference outside ") + R::Tables[target].name);
        Set<uint64_t>(output, field(table, row, offset), address(target, index));
    };

    // Source header: constants shared by every native retail export (27 measured), this moveset's tables and
    // aliases, and the placeholder identity. Strings and the string block end are placeholders the importer
    // rebinds from the live root; runtime resource slots stay null as in every export.
    const auto source = snapshot.sourceHeaderOffset;
    using H = S8::MovesetHeader;
    Set<uint32_t>(output, source + offsetof(H, skip_animation_lookup), 0x00010000); // +0 skip-animation-lookup 0, +2 populated 1
    Set<uint32_t>(output, source + offsetof(H, compile_date), RetailHeaderCompileDate);
    memcpy(output.data() + source + offsetof(H, signature), "TEK", 4);
    for (size_t at = offsetof(H, strings); at < offsetof(H, orig_aliases); at += sizeof(gameAddr)) Set<uint64_t>(output, source + at, Base);
    Set<uint64_t>(output, source + offsetof(H, _0x170), Base);
    Set<uint32_t>(output, source + offsetof(H, previous_identity), IdentityCode(ConvertedCharacter - 1));
    Set<uint32_t>(output, source + offsetof(H, identity), IdentityCode(ConvertedCharacter));
    for (size_t table = 0; table < 20; ++table) {
        Set<uint64_t>(output, source + R::Tables[table].pointerOffset, entries[table].address);
        Set<uint64_t>(output, source + R::Tables[table].countOffset, counts[table]);
        Set<R::Entry>(output, snapshot.directoryOffset + table * sizeof(R::Entry), entries[table]);
    }
    for (size_t slot = 0; slot < std::size(H{}.orig_aliases); ++slot) {
        Set<uint16_t>(output, source + offsetof(H, orig_aliases) + slot * 2, info.orig_aliases[1]);
        Set<uint16_t>(output, source + offsetof(H, current_aliases) + slot * 2, info.current_aliases[1]);
    }
    for (uint32_t slot = 0; slot < 56; ++slot) {
        Set<uint16_t>(output, source + offsetof(H, orig_aliases) + AliasSlot(slot) * 2, info.orig_aliases[slot]);
        Set<uint16_t>(output, source + offsetof(H, current_aliases) + AliasSlot(slot) * 2, info.current_aliases[slot]);
    }
    // Retail holds 32 unknown aliases (0x120..0x15F); T7 has 36. Copy only what fits, so the identity words at
    // 0x160..0x167 written above survive.
    static_assert(sizeof(H{}.unknown_aliases) <= sizeof(info.unknown_aliases));
    memcpy(output.data() + source + offsetof(H, unknown_aliases), info.unknown_aliases, sizeof(H{}.unknown_aliases));

    // 0 reactions
    for (uint64_t row = 0; row < counts[0]; ++row) {
        auto r = t7.Row(0, row);
        memcpy(output.data() + field(0, row), r.data(), r.size());
        for (uint32_t i = 0; i < 7; ++i) ref(0, row, AT(Reactions, pushbacks) + i * uint32_t(sizeof(gameAddr)), 4, Get<uint64_t>(r, i * 8));
        // Retail reads reaction move ids only from 0x50..0x6A (picker 0x1418FE330); 0x4E is the downed rotation,
        // 0 in every retail file (RE project findings log 2026-11-14), so it is not a move id to remap.
        Set<uint16_t>(output, field(0, row, AT(Reactions, downed_rotation)), 0);
        for (uint32_t i = 0; i < 14; ++i) Set<uint16_t>(output, field(0, row, AT(Reactions, moveid) + i * 2), MoveId(Get<uint16_t>(r, 0x50 + i * 2)));
    }
    // 1 requirements
    for (uint64_t row = 0; row < t7.count[1]; ++row)
        for (size_t i = 0; i < 5; ++i) Set<uint32_t>(output, field(1, row, AT(Requirement, condition) + i * 4), requirementRows[row][i]);
    Set<uint32_t>(output, field(1, guardRow, AT(Requirement, condition)), 1100);
    // 2 hit conditions
    for (uint64_t row = 0; row < counts[2]; ++row) {
        auto r = t7.Row(2, row);
        memcpy(output.data() + field(2, row), r.data(), r.size());
        ref(2, row, AT(HitCondition, requirements), 1, Get<uint64_t>(r, 0)); ref(2, row, AT(HitCondition, reactions), 0, Get<uint64_t>(r, 16));
    }
    // 3 projectiles: T7 0xa8 -> retail 0xe0, measured on the rows Kazuya, Claudio, Armor King, Dragunov and the shared
    // items keep between the games. 0x00..0x34 unchanged (vfx 62 -> 2081, 66 -> 2083), hit level 0x38 -> 0x3c,
    // voiceclip 0x54 -> 0x68 {category, 0, id}, hit condition 0x60 -> 0x90, cancel 0x68 -> 0x98, 0x74 -> 0xa4
    // (0xa8 = 1), 0x78 -> 0xac (only rows of vfx 0/62 keep it), 0x80 -> 0xb8, 0x84 -> 0xbc, 0x94 -> 0xcc.
    for (uint64_t row = 0; row < counts[3]; ++row) {
        auto r = t7.Row(3, row);
        memcpy(output.data() + field(3, row), r.data(), 0x38);
        const auto vfx = Get<uint32_t>(r, 0);
        if (auto effect = rules.Map("t7_t8_vfx").Map(ConversionRules::ToT8, vfx)) Set<uint32_t>(output, field(3, row, AT(Projectile, vfx_id)), *effect);
        Set<uint32_t>(output, field(3, row, AT(Projectile, hit_level)), Get<uint32_t>(r, 0x38));
        if (const auto clip = Get<uint32_t>(r, 0x54); clip && clip != UINT32_MAX) {
            const auto c = Voiceclip(clip);
            for (size_t k = 0; k < 3; ++k) Set<uint32_t>(output, field(3, row, AT(Projectile, voiceclip) + k * 4), c[k]);
        }
        ref(3, row, AT(Projectile, hit_condition), 2, Get<uint64_t>(r, 0x60)); ref(3, row, AT(Projectile, cancel), 6, Get<uint64_t>(r, 0x68));
        Set<uint32_t>(output, field(3, row, AT(Projectile, _0xA4)), Get<uint32_t>(r, 0x74));
        Set<uint32_t>(output, field(3, row, AT(Projectile, _0xA8)), 1);
        if (vfx == 0 || vfx == 62) Set<uint32_t>(output, field(3, row, AT(Projectile, _0xAC)), Get<uint32_t>(r, 0x78));
        Set<uint32_t>(output, field(3, row, AT(Projectile, _0xB8)), Get<uint32_t>(r, 0x80));
        Set<uint32_t>(output, field(3, row, AT(Projectile, _0xBC)), Get<uint32_t>(r, 0x84));
        Set<uint32_t>(output, field(3, row, AT(Projectile, _0xCC)), Get<uint32_t>(r, 0x94));
    }
    // 4 pushbacks, 5 pushback extra, 8 cancel extra, 15 inputs, 16 parry, 17 cameras: same layout in both games
    for (size_t table : {size_t(4), size_t(5), size_t(8), size_t(15), size_t(16), size_t(17)})
        for (uint64_t row = 0; row < counts[table]; ++row) {
            auto r = t7.Row(table, row);
            memcpy(output.data() + field(table, row), r.data(), r.size());
        }
    for (uint64_t row = 0; row < counts[4]; ++row) ref(4, row, AT(Pushback, extra), 5, Get<uint64_t>(t7.Row(4, row), 8));
    // 6 cancels, 7 group cancels
    for (size_t table : {size_t(6), size_t(7)})
        for (uint64_t row = 0; row < counts[table]; ++row) {
            auto r = t7.Row(table, row);
            memcpy(output.data() + field(table, row), r.data(), r.size());
            const auto sourceCommand = Get<uint64_t>(r, 0), command = Command(sourceCommand);
            Set<uint64_t>(output, field(table, row, AT(Cancel, command)), command);
            // T7 walk-out cancels open on frame 1; every retail T8 counterpart opens on frame 5
            // (all 1,194 paired walk cancels of Kazuya, Jin, Paul, Law, King and Hwoarang).
            const auto t7Frame = rules.Value("t7_t8_walk_cancel_t7_frame"), t8Frame = rules.Value("t7_t8_walk_cancel_t8_frame");
            if (!(sourceCommand >> 32) && sourceCommand < 0x8000 && rules.Map("t7_t8_walk").Has(ConversionRules::ToT8, static_cast<uint32_t>(sourceCommand)) &&
                Get<uint32_t>(r, 24) == t7Frame && Get<uint32_t>(r, 32) == t7Frame) {
                Set<uint32_t>(output, field(table, row, AT(Cancel, window_start)), t8Frame); Set<uint32_t>(output, field(table, row, AT(Cancel, starting_frame)), t8Frame);
                ++rep.walkCancelsRetimed;
            }
            ref(table, row, AT(Cancel, requirements), 1, Get<uint64_t>(r, 8)); ref(table, row, AT(Cancel, extra), 8, Get<uint64_t>(r, 16));
            if (command != Command(0x800b)) Set<uint16_t>(output, field(table, row, AT(Cancel, move_id)), MoveId(Get<uint16_t>(r, 36)));
        }
    // 9/10/11 rebuilt property lists
    for (size_t slot = 0; slot < 3; ++slot)
        for (uint64_t row = 0; row < builtLists[slot].size(); ++row)
            memcpy(output.data() + field(9 + slot, row), builtLists[slot][row].data(), builtLists[slot][row].size());
    // 13 voiceclips
    for (size_t i = 0; i < clips.size(); ++i)
        for (size_t k = 0; k < 3; ++k) Set<uint32_t>(output, field(13, i, AT(Voiceclip, category) + k * 4), clips[i][k]);
    // 14 input sequences
    for (uint64_t row = 0; row < counts[14]; ++row) {
        auto r = t7.Row(14, row);
        memcpy(output.data() + field(14, row), r.data(), r.size());
        ref(14, row, AT(InputSequence, inputs), 15, Get<uint64_t>(r, 8));
    }
    // 18 throws: side, then the camera row
    for (uint64_t row = 0; row < counts[18]; ++row) {
        auto r = t7.Row(18, row);
        Set<uint64_t>(output, field(18, row, AT(ThrowCamera, side)), Get<uint64_t>(r, 0));
        ref(18, row, AT(ThrowCamera, camera), 17, Get<uint64_t>(r, 8));
    }
    for (const auto& p : pending) ref(p.table, p.row, p.field, p.target, p.index);

    // Animations: every distinct T7 body animation is converted onto the built-in skeleton and embedded under its
    // own instance. The memory-only import registers each one; no carrier or loaded resource is involved.
    const auto anims = payload.subspan(blocks.animationBlock, blocks.motaBlock - blocks.animationBlock);
    std::map<uint64_t, uint32_t> instanceFor;   // T7 animation offset -> instance
    std::vector<std::pair<uint32_t, std::vector<Byte>>> customAssets; // instance, PANM
    std::set<uint64_t> failed;
    uint64_t customBytes = 0;
    if (convertAnimations)
        for (uint64_t row = 0; row < moveCount; ++row) {
            const auto offset = static_cast<uint64_t>(moves[row].anim_addr);
            if (instanceFor.count(offset) || failed.count(offset)) continue;
            try {
                if (offset >= anims.size()) throw std::runtime_error("animation outside block");
                // Cheap checks before the decode, whose 0x64 cost grows with frames squared: the layout (one walk) must
                // hold the 23 body channels the conversion reads and no more frames than it can hold.
                const auto layout = TAnimUtils::ReadAnimationLayout(anims.subspan(offset));
                if (layout.bones < T7AnimToRetailT8::BodyChannels) throw std::invalid_argument("T7 animation has fewer than 23 body channels");
                if (layout.frames > T7AnimToRetailT8::MaxConvertibleFrames()) throw std::invalid_argument("T7 animation has more frames than its conversion can hold");
                // The attempt is charged to the budget before it runs, at the PANM size a conversion of this length
                // writes, and stays charged when the animation then fails: a failing animation costs what a converted
                // one does. Replaced by the real size on success.
                const uint64_t attempt = uint64_t(layout.frames) * ((32ULL << 20) / T7AnimToRetailT8::MaxConvertibleFrames());
                customBytes += attempt;
                if (customBytes > (1ULL << 30)) throw std::length_error("Converted T7 animations exceed the retail snapshot budget");
                auto bytes = T7AnimToRetailT8::Convert(RetailPanm::DecodeT7(anims.subspan(offset), T7AnimToRetailT8::MaxConvertibleFrames()));
                customBytes -= attempt;
                const auto instance = FirstAnimationInstance + static_cast<uint32_t>(customAssets.size());
                instanceFor[offset] = instance;
                // Total budget: a converted file must still fit a retail snapshot (1 GiB); fail early, not at Serialize.
                customBytes += bytes.size();
                if (customBytes > (1ULL << 30)) throw std::length_error("Converted T7 animations exceed the retail snapshot budget");
                customAssets.push_back({instance, std::move(bytes)});
            } catch (const std::length_error&) { throw; }
            catch (const std::bad_alloc&) { throw; } // out of memory is not a per-animation failure (no silent idle fallback)
            catch (const std::exception& error) {
                // A T7 animation that does not convert: its moves play the idle animation (counted in the report).
                DEBUG_ERR("T7 animation of move %llu (%s) does not convert: %s", static_cast<unsigned long long>(row),
                    moveNames[row].c_str(), error.what());
                failed.insert(offset); ++rep.animationsConversionFailed;
            }
        }
    const auto idleAnimation = info.current_aliases[1] < moveCount ? instanceFor.find(moves[info.current_aliases[1]].anim_addr) : instanceFor.end();

    // 12 moves: built from scratch (no template record).
    for (uint64_t row = 0; row < moveCount; ++row) {
        const auto& m = moves[row];
        const auto at = field(12, row);
        for (const auto& [offset, value] : Rules().t8MoveDefaults) Set<uint32_t>(output, at + offset, value);
        SetEncodedField(output, at, row, AT(Move, name_key), RetailNames::Hash(moveNames[row]));
        SetEncodedField(output, at, row, AT(Move, anim_key), RetailNames::Hash(animNames[row]));
        SetEncodedField(output, at, row, AT(Move, vuln), m.vuln);
        SetEncodedField(output, at, row, AT(Move, hit_level), m.hitlevel);
        SetEncodedField(output, at, row, AT(Move, character_id), IdentityCode(ConvertedCharacter - 1));
        SetEncodedField(output, at, row, AT(Move, move_key), RetailNames::Hash(moveNames[row] + "#t7"));
        Set<uint64_t>(output, at + AT(Move, name_addr), Base); Set<uint64_t>(output, at + AT(Move, anim_name_addr), Base);
        // Body handle: the move's converted animation; a move whose animation did not convert plays the idle's.
        auto animation = instanceFor.find(static_cast<uint64_t>(m.anim_addr));
        if (animation != instanceFor.end()) { ++rep.animationsConverted; rep.convertedMoves.push_back(moveNames[row]); }
        else { animation = idleAnimation; ++rep.animationsFallback; }
        Set<uint64_t>(output, at + AT(Move, anim_addr), animation != instanceFor.end() ? AnimationHandle(animation->second) : 1);
        ref(12, row, AT(Move, cancels), 6, m.cancel_addr);
        ref(12, row, AT(Move, cancels_2), 6, m._0x28_cancel_addr); Set<int32_t>(output, at + AT(Move, cancel_gate_2), m._0x30_int__0x28_related);
        ref(12, row, AT(Move, cancels_3), 6, m._0x38_cancel_addr); Set<int32_t>(output, at + AT(Move, cancel_gate_3), m._0x40_int__0x38_related);
        ref(12, row, AT(Move, cancels_4), 6, m._0x48_cancel_addr); Set<int32_t>(output, at + AT(Move, cancel_gate_4), m._0x50_int__0x48_related);
        Set<uint16_t>(output, at + AT(Move, transition), MoveId(m.transition)); Set<int16_t>(output, at + AT(Move, end_rotation), m._0x56_short);
        ref(12, row, AT(Move, hit_conditions), 2, m.hit_condition_addr);
        Set<uint32_t>(output, at + AT(Move, animation_length), m.anim_len); Set<uint32_t>(output, at + AT(Move, airborne_start), m.airborne_start);
        Set<uint32_t>(output, at + AT(Move, airborne_end), m.airborne_end); Set<uint32_t>(output, at + AT(Move, ground_fall), m.ground_fall);
        ref(12, row, AT(Move, voiceclip), 13, moveClips[row]);
        ref(12, row, AT(Move, extra_properties), 9, moveLists[row][0]); ref(12, row, AT(Move, start_properties), 10, moveLists[row][1]);
        ref(12, row, AT(Move, end_properties), 11, moveLists[row][2]);
        Set<int32_t>(output, at + AT(Move, _0x150), m._0x98_int); Set<uint32_t>(output, at + AT(Move, _0x154), 0);
        Set<uint32_t>(output, at + AT(Move, active_start), m.first_active_frame); Set<uint32_t>(output, at + AT(Move, active_end), m.last_active_frame);
        // Hitboxes. A T7 dword packs (bone, size) byte pairs; native T8 moves use the T7 halves verbatim when the
        // low half is a bare bone (`0x0f100010` -> 0x10, 0x0f10; ~75% of paired moves). A lone packed code
        // (`0x0C0D`) whiffed live, so other dwords keep bone-only codes.
        const uint32_t low = m.hitbox_location & 0xffffU, high = m.hitbox_location >> 16;
        const bool packed = low && !(low >> 8) && (high >> 8);
        const uint32_t bones[2] = {packed ? low : low & 0xffU, packed ? high : high & 0xffU};
        if (packed) ++rep.packedHitboxes;
        for (size_t slot = 0, used = 0; slot < 2; ++slot) {
            if (!bones[slot] || (slot == 1 && bones[1] == bones[0])) continue;
            const auto hitbox = at + AT(Move, hitboxes) + used * sizeof(S8::Hitbox);
            Set<uint32_t>(output, hitbox + AT(Hitbox, start), m.first_active_frame);
            Set<uint32_t>(output, hitbox + AT(Hitbox, last), m.last_active_frame);
            Set<uint32_t>(output, hitbox + AT(Hitbox, location), bones[slot]); ++used;
        }
        Set<int16_t>(output, at + AT(Move, collision), m._0xA8_short); Set<uint16_t>(output, at + AT(Move, distance), m.distance);
    }

    if (!customAssets.empty()) {
        // Version 2 T8AN directory: one entry per converted animation, keyed by its instance.
        snapshot.version = 2;
        output.resize(align(output.size()));
        const uint64_t directoryAt = output.size();
        R::AssetDirectory directory{};
        memcpy(directory.signature, "T8AN", 4);
        directory.count = static_cast<uint32_t>(customAssets.size());
        directory.entriesOffset = directoryAt + sizeof(R::AssetDirectory);
        directory.dataOffset = align(directory.entriesOffset + directory.count * sizeof(R::AssetEntry));
        output.resize(directory.dataOffset);
        Set<R::AssetDirectory>(output, directoryAt, directory);
        for (size_t i = 0; i < customAssets.size(); ++i) {
            output.resize(align(output.size()));
            const auto& asset = customAssets[i];
            R::AssetEntry entry{T7AnimToRetailT8::CustomAssetAddress, output.size(), asset.second.size(), asset.first, 0};
            Set<R::AssetEntry>(output, directory.entriesOffset + i * sizeof(R::AssetEntry), entry);
            output.insert(output.end(), asset.second.begin(), asset.second.end());
        }
        rep.customAnimationBytes = output.size() - directoryAt;
    }
    snapshot.size = output.size();
    Set<R::Header>(output, 0, snapshot);
    TKMovesetHeader outer = header;
    strcpy_s(outer.version_string, MOVESET_VERSION_STRING);
    strcpy_s(outer.orig_character_name, header.GetOrigCharacterName());
    outer.characterId = ConvertedCharacter;
    strcpy_s(outer.origin, "Tekken 8"); // the game the file now imports into
    outer.conversion_origin = GameVersions::T8::FROM_T7;
    // The import profile travels with the moveset; a fixed character moves to T8 numbering (no counterpart: target).
    auto outputProfile = profile;
    if (outputProfile.identity == ImportProfile::Profile::Identity::Fixed) {
        const auto t8 = CharacterT7ToT8(static_cast<uint32_t>(outputProfile.character));
        if (t8 == UINT32_MAX) outputProfile.identity = ImportProfile::Profile::Identity::AdoptTarget;
        else outputProfile.character = static_cast<int>(t8);
    }
    const auto properties = ImportProfile::ToProperties(outputProfile);
    // Names section: the T7 move and animation names whose hashes the records now hold.
    R::NameMap names;
    for (uint64_t row = 0; row < moveCount; ++row) {
        if (!moveNames[row].empty()) names.emplace(RetailNames::Hash(moveNames[row]), moveNames[row]);
        if (!animNames[row].empty()) names.emplace(RetailNames::Hash(animNames[row]), animNames[row]);
    }
    std::vector<R::OuterSection> sections;
    if (!names.empty()) sections.push_back(R::NamesSection(names));
    auto file = R::Serialize(output, outer, sections, &properties);
    auto& emitted = *reinterpret_cast<const TKMovesetHeader*>(file.data());
    if (!RetailT8Import::Prepare(emitted, {file.data() + emitted.moveset_data_start, file.size() - emitted.moveset_data_start}).Ok())
        throw std::runtime_error("Converted T8 reference graph is invalid");
    for (auto [id, n] : unresolvedRequirements) rep.unresolvedRequirementIds.push_back({id, n});
    for (auto [id, n] : unresolvedProperties) rep.droppedPropertyIds.push_back({id, n});
    rep.moves = moveCount; rep.convertedCharacter = ConvertedCharacter; rep.sourceCharacter = header.characterId;
    return file;
}
}
#undef AT

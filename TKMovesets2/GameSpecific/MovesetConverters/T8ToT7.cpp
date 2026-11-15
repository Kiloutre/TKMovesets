// Self-contained retail Tekken 8 -> Tekken 7 conversion.
//
// The inverse of T7ToRetailT8Standalone.cpp: every T7 record is built from the retail snapshot plus fixed tables
// shipped with the tool. Record layouts and field offsets are the forward converter's, checked on 54,151 moves that
// carry the same name in the T7 and retail exports of 31 characters (vuln, hit level, frames, hitboxes: 97-100%
// equal). Requirement/property IDs go through fixed rules, then the pooled T8 -> T7 votes measured on those pairs
// (T8ToT7Votes.inc), then the alias catalog read backwards. Body animations are converted from their PANM (raw,
// static and packed tracks) to T7 0x64. No other moveset is read. See docs/conversion.md.
#include "T8ToT7.hpp"
#include "ConversionRules.hpp"
#include "RetailT8Aliases.hpp"
#include "RetailT8AnimToT7.hpp"
#include "RetailPanm.hpp"
#include "Extractor_t8/RetailSnapshot.hpp"
#include "Extractor_t7/T7ExportSnapshot.hpp"
#include "Importer_t7/Importer_t7_Utils.hpp"
#include "Editor_t8Retail/RetailEditorData.hpp"
#include "GameStaticData.hpp"
#include "Structs_t7.h"
#include "GameIDs.hpp"
#include "constants.h"
#include "debug.hpp"
#include <algorithm>
#include <bit>
#include <functional>
#include <cstring>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <nlohmann/json.hpp>

namespace T8ToT7 {
namespace {
namespace R = RetailT8Snapshot;
namespace G = StructsT7_gameAddr;
constexpr uint64_t Null7 = UINT64_MAX;
constexpr uint64_t ListLimit = 4096;

template<class T> T Get(std::span<const Byte> bytes, uint64_t offset) {
    if (offset > bytes.size() || sizeof(T) > bytes.size() - offset) throw std::runtime_error("Truncated retail T8 field");
    T result; memcpy(&result, bytes.data() + offset, sizeof(T)); return result;
}
template<class T> void Set(std::vector<Byte>& bytes, uint64_t offset, T value) {
    if (offset > bytes.size() || sizeof(T) > bytes.size() - offset) throw std::runtime_error("Conversion write outside output");
    memcpy(bytes.data() + offset, &value, sizeof(T));
}
// Retail encrypted 32-bit move field (value qword, key qword), as T7ToRetailT8Standalone.cpp decodes it.
std::optional<uint32_t> Decode(uint64_t value, uint64_t key) {
    uint32_t check = 0, x = static_cast<uint32_t>(value);
    for (int shift = 0; shift < 32; shift += 8, x >>= 8) check ^= x ^ static_cast<uint32_t>(std::rotl(key, shift + 8));
    if (!check) check = 1;
    if (static_cast<uint32_t>(value >> 32) != check) return {};
    auto s = value ^ 0x1dULL;
    return static_cast<uint32_t>((std::rotl(key, static_cast<int>(s & 31)) & ~31ULL) ^ s);
}
// T7 character id identity code, the same as retail's: n << 16 | (0x10000 - n). T7 moves hold code(id - 1) + row in
// moveId_val1 (low half) / moveId_val2 (high half): every move of Jin (6) and Kazuya (9) in their T7 exports.
constexpr uint32_t IdentityCode(uint32_t n) { return (n << 16) | ((0x10000 - n) & 0xffff); }

// Rules from t7_t8_conversion_rules.ini (built-in defaults: Rules/t7_t8_conversion_rules.ini; docs/conversion-rules.md).
const ConversionRules::Rules& Rules() { return ConversionRules::Current(); }
// Retail alias slots (map:t7_t8_alias_slot): T8 inserted three slots at index 30 and one at 58.
uint32_t AliasSlot(uint32_t slot) { return Rules().Map("t7_t8_alias_slot").Map(ConversionRules::ToT8, slot).value_or(slot); }
std::optional<uint32_t> T7AliasSlot(uint32_t slot) { return Rules().Map("t7_t8_alias_slot").Map(ConversionRules::ToT7, slot); }

// Special cancel commands (map:t7_t8_cmd). Only what the paired cancel lists prove is mapped: 0x8005 -> T7 0x8003 (14/14),
// 0x8006 -> 0x8004 (12/12), group cancel 0x8012 -> 0x800b (1,707) and the input sequences from 0x8014 -> 0x800d.
// The forward rule's +2 does not hold the other way: retail 0x8007 is a T8 sidestep cancel (idle -> sSTEP_01L/R
// on 150/151) that pairs with nothing in T7; shifted to T7 0x8005 it fired every frame and the character kept
// sidestepping (live, T8 Kazuya on T7 Jin, 2026-11-14). A retail special command without a rule has no T7 command.
std::optional<uint64_t> Command(uint64_t command) {
    if ((command >> 32) || command < 0x8000) return command;
    return Rules().Map("t7_t8_cmd").Map(ConversionRules::ToT7, static_cast<uint32_t>(command));
}

// Retail properties that play a hand, face, wing or camera animation by id (list:t7_t8_drop_t8_props). The T7 importer keeps
// the target's own MOTAs, so those ids would play another character's assets: kept dropped.
bool PlaysUnconvertedAsset(uint32_t retail) { return Rules().InList("t7_t8_drop_t8_props", retail); }

struct Vote { bool property; uint32_t source, target, votes, total; };
constexpr Vote Votes[] = {
#include "T8ToT7Votes.inc"
};
std::optional<uint32_t> VoteFor(bool property, uint32_t id) {
    const auto found = std::lower_bound(std::begin(Votes), std::end(Votes), std::pair{property, id},
        [](const Vote& row, auto key) { return std::pair{row.property, row.source} < key; });
    if (found == std::end(Votes) || found->property != property || found->source != id) return {};
    return found->target;
}
// The alias catalog read backwards: retail id -> the T7 id with the best confidence (lowest id on ties).
int Rank(const char* confidence) {
    if (!strcmp(confidence, "static")) return 0;
    if (!strncmp(confidence, "handler", 7)) return 1;
    if (!strcmp(confidence, "strong")) return 2;
    if (!strcmp(confidence, "interp")) return 3;
    if (!strcmp(confidence, "weak")) return 4;
    return 5;
}
struct CatalogEntry { uint32_t t7; int rank; };
const std::map<std::pair<bool, uint32_t>, CatalogEntry>& Catalog() {
    static const auto table = [] {
        std::map<std::pair<bool, uint32_t>, CatalogEntry> result;
        for (const auto& alias : RetailT8::Aliases) {
            if (alias.target == UINT32_MAX) continue;
            const CatalogEntry entry{alias.source, Rank(alias.confidence)};
            auto [it, inserted] = result.emplace(std::pair{alias.property, alias.target}, entry);
            if (!inserted && (entry.rank < it->second.rank || (entry.rank == it->second.rank && entry.t7 < it->second.t7)))
                it->second = entry;
        }
        return result;
    }();
    return table;
}
const CatalogEntry* CatalogFor(bool property, uint32_t id) {
    const auto& table = Catalog();
    const auto found = table.find({property, id});
    return found == table.end() ? nullptr : &found->second;
}

// Properties whose parameter encoding differs (t8_to_t7_prop transforms, the inverse of the forward ones). Returns
// {t7 id, t7 value}; `drop` is set for a recognized retail id that has no T7 value.
struct T7Property { uint32_t id, value; };
std::optional<T7Property> ParamProperty(uint32_t id, const std::array<uint32_t, 5>& p, bool& drop,
    const std::function<uint16_t(uint16_t)>& moveId, const ConversionRules::Settings& settings) {
    drop = false;
    uint32_t target, value;
    switch (Rules().ToT7(id, p, target, value, settings)) {
    case ConversionRules::Rules::Applied::Mapped: return T7Property{target, value};
    case ConversionRules::Rules::Applied::Drop: drop = true; return {};
    default: break;
    }
    // Current-alias overwrite / reset blocks (map:t7_t8_prop_alias_block: retail 0x850D.. / 0x852E.. = T7 0x83C4.. / 0x83E5..).
    const auto size = Rules().Value("t7_t8_prop_alias_block_size");
    for (const auto& block : Rules().Map("t7_t8_prop_alias_block").lines) {
        if (!block.forward[ConversionRules::ToT7] || id < block.first[1] || id >= block.first[1] + size) continue;
        const auto slot = T7AliasSlot(id - block.first[1]);
        if (!slot || *slot >= size) { drop = true; return {}; }
        return T7Property{block.first[0] + *slot, (p[0] & 0xffff0000U) | moveId(static_cast<uint16_t>(p[0]))};
    }
    return {};
}

uint32_t CharacterT7ToT8(uint32_t id) { return Rules().Map("t7_t8_character").Map(ConversionRules::ToT8, id).value_or(UINT32_MAX); }

struct T8View {
    std::span<const Byte> payload;
    std::array<R::Entry, 20> entries{};
    uint64_t Count(size_t table) const { return entries[table].count; }
    std::span<const Byte> Row(size_t table, uint64_t row) const {
        if (row >= entries[table].count) throw std::runtime_error(std::string("Retail reference outside ") + R::Tables[table].name);
        return payload.subspan(entries[table].offset + row * entries[table].stride, entries[table].stride);
    }
    // Row of a captured pointer into `table`; nullopt for null or outside the table.
    std::optional<uint64_t> Index(size_t table, uint64_t pointer) const {
        const auto& e = entries[table];
        if (!pointer || !e.count || pointer < e.address || (pointer - e.address) % e.stride || (pointer - e.address) / e.stride >= e.count)
            return {};
        return (pointer - e.address) / e.stride;
    }
};

uint64_t Align(uint64_t value) { return (value + 7) & ~uint64_t(7); }
}

uint32_t CharacterT8ToT7(uint32_t id) { return Rules().Map("t7_t8_character").Map(ConversionRules::ToT7, id).value_or(UINT32_MAX); }

nlohmann::json ReportJson(const Report& r) {
    nlohmann::json unresolved = nlohmann::json::array(), dropped = nlohmann::json::array();
    for (auto [id, n] : r.unresolvedRequirementIds) unresolved.push_back({{"id", id}, {"rows", n}});
    for (auto [id, n] : r.droppedPropertyIds) dropped.push_back({{"id", id}, {"rows", n}});
    return {{"moves", r.moves}, {"source_character", r.sourceCharacter}, {"converted_character", r.convertedCharacter},
        {"placeholder_character", r.placeholderCharacter}, {"external_cancel_lists", r.externalCancels},
        {"dangling_references", r.danglingReferences}, {"special_cancels_disabled", r.specialCancelsDisabled},
        {"walk_cancels_retimed", r.walkCancelsRetimed},
        {"animations", {{"converted", r.animationsConverted}, {"missing", r.animationsMissing},
            {"conversion_failed", r.animationsConversionFailed}, {"fallback_moves", r.animationsFallback}, {"bytes", r.animationBytes}}},
        {"voiceclips", {{"converted", r.voiceclipsConverted}, {"dropped", r.voiceclipsDropped}}},
        {"rule_fixes", {{"changed", r.ruleFixesChanged}, {"dropped", r.ruleFixesDropped}, {"guarded_lists", r.ruleGuards}}},
        {"requirements", {{"fixed", r.requirementsFixed}, {"cross_character_votes", r.requirementsVoted}, {"catalog", r.requirementsCatalog},
            {"unresolved", r.requirementsUnresolved}, {"unresolved_assumed_true", r.requirementsAssumedTrue},
            {"unresolved_properties_dropped", r.requirementPropertiesDropped}, {"unresolved_ids", unresolved}}},
        {"properties", {{"fixed", r.propertiesFixed}, {"cross_character_votes", r.propertiesVoted}, {"catalog", r.propertiesCatalog},
            {"dropped", r.propertiesDropped}, {"dropped_ids", dropped}, {"extra_requirements_dropped", r.extraPropertyRequirementsDropped}}}};
}

std::vector<Byte> Convert(const TKMovesetHeader& header, std::span<const Byte> payload, Report* report, bool convertAnimations,
    const ImportProfile::Profile& profile) {
    const auto settings = ImportProfile::ConversionSettings(profile);
    if (header.gameId != GameId_T8 || header.minorVersion != GameVersions::T8::RETAIL)
        throw std::invalid_argument("T7 conversion requires a retail Tekken 8 moveset");
    R::Validate(payload);
    Report local; auto& rep = report ? *report : local; rep = {};
    const auto& rules = Rules();
    const auto neverTrue = rules.Value("t8_to_t7_req_never_true");
    const bool assumeZeroTrue = rules.Value("t7_t8_req_unknown_zero_params_true") != 0;

    // ---- Views ---------------------------------------------------------
    const auto snapshot = Get<R::Header>(payload, 0);
    T8View t8{payload};
    for (size_t i = 0; i < 20; ++i) t8.entries[i] = Get<R::Entry>(payload, snapshot.directoryOffset + i * sizeof(R::Entry));
    const auto source = payload.subspan(snapshot.sourceHeaderOffset, R::SourceHeaderSize);
    const auto moveCount = t8.Count(12);
    if (!moveCount) throw std::runtime_error("Retail moveset has no moves");
    const auto native = R::ReadExtension(payload);
    std::array<uint16_t, 60> origAliases{}, currentAliases{};
    for (size_t slot = 0; slot < 60; ++slot) {
        origAliases[slot] = Get<uint16_t>(source, 0x30 + slot * 2);
        currentAliases[slot] = Get<uint16_t>(source, 0xa8 + slot * 2);
    }
    // Own move ids: alias slots move back to T7's numbering; a retail-only slot becomes the row it currently names.
    auto moveId = [&](uint16_t id) -> uint16_t {
        if (id < 0x8000 || id >= 0x8000 + 60) return id;
        if (auto slot = T7AliasSlot(id - 0x8000); slot && *slot < 56) return static_cast<uint16_t>(0x8000 + *slot);
        return currentAliases[id - 0x8000];
    };

    // ---- Character identity ----------------------------------------------
    const uint32_t ownT8 = header.characterId;
    const auto counterpart = CharacterT8ToT7(ownT8);
    const uint32_t ownT7 = counterpart == UINT32_MAX ? rules.Value("t8_to_t7_placeholder_character") : counterpart;
    rep.sourceCharacter = ownT8; rep.convertedCharacter = ownT7; rep.placeholderCharacter = counterpart == UINT32_MAX;

    // ---- ID resolution ---------------------------------------------------
    std::map<uint32_t, uint64_t> unresolvedRequirements, droppedProperties;
    auto mapId = [&](bool property, uint32_t id, uint64_t& fixed, uint64_t& voted, uint64_t& catalog) -> std::optional<uint32_t> {
        if (const auto* entry = CatalogFor(property, id); entry && entry->rank == 0) { ++catalog; return entry->t7; }
        if (auto vote = VoteFor(property, id)) { ++voted; return vote; }
        if (const auto* entry = CatalogFor(property, id)) { ++catalog; return entry->t7; }
        (void)fixed; return {};
    };
    auto mapRequirement = [&](uint32_t id) -> std::optional<uint32_t> {
        // map:t7_t8_req first: the terminator 1100 -> 881, story checks 667/668 -> 558/559 and any override.
        uint32_t fixedTarget;
        switch (rules.Map("t7_t8_req").Find(ConversionRules::ToT7, id, fixedTarget)) {
        case ConversionRules::IdMap::Result::Mapped: ++rep.requirementsFixed; return fixedTarget;
        case ConversionRules::IdMap::Result::None: return {};
        default: break;
        }
        if (id >= 0x8000 && PlaysUnconvertedAsset(id)) return {};
        return mapId(false, id, rep.requirementsFixed, rep.requirementsVoted, rep.requirementsCatalog);
    };
    auto mapProperty = [&](uint32_t id) -> std::optional<uint32_t> {
        if (id == 0) return 0u;
        if (PlaysUnconvertedAsset(id)) return {};
        uint32_t fixedTarget; // map:t7_t8_prop overrides first
        switch (rules.Map("t7_t8_prop").Find(ConversionRules::ToT7, id, fixedTarget)) {
        case ConversionRules::IdMap::Result::Mapped: ++rep.propertiesFixed; return fixedTarget;
        case ConversionRules::IdMap::Result::None: return {};
        default: break;
        }
        return mapId(true, id, rep.propertiesFixed, rep.propertiesVoted, rep.propertiesCatalog);
    };
    auto params = [](std::span<const Byte> row, uint32_t at, size_t count) {
        std::array<uint32_t, 5> p{};
        for (size_t i = 0; i < count; ++i) p[i] = Get<uint32_t>(row, at + i * 4);
        return p;
    };

    // t8_to_t7_fix rules on the T7 records written below (ConversionRules::FixKinds), counted in the report.
    auto fix = [&](std::string_view kind, uint32_t id, uint32_t v, const std::array<uint32_t, 5>& p = {}) {
        const auto f = rules.Fix(kind, id, v, p, settings);
        if (f.drop) ++rep.ruleFixesDropped; else if (f.changed) ++rep.ruleFixesChanged;
        return f;
    };

    // ---- 1 requirements: row for row, then a never-true list for disabled cancels -----------------------
    std::vector<std::pair<uint32_t, uint32_t>> requirements(t8.Count(1));
    for (uint64_t row = 0; row < t8.Count(1); ++row) {
        const auto r = t8.Row(1, row);
        const auto id = Get<uint32_t>(r, 0);
        const auto p = params(r, 4, 4);
        bool drop = false;
        std::pair<uint32_t, uint32_t> out{0, 0};
        if (auto profiled = ImportProfile::RequirementOverride(profile, ImportProfile::Game::T8, id, {p[0], p[1], p[2], p[3]})) {
            // Decided by the moveset's import profile (story, modes, items, body, stage...): before the rules.
            out = *profiled ? std::pair<uint32_t, uint32_t>{0, 0} : std::pair<uint32_t, uint32_t>{neverTrue, 0}; ++rep.requirementsFixed;
        } else if (auto holds = rules.RequirementHolds(ConversionRules::ToT7, id, 0, p, settings)) {
            // t8_to_t7_req condition: decided for normal play at conversion (e.g. 777 Story Flag == X: versus runs with
            // the story finished, so only Jin's story-complete stance moves stay available).
            out = *holds ? std::pair<uint32_t, uint32_t>{0, 0} : std::pair<uint32_t, uint32_t>{neverTrue, 0}; ++rep.requirementsFixed;
        } else if (auto converted = ParamProperty(id, p, drop, moveId, settings)) { out = {converted->id, converted->value}; ++rep.propertiesFixed; }
        else if (auto mapped = drop ? std::optional<uint32_t>() : mapRequirement(id)) {
            out = {*mapped, p[0]};
            // Character identity checks (retail 220..227 = T7 217..224) use character ids: the moveset's own becomes
            // the converted identity (which the T7 importer swaps for the target's), others their T7 counterpart.
            if (id >= 220 && id <= 227 && (p[0] & 0xffff) < 0x81) {
                const auto character = p[0] & 0xffff;
                auto c = character == ownT8 ? ownT7 : CharacterT8ToT7(character);
                out.second = (p[0] & 0xffff0000U) | (c == UINT32_MAX ? 0xfeU : c);
            }
        } else if (id >= 0x8000) {
            // A property call placed in a requirement list (side effect, not a condition): dropped as a no-op.
            ++rep.requirementPropertiesDropped;
        } else if (assumeZeroTrue && !drop && p == std::array<uint32_t, 5>{}) {
            // Unknown "flag == 0" checks hold in normal play (default state). A row a transform dropped is not unknown:
            // it stays never true below, whatever its parameters.
            ++rep.requirementsAssumedTrue;
        } else {
            // Unknown predicate: never true. 901 has no case in T7's condition evaluator (highest 875), which then
            // returns false; native T7 movesets use it that way.
            out = {neverTrue, 0};
            ++rep.requirementsUnresolved; ++unresolvedRequirements[id];
        }
        if (out.first != 881) { // t8_to_t7_fix:req on the T7 row (drop: never true)
            const auto f = fix("req", out.first, out.second, p);
            if (f.drop) out = {neverTrue, 0}; else out.second = f.v;
        }
        requirements[row] = out;
    }
    const uint64_t neverRow = requirements.size();
    requirements.push_back({neverTrue, 0}); requirements.push_back({881, 0});

    // ---- 9/10/11 property lists, rebuilt (rows without a T7 id are left out) ----------------------------------
    std::array<std::vector<std::vector<Byte>>, 3> lists;    // T7 rows
    std::array<std::map<uint64_t, uint64_t>, 3> listStart;   // retail head row -> T7 row
    auto requirementRef = [&](uint64_t pointer) -> uint64_t {
        if (auto row = t8.Index(1, pointer)) return *row;
        if (pointer) ++rep.danglingReferences;
        return Null7;
    };
    auto buildList = [&](size_t slot, uint64_t pointer) -> uint64_t {
        const size_t table = 9 + slot;
        const auto head = t8.Index(table, pointer);
        if (!head) { if (pointer) ++rep.danglingReferences; return Null7; }
        if (auto found = listStart[slot].find(*head); found != listStart[slot].end()) return found->second;
        auto& rows = lists[slot];
        const auto start = rows.size();
        for (uint64_t row = *head; row < t8.Count(table) && row - *head < ListLimit; ++row) {
            const auto r = t8.Row(table, row);
            const bool extra = table == 9;
            const auto id = Get<uint32_t>(r, extra ? 16 : 8);
            const auto p = params(r, extra ? 20 : 12, 5);
            const bool last = extra ? id == 0 : id == 1100;
            if (!last && ImportProfile::StripsProperty(profile, ImportProfile::Game::T8, id)) { ++rep.propertiesDropped; continue; }
            std::vector<Byte> out(extra ? sizeof(StructsT7::ExtraMoveProperty) : sizeof(StructsT7::OtherMoveProperty), 0);
            std::optional<T7Property> mapped;
            bool drop = false;
            if (last) mapped = T7Property{extra ? 0u : 881u, 0};
            else if (extra && id == rules.Pair("t7_t8_projectile_prop")[1]) { // projectile spawn: the value is the projectile row
                if (p[0] < t8.Count(3)) { mapped = T7Property{rules.Pair("t7_t8_projectile_prop")[0], p[0]}; ++rep.propertiesFixed; } else drop = true;
            } else if (extra && id == rules.Pair("t7_t8_throw_prop")[1]) { // throw: the value is the throw row
                if (p[0] < t8.Count(18)) { mapped = T7Property{rules.Pair("t7_t8_throw_prop")[0], p[0]}; ++rep.propertiesFixed; } else drop = true;
            } else if (auto converted = ParamProperty(id, p, drop, moveId, settings)) { mapped = converted; ++rep.propertiesFixed; }
            else if (!drop) { if (auto target = mapProperty(id)) mapped = T7Property{*target, p[0]}; }
            if (mapped && !last) { // t8_to_t7_fix:prop on the T7 row (drop: left out)
                const auto f = fix("prop", mapped->id, mapped->value, p);
                if (f.drop) mapped.reset(); else mapped->value = f.v;
            }
            if (!mapped) { ++rep.propertiesDropped; ++droppedProperties[id]; continue; }
            if (extra) {
                Set<uint32_t>(out, 0, last ? 0 : Get<uint32_t>(r, 0));
                Set<uint32_t>(out, 4, mapped->id); Set<uint32_t>(out, 8, mapped->value);
                // T7 extra properties have no requirement list: a retail row gated by one (+8; a list that holds more
                // than its 1100 end) fires unconditionally in T7. Written anyway, counted in the report.
                if (!last) {
                    const auto gate = t8.Index(1, Get<uint64_t>(r, 8));
                    if (gate && Get<uint32_t>(t8.Row(1, *gate), 0) != 1100) ++rep.extraPropertyRequirementsDropped;
                }
            } else {
                Set<uint64_t>(out, 0, last ? Null7 : requirementRef(Get<uint64_t>(r, 0)));
                Set<uint32_t>(out, 8, mapped->id); Set<uint32_t>(out, 12, mapped->value);
            }
            rows.push_back(std::move(out));
            if (last) { listStart[slot][*head] = start; return start; }
            if (rows.size() > (64ULL << 20) / 0x10) throw std::length_error("Converted property lists exceed budget");
        }
        throw std::runtime_error("Unterminated retail property list");
    };

    // ---- 13 voiceclips, rebuilt: categories 0/1/6/7 keep their ids; retail category 2 has no T7 value ----------
    std::vector<uint32_t> clips;
    std::map<uint64_t, uint64_t> clipStart;
    auto buildClips = [&](uint64_t pointer) -> uint64_t {
        const auto head = t8.Index(13, pointer);
        if (!head) { if (pointer) ++rep.danglingReferences; return Null7; }
        if (auto found = clipStart.find(*head); found != clipStart.end()) return found->second;
        std::vector<uint32_t> list;
        bool terminated = false;
        for (uint64_t row = *head; row < t8.Count(13) && row - *head < ListLimit; ++row) {
            const auto r = t8.Row(13, row);
            const auto category = Get<uint32_t>(r, 0), id = Get<uint32_t>(r, 8);
            if (category == UINT32_MAX) { terminated = true; break; }
            if (category > 0xff || rules.InList("t8_to_t7_voice_drop", category)) { ++rep.voiceclipsDropped; continue; }
            const auto f = fix("voice", category, id & 0xffffffU); // t8_to_t7_fix:voice (drop: left out)
            if (f.drop) { ++rep.voiceclipsDropped; continue; }
            list.push_back((category << 24) | (f.v & 0xffffffU)); ++rep.voiceclipsConverted;
        }
        if (!terminated || list.empty()) return clipStart[*head] = Null7;
        const auto start = clips.size();
        clips.insert(clips.end(), list.begin(), list.end()); clips.push_back(UINT32_MAX);
        return clipStart[*head] = start;
    };

    // ---- 6/7 cancels: the table, then the external lists a v4 file carries ------------------------------------
    std::vector<std::vector<Byte>> cancels;
    std::map<uint64_t, uint64_t> externalStart; // captured pointer -> T7 row
    // t8_to_t7_guard: a requirement list with no condition (rows 0 and properties only) is always true in T7, a
    // property row being a side effect that passes. A guard rule puts a condition row in front of such a list (a copy,
    // the original rows stay for their other users). T8 Heihachi's idle cancels into itself through "set flag 0x1b
    // to 0" alone, ahead of every input cancel: in T7 that restarts the idle each frame.
    std::map<uint64_t, uint64_t> guardedStart; // T7 list head -> guarded copy
    auto guarded = [&](uint64_t head) -> uint64_t {
        if (head >= requirements.size()) return head;
        if (auto found = guardedStart.find(head); found != guardedStart.end()) return found->second;
        std::vector<std::pair<uint32_t, uint32_t>> rows, guards;
        for (auto row = head; row < requirements.size() && rows.size() < ListLimit; ++row) {
            const auto requirement = requirements[row];
            if (requirement.first == 881) break;
            if (requirement.first && requirement.first < 0x8000) return guardedStart[head] = head; // has a condition
            rows.push_back(requirement);
            if (auto guard = rules.Guard(requirement.first, requirement.second, settings)) guards.push_back(*guard);
        }
        if (guards.empty()) return guardedStart[head] = head;
        const auto start = requirements.size();
        requirements.insert(requirements.end(), guards.begin(), guards.end());
        requirements.insert(requirements.end(), rows.begin(), rows.end());
        requirements.push_back({881, 0});
        ++rep.ruleGuards;
        return guardedStart[head] = start;
    };
    auto convertCancel = [&](std::span<const Byte> r) {
        std::vector<Byte> out(r.begin(), r.begin() + sizeof(G::Cancel));
        const auto retail = Get<uint64_t>(r, 0);
        auto command = Command(retail);
        uint64_t requirement = requirementRef(Get<uint64_t>(r, 8));
        // A retail special command with no rule keeps its row behind the never-true requirement. Its raw value is kept
        // unless it lands on a T7 list command (0x800b group cancel, 0x800c group end, 0x800d.. input sequence), whose
        // move id / command is an index: retail 0x800b (in every retail file, next to 0x8007 / 0x800a) would point into
        // the group cancel table with a move id (Clive: 0x7f6 > 1572 groups). Those rows get command 0 instead.
        if (!command) {
            command = retail < 0x800b ? retail : 0; requirement = neverRow; ++rep.specialCancelsDisabled;
        }
        Set<uint64_t>(out, 0, *command);
        // Walk-out cancels: retail opens them on frame 5, T7 0x410/0x440 on frame 1 (18,507 of 18,507 pairs).
        const auto t7Frame = rules.Value("t7_t8_walk_cancel_t7_frame"), t8Frame = rules.Value("t7_t8_walk_cancel_t8_frame");
        const auto walk = retail >> 32 || retail >= 0x8000 ? std::nullopt : rules.Map("t7_t8_walk").Map(ConversionRules::ToT7, static_cast<uint32_t>(retail));
        if (walk && Get<uint32_t>(r, 24) == t8Frame && Get<uint32_t>(r, 32) == t8Frame) {
            Set<uint64_t>(out, 0, *walk); Set<uint32_t>(out, 24, t7Frame); Set<uint32_t>(out, 32, t7Frame);
            ++rep.walkCancelsRetimed;
        }
        Set<uint64_t>(out, 8, requirement);
        auto extra = t8.Index(8, Get<uint64_t>(r, 16));
        if (!extra) { ++rep.danglingReferences; extra = 0; }
        Set<uint64_t>(out, 16, *extra);
        if (Command(retail) != 0x800bu) Set<uint16_t>(out, 36, moveId(Get<uint16_t>(r, 36)));
        // t8_to_t7_fix:cancel on the T7 command (v = target move id; drop: never true).
        if (const auto t7Command = Get<uint64_t>(out, 0); !(t7Command >> 32)) {
            const auto f = fix("cancel", static_cast<uint32_t>(t7Command), Get<uint16_t>(out, 36));
            if (f.drop) Set<uint64_t>(out, 8, neverRow); else Set<uint16_t>(out, 36, static_cast<uint16_t>(f.v));
        }
        // Input cancels have their command as a condition: only command-0 cancels can be always true.
        if (!Get<uint64_t>(out, 0)) Set<uint64_t>(out, 8, guarded(Get<uint64_t>(out, 8)));
        return out;
    };
    for (uint64_t row = 0; row < t8.Count(6); ++row) cancels.push_back(convertCancel(t8.Row(6, row)));
    std::vector<std::vector<Byte>> groupCancels;
    for (uint64_t row = 0; row < t8.Count(7); ++row) groupCancels.push_back(convertCancel(t8.Row(7, row)));
    auto cancelRef = [&](uint32_t moveRow, uint32_t field, uint64_t pointer) -> uint64_t {
        if (!pointer) return Null7;
        if (auto row = t8.Index(6, pointer)) return *row;
        if (auto found = externalStart.find(pointer); found != externalStart.end()) return found->second;
        if (native) {
            if (auto list = native->cancels.find({moveRow, field}); list != native->cancels.end() && !list->second.empty()) {
                const auto start = cancels.size();
                for (size_t at = 0; at + 40 <= list->second.size(); at += 40)
                    cancels.push_back(convertCancel(std::span<const Byte>(list->second).subspan(at, 40)));
                ++rep.externalCancels;
                return externalStart[pointer] = start;
            }
        }
        ++rep.danglingReferences; return Null7;
    };

    // ---- Animations: every embedded body PANM, converted once -----------------------------------------------
    std::map<uint32_t, std::span<const Byte>> panms; // instance -> PANM
    for (const auto& asset : R::Assets(payload))
        if (asset.type == 0 && asset.size) panms.emplace(asset.instance, payload.subspan(asset.offset, asset.size));
    std::vector<Byte> animations;
    std::map<uint32_t, uint64_t> animationAt; // instance -> offset in the animation block
    std::set<uint32_t> failed;
    uint64_t attempted = 0; // channel bytes ToChannels was asked for, converted or not
    auto animationFor = [&](uint64_t row) -> std::optional<uint64_t> {
        const auto handle = Get<uint64_t>(t8.Row(12, row), 0x50);
        if (!(handle >> 32)) return {};
        const auto instance = static_cast<uint32_t>(handle);
        if (auto found = animationAt.find(instance); found != animationAt.end()) return found->second;
        if (failed.count(instance)) return {};
        const auto panm = panms.find(instance);
        if (panm == panms.end()) { ++rep.animationsMissing; failed.insert(instance); return {}; }
        try {
            const auto decoded = RetailPanm::Decode(panm->second);
            // ToChannels allocates and walks every frame (23 channels, 12 bytes each) before it can fail, and a PANM of
            // static tracks holds any frame count in a few bytes. The work is charged before it runs and stays charged
            // when the animation then fails, so failing animations cannot each cost it for free.
            constexpr uint64_t ChannelBytes = std::size(RetailT8AnimToT7::ChannelTypes) * 3 * sizeof(float);
            attempted += decoded.frames * ChannelBytes;
            if (attempted > (1ULL << 30)) throw std::length_error("Retail animations exceed the conversion budget");
            // More than 65535 frames only fit 0xC8 (the whole channel block): refused before the work when it cannot fit
            // the file budget checked below.
            if (decoded.frames > 0xFFFF && Align(animations.size()) + 8 + std::size(RetailT8AnimToT7::ChannelTypes) * 4 +
                decoded.frames * ChannelBytes > (192ULL << 20)) throw std::length_error("Converted animations exceed the T7 file budget");
            const auto channels = RetailT8AnimToT7::ToChannels(decoded);
            std::vector<Byte> bytes;
            try { bytes = RetailT8AnimToT7::Write64(channels); }
            catch (const std::invalid_argument&) { bytes = RetailT8AnimToT7::WriteC8(channels); }
            const auto at = Align(animations.size());
            if (at + bytes.size() > (192ULL << 20)) throw std::length_error("Converted animations exceed the T7 file budget");
            animations.resize(at); animations.insert(animations.end(), bytes.begin(), bytes.end());
            ++rep.animationsConverted;
            return animationAt[instance] = at;
        } catch (const std::length_error&) { throw; }
        catch (const std::bad_alloc&) { throw; }
        catch (const std::exception& error) {
            DEBUG_ERR("Retail animation %08X (move %llu) does not convert: %s", instance, static_cast<unsigned long long>(row), error.what());
            ++rep.animationsConversionFailed; failed.insert(instance); return {};
        }
    };
    std::vector<std::optional<uint64_t>> moveAnimation(moveCount);
    if (convertAnimations)
        for (uint64_t row = 0; row < moveCount; ++row) moveAnimation[row] = animationFor(row);
    // Moves without a converted animation play the idle's; without any, a one-frame rest pose.
    std::optional<uint64_t> fallback;
    if (const auto idle = currentAliases[1]; idle < moveCount) fallback = moveAnimation[idle];
    if (!fallback && !animationAt.empty()) fallback = animationAt.begin()->second;
    if (!fallback) {
        RetailPanm::T7Animation rest{1, 23, std::vector<float>(23 * 3, 0.0f), false};
        const auto bytes = RetailT8AnimToT7::WriteC8(rest);
        fallback = Align(animations.size());
        animations.resize(*fallback); animations.insert(animations.end(), bytes.begin(), bytes.end());
    }

    // ---- Names ---------------------------------------------------------
    std::vector<Byte> names;
    std::map<std::string, uint64_t> nameAt;
    auto text = [&](const std::string& value) -> uint64_t {
        if (auto found = nameAt.find(value); found != nameAt.end()) return found->second;
        const auto at = names.size();
        names.insert(names.end(), value.begin(), value.end()); names.push_back(0);
        return nameAt[value] = at;
    };
    auto hex = [](const char* prefix, uint32_t value) {
        char buffer[32]; snprintf(buffer, sizeof(buffer), "%s%08X", prefix, value); return std::string(buffer);
    };
    // Move and animation names are Kamui hashes in retail records. Resolve them as the retail editor does: the file's
    // custom names (editor metadata), its names section (T8NM), then the shipped t8-retail name table. Strings captured
    // from the live game (v4 extension) take precedence; unresolved hashes keep the move_/anim_ placeholder.
    std::map<uint32_t, std::string> customNames, fileNames;
    try {
        const auto metadata = RetailEditorData::Read(header, payload);
        customNames = metadata.names; fileNames = metadata.fileNames;
    } catch (const std::exception&) {}
    static const GameStaticData shippedNames = GameStaticData::LoadOptional("t8-retail");
    auto hashName = [&](std::optional<uint64_t> hash) -> std::optional<std::string> {
        if (!hash || *hash > UINT32_MAX) return std::nullopt;
        const auto key = static_cast<uint32_t>(*hash);
        if (auto found = customNames.find(key); found != customNames.end() && !found->second.empty()) return found->second;
        if (auto found = fileNames.find(key); found != fileNames.end() && !found->second.empty()) return found->second;
        if (auto* found = shippedNames.FindName(key); found && !found->empty()) return *found;
        return std::nullopt;
    };
    std::string characterName = header.GetOrigCharacterName();
    if (characterName.empty()) characterName = header.target_character;
    const auto characterAt = text(std::string(MOVESET_EXTRACTED_NAME_PREFIX) + characterName + "]");
    const auto creatorAt = text("TKMovesets2"), dateAt = text("Converted from Tekken 8");

    // ---- 12 moves ------------------------------------------------------
    std::vector<G::Move> moves(moveCount);
    for (uint64_t row = 0; row < moveCount; ++row) {
        const auto r = t8.Row(12, row);
        auto& m = moves[row];
        memset(&m, 0, sizeof(m));
        auto field = [&](uint32_t at) { return Decode(Get<uint64_t>(r, at), Get<uint64_t>(r, at + 8)); };
        std::optional<std::string> name, animationName;
        if (native && row < native->names.size()) name = native->names[row];
        if (native && row < native->animations.size()) animationName = native->animations[row];
        if (!name || name->empty()) name = hashName(field(0x00));
        if (!animationName || animationName->empty()) animationName = hashName(field(0x20));
        m.name_addr = text(name && !name->empty() ? *name : hex("move_", field(0x00).value_or(0)));
        m.anim_name_addr = text(animationName && !animationName->empty() ? *animationName : hex("anim_", field(0x20).value_or(0)));
        if (moveAnimation[row]) m.anim_addr = *moveAnimation[row];
        else { m.anim_addr = *fallback; ++rep.animationsFallback; }
        m.vuln = field(0x58).value_or(0);
        m.hitlevel = field(0x78).value_or(0);
        const auto row32 = static_cast<uint32_t>(row);
        m.cancel_addr = cancelRef(row32, 0x98, Get<uint64_t>(r, 0x98));
        m._0x30_int__0x28_related = Get<int32_t>(r, 0xa8);
        m._0x40_int__0x38_related = Get<int32_t>(r, 0xb8);
        m._0x50_int__0x48_related = Get<int32_t>(r, 0xc8);
        // Secondary cancel banks exist only when their gate is at least 1.
        m._0x28_cancel_addr = m._0x30_int__0x28_related >= 1 ? cancelRef(row32, 0xa0, Get<uint64_t>(r, 0xa0)) : Null7;
        m._0x38_cancel_addr = m._0x40_int__0x38_related >= 1 ? cancelRef(row32, 0xb0, Get<uint64_t>(r, 0xb0)) : Null7;
        m._0x48_cancel_addr = static_cast<int32_t>(m._0x50_int__0x48_related) >= 1 ? cancelRef(row32, 0xc0, Get<uint64_t>(r, 0xc0)) : Null7;
        m.transition = moveId(Get<uint16_t>(r, 0xcc));
        m._0x56_short = Get<int16_t>(r, 0xce);
        const auto identity = IdentityCode(ownT7 - 1) + row32;
        m.moveId_val1 = static_cast<uint16_t>(identity); m.moveId_val2 = static_cast<uint16_t>(identity >> 16);
        if (auto hit = t8.Index(2, Get<uint64_t>(r, 0x110))) m.hit_condition_addr = *hit;
        else { m.hit_condition_addr = Null7; if (Get<uint64_t>(r, 0x110)) ++rep.danglingReferences; }
        m.anim_len = Get<uint32_t>(r, 0x120); m.airborne_start = Get<uint32_t>(r, 0x124);
        m.airborne_end = Get<uint32_t>(r, 0x128); m.ground_fall = Get<uint32_t>(r, 0x12c);
        m.voicelip_addr = buildClips(Get<uint64_t>(r, 0x130));
        m.extra_move_property_addr = buildList(0, Get<uint64_t>(r, 0x138));
        m.move_start_extraprop_addr = buildList(1, Get<uint64_t>(r, 0x140));
        m.move_end_extraprop_addr = buildList(2, Get<uint64_t>(r, 0x148));
        m._0x98_int = Get<int32_t>(r, 0x150);
        // Hitboxes: T7 packs the first two retail hitbox bones (low, high), 98.3% of paired moves.
        m.hitbox_location = (Get<uint32_t>(r, 0x198) << 16) | (Get<uint32_t>(r, 0x168) & 0xffffU);
        m.first_active_frame = Get<uint32_t>(r, 0x158); m.last_active_frame = Get<uint32_t>(r, 0x15c);
        m._0xA8_short = Get<int16_t>(r, 0x2e0); m.distance = Get<uint16_t>(r, 0x2e2);
    }
    // t8_to_t7_fix:move on each u32 field a rule names (never a reference field: the rules parser refuses those).
    for (const auto offset : rules.FixIds("move")) {
        // Validate() bounds the offset; checked again here so a rules bug cannot write outside the move record.
        if (offset % 4 || offset > sizeof(G::Move) - 4) throw std::runtime_error("t8_to_t7_fix:move offset outside the T7 move");
        for (auto& m : moves) {
            uint32_t value; memcpy(&value, reinterpret_cast<Byte*>(&m) + offset, 4);
            value = fix("move", offset, value).v;
            memcpy(reinterpret_cast<Byte*>(&m) + offset, &value, 4);
        }
    }
    rep.moves = moveCount;

    // ---- T7 records ------------------------------------------------------
    std::array<uint64_t, 19> counts{};
    for (size_t i = 0; i < 19; ++i) counts[i] = t8.Count(i);
    counts[1] = requirements.size(); counts[6] = cancels.size(); counts[7] = groupCancels.size();
    counts[9] = lists[0].size(); counts[10] = lists[1].size(); counts[11] = lists[2].size(); counts[13] = clips.size();
    std::array<uint64_t, 19> offsets{};
    std::vector<Byte> records;
    for (size_t i = 0; i < 19; ++i) {
        offsets[i] = Align(records.size());
        if (counts[i] > (192ULL << 20) / T7Export::RecordSizes[i]) throw std::length_error("Converted T7 tables exceed the file budget");
        records.resize(offsets[i] + counts[i] * T7Export::RecordSizes[i]);
    }
    auto at = [&](size_t table, uint64_t row, size_t offset = 0) { return offsets[table] + row * T7Export::RecordSizes[table] + offset; };
    auto index = [&](size_t table, uint64_t pointer) -> uint64_t {
        if (auto row = t8.Index(table, pointer)) return *row;
        if (pointer) ++rep.danglingReferences;
        return Null7;
    };
    // 0 reactions: same layout; pushback pointers become rows, the 15 move ids get T7's alias numbering.
    for (uint64_t row = 0; row < counts[0]; ++row) {
        const auto r = t8.Row(0, row);
        memcpy(records.data() + at(0, row), r.data(), T7Export::RecordSizes[0]);
        for (uint32_t i = 0; i < 7; ++i) Set<uint64_t>(records, at(0, row, i * 8), index(4, Get<uint64_t>(r, i * 8)));
        for (uint32_t i = 0; i < 15; ++i) Set<uint16_t>(records, at(0, row, 0x4e + i * 2), moveId(Get<uint16_t>(r, 0x4e + i * 2)));
    }
    for (uint64_t row = 0; row < counts[1]; ++row) {
        Set<uint32_t>(records, at(1, row), requirements[row].first); Set<uint32_t>(records, at(1, row, 4), requirements[row].second);
    }
    for (uint64_t row = 0; row < counts[2]; ++row) {
        const auto r = t8.Row(2, row);
        memcpy(records.data() + at(2, row), r.data(), T7Export::RecordSizes[2]);
        Set<uint64_t>(records, at(2, row), index(1, Get<uint64_t>(r, 0))); Set<uint64_t>(records, at(2, row, 16), index(0, Get<uint64_t>(r, 16)));
    }
    // 3 projectiles: retail 0xe0 -> T7 0xa8 (inverse of the forward layout).
    for (uint64_t row = 0; row < counts[3]; ++row) {
        const auto r = t8.Row(3, row);
        memcpy(records.data() + at(3, row), r.data(), 0x38);
        const auto vfx = Get<uint32_t>(r, 0);
        if (auto effect = rules.Map("t7_t8_vfx").Map(ConversionRules::ToT7, vfx)) Set<uint32_t>(records, at(3, row), *effect);
        Set<uint32_t>(records, at(3, row, 0x38), Get<uint32_t>(r, 0x3c));
        const auto category = Get<uint32_t>(r, 0x68), clip = Get<uint32_t>(r, 0x70);
        if (category != UINT32_MAX && category <= 0xff && (category || clip) && !rules.InList("t8_to_t7_voice_drop", category))
            Set<uint32_t>(records, at(3, row, 0x54), (category << 24) | (clip & 0xffffffU));
        Set<uint64_t>(records, at(3, row, 0x60), index(2, Get<uint64_t>(r, 0x90)));
        const auto cancel = Get<uint64_t>(r, 0x98);
        Set<uint64_t>(records, at(3, row, 0x68), cancel ? index(6, cancel) : Null7);
        Set<uint32_t>(records, at(3, row, 0x74), Get<uint32_t>(r, 0xa4));
        Set<uint32_t>(records, at(3, row, 0x78), Get<uint32_t>(r, 0xac));
        Set<uint32_t>(records, at(3, row, 0x80), Get<uint32_t>(r, 0xb8));
        Set<uint32_t>(records, at(3, row, 0x84), Get<uint32_t>(r, 0xbc));
        Set<uint32_t>(records, at(3, row, 0x94), Get<uint32_t>(r, 0xcc));
        for (const auto offset : rules.FixIds("projectile")) { // t8_to_t7_fix:projectile (no reference fields)
            if (offset % 4 || offset > T7Export::RecordSizes[3] - 4) throw std::runtime_error("t8_to_t7_fix:projectile offset outside the T7 projectile");
            Set<uint32_t>(records, at(3, row, offset), fix("projectile", offset, Get<uint32_t>(records, at(3, row, offset))).v);
        }
    }
    // 4 pushbacks, 5 pushback extra, 8 cancel extra, 14 input sequences, 15 inputs, 16 parry, 17 cameras, 18 throws:
    // same layouts in both games.
    for (size_t table : {size_t(4), size_t(5), size_t(8), size_t(14), size_t(15), size_t(16), size_t(17), size_t(18)})
        for (uint64_t row = 0; row < counts[table]; ++row)
            memcpy(records.data() + at(table, row), t8.Row(table, row).data(), T7Export::RecordSizes[table]);
    for (uint64_t row = 0; row < counts[4]; ++row) Set<uint64_t>(records, at(4, row, 8), index(5, Get<uint64_t>(t8.Row(4, row), 8)));
    for (uint64_t row = 0; row < counts[14]; ++row) {
        const auto r = t8.Row(14, row);
        Set<uint64_t>(records, at(14, row, 8), Get<uint16_t>(r, 2) ? index(15, Get<uint64_t>(r, 8)) : Null7);
    }
    for (uint64_t row = 0; row < counts[18]; ++row) Set<uint64_t>(records, at(18, row, 8), index(17, Get<uint64_t>(t8.Row(18, row), 8)));
    // 6/7 cancels, with the option bits T7's loader computes (T7DiskMoveset.cpp; relocate 0x140715250).
    constexpr uint32_t NoRoute[] = {0x22e, 0x22f, 0x27a, 0x32b, 0x27c};
    for (size_t table : {size_t(6), size_t(7)}) {
        const auto& rows = table == 6 ? cancels : groupCancels;
        for (uint64_t row = 0; row < rows.size(); ++row) {
            auto c = rows[row];
            std::vector<uint32_t> ids;
            for (auto r = Get<uint64_t>(c, 8); r < requirements.size() && ids.size() < ListLimit; ++r) {
                if (requirements[r].first == 881) break;
                ids.push_back(requirements[r].first);
            }
            auto has = [&](uint32_t id) { return std::find(ids.begin(), ids.end(), id) != ids.end(); };
            const bool command0 = Get<uint64_t>(c, 0) == 0;
            uint16_t option = command0 ? 1 : 0;
            if (std::any_of(ids.begin(), ids.end(), [](uint32_t id) { return id >= 0x32c && !(id & 0x8000); })) option |= 2;
            if (std::any_of(ids.begin(), ids.end(), [](uint32_t id) { return id >= 0x349 && !(id & 0x8000); })) option |= 4;
            if (has(0x84ce)) option |= 8;
            if ((!command0 || has(0x83) || has(0x84) || has(0x85)) && std::none_of(std::begin(NoRoute), std::end(NoRoute), has)) option |= 0x10;
            const auto extraRow = Get<uint64_t>(c, 16);
            const auto extra = extraRow < counts[8] ? Get<uint32_t>(t8.Row(8, extraRow), 0) : 0;
            if ((extra & 0x3c00) != 0x2800) option |= 0x40;
            if ((extra & 0x3f) == 0x16) option |= 0x80;
            const auto move = Get<uint16_t>(c, 36);
            if (table == 6 && (move & 0x8000) && (move == 0x8024 || move <= 0x801d || move >= 0x8028)) option |= 0x100;
            Set<uint16_t>(c, 38, option);
            memcpy(records.data() + at(table, row), c.data(), c.size());
        }
    }
    for (size_t slot = 0; slot < 3; ++slot)
        for (uint64_t row = 0; row < lists[slot].size(); ++row)
            memcpy(records.data() + at(9 + slot, row), lists[slot][row].data(), lists[slot][row].size());
    for (uint64_t row = 0; row < moveCount; ++row) memcpy(records.data() + at(12, row), &moves[row], sizeof(G::Move));
    for (uint64_t row = 0; row < clips.size(); ++row) Set<uint32_t>(records, at(13, row), clips[row]);

    // ---- Payload (the T7 memory exporter's layout) -------------------------------------------------------------
    T7Export::Snapshot result;
    auto& blocks = result.blocks;
    blocks.movesetInfoBlock = 0;
    blocks.tableBlock = offsetof(G::MovesetInfo, table); blocks.motalistsBlock = offsetof(G::MovesetInfo, motas);
    blocks.nameBlock = sizeof(G::MovesetInfo);
    blocks.movesetBlock = Align(blocks.nameBlock + names.size());
    blocks.animationBlock = Align(blocks.movesetBlock + records.size());
    blocks.motaBlock = Align(blocks.animationBlock + animations.size());
    blocks.movelistBlock = blocks.motaBlock; // no MOTA: the importer keeps the target's
    result.payload.resize(blocks.movelistBlock + 8);
    G::MovesetInfo info{};
    info._0x0[0] = 0; info._0x0[1] = 0; info.isInitialized = true;
    memcpy(info._0x3, "\0HID\0", 5);
    info.character_name_addr = reinterpret_cast<char*>(blocks.nameBlock + characterAt);
    info.character_creator_addr = reinterpret_cast<char*>(blocks.nameBlock + creatorAt);
    info.date_addr = reinterpret_cast<char*>(blocks.nameBlock + dateAt);
    info.fulldate_addr = reinterpret_cast<char*>(blocks.nameBlock + dateAt);
    for (uint32_t slot = 0; slot < 56; ++slot) {
        info.orig_aliases[slot] = origAliases[AliasSlot(slot)];
        info.current_aliases[slot] = currentAliases[AliasSlot(slot)];
    }
    // Retail holds 32 unknown aliases (0x120..0x15F; 0x160.. are the identity words), T7 36: the inverse of the forward
    // copy. T7 slots 32..35 have no retail counterpart and stay 0.
    constexpr size_t RetailUnknownAliases = 0x160 - 0x120;
    static_assert(RetailUnknownAliases <= sizeof(info.unknown_aliases));
    memcpy(info.unknown_aliases, source.data() + 0x120, RetailUnknownAliases);
    for (size_t i = 0; i < 19; ++i) { info.table.entries[i].listAddr = offsets[i]; info.table.entries[i].listCount = counts[i]; }
    memset(&info.motas, 0xff, sizeof(info.motas));
    memcpy(result.payload.data(), &info, sizeof(info));
    auto copy = [&](uint64_t offset, const std::vector<Byte>& bytes) { if (!bytes.empty()) memcpy(result.payload.data() + offset, bytes.data(), bytes.size()); };
    copy(blocks.nameBlock, names); copy(blocks.movesetBlock, records); copy(blocks.animationBlock, animations);
    rep.animationBytes = animations.size();
    std::string error;
    if (!T7ImportUtils::ValidateT7Semantics(blocks, result.payload.data(), result.payload.size(), &error))
        throw std::runtime_error("Converted T7 moveset is invalid: " + error);

    TKMovesetHeader output;
    output.flags = 0; output.game_specific_flags = 0;
    output.gameId = GameId_T7; output.minorVersion = GameVersions::T7::NONE;
    output.conversion_origin = GameVersions::T7::FROM_T8;
    output.characterId = ownT7;
    memset(output.gameVersionHash, 0, sizeof(output.gameVersionHash));
    strcpy_s(output.version_string, sizeof(output.version_string), MOVESET_VERSION_STRING);
    strcpy_s(output.origin, "Tekken 7"); // the game the file now imports into; conversion_origin keeps the source
    strncpy_s(output.target_character, sizeof(output.target_character), header.target_character, _TRUNCATE);
    strncpy_s(output.orig_character_name, sizeof(output.orig_character_name), characterName.c_str(), _TRUNCATE);
    output.date = header.date; output.extraction_date = header.extraction_date;
    for (auto [id, n] : unresolvedRequirements) rep.unresolvedRequirementIds.push_back({id, n});
    for (auto [id, n] : droppedProperties) rep.droppedPropertyIds.push_back({id, n});
    // The import profile travels with the moveset; a fixed character moves to T7 numbering (no counterpart: target).
    auto outputProfile = profile;
    if (outputProfile.identity == ImportProfile::Profile::Identity::Fixed) {
        const auto t7 = CharacterT8ToT7(static_cast<uint32_t>(outputProfile.character));
        if (t7 == UINT32_MAX) outputProfile.identity = ImportProfile::Profile::Identity::AdoptTarget;
        else outputProfile.character = static_cast<int>(t7);
    }
    const auto properties = ImportProfile::ToProperties(outputProfile);
    auto file = T7Export::Serialize(result, output, &properties);
    // The header is copied as a struct: zero its padding so one input always gives the same bytes.
    using H = TKMovesetHeader;
    const std::pair<size_t, size_t> fields[] = {
#define FIELD(name) {offsetof(H, name), sizeof(H::name)}
        FIELD(_signature), FIELD(version_string), FIELD(gameVersionHash), FIELD(gameId), FIELD(minorVersion), FIELD(header_size),
        FIELD(block_list), FIELD(block_list_size), FIELD(moveset_data_start), FIELD(moveset_data_size), FIELD(compressionType),
        FIELD(crc32), FIELD(orig_crc32), FIELD(date), FIELD(extraction_date), FIELD(characterId), FIELD(flags), FIELD(origin),
        FIELD(target_character), FIELD(orig_character_name), FIELD(game_specific_flags), FIELD(conversion_origin)
#undef FIELD
    };
    std::vector<bool> used(sizeof(H));
    for (auto [at, size] : fields) for (size_t i = 0; i < size; ++i) used[at + i] = true;
    for (size_t i = 0; i < sizeof(H); ++i) if (!used[i]) file[i] = 0;
    return file;
}
}

#include "ImportProfile.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace ImportProfile {
namespace {
constexpr size_t PropertyLimit = 4096;
bool Ours(int id) { return id >= IdVersion && id <= IdGlobalWrites; }
uint64_t Align8(uint64_t value) { return (value + 7) & ~uint64_t(7); }
}

std::vector<TKMovesetProperty> ToProperties(const Profile& p) {
    return {{IdVersion, Version}, {IdIdentity, static_cast<int>(p.identity)}, {IdCharacter, p.character},
            {IdGender, static_cast<int>(p.gender)}, {IdStoryFlag, p.storyFlag}, {IdInHeat, p.inHeat ? 1 : 0},
            {IdFlagsClear, p.flagsClear ? 1 : 0}, {IdStoryMode, static_cast<int>(p.storyMode)},
            {IdDlcStory, static_cast<int>(p.dlcStory)}, {IdWaveIndex, p.waveIndex}, {IdBattleScript, p.battleScript},
            {IdSpecialModes, static_cast<int>(p.specialModes)}, {IdItem, static_cast<int>(p.item)},
            {IdBodyType, static_cast<int>(p.bodyType)}, {IdStage, static_cast<int>(p.stage)},
            {IdStoryEffects, static_cast<int>(p.storyEffects)}, {IdGlobalWrites, static_cast<int>(p.globalWrites)}};
}

Profile FromProperties(std::span<const TKMovesetProperty> properties) {
    Profile p;
    for (const auto& [id, value] : properties) {
        switch (id) {
        case IdIdentity: if (value == 0 || value == 1) p.identity = static_cast<Profile::Identity>(value); break;
        case IdCharacter: if (value >= 0 && value <= 0xff) p.character = value; break;
        case IdGender: if (value >= 0 && value <= 2) p.gender = static_cast<Profile::Gender>(value); break;
        case IdStoryFlag: if (value >= StoryFlagGameDecides && value <= 0xffff) p.storyFlag = value; break;
        case IdInHeat: p.inHeat = value != 0; break;
        case IdFlagsClear: p.flagsClear = value != 0; break;
        case IdStoryMode: if (value == 0 || value == 1) p.storyMode = static_cast<Profile::Toggle>(value); break;
        case IdDlcStory: if (value == 0 || value == 1) p.dlcStory = static_cast<Profile::Toggle>(value); break;
        case IdWaveIndex: if (value >= ValueGameDecides && value <= 0xffff) p.waveIndex = value; break;
        case IdBattleScript: if (value >= ValueGameDecides) p.battleScript = value; break;
        case IdSpecialModes: if (value == 0 || value == 1) p.specialModes = static_cast<Profile::Toggle>(value); break;
        case IdItem: if (value >= 0 && value <= 2) p.item = static_cast<Profile::Item>(value); break;
        case IdBodyType: if (value >= 0 && value <= 3) p.bodyType = static_cast<Profile::Body>(value); break;
        case IdStage: if (value == 0 || value == 1) p.stage = static_cast<Profile::Toggle>(value); break;
        case IdStoryEffects: if (value == 0 || value == 1) p.storyEffects = static_cast<Profile::Strip>(value); break;
        case IdGlobalWrites: if (value == 0 || value == 1) p.globalWrites = static_cast<Profile::Strip>(value); break;
        default: break; // other properties and newer options
        }
    }
    return p;
}

std::vector<TKMovesetProperty> ReadProperties(std::span<const Byte> file) {
    if (file.size() < sizeof(TKMovesetHeader)) return {};
    TKMovesetHeader header; memcpy(&header, file.data(), sizeof(header));
    if (strncmp(header._signature, "TKM2", 4) || header.block_list > file.size()) return {};
    // The list starts at the (8-aligned) header size and ends at END or at the block list.
    std::vector<TKMovesetProperty> result;
    for (uint64_t at = Align8(header.header_size); at + sizeof(TKMovesetProperty) <= header.block_list && result.size() < PropertyLimit;
         at += sizeof(TKMovesetProperty)) {
        TKMovesetProperty property; memcpy(&property, file.data() + at, sizeof(property));
        if (property.id == TKMovesetProperty_END) break;
        result.push_back(property);
    }
    return result;
}

std::vector<Byte> WithProfile(std::span<const Byte> file, const Profile& profile) {
    if (file.size() < sizeof(TKMovesetHeader)) throw std::runtime_error("Not a moveset file");
    TKMovesetHeader header; memcpy(&header, file.data(), sizeof(header));
    if (strncmp(header._signature, "TKM2", 4) || !header.ValidateHeader(file.size())) throw std::runtime_error("Invalid moveset header");
    const uint64_t listAt = Align8(header.header_size);
    const uint64_t blockBytes = uint64_t(header.block_list_size) * sizeof(uint64_t);
    if (listAt > header.block_list || header.block_list + blockBytes > header.moveset_data_start)
        throw std::runtime_error("Unexpected moveset header layout");
    auto properties = ReadProperties(file);
    std::erase_if(properties, [](const TKMovesetProperty& p) { return Ours(p.id); });
    for (const auto& p : ToProperties(profile)) properties.push_back(p);
    properties.push_back({TKMovesetProperty_END, 0});
    const uint64_t listBytes = Align8(properties.size() * sizeof(TKMovesetProperty));
    // Whatever lay between the block list and the payload start (padding) is kept with the block list.
    const uint64_t tail = header.moveset_data_start - header.block_list;
    std::vector<Byte> out(listAt + listBytes + tail + (file.size() - header.moveset_data_start), 0);
    memcpy(out.data(), file.data(), listAt);
    memcpy(out.data() + listAt, properties.data(), properties.size() * sizeof(TKMovesetProperty));
    memcpy(out.data() + listAt + listBytes, file.data() + header.block_list, tail + (file.size() - header.moveset_data_start));
    header.block_list = static_cast<uint32_t>(listAt + listBytes);
    header.moveset_data_start = static_cast<uint32_t>(header.block_list + tail);
    // A header shorter than the struct (header_size 208..248) is followed by the property list
    // written above: only the bytes that belong to the header may be replaced.
    memcpy(out.data(), &header, static_cast<size_t>((std::min<uint64_t>)(sizeof(header), listAt)));
    return out;
}

namespace {
bool In(uint32_t id, std::initializer_list<uint32_t> ids) { return std::find(ids.begin(), ids.end(), id) != ids.end(); }
// A "value >= X" / "<= X" / "== X" check against a known value (X = the first parameter).
std::optional<bool> Compare(uint32_t id, uint32_t ge, uint32_t le, uint32_t eq, uint32_t x, uint32_t value) {
    if (id == ge) return value >= x;
    if (id == le) return value <= x;
    if (id == eq) return value == x;
    return std::nullopt;
}
}

// One table for every import route and both conversions (in the moveset's own game numbering). Ids and meanings come
// from the requirement label review (docs/import-profile.md); "Versus" values are what a standard versus match holds.
std::optional<bool> RequirementOverride(const Profile& p, Game game, uint32_t id, const std::array<uint32_t, 4>& params) {
    using T = Profile::Toggle;
    const uint32_t x = params[0];
    if (game == Game::T7) {
        if (p.gender != Profile::Gender::FromTarget && (id == 583 || id == 584)) // Character is Male / Female
            return (id == 583) == (p.gender == Profile::Gender::Male);
        if (p.storyMode == T::Versus && In(id, {558, 559, 636, 811})) return false; // story battle (number), story assist
        if (p.specialModes == T::Versus && In(id, {556, 560, 634, 765})) return false; // arcade, customization, treasure, arcade cutscene
        if (p.item == Profile::Item::None && In(id, {609, 643})) return false; // has item, Kunimitsu mask
        if (p.item == Profile::Item::All && In(id, {609, 643})) return true;
        if (p.stage == T::Versus && id == 573) return false; // stage ID
        if (p.stage == T::Versus && id == 574) return true;  // stage NOT ID
        return std::nullopt;
    }
    if (p.gender != Profile::Gender::FromTarget && (id == 689 || id == 690)) // player is a male / female character
        return (id == 689) == (p.gender == Profile::Gender::Male);
    if (p.storyFlag != StoryFlagGameDecides && id == 777) return x == static_cast<uint32_t>(p.storyFlag); // story flag == X
    if (p.storyMode == T::Versus) {
        if (In(id, {667, 668, 786, 789, 806, 1014})) return false; // story mode, story battle number, story flags
        if (In(id, {778, 796})) return true;                       // not story mode, not in story battle X
        if (auto r = Compare(id, 484, 485, 486, x, 0)) return r;   // story scripted flag >= / <= / == X: 0 outside story
    }
    if (p.dlcStory == T::Versus) {
        if (In(id, {801, 802, 810})) return false; // DLC story battle, battle number, DLC story flags
        if (id == 803) return true;                // DLC story not battle number
    }
    if (p.waveIndex != ValueGameDecides) {
        const auto v = static_cast<uint32_t>(p.waveIndex);
        if (auto r = Compare(id, 783, 784, 785, x, v)) return r; // wave / enemy index >= / <= / == X
        if (id == 1016) return v == x;                           // wave enemy index (story mode)
    }
    if (p.battleScript != ValueGameDecides)
        if (auto r = Compare(id, 769, 770, 771, x, static_cast<uint32_t>(p.battleScript))) return r; // battle mgr value128
    if (p.specialModes == T::Versus && In(id, {664, 666, 764, 765, 766, 767, 772})) return false; // arcade, Tekken Ball, episode
    if (p.item == Profile::Item::None && id == 718) return false; // has item
    if (p.item == Profile::Item::All && id == 718) return true;
    if (p.bodyType != Profile::Body::Default && (id == 687 || id == 691)) { // player is a bear / a big character
        if (id == 687) return p.bodyType == Profile::Body::Bear;
        return p.bodyType == Profile::Body::Big || p.bodyType == Profile::Body::Bear;
    }
    if (p.stage == T::Versus) {
        if (id == 682 || id == 775) return false; // stage ID == X, stage ID number
        if (id == 683) return true;               // stage ID != X
    }
    return std::nullopt;
}

bool StripsProperty(const Profile& p, Game game, uint32_t id) {
    const bool story = p.storyEffects == Profile::Strip::Strip, global = p.globalWrites == Profile::Strip::Strip;
    if (game == Game::T7)
        return story && In(id, {33347, 33348, 33439, 33437, 33807, 33808, 33812, 33813, 33814, 33471, 33994});
    if (story && ((id >= 33109 && id <= 33111) || In(id, {34130, 34131, 34136, 34137, 34144, 34145, 34149, 34151, 34152,
        34153, 34154, 34155, 34156, 34160, 34161, 34168, 33760, 33555, 33709, 33549, 33788})))
        return true;
    return global && In(id, {34159, 33536, 33674, 33781, 33743, 34027});
}

std::map<std::string, uint32_t> ConversionSettings(const Profile& p) {
    return {{"story_flag", static_cast<uint32_t>(p.storyFlag == StoryFlagGameDecides ? 0 : p.storyFlag)},
            {"in_heat", p.inHeat ? 1u : 0u}, {"flags_clear", p.flagsClear ? 1u : 0u}};
}

std::string Describe(const Profile& p) {
    std::string text = p.identity == Profile::Identity::AdoptTarget ? "identity: target" : "identity: character " + std::to_string(p.character);
    text += p.gender == Profile::Gender::FromTarget ? ", gender: target" : p.gender == Profile::Gender::Male ? ", gender: male" : ", gender: female";
    text += p.storyFlag == StoryFlagGameDecides ? ", story flag: game" : ", story flag: " + std::to_string(p.storyFlag);
    text += p.inHeat ? ", in Heat" : "";
    text += p.flagsClear ? "" : ", flags set";
    text += p.storyMode == Profile::Toggle::Versus ? ", not in story" : "";
    text += p.dlcStory == Profile::Toggle::Versus ? ", not in DLC story" : "";
    if (p.waveIndex != ValueGameDecides) text += ", wave " + std::to_string(p.waveIndex);
    if (p.battleScript != ValueGameDecides) text += ", battle script " + std::to_string(p.battleScript);
    text += p.specialModes == Profile::Toggle::Versus ? ", no special modes" : "";
    text += p.item == Profile::Item::None ? ", no items" : p.item == Profile::Item::All ? ", all items" : "";
    text += p.bodyType == Profile::Body::Normal ? ", normal body" : p.bodyType == Profile::Body::Big ? ", big body" : p.bodyType == Profile::Body::Bear ? ", bear" : "";
    text += p.stage == Profile::Toggle::Versus ? ", any stage" : "";
    text += p.storyEffects == Profile::Strip::Strip ? ", story effects stripped" : "";
    text += p.globalWrites == Profile::Strip::Strip ? ", global writes stripped" : "";
    return text;
}
}

#include "LegacyJsonMoveset.hpp"
#include "Extractor_t7/T7ExportSnapshot.hpp"
#include "GameProcess.hpp"
#include "GameIDs.hpp"
#include "ExtractorSettings.hpp"
#include "Structs_t7.h"
#include "Structs_ttt2.h"
#include "Animations.hpp"
#include "Compression.hpp"
#include "FileIO.hpp"
#include "Helpers.hpp"
#include "constants.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <ctime>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>

namespace LegacyJson {
namespace {
namespace G = StructsT7_gameAddr;
namespace T2 = StructsTTT2;
using json = nlohmann::json;
using Path = std::filesystem::path;

size_t Aligned(size_t value) { return (value + 7) & ~size_t(7); }
void Check(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }
std::string Utf8(const Path& path) { return Helpers::to_utf8(path.wstring()); }

std::vector<Byte> ReadFile(const Path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    Check(file.good(), "Cannot read " + Utf8(path));
    const auto size = static_cast<size_t>(file.tellg());
    Check(size <= (256ULL << 20), Utf8(path) + " is too large");
    std::vector<Byte> bytes(size);
    file.seekg(0);
    Check(!size || file.read(reinterpret_cast<char*>(bytes.data()), size).good(), "Cannot read " + Utf8(path));
    return bytes;
}

// JSON integers: the extractor wrote unsigned values, and -1 (or another negative number) for a missing reference.
uint64_t U(const json& value) {
    if (value.is_number_unsigned()) return value.get<uint64_t>();
    if (value.is_number_integer()) return static_cast<uint64_t>(value.get<int64_t>());
    throw std::runtime_error("Expected an integer in the moveset JSON");
}
uint64_t U(const json& object, const char* key) { return U(object.at(key)); }
int64_t S(const json& value) { return static_cast<int64_t>(U(value)); }
int64_t S(const json& object, const char* key) { return S(object.at(key)); }

// As the memory exporters name characters: "[KAZUYA]" / "KAZUYA" -> "Kazuya", "FRV_mod" -> "Frv Mod".
std::string DisplayName(std::string value) {
    if (value.starts_with("[")) value.erase(0, 1);
    if (value.ends_with("]")) value.pop_back();
    bool start = true;
    for (auto& character : value) {
        const auto byte = static_cast<unsigned char>(character);
        if (!std::isprint(byte) || strchr("<>:\"/\\|?*", byte) || character == '_') character = ' ';
        else if (std::isalpha(byte)) character = static_cast<char>(start ? std::toupper(byte) : std::tolower(byte));
        start = strchr(" -.", character) != nullptr;
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '.')) value.pop_back();
    while (!value.empty() && value.front() == ' ') value.erase(0, 1);
    if (value.empty()) value = "Unknown";
    if (value.size() > 31) value.resize(31);
    return value;
}

// "2021-04-26 12:05:16.092844+00:00" (UTC) -> Unix seconds; 0 when unreadable.
uint64_t ParseDate(const std::string& value) {
    std::tm time{};
    if (sscanf_s(value.c_str(), "%d-%d-%d %d:%d:%d", &time.tm_year, &time.tm_mon, &time.tm_mday, &time.tm_hour, &time.tm_min, &time.tm_sec) != 6) return 0;
    time.tm_year -= 1900; time.tm_mon -= 1;
    const auto seconds = _mkgmtime(&time);
    return seconds > 0 ? static_cast<uint64_t>(seconds) : 0;
}

enum List { Reactions, Requirement, HitCondition, Projectile, Pushback, PushbackExtra, Cancel, GroupCancel, CancelExtra, ExtraProp,
    StartProp, EndProp, Move, Voiceclip, InputSequence, Input, Parry, CameraData, ThrowCamera, ListCount };
// The JSON list of each table entry. The extractor did not save the move start and end property lists.
constexpr const char* Keys[ListCount] = {"reaction_list", "requirements", "hit_conditions", "projectiles", "pushbacks", "pushback_extras",
    "cancels", "group_cancels", "cancel_extradata", "extra_move_properties", nullptr, nullptr, "moves", "voiceclips", "input_sequences",
    "input_extradata", "parry_related", "throw_extras", "throws"};

struct Source {
    Path folder;
    json m;
    bool t7 = false, ps3 = false;
    std::array<uint64_t, ListCount> counts{};
    std::vector<std::string> animationOrder;        // first use, as the exporters order them
    std::map<std::string, std::vector<Byte>> animations; // little-endian, trimmed to the animation's size
    std::map<std::string, std::string> dataOf;      // animation name -> the animation whose data it uses
    std::array<std::vector<Byte>, 12> motas;        // little-endian; empty when not kept
    Report report;

    const json& Records(List list) const { return m.at(Keys[list]); }
    // A record reference: -1 for none. The extractor wrote (pointer - list) / size, so a null pointer gave a negative index.
    int64_t Ref(List list, int64_t index) {
        if (index < 0) return -1;
        if (static_cast<uint64_t>(index) >= counts[list]) { ++report.droppedReferences; return -1; }
        return index;
    }
};

// The extractor's byte swapper (ByteSwap.py, Swap64Anim) sizes each bone's base pose with `descriptor - 4 < 4`, unsigned
// in the game's code but not in Python: bones with a descriptor below 4 (12-byte base pose, counted in the float bones)
// got 6 bytes, so everything from the first such bone on was swapped at the wrong offsets. Each of its steps is a byte
// swap: running the same steps again, with the values it read (descriptors, frames, float bones, now little-endian),
// gives the game's big-endian animation back. Returns false when this is not such a 0x64 animation.
bool UndoExtractorSwap(Byte* animation, size_t size) {
    if (size < 4 || animation[0] != 0x64 || animation[1] != 0) return false;
    uint16_t bones; memcpy(&bones, animation + 2, 2);
    const size_t postDescriptors = 4 + bones * 2ULL;
    if (!bones || bones > 255 || postDescriptors + 6 > size) return false;
    std::vector<uint16_t> descriptors(bones);
    memcpy(descriptors.data(), animation + 4, bones * 2ULL);
    if (std::none_of(descriptors.begin(), descriptors.end(), [](uint16_t d) { return d < 4; })) return false;
    uint16_t frames, floatBones;
    memcpy(&frames, animation + postDescriptors, 2); memcpy(&floatBones, animation + postDescriptors + 4, 2);
    size_t end = postDescriptors + 6 + floatBones * 4ULL;
    for (auto d : descriptors) end += static_cast<int>(d) - 4 < 4 ? 6 : 12; // the Python test, signed
    end += ((frames + 14ULL) >> 4) * 4;
    if (end > size) return false;
    auto swap = [&](size_t a, size_t b) { std::swap(animation[a], animation[b]); };
    swap(0, 1); swap(2, 3);
    for (size_t i = 0; i < bones; ++i) swap(4 + i * 2, 5 + i * 2);
    swap(postDescriptors, postDescriptors + 1); swap(postDescriptors + 4, postDescriptors + 5);
    size_t at = postDescriptors + 6;
    for (size_t i = 0; i < floatBones; ++i, at += 4) { swap(at, at + 3); swap(at + 1, at + 2); }
    for (auto d : descriptors) {
        if (static_cast<int>(d) - 4 < 4) { swap(at, at + 1); swap(at + 2, at + 3); swap(at + 4, at + 5); at += 6; }
        else { swap(at, at + 3); swap(at + 1, at + 2); swap(at + 4, at + 7); swap(at + 5, at + 6); swap(at + 8, at + 11); swap(at + 9, at + 10); at += 12; }
    }
    for (size_t i = 0; i < (frames + 14ULL) >> 4; ++i, at += 4) { swap(at, at + 3); swap(at + 1, at + 2); }
    return true;
}

// The size of an animation, after the repairs below; 0 when it cannot be read. `bytes` may be modified (swapped,
// padded). The extractor guessed where each animation ends and often cut its last byte, a 0 in the game (46 of
// Kazuya's 1310 animations, compared with the game's files): such data gets up to 8 zero bytes back (`padded`).
uint64_t RepairAnimation(std::vector<Byte>& bytes, bool* padded, bool* unswapped) {
    const auto original = bytes;
    for (bool undo : {false, true}) {
        bytes = original;
        if (undo && !UndoExtractorSwap(bytes.data(), bytes.size())) break;
        const auto length = bytes.size();
        auto size = TAnimUtils::FromMemory::Safe::GetAnimSize(bytes.data(), bytes.size());
        while (!size && length && bytes.size() < length + 8) {
            bytes.push_back(0);
            size = TAnimUtils::FromMemory::Safe::GetAnimSize(bytes.data(), bytes.size());
        }
        if (!size) continue;
        if (padded) *padded = size > length;
        if (unswapped) *unswapped = undo;
        bytes.resize(static_cast<size_t>(size));
        if (!TAnimUtils::FromMemory::IsLittleEndian(bytes.data()) && !TAnimUtils::FromMemory::ByteswapAnimation(bytes.data(), bytes.size()))
            return 0;
        return size;
    }
    bytes = original;
    return 0;
}

std::optional<std::vector<Byte>> LoadAnimation(const Path& path, bool* padded = nullptr, bool* unswapped = nullptr) {
    if (!std::filesystem::is_regular_file(path)) return std::nullopt;
    auto bytes = ReadFile(path);
    if (!RepairAnimation(bytes, padded, unswapped)) return std::nullopt;
    return bytes;
}

// A MOTA file, little-endian and trimmed to its last animation; empty when the file is not valid MOTA data.
std::vector<Byte> LoadMota(const Path& path, uint64_t& unswapped) {
    if (!std::filesystem::is_regular_file(path)) return {};
    auto bytes = ReadFile(path);
    if (bytes.size() < 20 || memcmp(bytes.data(), "MOTA", 4)) return {};
    if (bytes[5] == 1 && !TAnimUtils::FromMemory::ByteswapMota(bytes.data(), bytes.size())) return {};
    uint32_t count; memcpy(&count, bytes.data() + 12, 4);
    if (count > (bytes.size() - 20) / 4) return {};
    std::set<uint32_t> offsets;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t offset; memcpy(&offset, bytes.data() + 20 + i * 4ULL, 4);
        if (offset < 20ULL + count * 4ULL || offset >= bytes.size()) return {};
        offsets.insert(offset);
    }
    uint64_t end = 20;
    for (auto it = offsets.begin(); it != offsets.end(); ++it) {
        const auto next = std::next(it);
        const uint64_t limit = next == offsets.end() ? bytes.size() : *next;
        // Animations come out little-endian: the extractor swapped some MOTA headers without their animations, and
        // mis-swapped others (UndoExtractorSwap)
        std::vector<Byte> animation(bytes.begin() + *it, bytes.begin() + limit);
        bool padded = false, undone = false;
        const auto size = RepairAnimation(animation, &padded, &undone);
        if (!size || (padded && next != offsets.end())) return {};
        if (*it + size > bytes.size()) bytes.resize(static_cast<size_t>(*it + size));
        memcpy(bytes.data() + *it, animation.data(), static_cast<size_t>(size));
        if (undone) ++unswapped;
        end = (std::max)(end, *it + size);
    }
    bytes.resize(static_cast<size_t>(end));
    return bytes;
}

// Folders where a missing animation is looked for, as the extractor's importer did: the other extractions of the same
// game first, then folders without a game prefix (animpool).
std::vector<Path> SearchFolders(const Path& folder, bool t7) {
    std::vector<Path> same, pools;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(folder.parent_path(), error)) {
        if (!entry.is_directory() || entry.path() == folder || !std::filesystem::is_directory(entry.path() / "anim")) continue;
        const auto name = Utf8(entry.path().filename());
        if (t7 ? name.starts_with("t7_") : (name.starts_with("tag2_") || name.starts_with("rpcs3_tag2_"))) same.push_back(entry.path());
        else if (name.find('_') == std::string::npos) pools.push_back(entry.path());
    }
    std::sort(same.begin(), same.end()); std::sort(pools.begin(), pools.end());
    same.insert(same.end(), pools.begin(), pools.end());
    return same;
}

Source Load(const Path& folder) {
    Source s;
    s.folder = std::filesystem::absolute(folder);
    Path jsonPath;
    for (const auto& entry : std::filesystem::directory_iterator(s.folder))
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            Check(jsonPath.empty(), Utf8(s.folder.filename()) + " holds more than one JSON file");
            jsonPath = entry.path();
        }
    Check(!jsonPath.empty(), Utf8(s.folder.filename()) + " holds no moveset JSON");
    {
        const auto bytes = ReadFile(jsonPath);
        s.m = json::parse(bytes.begin(), bytes.end());
    }
    const auto version = s.m.at("version").get<std::string>();
    Check(version == "Tekken7" || version == "Tag2", "Unsupported moveset version '" + version + "' (Tekken7 and Tag2 only)");
    s.t7 = version == "Tekken7";
    const auto folderName = Utf8(s.folder.filename());
    s.ps3 = folderName.starts_with("rpcs3_");
    for (int i = 0; i < ListCount; ++i) if (Keys[i]) s.counts[i] = s.m.at(Keys[i]).size();
    // The extractor read one input past the end of the list (input_extradata_size + 1)
    if (s.counts[Input]) --s.counts[Input];
    Check(s.counts[Move] > 0, "Moveset has no moves");
    Check(s.m.at("aliases").size() == 148, "Expected 148 aliases");

    // Character names: the moveset list shows the folder's name (mods keep the character's in-game name)
    std::string shortName = folderName;
    for (const char* prefix : {"rpcs3_tag2_", "tag2_", "t7_"})
        if (shortName.starts_with(prefix)) { shortName.erase(0, strlen(prefix)); break; }
    s.report.name = DisplayName(shortName);
    s.report.game = s.t7 ? "T7" : "TTT2";
    s.report.characterId = static_cast<uint32_t>(U(s.m, "character_id"));
    s.report.moves = s.counts[Move];

    // Animations, by name
    std::vector<Path> search;
    bool searched = false;
    for (const auto& move : s.Records(Move)) {
        const auto name = move.at("anim_name").get<std::string>();
        if (s.dataOf.contains(name)) continue;
        const auto file = Helpers::string_to_wstring(name) + L".bin";
        bool padded = false, unswapped = false;
        auto bytes = LoadAnimation(s.folder / "anim" / file, &padded, &unswapped);
        if (!bytes) {
            if (!searched) { search = SearchFolders(s.folder, s.t7); searched = true; }
            for (const auto& other : search) if ((bytes = LoadAnimation(other / "anim" / file, &padded, &unswapped))) break;
            if (!bytes) {
                // Not readable anywhere: the move plays the moveset's first animation
                Check(!s.animationOrder.empty(), "The first move's animation '" + name + "' is missing or invalid");
                s.report.replacedAnimations.push_back(name);
                s.dataOf.emplace(name, s.animationOrder.front());
                continue;
            }
            s.report.borrowedAnimations.push_back(name);
        }
        s.dataOf.emplace(name, name);
        if (padded) ++s.report.paddedAnimations;
        if (unswapped) ++s.report.unswappedAnimations;
        s.animationOrder.push_back(name);
        s.animations.emplace(name, std::move(*bytes));
    }
    s.report.animations = s.animationOrder.size();

    // MOTA 0 and 1 are placeholders the extractor wrote instead of the game's data
    for (int i = 2; i < 12; ++i) {
        s.motas[i] = LoadMota(s.folder / (L"mota_" + std::to_wstring(i) + L".bin"), s.report.unswappedMotaAnimations);
        (s.motas[i].empty() ? s.report.skippedMotas : s.report.motas).push_back(i);
    }
    return s;
}

template<class T> T& At(std::vector<Byte>& bytes, size_t offset) { return *reinterpret_cast<T*>(bytes.data() + offset); }

// -- Tekken 7: an image of the moveset as the game holds it, exported by the memory exporter -- //

class ImageProcess : public GameProcess {
    std::map<gameAddr, std::vector<Byte>> m_regions;
protected:
    bool ReadMemoryRaw(gameAddr address, void* data, size_t size) const override {
        auto it = m_regions.upper_bound(address);
        if (it == m_regions.begin()) return false;
        --it;
        if (address - it->first > it->second.size() || size > it->second.size() - (address - it->first)) return false;
        memcpy(data, it->second.data() + (address - it->first), size); return true;
    }
public:
    void Add(gameAddr address, std::vector<Byte> bytes) { m_regions[address] = std::move(bytes); }
};

std::vector<Byte> BuildT7(Source& s) {
    constexpr gameAddr Root = 0x100000000ULL;
    const auto& m = s.m;
    std::array<size_t, ListCount> at{};
    std::vector<Byte> image(sizeof(G::MovesetInfo));
    for (int i = 0; i < ListCount; ++i) { at[i] = Aligned(image.size()); image.resize(at[i] + s.counts[i] * T7Export::RecordSizes[i]); }
    auto record = [&](List list, uint64_t index) { return at[list] + index * T7Export::RecordSizes[list]; };
    auto pointer = [&](List list, int64_t index) -> gameAddr { index = s.Ref(list, index); return index < 0 ? 0 : Root + record(list, index); };
    std::map<std::string, gameAddr> strings;
    auto text = [&](const std::string& value) {
        if (auto found = strings.find(value); found != strings.end()) return found->second;
        const auto offset = image.size();
        image.insert(image.end(), value.begin(), value.end()); image.push_back(0);
        return strings.emplace(value, Root + offset).first->second;
    };
    std::map<std::string, gameAddr> animations;
    auto place = [&](const std::vector<Byte>& bytes) {
        const auto offset = Aligned(image.size());
        image.resize(offset + bytes.size()); memcpy(image.data() + offset, bytes.data(), bytes.size());
        return Root + offset;
    };

    const auto characterName = text(m.at("tekken_character_name").get<std::string>());
    const auto creator = text(m.at("creator_name").get<std::string>()), date = text(m.at("date").get<std::string>()),
        fullDate = text(m.at("fulldate").get<std::string>());
    for (const auto& move : s.Records(Move)) { text(move.at("name").get<std::string>()); text(move.at("anim_name").get<std::string>()); }
    for (const auto& name : s.animationOrder) animations[name] = place(s.animations.at(name));
    std::array<gameAddr, 12> motas{};
    for (int i = 0; i < 12; ++i) if (!s.motas[i].empty()) motas[i] = place(s.motas[i]);

    for (uint64_t i = 0; i < s.counts[Reactions]; ++i) {
        const auto& j = s.Records(Reactions)[i];
        auto& r = At<G::Reactions>(image, record(Reactions, i));
        for (int k = 0; k < 7; ++k) r.pushbacks[k] = pointer(Pushback, S(j.at("pushback_indexes").at(k)));
        const auto& d = j.at("u1list");
        r.front_direction = (uint16_t)U(d.at(0)); r.back_direction = (uint16_t)U(d.at(1)); r.left_side_direction = (uint16_t)U(d.at(2));
        r.right_side_direction = (uint16_t)U(d.at(3)); r.front_counterhit_direction = (uint16_t)U(d.at(4)); r.downed_direction = (uint16_t)U(d.at(5));
        r.vertical_pushback = (uint16_t)U(j, "vertical_pushback");
        r.default_moveid = (uint16_t)U(j, "standing"); r.crouch_moveid = (uint16_t)U(j, "crouch");
        r.counterhit_moveid = (uint16_t)U(j, "ch"); r.crouch_counterhit_moveid = (uint16_t)U(j, "crouch_ch");
        r.left_side_moveid = (uint16_t)U(j, "left_side"); r.crouch_left_side_moveid = (uint16_t)U(j, "left_side_crouch");
        r.right_side_moveid = (uint16_t)U(j, "right_side"); r.crouch_right_side_moveid = (uint16_t)U(j, "right_side_crouch");
        r.backturned_moveid = (uint16_t)U(j, "back"); r.crouch_backturned_moveid = (uint16_t)U(j, "back_crouch");
        r.block_moveid = (uint16_t)U(j, "block"); r.crouch_block_moveid = (uint16_t)U(j, "crouch_block");
        r.wallslump_moveid = (uint16_t)U(j, "wallslump"); r.downed_moveid = (uint16_t)U(j, "downed");
    }
    for (uint64_t i = 0; i < s.counts[Requirement]; ++i) {
        const auto& j = s.Records(Requirement)[i];
        auto& r = At<StructsT7::Requirement>(image, record(Requirement, i));
        r.condition = (uint32_t)U(j, "req"); r.param_unsigned = (uint32_t)U(j, "param");
    }
    for (uint64_t i = 0; i < s.counts[HitCondition]; ++i) {
        const auto& j = s.Records(HitCondition)[i];
        auto& r = At<G::HitCondition>(image, record(HitCondition, i));
        r.requirements_addr = pointer(Requirement, S(j, "requirement_idx")); r.damage = (uint32_t)U(j, "damage");
        r.reactions_addr = pointer(Reactions, S(j, "reaction_list_idx"));
    }
    for (uint64_t i = 0; i < s.counts[Projectile]; ++i) {
        const auto& j = s.Records(Projectile)[i];
        auto& r = At<G::Projectile>(image, record(Projectile, i));
        auto* bytes = reinterpret_cast<Byte*>(&r);
        const auto& u1 = j.at("u1"); const auto& u2 = j.at("u2");
        Check(u1.size() == 48 && u2.size() == 28, "Unexpected projectile layout");
        for (int k = 0; k < 48; ++k) { const auto v = (uint16_t)U(u1.at(k)); memcpy(bytes + k * 2, &v, 2); }
        for (int k = 0; k < 28; ++k) { const auto v = (uint16_t)U(u2.at(k)); memcpy(bytes + 0x70 + k * 2, &v, 2); }
        r.hit_condition_addr = pointer(HitCondition, S(j, "hit_condition_idx")); r.cancel_addr = pointer(Cancel, S(j, "cancel_idx"));
    }
    for (uint64_t i = 0; i < s.counts[Pushback]; ++i) {
        const auto& j = s.Records(Pushback)[i];
        auto& r = At<G::Pushback>(image, record(Pushback, i));
        r.duration = (uint16_t)U(j, "val1"); r.displacement = (uint16_t)U(j, "val2"); r.num_of_loops = (uint32_t)U(j, "val3");
        r.extradata_addr = pointer(PushbackExtra, S(j, "pushbackextra_idx"));
    }
    for (uint64_t i = 0; i < s.counts[PushbackExtra]; ++i)
        At<StructsT7::PushbackExtradata>(image, record(PushbackExtra, i)).horizontal_offset = (uint16_t)U(s.Records(PushbackExtra)[i]);
    for (auto list : {Cancel, GroupCancel})
        for (uint64_t i = 0; i < s.counts[list]; ++i) {
            const auto& j = s.Records(list)[i];
            auto& r = At<G::Cancel>(image, record(list, i));
            r.command = U(j, "command");
            r.requirements_addr = pointer(Requirement, S(j, "requirement_idx")); r.extradata_addr = pointer(CancelExtra, S(j, "extradata_idx"));
            r.detection_start = (uint32_t)U(j, "frame_window_start"); r.detection_end = (uint32_t)U(j, "frame_window_end");
            r.starting_frame = (uint32_t)U(j, "starting_frame"); r.move_id = (uint16_t)U(j, "move_id"); r.cancel_option = (uint16_t)U(j, "cancel_option");
        }
    for (uint64_t i = 0; i < s.counts[CancelExtra]; ++i)
        At<StructsT7::CancelExtradata>(image, record(CancelExtra, i)).value = (uint32_t)U(s.Records(CancelExtra)[i]);
    for (uint64_t i = 0; i < s.counts[ExtraProp]; ++i) {
        const auto& j = s.Records(ExtraProp)[i];
        auto& r = At<StructsT7::ExtraMoveProperty>(image, record(ExtraProp, i));
        r.starting_frame = (uint32_t)U(j, "type"); r.id = (uint32_t)U(j, "id"); r.value_unsigned = (uint32_t)U(j, "value");
    }
    for (uint64_t i = 0; i < s.counts[Move]; ++i) {
        const auto& j = s.Records(Move)[i];
        auto& r = At<G::Move>(image, record(Move, i));
        r.name_addr = strings.at(j.at("name").get<std::string>());
        const auto animation = j.at("anim_name").get<std::string>();
        r.anim_name_addr = strings.at(animation); r.anim_addr = animations.at(s.dataOf.at(animation));
        r.vuln = (uint32_t)U(j, "vuln"); r.hitlevel = (uint32_t)U(j, "hitlevel");
        r.cancel_addr = pointer(Cancel, S(j, "cancel_idx"));
        const auto u2 = U(j, "u2"), u4 = U(j, "u4");
        r._0x30_int__0x28_related = (int32_t)u2; r._0x34_int = (int32_t)(u2 >> 32);
        if (U(j, "u3")) ++s.report.droppedReferences; // a game pointer the JSON kept as is: no index to rebuild it from
        r._0x40_int__0x38_related = (int32_t)u4; r._0x44_int = (uint32_t)(u4 >> 32);
        r._0x50_int__0x48_related = (int32_t)U(j, "u6");
        r.transition = (uint16_t)U(j, "transition"); r._0x56_short = (int16_t)U(j, "u7");
        r.moveId_val1 = (uint16_t)U(j, "u8"); r.moveId_val2 = (uint16_t)U(j, "u8_2"); r._0x5C_short = (int16_t)U(j, "u9");
        r.hit_condition_addr = pointer(HitCondition, S(j, "hit_condition_idx"));
        r.anim_len = (uint32_t)U(j, "anim_max_len");
        r.airborne_start = (uint32_t)U(j, "u10"); r.airborne_end = (uint32_t)U(j, "u11"); r.ground_fall = (uint32_t)U(j, "u12");
        r.voicelip_addr = pointer(Voiceclip, S(j, "voiceclip_idx"));
        r.extra_move_property_addr = pointer(ExtraProp, S(j, "extra_properties_idx"));
        r._0x98_int = (int32_t)U(j, "u15"); r.hitbox_location = (uint32_t)U(j, "hitbox_location");
        r.first_active_frame = (uint32_t)U(j, "first_active_frame"); r.last_active_frame = (uint32_t)U(j, "last_active_frame");
        r._0xA8_short = (int16_t)U(j, "u16"); r.distance = (uint16_t)U(j, "u17"); r._0xAC_int = (int32_t)U(j, "u18");
    }
    for (uint64_t i = 0; i < s.counts[Voiceclip]; ++i) At<StructsT7::Voiceclip>(image, record(Voiceclip, i)).id = (uint32_t)U(s.Records(Voiceclip)[i]);
    for (uint64_t i = 0; i < s.counts[InputSequence]; ++i) {
        const auto& j = s.Records(InputSequence)[i];
        auto& r = At<G::InputSequence>(image, record(InputSequence, i));
        r.input_window_frames = (uint16_t)U(j, "u1"); r.input_amount = (uint16_t)U(j, "u2"); r._0x4_int = (int32_t)U(j, "u3");
        r.input_addr = pointer(Input, S(j, "extradata_idx"));
    }
    for (uint64_t i = 0; i < s.counts[Input]; ++i) {
        const auto& j = s.Records(Input)[i];
        auto& r = At<StructsT7::Input>(image, record(Input, i));
        r.direction = (uint32_t)U(j, "u1"); r.button = (uint32_t)U(j, "u2");
    }
    for (uint64_t i = 0; i < s.counts[Parry]; ++i) At<StructsT7::UnknownParryRelated>(image, record(Parry, i)).value = (uint32_t)U(s.Records(Parry)[i]);
    for (uint64_t i = 0; i < s.counts[CameraData]; ++i) {
        const auto& j = s.Records(CameraData)[i];
        auto& r = At<StructsT7::CameraData>(image, record(CameraData, i));
        const auto& u2 = j.at("u2");
        r._0x0_int = (uint32_t)U(j, "u1"); r._0x4_short = (uint16_t)U(u2.at(0)); r.left_side_camera_data = (uint16_t)U(u2.at(1));
        r.right_side_camera_data = (uint16_t)U(u2.at(2)); r._0xA_short = (uint16_t)U(u2.at(3));
    }
    for (uint64_t i = 0; i < s.counts[ThrowCamera]; ++i) {
        const auto& j = s.Records(ThrowCamera)[i];
        auto& r = At<G::ThrowCamera>(image, record(ThrowCamera, i));
        r.side = U(j, "u1"); r.cameradata_addr = pointer(CameraData, S(j, "throwextra_idx"));
    }

    auto& info = At<G::MovesetInfo>(image, 0);
    const Byte head[8] = {0, 0, 1, 0, 'H', 'I', 'D', 0}; // as the game's movesets start (isInitialized = 1)
    memcpy(&info, head, sizeof(head));
    info.character_name_addr = reinterpret_cast<char*>(characterName); info.character_creator_addr = reinterpret_cast<char*>(creator);
    info.date_addr = reinterpret_cast<char*>(date); info.fulldate_addr = reinterpret_cast<char*>(fullDate);
    const auto& aliases = m.at("aliases");
    for (int i = 0; i < 56; ++i) { info.orig_aliases[i] = (uint16_t)U(aliases.at(i)); info.current_aliases[i] = (uint16_t)U(aliases.at(56 + i)); }
    for (int i = 0; i < 36; ++i) info.unknown_aliases[i] = (uint16_t)U(aliases.at(112 + i));
    for (int i = 0; i < ListCount; ++i) { info.table.entries[i].listAddr = Root + at[i]; info.table.entries[i].listCount = s.counts[i]; }
    for (int i = 0; i < 12; ++i) info.motas.motas[i] = motas[i];
    info.motas._unknown_ = 0;

    ImageProcess process;
    process.Add(Root, std::move(image));
    ExtractSettings settings = 0;
    for (int i = 0; i < 12; ++i) settings |= ExtractSettings(1) << i;
    auto snapshot = T7Export::ReadSnapshot(process, Root, settings, 0);
    TKMovesetHeader header;
    header.flags = 0; header.game_specific_flags = 0;
    header.gameId = GameId_T7; header.minorVersion = GameVersions::T7::NONE; header.characterId = s.report.characterId;
    memset(header.gameVersionHash, 0, sizeof(header.gameVersionHash));
    strcpy_s(header.version_string, sizeof(header.version_string), MOVESET_VERSION_STRING);
    strncpy_s(header.origin, sizeof(header.origin), "Tekken 7", _TRUNCATE);
    strncpy_s(header.target_character, sizeof(header.target_character), s.report.name.c_str(), _TRUNCATE);
    const auto original = DisplayName(m.at("tekken_character_name").get<std::string>());
    strncpy_s(header.orig_character_name, sizeof(header.orig_character_name), original.c_str(), _TRUNCATE);
    header.date = Helpers::getCurrentTimestamp();
    const auto extracted = ParseDate(m.value("extraction_date", std::string()));
    header.extraction_date = extracted ? extracted : header.date;
    return T7Export::Serialize(snapshot, header);
}

// -- Tag Tournament 2: the file ExtractorTTT2 writes (little-endian records, indexes instead of pointers) -- //

std::vector<Byte> BuildTTT2(Source& s) {
    const auto& m = s.m;
    constexpr size_t Strides[ListCount] = {sizeof(T2::Reactions), sizeof(T2::Requirement), sizeof(T2::HitCondition), sizeof(T2::Projectile),
        sizeof(T2::Pushback), sizeof(T2::PushbackExtradata), sizeof(T2::Cancel), sizeof(T2::Cancel), sizeof(T2::CancelExtradata),
        sizeof(T2::ExtraMoveProperty), sizeof(T2::OtherMoveProperty), sizeof(T2::OtherMoveProperty), sizeof(T2::Move), sizeof(T2::Voiceclip),
        sizeof(T2::InputSequence), sizeof(T2::Input), sizeof(T2::UnknownParryRelated), sizeof(T2::CameraData), sizeof(T2::ThrowCamera)};
    constexpr gameAddr32 Missing = MOVESET_ADDR32_MISSING;
    auto index = [&](List list, int64_t value) -> gameAddr32 { value = s.Ref(list, value); return value < 0 ? Missing : static_cast<gameAddr32>(value); };

    // Name block: the character name (its '[' replaced by the extraction prefix), creator, dates, then move and animation names.
    // MovesetInfo string offsets count from the end of MovesetInfo (0x20C), move name offsets from the block start.
    std::vector<Byte> names;
    std::map<std::string, uint32_t> nameOffsets;
    auto text = [&](const std::string& value) {
        if (auto found = nameOffsets.find(value); found != nameOffsets.end()) return found->second;
        const auto offset = static_cast<uint32_t>(names.size());
        names.insert(names.end(), value.begin(), value.end()); names.push_back(0);
        return nameOffsets.emplace(value, offset).first->second;
    };
    std::string marked = m.at("tekken_character_name").get<std::string>();
    if (marked.starts_with("[")) marked.erase(0, 1);
    marked.insert(0, MOVESET_EXTRACTED_NAME_PREFIX);
    names.insert(names.end(), marked.begin(), marked.end()); names.push_back(0);
    const auto creator = text(m.at("creator_name").get<std::string>()), date = text(m.at("date").get<std::string>()),
        fullDate = text(m.at("fulldate").get<std::string>());
    for (const auto& move : s.Records(Move)) { text(move.at("name").get<std::string>()); text(move.at("anim_name").get<std::string>()); }

    std::array<size_t, ListCount> at{};
    std::vector<Byte> records;
    for (int i = 0; i < ListCount; ++i) { at[i] = Aligned(records.size()); records.resize(at[i] + s.counts[i] * Strides[i]); }
    records.resize(Aligned(records.size()));
    auto record = [&](List list, uint64_t i) { return at[list] + i * Strides[list]; };

    std::vector<Byte> animationBlock;
    std::map<std::string, uint32_t> animations;
    for (const auto& name : s.animationOrder) {
        const auto& bytes = s.animations.at(name);
        animations[name] = static_cast<uint32_t>(animationBlock.size());
        animationBlock.insert(animationBlock.end(), bytes.begin(), bytes.end());
    }
    std::vector<Byte> motaBlock;
    T2::MotaList motas;
    for (auto& mota : motas.motas) mota = Missing;
    for (int i = 0; i < 12; ++i) {
        if (s.motas[i].empty()) continue;
        motas.motas[i] = static_cast<gameAddr32>(motaBlock.size());
        motaBlock.insert(motaBlock.end(), s.motas[i].begin(), s.motas[i].end());
    }

    for (uint64_t i = 0; i < s.counts[Reactions]; ++i) {
        const auto& j = s.Records(Reactions)[i];
        auto& r = At<T2::Reactions>(records, record(Reactions, i));
        gameAddr32* pushbacks[7] = {&r.front_pushback, &r.backturned_pushback, &r.left_side_pushback, &r.right_side_pushback,
            &r.front_counterhit_pushback, &r.downed_pushback, &r.block_pushback};
        for (int k = 0; k < 7; ++k) *pushbacks[k] = index(Pushback, S(j.at("pushback_indexes").at(k)));
        const auto& d = j.at("u1list");
        r.front_direction = (uint16_t)U(d.at(0)); r.back_direction = (uint16_t)U(d.at(1)); r.left_side_direction = (uint16_t)U(d.at(2));
        r.right_side_direction = (uint16_t)U(d.at(3)); r.front_counterhit_direction = (uint16_t)U(d.at(4)); r.downed_direction = (uint16_t)U(d.at(5));
        r.vertical_pushback = (uint16_t)U(j, "vertical_pushback");
        r.default_moveid = (uint16_t)U(j, "standing"); r.crouch_moveid = (uint16_t)U(j, "crouch");
        r.counterhit_moveid = (uint16_t)U(j, "ch"); r.crouch_counterhit_moveid = (uint16_t)U(j, "crouch_ch");
        r.left_side_moveid = (uint16_t)U(j, "left_side"); r.crouch_left_side_moveid = (uint16_t)U(j, "left_side_crouch");
        r.right_side_moveid = (uint16_t)U(j, "right_side"); r.crouch_right_side_moveid = (uint16_t)U(j, "right_side_crouch");
        r.backturned_moveid = (uint16_t)U(j, "back"); r.crouch_backturned_moveid = (uint16_t)U(j, "back_crouch");
        r.block_moveid = (uint16_t)U(j, "block"); r.crouch_block_moveid = (uint16_t)U(j, "crouch_block");
        r.wallslump_moveid = (uint16_t)U(j, "wallslump"); r.downed_moveid = (uint16_t)U(j, "downed");
    }
    for (uint64_t i = 0; i < s.counts[Requirement]; ++i) {
        const auto& j = s.Records(Requirement)[i];
        auto& r = At<T2::Requirement>(records, record(Requirement, i));
        r.condition = (uint32_t)U(j, "req"); r.param_unsigned = (uint32_t)U(j, "param");
    }
    for (uint64_t i = 0; i < s.counts[HitCondition]; ++i) {
        const auto& j = s.Records(HitCondition)[i];
        auto& r = At<T2::HitCondition>(records, record(HitCondition, i));
        r.requirements_addr = index(Requirement, S(j, "requirement_idx")); r.damage = (uint32_t)U(j, "damage");
        r.reactions_addr = index(Reactions, S(j, "reaction_list_idx"));
    }
    // Projectiles: the extractor did not read TTT2 projectiles (it saved zeros), and neither do the TTT2 converters
    for (uint64_t i = 0; i < s.counts[Pushback]; ++i) {
        const auto& j = s.Records(Pushback)[i];
        auto& r = At<T2::Pushback>(records, record(Pushback, i));
        r.duration = (uint16_t)U(j, "val1"); r.displacement = (uint16_t)U(j, "val2"); r.num_of_loops = (uint32_t)U(j, "val3");
        r.extradata_addr = index(PushbackExtra, S(j, "pushbackextra_idx"));
    }
    for (uint64_t i = 0; i < s.counts[PushbackExtra]; ++i)
        At<T2::PushbackExtradata>(records, record(PushbackExtra, i)).horizontal_offset = (uint16_t)U(s.Records(PushbackExtra)[i]);
    for (auto list : {Cancel, GroupCancel})
        for (uint64_t i = 0; i < s.counts[list]; ++i) {
            const auto& j = s.Records(list)[i];
            auto& r = At<T2::Cancel>(records, record(list, i));
            r.command = U(j, "command"); // the extractor already read it as (second word << 32) | first word
            r.requirements_addr = index(Requirement, S(j, "requirement_idx")); r.extradata_addr = index(CancelExtra, S(j, "extradata_idx"));
            r.detection_start = (uint32_t)U(j, "frame_window_start"); r.detection_end = (uint32_t)U(j, "frame_window_end");
            r.starting_frame = (uint32_t)U(j, "starting_frame"); r.move_id = (uint16_t)U(j, "move_id"); r.cancel_option = (uint16_t)U(j, "cancel_option");
        }
    for (uint64_t i = 0; i < s.counts[CancelExtra]; ++i)
        At<T2::CancelExtradata>(records, record(CancelExtra, i)).value = (uint32_t)U(s.Records(CancelExtra)[i]);
    for (uint64_t i = 0; i < s.counts[ExtraProp]; ++i) {
        const auto& j = s.Records(ExtraProp)[i];
        auto& r = At<T2::ExtraMoveProperty>(records, record(ExtraProp, i));
        r.starting_frame = (uint32_t)U(j, "type"); r.id = (uint32_t)U(j, "id"); r.value_unsigned = (uint32_t)U(j, "value");
    }
    for (uint64_t i = 0; i < s.counts[Move]; ++i) {
        const auto& j = s.Records(Move)[i];
        auto& r = At<T2::Move>(records, record(Move, i));
        const auto animation = j.at("anim_name").get<std::string>();
        r.name_addr = nameOffsets.at(j.at("name").get<std::string>()); r.anim_name_addr = nameOffsets.at(animation);
        r.anim_addr = animations.at(s.dataOf.at(animation));
        r.vuln = (uint32_t)U(j, "vuln"); r.hitlevel = (uint32_t)U(j, "hitlevel");
        r.cancel_addr = index(Cancel, S(j, "cancel_idx"));
        r._0x28_cancel_addr = Missing; r._0x48_cancel_addr = Missing; // not saved by the extractor
        if (U(j, "u3")) ++s.report.droppedReferences;               // a game pointer the JSON kept as is
        r._0x38_cancel_addr = Missing;
        r._0x30_int__0x28_related = (int32_t)U(j, "u2"); r._0x40_int__0x38_related = (int32_t)U(j, "u4");
        r._0x50_int__0x48_related = (uint32_t)U(j, "u6");
        r.transition = (uint16_t)U(j, "transition"); r._0x56_short = (int16_t)U(j, "u7");
        r.moveId_val2 = (uint16_t)U(j, "u8_2"); r.moveId_val1 = (uint16_t)U(j, "u8");
        r.hit_condition_addr = index(HitCondition, S(j, "hit_condition_idx"));
        r.anim_len = (int32_t)U(j, "anim_max_len");
        r.airborne_start = (uint32_t)U(j, "u10"); r.airborne_end = (uint32_t)U(j, "u11"); r.ground_fall = (uint32_t)U(j, "u12");
        r.voicelip_addr = index(Voiceclip, S(j, "voiceclip_idx"));
        r.extra_move_property_addr = index(ExtraProp, S(j, "extra_properties_idx"));
        r.move_start_extraprop_addr = Missing; r.move_end_extraprop_addr = Missing;
        r._0x98_int = (int32_t)U(j, "u15");
        r.hitbox_location = (uint32_t)U(j, "hitbox_location"); // stored unswapped by both extractors
        r.first_active_frame = (uint32_t)U(j, "first_active_frame"); r.last_active_frame = (uint32_t)U(j, "last_active_frame");
        r._0x6c_short = (int16_t)U(j, "u16"); r.distance = (uint16_t)U(j, "u17");
    }
    for (uint64_t i = 0; i < s.counts[Voiceclip]; ++i) At<T2::Voiceclip>(records, record(Voiceclip, i)).id = (uint32_t)U(s.Records(Voiceclip)[i]);
    for (uint64_t i = 0; i < s.counts[InputSequence]; ++i) {
        const auto& j = s.Records(InputSequence)[i];
        auto& r = At<T2::InputSequence>(records, record(InputSequence, i));
        r._0x0_int8 = (int8_t)U(j, "u3"); r.input_window_frames = (uint8_t)U(j, "u1"); r.input_amount = (uint16_t)U(j, "u2");
        r.input_addr = index(Input, S(j, "extradata_idx"));
    }
    for (uint64_t i = 0; i < s.counts[Input]; ++i) {
        const auto& j = s.Records(Input)[i];
        auto& r = At<T2::Input>(records, record(Input, i));
        r.direction = (uint32_t)U(j, "u1"); r.button = (uint32_t)U(j, "u2");
    }
    for (uint64_t i = 0; i < s.counts[Parry]; ++i) At<T2::UnknownParryRelated>(records, record(Parry, i)).value = (uint32_t)U(s.Records(Parry)[i]);
    for (uint64_t i = 0; i < s.counts[CameraData]; ++i) {
        const auto& j = s.Records(CameraData)[i];
        auto& r = At<T2::CameraData>(records, record(CameraData, i));
        const auto& u2 = j.at("u2");
        r._0x0_int = (uint32_t)U(j, "u1"); r._0x4_short = (uint16_t)U(u2.at(0)); r.left_side_camera_data = (uint16_t)U(u2.at(1));
        r.right_side_camera_data = (uint16_t)U(u2.at(2)); r._0xA_short = (uint16_t)U(u2.at(3));
    }
    for (uint64_t i = 0; i < s.counts[ThrowCamera]; ++i) {
        const auto& j = s.Records(ThrowCamera)[i];
        auto& r = At<T2::ThrowCamera>(records, record(ThrowCamera, i));
        r._0x0_uint = (uint32_t)U(j, "u1"); r.cameradata_addr = index(CameraData, S(j, "throwextra_idx"));
    }

    T2::MovesetInfo info{};
    info.isInitialized = true;
    constexpr uint32_t NamesBase = sizeof(T2::MovesetInfo);
    static_assert(NamesBase == 0x20C);
    // ExtractorTTT2::CopyMovesetInfoBlock: the game's offsets (name at 0x20C), shifted by the prefix's growth, which puts
    // each string at 0x20C + its offset in the name block
    info.character_name_addr = NamesBase;
    info.character_creator_addr = NamesBase + creator; info.date_addr = NamesBase + date; info.fulldate_addr = NamesBase + fullDate;
    const auto& aliases = m.at("aliases");
    for (int i = 0; i < 56; ++i) { info.orig_aliases[i] = (uint16_t)U(aliases.at(i)); info.current_aliases[i] = (uint16_t)U(aliases.at(56 + i)); }
    for (int i = 0; i < 36; ++i) info.unknown_values[i] = (uint16_t)U(aliases.at(112 + i));
    for (int i = 0; i < ListCount; ++i) { info.table.entries[i].listAddr = static_cast<gameAddr32>(at[i]); info.table.entries[i].listCount = static_cast<uint32_t>(s.counts[i]); }
    info.motas = motas;

    // Blocks, as ExtractorTTT2::Extract lays them out
    T2::TKMovesetHeaderBlocks blocks{};
    const size_t infoSize = offsetof(T2::MovesetInfo, table), tableSize = sizeof(T2::MovesetTable), motaListSize = sizeof(T2::MotaList);
    const std::vector<Byte> movelist(8);
    blocks.movesetInfoBlock = 0;
    blocks.tableBlock = Aligned(infoSize);
    blocks.motalistsBlock = Aligned(blocks.tableBlock + tableSize);
    blocks.nameBlock = Aligned(blocks.motalistsBlock + motaListSize);
    blocks.movesetBlock = Aligned(blocks.nameBlock + names.size());
    blocks.animationBlock = Aligned(blocks.movesetBlock + records.size());
    blocks.motaBlock = Aligned(blocks.animationBlock + animationBlock.size());
    blocks.movelistBlock = Aligned(blocks.motaBlock + motaBlock.size());
    std::vector<Byte> payload(blocks.movelistBlock + movelist.size());
    auto copy = [&](uint64_t offset, const void* data, size_t size) { if (size) memcpy(payload.data() + offset, data, size); };
    copy(blocks.movesetInfoBlock, &info, infoSize);
    copy(blocks.tableBlock, &info.table, tableSize);
    copy(blocks.motalistsBlock, &info.motas, motaListSize);
    copy(blocks.nameBlock, names.data(), names.size());
    copy(blocks.movesetBlock, records.data(), records.size());
    copy(blocks.animationBlock, animationBlock.data(), animationBlock.size());
    copy(blocks.motaBlock, motaBlock.data(), motaBlock.size());
    copy(blocks.movelistBlock, movelist.data(), movelist.size());

    TKMovesetHeader header;
    TKMovesetProperty properties[1] = {{.id = TKMovesetProperty_END, .value = 0}};
    header.flags = 0; header.game_specific_flags = 0;
    header.gameId = GameId_TTT2; header.minorVersion = GameVersions::TTT2::RPCS3_BLES01702_0103; header.characterId = s.report.characterId;
    memset(header.gameVersionHash, 0, sizeof(header.gameVersionHash));
    strcpy_s(header.version_string, sizeof(header.version_string), MOVESET_VERSION_STRING);
    strncpy_s(header.origin, sizeof(header.origin), s.ps3 ? "Tekken Tag 2 PS3" : "Tekken Tag 2 Wii U", _TRUNCATE);
    strncpy_s(header.target_character, sizeof(header.target_character), s.report.name.c_str(), _TRUNCATE);
    const auto original = DisplayName(m.at("tekken_character_name").get<std::string>());
    strncpy_s(header.orig_character_name, sizeof(header.orig_character_name), original.c_str(), _TRUNCATE);
    header.date = Helpers::getCurrentTimestamp();
    const auto extracted = ParseDate(m.value("extraction_date", std::string()));
    header.extraction_date = extracted ? extracted : header.date;
    header.header_size = static_cast<uint32_t>(Aligned(sizeof(TKMovesetHeader)));
    header.block_list = header.header_size + static_cast<uint32_t>(Aligned(sizeof(properties)));
    header.block_list_size = static_cast<uint32_t>(std::size(blocks.blocks));
    header.moveset_data_start = header.block_list + static_cast<uint32_t>(Aligned(sizeof(blocks)));
    header.moveset_data_size = 0;
    header.compressionType = TKMovesetCompressionType_None;
    auto* p = payload.data();
    std::vector<std::pair<Byte*, uint64_t>> hashed = {
        {reinterpret_cast<Byte*>(properties), sizeof(properties)},
        {p + blocks.movesetInfoBlock, infoSize}, {p + blocks.tableBlock, tableSize}, {p + blocks.motalistsBlock, motaListSize},
        {p + blocks.movesetBlock, records.size()}, {p + blocks.animationBlock, animationBlock.size()}, {p + blocks.motaBlock, motaBlock.size()}};
    header.orig_crc32 = header.crc32 = Helpers::CalculateCrc32(hashed);

    std::vector<Byte> file(header.moveset_data_start + payload.size());
    memcpy(file.data(), &header, sizeof(header));
    memcpy(file.data() + header.header_size, properties, sizeof(properties));
    memcpy(file.data() + header.block_list, &blocks, sizeof(blocks));
    memcpy(file.data() + header.moveset_data_start, payload.data(), payload.size());
    return file;
}

std::string FileName(const Source& s) {
    if (s.t7) return "T7_" + s.report.name + " (MovesetExtractor).tkmvst";
    return "TTT2_" + s.report.name + (s.ps3 ? " (PS3).tkmvst" : " (Wii U).tkmvst");
}
}

std::vector<Byte> BuildFile(const std::filesystem::path& folder, Report* report) {
    auto source = Load(folder);
    auto file = source.t7 ? BuildT7(source) : BuildTTT2(source);
    if (report) *report = source.report;
    return file;
}

Report WriteMovesetFile(const std::filesystem::path& folder, const std::filesystem::path& directory, TKMovesetCompressionType_ codec) {
    auto source = Load(folder);
    source.report.path = directory / Helpers::string_to_wstring(FileName(source));
    if (std::filesystem::exists(source.report.path)) { source.report.existed = true; return source.report; }
    const auto file = source.t7 ? BuildT7(source) : BuildTTT2(source);
    uint64_t size = 0;
    std::unique_ptr<Byte[]> compressed(CompressionUtils::RAW::Moveset::Compress(file.data(), file.size(), codec, size));
    Check(compressed != nullptr, "Moveset compression failed");
    Check(FileIO::WriteAtomically(source.report.path, {compressed.get(), static_cast<size_t>(size)}), "Cannot write the moveset file");
    return source.report;
}
}

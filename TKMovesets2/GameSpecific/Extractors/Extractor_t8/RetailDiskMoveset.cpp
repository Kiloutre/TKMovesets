#include "RetailDiskMoveset.hpp"
#include "T7ToRetailT8.hpp"
#include "Editor_t8Retail/Editor_t8Retail.hpp"
#include "GameStaticData.hpp"
#include "Compression.hpp"
#include "FileIO.hpp"
#include "Helpers.hpp"
#include "constants.h"
#include "Structs_t8.h"
#include <memory>
#include <array>
#include <cstring>
#include <map>
#include <stdexcept>

namespace RetailT8Disk {
namespace {
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
template<class T> T Get(const std::vector<Byte>& b, uint64_t at) {
    Check(at <= b.size() && sizeof(T) <= b.size() - at, "Retail motbin field is outside the file");
    T v; memcpy(&v, b.data() + at, sizeof(v)); return v;
}
template<class T> void Put(std::vector<Byte>& b, uint64_t at, T v) {
    Check(at <= b.size() && sizeof(T) <= b.size() - at, "Retail motbin field is outside the file");
    memcpy(b.data() + at, &v, sizeof(v));
}
namespace S8 = StructsT8;
using Header = S8::MovesetHeader;
using Move = S8::Move;
constexpr uint64_t HeaderSize = sizeof(Header);
// Disk encryption of the six 0x20-byte move blocks (docs/formats/tkdata.md section 3): word j = K[PERM[j]] ^ t,
// except one word holding value ^ K[PERM[j]]; t is per-block noise.
constexpr std::array<uint32_t, 16> K = {
    0xADFB76C8, 0x7DEF1F1C, 0xA84B71E0, 0xD88448A2, 0x964F5B9E, 0x7EE2BC2C, 0xA27D5221, 0xBB9FE67D,
    0xAD15269F, 0xEC1A9785, 0x9BAE2F45, 0xA4296896, 0x275AA004, 0x37E22F31, 0x3803D4A7, 0x9B81329F};
constexpr std::array<int, 8> Perm = {4, 3, 2, 6, 15, 0, 1, 5};
constexpr std::array<uint32_t, 6> EncryptedFields = {uint32_t(offsetof(Move, name_key)), uint32_t(offsetof(Move, anim_key)),
    uint32_t(offsetof(Move, vuln)), uint32_t(offsetof(Move, hit_level)), uint32_t(offsetof(Move, character_id)), uint32_t(offsetof(Move, move_key))};
uint32_t DecodeDisk(const std::vector<Byte>& b, uint64_t at) {
    std::array<uint32_t, 8> x{};
    for (size_t j = 0; j < 8; ++j) x[j] = Get<uint32_t>(b, at + j * 4) ^ K[Perm[j]];
    for (const auto t : x) {
        int odd = -1, count = 0;
        for (size_t j = 0; j < 8; ++j) if (x[j] != t) { odd = static_cast<int>(j); ++count; }
        if (count == 1) return x[odd];
        if (count == 0) return t;
    }
    throw std::runtime_error("Retail motbin move field does not decode");
}
// Header (pointer offset, count offset, stride) of each table, in the relocate's order.
struct T { uint32_t pointer, count, stride; };
#define RETAIL_TABLE(member, record) T{uint32_t(offsetof(Header, member.address)), uint32_t(offsetof(Header, member.count)), uint32_t(sizeof(S8::record))}
constexpr T Requirements = RETAIL_TABLE(requirements, Requirement), HitConditions = RETAIL_TABLE(hit_conditions, HitCondition),
    Projectiles = RETAIL_TABLE(projectiles, Projectile), Pushbacks = RETAIL_TABLE(pushbacks, Pushback),
    PushbackExtra = RETAIL_TABLE(pushback_extra, PushbackExtra), Cancels = RETAIL_TABLE(cancels, Cancel),
    Groups = RETAIL_TABLE(group_cancels, Cancel), CancelExtra = RETAIL_TABLE(cancel_extra, CancelExtra),
    ExtraProps = RETAIL_TABLE(extra_properties, ExtraMoveProperty), StartProps = RETAIL_TABLE(start_properties, OtherMoveProperty),
    EndProps = RETAIL_TABLE(end_properties, OtherMoveProperty), Moves = RETAIL_TABLE(moves, Move),
    Voiceclips = RETAIL_TABLE(voiceclips, Voiceclip), InputSequences = RETAIL_TABLE(input_sequences, InputSequence),
    Inputs = RETAIL_TABLE(inputs, Input), Cameras = RETAIL_TABLE(cameras, CameraData), Throws = RETAIL_TABLE(throws, ThrowCamera),
    Dialogues = RETAIL_TABLE(dialogues, Dialogue),
    Reactions{uint32_t(offsetof(Header, reactions)), uint32_t(offsetof(Header, reactions_count)), uint32_t(sizeof(S8::Reactions))};
#undef RETAIL_TABLE
// Record field offsets.
#define AT(record, member) uint64_t(offsetof(S8::record, member))
}

void Relocate(std::vector<Byte>& image, uint64_t root, uint32_t domain, const RetailT8GameData::Anmbin& character,
    const RetailT8GameData::Anmbin& common, uint64_t placeholder) {
    Check(image.size() >= HeaderSize && !memcmp(image.data() + 8, "TEK", 3), "Not a retail TEK motbin");
    Check(Get<uint8_t>(image, offsetof(Header, relocated)) == 0, "Retail motbin is already relocated");
    // Header: strings to the shared placeholder, runtime fields cleared, every table pointer made absolute.
    for (uint64_t i = 0; i < std::size(Header{}.strings); ++i) Put<uint64_t>(image, offsetof(Header, strings) + i * sizeof(gameAddr), placeholder);
    Put<uint64_t>(image, offsetof(Header, _0x170), 0);
    constexpr uint64_t Previous = offsetof(Header, previous_identity), Identity = offsetof(Header, identity);
    if (domain == 0x74) for (auto [at, v] : {std::pair<uint64_t, int16_t>{Previous, -0x73}, {Previous + 2, 0x73}, {Identity, -0x74}, {Identity + 2, 0x74}}) Put<int16_t>(image, at, v);
    std::map<uint32_t, uint64_t> offset; // header pointer field -> table's file offset
    constexpr uint32_t FirstTable = offsetof(Header, reactions), LastTable = offsetof(Header, dialogues);
    for (uint32_t at = FirstTable; at <= LastTable; at += uint32_t(at == FirstTable ? offsetof(Header, requirements) - FirstTable : sizeof(S8::MovesetTable))) {
        const auto relative = Get<uint64_t>(image, at);
        Check(relative <= image.size() - HeaderSize, "Retail motbin table is outside the file");
        offset[at] = HeaderSize + relative;
        Put<uint64_t>(image, at, root + HeaderSize + relative);
    }
    for (uint64_t at = offsetof(Header, runtime_slots); at < HeaderSize; at += sizeof(gameAddr)) Put<uint64_t>(image, at, 0);
    auto count = [&](const T& t) {
        const auto n = static_cast<uint64_t>(Get<int32_t>(image, t.count) > 0 ? Get<int32_t>(image, t.count) : 0);
        Check(n <= (image.size() - offset.at(t.pointer)) / t.stride, "Retail motbin table exceeds the file");
        return n;
    };
    auto row = [&](const T& t, uint64_t i) { return offset.at(t.pointer) + i * t.stride; };
    auto address = [&](const T& t, uint64_t index) { return root + offset.at(t.pointer) + index * t.stride; };
    // Body animation handles from <code>.anmbin list 0 (count at +0x1c, offsets at +0x68).
    const auto body = character.List(0);
    // Moves
    for (uint64_t i = 0, n = count(Moves); i < n; ++i) {
        const auto m = row(Moves, i);
        Put<uint64_t>(image, m + AT(Move, name_addr), placeholder); Put<uint64_t>(image, m + AT(Move, anim_name_addr), placeholder);
        Put<uint64_t>(image, m + AT(Move, hit_conditions), address(HitConditions, Get<uint64_t>(image, m + AT(Move, hit_conditions))));
        if (Get<uint16_t>(image, offsetof(Header, skip_animation_lookup)) == 0) {
            const auto index = Get<uint32_t>(image, m + AT(Move, anim_addr)) & 0xffff3fffu;
            Check(index < body.size(), "Retail move animation index is outside the animation list");
            const uint32_t id = body[index];
            Put<uint64_t>(image, m + AT(Move, anim_addr), 0xEF00000000000000ULL | (uint64_t(domain & 0xff) << 40) | id);
            auto panm = character.Panm(0, id);
            if (!panm) panm = common.Panm(0, id);
            if (!panm) Put<uint32_t>(image, m + AT(Move, animation_length), 0xffffffffu); // the game had nothing to load: not loaded
            else {
                // AnimLength (0x14183ADE0) is the last frame index, frames - 1; the game stores it + 1.
                const int length = static_cast<int>(EditorT8Retail::PanmFrames(*panm)), v11c = Get<int32_t>(image, m + AT(Move, animation_length_adjuster));
                Put<int32_t>(image, m + AT(Move, animation_length), v11c > 1 ? length - v11c + 1 : length);
            }
        }
        Put<uint64_t>(image, m + AT(Move, cancels), address(Cancels, Get<uint64_t>(image, m + AT(Move, cancels))));
        for (auto [field, gate] : {std::pair<uint64_t, uint64_t>{AT(Move, cancels_2), AT(Move, cancel_gate_2)},
            {AT(Move, cancels_3), AT(Move, cancel_gate_3)}, {AT(Move, cancels_4), AT(Move, cancel_gate_4)}})
            Put<uint64_t>(image, m + field, Get<int32_t>(image, m + gate) < 1 ? 0 : address(Cancels, Get<uint64_t>(image, m + field)));
        for (auto [field, table] : {std::pair<uint64_t, T>{AT(Move, extra_properties), ExtraProps}, {AT(Move, start_properties), StartProps},
            {AT(Move, end_properties), EndProps}, {AT(Move, voiceclip), Voiceclips}}) {
            const auto v = Get<uint64_t>(image, m + field);
            Put<uint64_t>(image, m + field, v == UINT64_MAX ? 0 : address(table, v));
        }
    }
    // Cancels and group cancels: list pointers, then the option bits the game derives at load.
    const uint64_t requirements = root + offset.at(Requirements.pointer), extras = root + offset.at(CancelExtra.pointer);
    auto conditions = [&](uint64_t cancel) {   // requirement ids (and first params) up to the 1100 terminator
        std::vector<std::pair<uint32_t, uint32_t>> list;
        for (uint64_t at = offset.at(Requirements.pointer) + (Get<uint64_t>(image, cancel + AT(Cancel, requirements)) - requirements);; at += Requirements.stride) {
            const auto id = Get<uint32_t>(image, at);
            if (id == 0x44c) return list;
            list.push_back({id, Get<uint32_t>(image, at + AT(Requirement, params))});
            Check(list.size() < 100000, "Unterminated retail requirement list");
        }
    };
    constexpr uint32_t NoRoute[] = {0x29b, 0x29c, 0x3ff, 0x2e9, 0x321, 0x322}; // 0x1475BCE68
    for (const auto* table : {&Cancels, &Groups}) {
        const bool main = table == &Cancels;
        for (uint64_t i = 0, n = count(*table); i < n; ++i) {
            const auto c = row(*table, i);
            Put<uint64_t>(image, c + AT(Cancel, requirements), requirements + Get<uint64_t>(image, c + AT(Cancel, requirements)) * Requirements.stride);
            Put<uint64_t>(image, c + AT(Cancel, extra), extras + Get<uint64_t>(image, c + AT(Cancel, extra)) * CancelExtra.stride);
            const auto list = conditions(c);
            auto has = [&](uint32_t id) { for (auto [r, p] : list) if (r == id) return true; return false; };
            const bool command0 = Get<uint64_t>(image, c + AT(Cancel, command)) == 0;
            uint16_t option = command0 ? 1 : 0;
            for (auto [r, p] : list) if (r > 0x406 && r != 0x419 && !(r & 0x8000)) { option |= 2; break; }
            for (auto [r, p] : list) if (r > 0x423 && !(r & 0x8000)) { option |= 4; break; }
            if (has(0x87fb)) option |= 8;
            if (main) for (auto [r, p] : list) if (r == 0x9f && p == 1) { option |= 0x200; break; }
            bool noRoute = false; for (auto id : NoRoute) noRoute |= has(id);
            if ((!command0 || has(0x83) || has(0x84) || has(0x85) || has(0x86) || has(0x87)) && !noRoute) option |= 0x10;
            const auto extra = Get<uint32_t>(image, offset.at(CancelExtra.pointer) + (Get<uint64_t>(image, c + AT(Cancel, extra)) - extras));
            if ((extra & 0x3c00) != 0x2800 && (extra & 0x3c00) != 0x2c00) option |= 0x40;
            if ((extra & 0x3f) == 0x16) option |= 0x80;
            const auto move = Get<uint16_t>(image, c + AT(Cancel, move_id));
            if (main && (move & 0x8000) && (move == 0x8027 || move < 0x8021 || move > 0x802a)) option |= 0x100;
            Put<uint16_t>(image, c + AT(Cancel, option), option);
        }
    }
    for (uint64_t i = 0, n = count(HitConditions); i < n; ++i) {
        const auto h = row(HitConditions, i);
        Put<uint64_t>(image, h + AT(HitCondition, requirements), requirements + Get<uint64_t>(image, h + AT(HitCondition, requirements)) * Requirements.stride);
        Put<uint64_t>(image, h + AT(HitCondition, reactions), address(Reactions, Get<uint64_t>(image, h + AT(HitCondition, reactions))));
    }
    for (uint64_t i = 0, n = count(InputSequences); i < n; ++i)
        Put<uint64_t>(image, row(InputSequences, i) + AT(InputSequence, inputs), address(Inputs, Get<uint64_t>(image, row(InputSequences, i) + AT(InputSequence, inputs))));
    for (uint64_t i = 0, n = count(ExtraProps); i < n; ++i)
        Put<uint64_t>(image, row(ExtraProps, i) + AT(ExtraMoveProperty, requirements), requirements + Get<uint64_t>(image, row(ExtraProps, i) + AT(ExtraMoveProperty, requirements)) * Requirements.stride);
    for (const auto* table : {&StartProps, &EndProps})
        for (uint64_t i = 0, n = count(*table); i < n; ++i)
            Put<uint64_t>(image, row(*table, i) + AT(OtherMoveProperty, requirements), requirements + Get<uint64_t>(image, row(*table, i) + AT(OtherMoveProperty, requirements)) * Requirements.stride);
    for (uint64_t i = 0, n = count(Projectiles); i < n; ++i) {
        const auto p = row(Projectiles, i);
        if (const auto v = Get<uint64_t>(image, p + AT(Projectile, hit_condition))) Put<uint64_t>(image, p + AT(Projectile, hit_condition), address(HitConditions, v));
        if (const auto v = Get<uint64_t>(image, p + AT(Projectile, cancel))) Put<uint64_t>(image, p + AT(Projectile, cancel), address(Cancels, v));
        if (!Get<int32_t>(image, p + AT(Projectile, _0xAC))) Put<int32_t>(image, p + AT(Projectile, _0xAC), Get<int32_t>(image, p + AT(Projectile, _0x30)) * 2);
        if (!Get<int32_t>(image, p + AT(Projectile, _0xB0))) Put<int32_t>(image, p + AT(Projectile, _0xB0), Get<int32_t>(image, p + AT(Projectile, _0x34)));
    }
    for (uint64_t i = 0, n = count(Reactions); i < n; ++i)
        for (uint64_t k = 0; k < std::size(S8::Reactions{}.pushbacks); ++k) {
            const auto at = row(Reactions, i) + AT(Reactions, pushbacks) + k * sizeof(gameAddr);
            Put<uint64_t>(image, at, address(Pushbacks, Get<uint64_t>(image, at)));
        }
    for (uint64_t i = 0, n = count(Pushbacks); i < n; ++i)
        Put<uint64_t>(image, row(Pushbacks, i) + AT(Pushback, extra), address(PushbackExtra, Get<uint64_t>(image, row(Pushbacks, i) + AT(Pushback, extra))));
    for (uint64_t i = 0, n = count(Throws); i < n; ++i)
        Put<uint64_t>(image, row(Throws, i) + AT(ThrowCamera, camera), address(Cameras, Get<uint64_t>(image, row(Throws, i) + AT(ThrowCamera, camera))));
    for (uint64_t i = 0, n = count(Dialogues); i < n; ++i)
        Put<uint64_t>(image, row(Dialogues, i) + AT(Dialogue, requirements), requirements + Get<uint64_t>(image, row(Dialogues, i) + AT(Dialogue, requirements)) * Requirements.stride);
    // Encrypted move fields: the game holds them in its memory encoding (value, key, check slots).
    for (uint64_t i = 0, n = count(Moves); i < n; ++i)
        for (const auto field : EncryptedFields)
            T7ToRetailT8::EncodeRetailField(image, row(Moves, i), i, field, DecodeDisk(image, row(Moves, i) + field));
    Put<uint8_t>(image, offsetof(Header, relocated), 1);
}

namespace {
// The relocated image, served as a process to the same capture code as a live export.
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
}

std::string MissingAnimationsNote(const std::array<uint64_t, 6>& missing, bool fromArchive) {
    constexpr const char* categories[] = {"body", "hands", "face", "swing", "camera", "extra"};
    uint64_t total = 0; std::string breakdown;
    for (size_t i = 0; i < missing.size(); ++i)
        if (missing[i]) { total += missing[i]; breakdown += (breakdown.empty() ? "" : ", ") + std::string(categories[i]) + " " + std::to_string(missing[i]); }
    if (!total) return {};
    return std::to_string(total) + (total == 1 ? " animation is" : " animations are") + " not in this file (" + breakdown + "). " +
        (fromArchive
        ? "The game's own files (tkdata.bin) hold no data for them, so their moves keep the same animation ids as the game's own moveset."
        : "The game had not loaded them and reading them from tkdata.bin was turned off. Importing reads them from the game's tkdata.bin; ids the game has no data for keep their ids.");
}

std::string FileName(const std::string& code) {
    const auto names = GameStaticData::LoadOptional("t8-retail");
    const auto character = names.FindCharacter(code);
    auto name = "T8_" + (character && !character->name.empty() ? character->name : code) + ".tkmvst";
    for (auto& c : name) if (strchr("<>:\"/\\|?*", c)) c = '_';
    return name;
}

Report WriteMovesetFile(const std::filesystem::path& tkdata, const std::string& code, const RetailT8GameData::Anmbin& common,
    const std::filesystem::path& path, uint8_t categories, TKMovesetCompressionType_ codec) {
    if (std::filesystem::exists(path)) throw std::runtime_error("Output moveset file already exists");
    const auto names = GameStaticData::LoadOptional("t8-retail");
    const auto character = names.FindCharacter(code);
    if (!character || !character->id) throw std::runtime_error("No character id is known for " + code);
    const auto motbin = RetailT8GameData::ReadTkdataFile(tkdata, "mothead/bin/" + code + ".motbin");
    const RetailT8GameData::Anmbin archive(RetailT8GameData::ReadTkdataFile(tkdata, "mothead/bin/" + code + ".anmbin"));
    Report report;
    const auto payload = Build(motbin, *character->id, archive, common, categories, &report);
    TKMovesetHeader header{};
    auto name = character->name.empty() ? code : character->name;
    if (name.size() >= sizeof(header.target_character)) name = code;
    // The header holds the player character id (runtime_id, else the asset id), as memory exports do; the handle
    // domain above is the asset id.
    header.characterId = character->runtimeId.value_or(*character->id); header.flags = 0;
    header.date = header.extraction_date = Helpers::getCurrentTimestamp();
    strcpy_s(header.origin, "Tekken 8"); strcpy_s(header.version_string, MOVESET_VERSION_STRING);
    strcpy_s(header.target_character, name.c_str()); strcpy_s(header.orig_character_name, name.c_str());
    std::vector<RetailT8Snapshot::OuterSection> sections;
    // No notes: the only animations not carried are ids the game itself has no data for (not missing).
    // Names section: records only hold name hashes, so the file carries the strings from the shipped name table.
    if (const auto known = RetailT8Snapshot::CollectNames(payload, [&](uint32_t hash) { return names.FindName(hash); }); !known.empty())
        sections.push_back(RetailT8Snapshot::NamesSection(known));
    const auto file = RetailT8Snapshot::Serialize(payload, header, sections);
    uint64_t size = 0;
    std::unique_ptr<Byte[]> compressed(CompressionUtils::RAW::Moveset::Compress(file.data(), file.size(), codec, size));
    if (!compressed) throw std::runtime_error("Moveset compression failed");
    if (!FileIO::WriteAtomically(path, {compressed.get(), static_cast<size_t>(size)})) throw std::runtime_error("Cannot write the moveset file");
    return report;
}

std::vector<Byte> Build(std::span<const Byte> motbin, uint32_t domain, const RetailT8GameData::Anmbin& character,
    const RetailT8GameData::Anmbin& common, uint8_t categories, Report* report) {
    constexpr uint64_t Root = 0x0000020000000000ULL, Placeholder = 0x00000200FFFF0000ULL;
    std::vector<Byte> image(motbin.begin(), motbin.end());
    Check(image.size() >= HeaderSize + sizeof(Move), "Retail motbin is too small");
    Relocate(image, Root, domain, character, common, Placeholder);
    ImageProcess process;
    process.Add(Placeholder, std::vector<Byte>(16, 0)); // the shared placeholder string: empty
    process.Add(Root, std::move(image));
    // Animations: every identity the character's archive declares in the selected categories, as the game's map holds
    // them; bytes from the character's archive, else com.anmbin; none when the game's files have no data for it.
    std::vector<RetailT8Snapshot::GivenAnimation> animations;
    Report r; r.domain = domain;
    for (const auto& [type, instance] : character.DeclaredIdentities()) {
        if (!(categories & (1u << type))) continue;
        auto panm = character.Panm(type, instance);
        if (!panm) panm = common.Panm(type, instance);
        RetailT8Snapshot::GivenAnimation given{type, instance, {}};
        if (panm) { given.panm.assign(panm->begin(), panm->end()); ++r.animations; } else ++r.dataless[type];
        animations.push_back(std::move(given));
    }
    auto payload = RetailT8Snapshot::CaptureWithAnimations(process, Root, animations);
    r.moves = RetailT8Snapshot::Table(payload, 12).size() / sizeof(Move);
    if (report) *report = r;
    return payload;
}
}
#undef AT

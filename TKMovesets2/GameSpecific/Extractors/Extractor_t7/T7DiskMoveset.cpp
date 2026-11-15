#include "T7DiskMoveset.hpp"
#include "T7ExportSnapshot.hpp"
#include "GameProcess.hpp"
#include "GameIDs.hpp"
#include "ExtractorSettings.hpp"
#include "Compression.hpp"
#include "FileIO.hpp"
#include "Helpers.hpp"
#include "constants.h"
#include <Windows.h>
#include <bcrypt.h>
#include <zlib.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <stdexcept>

namespace T7Disk {
namespace {
namespace G = StructsT7_gameAddr;
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
template<class T> T Get(const std::vector<Byte>& b, uint64_t at) {
    Check(at <= b.size() && sizeof(T) <= b.size() - at, "T7 moveset field is outside the file");
    T v; memcpy(&v, b.data() + at, sizeof(v)); return v;
}
template<class T> void Put(std::vector<Byte>& b, uint64_t at, T v) {
    Check(at <= b.size() && sizeof(T) <= b.size() - at, "T7 moveset field is outside the file");
    memcpy(b.data() + at, &v, sizeof(v));
}
constexpr char Key[] = "epsNJf#ut&v3fVrndlYkp&Sq@5Ry?BHA";  // tkdata.bin AES-256 key (public: Tekken 7 modding tools' tkdata.bms)
constexpr uint64_t DataRoot = 0x2E8;                       // HID offsets are relative to the end of MovesetInfo

void Decrypt(std::vector<Byte>& data) {
    if (data.size() % 16) data.resize(data.size() + 16 - data.size() % 16, 0);
    BCRYPT_ALG_HANDLE algorithm = nullptr; BCRYPT_KEY_HANDLE handle = nullptr;
    Check(BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_AES_ALGORITHM, nullptr, 0)), "Cannot start tkdata decryption");
    std::unique_ptr<void, void(*)(void*)> closeAlgorithm(algorithm, [](void* a) { BCryptCloseAlgorithmProvider(a, 0); });
    Check(BCRYPT_SUCCESS(BCryptSetProperty(algorithm, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_ECB)),
        sizeof(BCRYPT_CHAIN_MODE_ECB), 0)) && BCRYPT_SUCCESS(BCryptGenerateSymmetricKey(algorithm, &handle, nullptr, 0,
        reinterpret_cast<PUCHAR>(const_cast<char*>(Key)), 32, 0)), "Cannot start tkdata decryption");
    std::unique_ptr<void, void(*)(void*)> closeKey(handle, [](void* k) { BCryptDestroyKey(k); });
    for (size_t at = 0; at < data.size();) {
        const auto length = static_cast<ULONG>((std::min)(data.size() - at, size_t(1) << 28));
        ULONG written = 0;
        Check(BCRYPT_SUCCESS(BCryptDecrypt(handle, data.data() + at, length, nullptr, nullptr, 0, data.data() + at, length, &written, 0)) &&
            written == length, "tkdata decryption failed");
        at += length;
    }
}
std::vector<Byte> Inflate(std::span<const Byte> input, uint64_t expected) {
    z_stream stream{};
    Check(inflateInit(&stream) == Z_OK, "Cannot start tkdata decompression");
    std::unique_ptr<z_stream, decltype(&inflateEnd)> owner(&stream, inflateEnd);
    std::vector<Byte> out(static_cast<size_t>(expected ? expected : input.size() * 4));
    stream.next_in = const_cast<Bytef*>(input.data()); stream.avail_in = static_cast<uInt>(input.size());
    for (;;) {
        stream.next_out = out.data() + stream.total_out; stream.avail_out = static_cast<uInt>(out.size() - stream.total_out);
        const auto result = inflate(&stream, Z_NO_FLUSH);
        if (result == Z_STREAM_END) break;
        Check(result == Z_OK || result == Z_BUF_ERROR, "Invalid tkdata zlib data");
        if (stream.avail_out == 0) { Check(out.size() < (1ULL << 31), "tkdata file is too large"); out.resize(out.size() * 2); }
        else Check(stream.avail_in != 0, "Truncated tkdata zlib data");
    }
    out.resize(stream.total_out);
    return out;
}
std::vector<Byte> ReadAt(std::ifstream& file, uint64_t size, uint64_t at, uint64_t length) {
    Check(at <= size && length <= size - at && length <= (2ULL << 30), "tkdata extent is outside the archive");
    std::vector<Byte> bytes(static_cast<size_t>(length));
    file.clear(); file.seekg(static_cast<std::streamoff>(at));
    Check(bool(file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(length))), "Cannot read tkdata.bin");
    return bytes;
}
struct Entry { std::string name; uint64_t flags, offset, stored, size; };
std::vector<Entry> ReadToc(std::ifstream& file, uint64_t size) {
    const auto magic = ReadAt(file, size, 0, 16);
    Check(!memcmp(magic.data(), "_TEKKEN7CSFILES_", 16), "Not a Tekken 7 tkdata.bin");
    const auto footer = ReadAt(file, size, size - 0x68, 16);
    const auto offset = Get<uint64_t>(footer, 0), stored = Get<uint64_t>(footer, 8);
    auto toc = ReadAt(file, size, offset, stored);
    Decrypt(toc);
    toc.resize(static_cast<size_t>(stored));
    if (!toc.empty() && toc[0] == 0x78) toc = Inflate(toc, 0);
    Check(toc.size() >= 32 && !memcmp(toc.data(), "tsLniBNB", 8), "tkdata.bin file list is invalid");
    const auto count = Get<uint64_t>(toc, 8), info = Get<uint64_t>(toc, 16);
    auto names = Get<uint64_t>(toc, 24);
    Check(count <= 100000 && info <= toc.size() && count * 64 <= toc.size() - info, "tkdata.bin file list is invalid");
    std::vector<Entry> entries;
    for (uint64_t i = 0; i < count; ++i) {
        Check(names < toc.size(), "tkdata.bin file list is invalid");
        const auto end = std::find(toc.begin() + static_cast<ptrdiff_t>(names), toc.end(), Byte(0));
        Check(end != toc.end(), "tkdata.bin file list is invalid");
        Entry e{std::string(toc.begin() + static_cast<ptrdiff_t>(names), end), Get<uint64_t>(toc, info + i * 64 + 8),
            Get<uint64_t>(toc, info + i * 64 + 24), Get<uint64_t>(toc, info + i * 64 + 32), Get<uint64_t>(toc, info + i * 64 + 40)};
        names = static_cast<uint64_t>(end - toc.begin()) + 1;
        entries.push_back(std::move(e));
    }
    return entries;
}
uint64_t Open(const std::filesystem::path& tkdata, std::ifstream& file) {
    std::error_code error;
    const auto size = std::filesystem::file_size(tkdata, error);
    if (error) throw std::runtime_error("The game's tkdata.bin was not found at " + tkdata.string());
    file.open(tkdata, std::ios::binary);
    Check(file && size >= 16 + 0x68, "Cannot open tkdata.bin");
    return size;
}
std::string DisplayName(std::string value) { // as the memory exporter names files (Extractor_t7.cpp)
    if (value.starts_with(MOVESET_EXTRACTED_NAME_PREFIX)) value.erase(0, strlen(MOVESET_EXTRACTED_NAME_PREFIX));
    if (value.starts_with("[")) value.erase(0, 1);
    if (value.ends_with("]")) value.pop_back();
    bool start = true;
    for (auto& character : value) {
        const auto byte = static_cast<unsigned char>(character);
        if (!std::isprint(byte) || strchr("<>:\\\"/\\\\|?*", byte) || character == '_') character = ' ';
        else if (std::isalpha(byte)) character = static_cast<char>(start ? std::toupper(byte) : std::tolower(byte));
        start = strchr(" -.", character) != nullptr;
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '.')) value.pop_back();
    if (value.empty()) value = "Unknown";
    if (value.size() > 31) value.resize(31);
    return value;
}
// Animation offsets of a MOTA (big- or little-endian: byte 5 is 1 for big-endian), absolute in its process region.
std::vector<uint64_t> MotaAnimations(const std::vector<Byte>& bytes, uint64_t at, uint64_t base) {
    Check(at + 20 <= bytes.size() && !memcmp(bytes.data() + at, "MOTA", 4), "Invalid MOTA");
    const bool big = bytes[at + 5] == 1;
    auto u32 = [&](uint64_t o) { auto v = Get<uint32_t>(bytes, o); return big ? _byteswap_ulong(v) : v; };
    const auto count = u32(at + 12);
    Check(count <= 100000, "Invalid MOTA count");
    std::vector<uint64_t> result;
    for (uint32_t i = 0; i < count; ++i) result.push_back(base + at + u32(at + 20 + i * 4ULL));
    return result;
}
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

std::filesystem::path TkdataPath(const std::filesystem::path& gameExecutable) {
    // TekkenGame/Binaries/Win64/TekkenGame-Win64-Shipping.exe
    return gameExecutable.parent_path().parent_path().parent_path() / "Content" / "Binary" / "tkdata.bin";
}

std::vector<std::string> TkdataNames(const std::filesystem::path& tkdata) {
    std::ifstream file; const auto size = Open(tkdata, file);
    std::vector<std::string> names;
    for (auto& e : ReadToc(file, size)) names.push_back(std::move(e.name));
    return names;
}

std::vector<Byte> ReadTkdataFile(const std::filesystem::path& tkdata, std::string_view name) {
    std::ifstream file; const auto size = Open(tkdata, file);
    for (const auto& e : ReadToc(file, size)) {
        if (_stricmp(e.name.c_str(), std::string(name).c_str())) continue;
        auto data = ReadAt(file, size, e.offset + 0x10, e.stored);
        Decrypt(data);
        if (e.flags & 1) return Inflate(std::span<const Byte>(data).first(static_cast<size_t>(e.stored)), e.size);
        Check(e.size <= data.size(), "tkdata.bin entry is invalid");
        data.resize(static_cast<size_t>(e.size));
        return data;
    }
    throw std::runtime_error("tkdata.bin has no " + std::string(name));
}

std::vector<std::string> Characters(const std::filesystem::path& tkdata) {
    std::vector<std::string> codes;
    for (const auto& name : TkdataNames(tkdata)) {
        constexpr std::string_view prefix = "mothead\\bin\\", suffix = "mot.bin";
        if (name.size() <= prefix.size() + suffix.size() || _strnicmp(name.c_str(), prefix.data(), prefix.size()) ||
            _stricmp(name.c_str() + name.size() - suffix.size(), suffix.data())) continue;
        auto code = name.substr(prefix.size(), name.size() - prefix.size() - suffix.size());
        if (code != "test") codes.push_back(code);
    }
    std::sort(codes.begin(), codes.end());
    return codes;
}

std::vector<std::pair<std::string, std::string>> CharacterNames(const std::filesystem::path& tkdata) {
    std::ifstream file; const auto size = Open(tkdata, file);
    const auto entries = ReadToc(file, size);
    std::vector<std::pair<std::string, std::string>> result;
    for (const auto& code : Characters(tkdata)) {
        std::string name = code;
        for (const auto& e : entries) {
            if (_stricmp(e.name.c_str(), ("mothead\\bin\\" + code + "mot.bin").c_str())) continue;
            try {
                auto data = ReadAt(file, size, e.offset + 0x10, e.stored);
                Decrypt(data);
                const auto bytes = (e.flags & 1) ? Inflate(std::span<const Byte>(data).first(static_cast<size_t>(e.stored)), e.size) : data;
                const auto at = DataRoot + Get<uint64_t>(bytes, offsetof(G::MovesetInfo, character_name_addr));
                if (at < bytes.size()) {
                    const auto end = std::find(bytes.begin() + static_cast<ptrdiff_t>(at), bytes.end(), Byte(0));
                    name = DisplayName(std::string(bytes.begin() + static_cast<ptrdiff_t>(at), end));
                }
            } catch (const std::exception&) {}
            break;
        }
        result.push_back({code, name});
    }
    return result;
}

std::vector<Byte> BuildFile(const std::filesystem::path& tkdata, const std::string& code, const std::vector<Byte>& common, Report* report) {
    constexpr gameAddr Root = 0x0000010000000000ULL, Arc = 0x0000011000000000ULL, Common = 0x0000012000000000ULL, Movelist = 0x0000013000000000ULL;
    auto image = ReadTkdataFile(tkdata, "mothead\\bin\\" + code + "mot.bin");
    auto arc = ReadTkdataFile(tkdata, "mothead\\mask\\" + code + "rotsm.arc");
    // Non-roster movesets (bs7, ja4, ykz, zak) have no move list: exported without one, as from memory.
    const auto listName = "mothead\\movelist\\" + code + ".mvl";
    const auto names = TkdataNames(tkdata);
    const bool hasMovelist = std::any_of(names.begin(), names.end(), [&](const std::string& n) { return !_stricmp(n.c_str(), listName.c_str()); });
    auto movelist = hasMovelist ? ReadTkdataFile(tkdata, listName) : std::vector<Byte>{};
    Check(image.size() >= DataRoot && !memcmp(image.data() + 4, "HID", 3), "Not a Tekken 7 HID moveset");
    // Training move list: on disk, text offsets are relative to the string block and input sequences are indexes into the
    // input array; in memory both are byte offsets relative to their own row (what the exporter reads).
    if (hasMovelist) {
        using namespace StructsT7;
        Check(movelist.size() >= sizeof(MvlHead) && !memcmp(movelist.data(), "MVLT", 4), "Invalid T7 move list");
        auto head = Get<MvlHead>(movelist, 0);
        // _unk0x4 low half: 5 on disk, 1000 in memory (Jin, Kazuya, Anna); the high half is kept.
        if ((head._unk0x4 & 0xffff) == 5) { head._unk0x4 = (head._unk0x4 & ~0xffff) | 1000; Put(movelist, 0, head); }
        constexpr int64_t strings = offsetof(MvlHead, translation_strings);
        for (int32_t i = 0; i < head.displayables_count; ++i) {
            const uint64_t row = head.displayables_offset + uint64_t(i) * sizeof(MvlDisplayable);
            auto item = Get<MvlDisplayable>(movelist, row);
            for (auto& offset : item.all_translation_offsets) offset = static_cast<int32_t>(strings + offset - static_cast<int64_t>(row));
            Put(movelist, row, item);
        }
        for (int32_t i = 0; i < head.playables_count; ++i) {
            const uint64_t row = head.playables_offset + uint64_t(i) * sizeof(MvlPlayable);
            auto item = Get<MvlPlayable>(movelist, row);
            if (item.input_count) item.input_sequence_offset = static_cast<uint32_t>(head.inputs_offset + uint64_t(item.input_sequence_offset) * sizeof(MvlInput) - row);
            Put(movelist, row, item);
        }
    }
    auto at = [&](uint64_t relative) { Check(relative <= image.size() - DataRoot, "T7 offset is outside the file"); return Root + DataRoot + relative; };
    // Header (MovesetInfo): strings, the 19 table pointers and the 12 MOTA slots become absolute.
    for (auto field : {offsetof(G::MovesetInfo, character_name_addr), offsetof(G::MovesetInfo, character_creator_addr),
                       offsetof(G::MovesetInfo, date_addr), offsetof(G::MovesetInfo, fulldate_addr)})
        Put<uint64_t>(image, field, at(Get<uint64_t>(image, field)));
    std::array<std::pair<gameAddr, uint64_t>, 19> tables{}; // address, count
    for (size_t i = 0; i < 19; ++i) {
        const auto field = offsetof(G::MovesetInfo, table) + i * 16;
        tables[i] = {at(Get<uint64_t>(image, field)), Get<uint64_t>(image, field + 8)};
        Put<uint64_t>(image, field, tables[i].first);
    }
    // MOTA slots: even ones live in the motbin, odd ones are <code>rotsm.arc's entries 0..5 (loaded beside it).
    const auto arcCount = Get<uint32_t>(arc, 0);
    Check(arcCount <= 6, "Unexpected MOTA archive layout");
    std::array<gameAddr, 12> slots{};
    for (size_t slot = 0; slot < 12; ++slot) {
        const auto field = offsetof(G::MovesetInfo, motas) + slot * 8;
        if (slot % 2 == 0) { const auto v = Get<uint64_t>(image, field); slots[slot] = v ? at(v) : 0; }
        else if (slot / 2 < arcCount) slots[slot] = Arc + Get<uint32_t>(arc, 4 + 8 * (slot / 2));
        Put<uint64_t>(image, field, slots[slot]);
    }
    Put<uint64_t>(image, offsetof(G::MovesetInfo, motas) + 12 * 8, 0);
    Put<uint8_t>(image, offsetof(G::MovesetInfo, isInitialized), 1);
    // Record references: index -> address in its table (-1 = none).
    auto record = [&](size_t table, uint64_t row) { return tables[table].first - Root + row * T7Export::RecordSizes[table]; }; // image offset
    auto ref = [&](uint64_t field, size_t target) {
        const auto v = Get<uint64_t>(image, field);
        if (v == UINT64_MAX) { Put<uint64_t>(image, field, 0); return; }
        Check(v < tables[target].second, "T7 record reference is outside its table");
        Put<uint64_t>(image, field, tables[target].first + v * T7Export::RecordSizes[target]);
    };
    enum { Reactions, Requirement, HitCondition, Projectile, Pushback, PushbackExtra, Cancel, GroupCancel, CancelExtra, ExtraProp,
           StartProp, EndProp, Move, Voiceclip, InputSequence, Input, Parry, CameraData, ThrowCamera };
    // Animations: flag 0x8000 = common.arc, 0x4000 = MOTA slot 1 (rotsm.arc entry 0), none = MOTA slot 0 (in the motbin).
    const auto shared = MotaAnimations(common, 0, Common);
    const auto slot1 = slots[1] ? MotaAnimations(arc, slots[1] - Arc, Arc) : std::vector<uint64_t>{};
    const auto slot0 = slots[0] ? MotaAnimations(image, slots[0] - Root, Root) : std::vector<uint64_t>{};
    std::map<gameAddr, int> used;
    auto frames = [&](gameAddr address) -> int32_t { // frame count of a disk animation (big-endian 0x64 / 0xC8 header)
        const std::vector<Byte>& bytes = address >= Common ? common : address >= Arc ? arc : image;
        const auto offset = address - (address >= Common ? Common : address >= Arc ? Arc : Root);
        const auto type = _byteswap_ushort(Get<uint16_t>(bytes, offset)), bones = _byteswap_ushort(Get<uint16_t>(bytes, offset + 2));
        if (type == 0xC8) return static_cast<int32_t>(_byteswap_ulong(Get<uint32_t>(bytes, offset + 4)));
        Check(type == 0x64, "Unknown T7 animation type");
        return _byteswap_ushort(Get<uint16_t>(bytes, offset + 4 + bones * 2ULL));
    };
    for (uint64_t i = 0; i < tables[Move].second; ++i) {
        const auto m = record(Move, i);
        for (auto field : {offsetof(G::Move, name_addr), offsetof(G::Move, anim_name_addr)}) Put<uint64_t>(image, m + field, at(Get<uint64_t>(image, m + field)));
        const auto key = Get<uint64_t>(image, m + offsetof(G::Move, anim_addr));
        const auto& list = (key & 0x8000) ? shared : (key & 0x4000) ? slot1 : slot0;
        const auto index = key & ((key & 0xC000) ? 0x3fff : UINT64_MAX);
        Check(index < list.size(), "T7 move animation key is outside its MOTA");
        Put<uint64_t>(image, m + offsetof(G::Move, anim_addr), list[index]); used[list[index]] = 1;
        // The game's loader (T7 relocate, cancel loop at 0x140715250) fills the length: the animation's frame count.
        Put<int32_t>(image, m + offsetof(G::Move, anim_len), frames(list[index]));
        ref(m + offsetof(G::Move, cancel_addr), Cancel);
        // Secondary cancel lists exist only when their gate is at least 1 (as in T8's relocate).
        for (auto [field, gate] : {std::pair<size_t, size_t>{offsetof(G::Move, _0x28_cancel_addr), offsetof(G::Move, _0x30_int__0x28_related)},
                                   {offsetof(G::Move, _0x38_cancel_addr), offsetof(G::Move, _0x40_int__0x38_related)},
                                   {offsetof(G::Move, _0x48_cancel_addr), offsetof(G::Move, _0x50_int__0x48_related)}})
            if (Get<int32_t>(image, m + gate) < 1) Put<uint64_t>(image, m + field, 0); else ref(m + field, Cancel);
        ref(m + offsetof(G::Move, hit_condition_addr), HitCondition); ref(m + offsetof(G::Move, voicelip_addr), Voiceclip);
        ref(m + offsetof(G::Move, extra_move_property_addr), ExtraProp);
        ref(m + offsetof(G::Move, move_start_extraprop_addr), StartProp); ref(m + offsetof(G::Move, move_end_extraprop_addr), EndProp);
    }
    for (uint64_t i = 0; i < tables[Projectile].second; ++i) {
        const auto r = record(Projectile, i);
        for (auto [field, table] : {std::pair<size_t, int>{offsetof(G::Projectile, cancel_addr), Cancel}, {offsetof(G::Projectile, hit_condition_addr), HitCondition}})
            if (Get<uint64_t>(image, r + field)) ref(r + field, table); // 0 = none on disk (memory: null)
        if (!Get<int32_t>(image, r + 0x78)) Put<int32_t>(image, r + 0x78, Get<int32_t>(image, r + 0x30) * 2);
        if (!Get<int32_t>(image, r + 0x7c)) Put<int32_t>(image, r + 0x7c, Get<int32_t>(image, r + 0x34));
    }
    // Cancel option bits, computed by the game at load (T7 relocate 0x140715250 / group loop 0x140715390; helpers
    // 0x1406FD3B0, 0x1406FD3E0, 0x1406FD7E0, 0x1406FD6A0 with the id table 0x1434416F0). Requirement lists end at 881.
    constexpr uint32_t NoRoute[] = {0x22e, 0x22f, 0x27a, 0x32b, 0x27c};
    for (auto table : {Cancel, GroupCancel})
        for (uint64_t i = 0; i < tables[table].second; ++i) {
            const auto c = record(table, i);
            std::vector<uint32_t> ids;
            for (auto row = Get<uint64_t>(image, c + offsetof(G::Cancel, requirements_addr)); row < tables[Requirement].second; ++row) {
                const auto id = Get<uint32_t>(image, record(Requirement, row));
                if (id == 881) break;
                ids.push_back(id);
            }
            auto has = [&](uint32_t id) { return std::find(ids.begin(), ids.end(), id) != ids.end(); };
            const bool command0 = Get<uint64_t>(image, c) == 0;
            uint16_t option = command0 ? 1 : 0;
            if (std::any_of(ids.begin(), ids.end(), [](uint32_t id) { return id >= 0x32c && !(id & 0x8000); })) option |= 2;
            if (std::any_of(ids.begin(), ids.end(), [](uint32_t id) { return id >= 0x349 && !(id & 0x8000); })) option |= 4;
            if (has(0x84ce)) option |= 8;
            if ((!command0 || has(0x83) || has(0x84) || has(0x85)) && std::none_of(std::begin(NoRoute), std::end(NoRoute), has)) option |= 0x10;
            const auto extraIndex = Get<uint64_t>(image, c + offsetof(G::Cancel, extradata_addr));
            Check(extraIndex < tables[CancelExtra].second, "T7 cancel extradata is outside its table");
            const auto extra = Get<uint32_t>(image, record(CancelExtra, extraIndex));
            if ((extra & 0x3c00) != 0x2800) option |= 0x40;
            if ((extra & 0x3f) == 0x16) option |= 0x80;
            const auto move = Get<uint16_t>(image, c + offsetof(G::Cancel, move_id));
            if (table == Cancel && (move & 0x8000) && (move == 0x8024 || move <= 0x801d || move >= 0x8028)) option |= 0x100;
            Put<uint16_t>(image, c + offsetof(G::Cancel, cancel_option), option);
            ref(record(table, i) + offsetof(G::Cancel, requirements_addr), Requirement);
            ref(record(table, i) + offsetof(G::Cancel, extradata_addr), CancelExtra);
        }
    for (uint64_t i = 0; i < tables[Reactions].second; ++i)
        for (size_t k = 0; k < std::size(G::Reactions{}.pushbacks); ++k) ref(record(Reactions, i) + offsetof(G::Reactions, pushbacks) + k * 8, Pushback);
    for (uint64_t i = 0; i < tables[InputSequence].second; ++i)
        if (Get<uint16_t>(image, record(InputSequence, i) + offsetof(G::InputSequence, input_amount))) ref(record(InputSequence, i) + offsetof(G::InputSequence, input_addr), Input);
    for (uint64_t i = 0; i < tables[ThrowCamera].second; ++i) ref(record(ThrowCamera, i) + offsetof(G::ThrowCamera, cameradata_addr), CameraData);
    for (uint64_t i = 0; i < tables[HitCondition].second; ++i) {
        ref(record(HitCondition, i) + offsetof(G::HitCondition, requirements_addr), Requirement);
        ref(record(HitCondition, i) + offsetof(G::HitCondition, reactions_addr), Reactions);
    }
    for (uint64_t i = 0; i < tables[Pushback].second; ++i) ref(record(Pushback, i) + offsetof(G::Pushback, extradata_addr), PushbackExtra);
    for (auto table : {StartProp, EndProp})
        for (uint64_t i = 0; i < tables[table].second; ++i) ref(record(table, i) + offsetof(G::OtherMoveProperty, requirements_addr), Requirement);
    // Character id: the moves' moveId_val2 (the memory export's header id on Jin, Kazuya and Anna).
    std::map<uint16_t, uint64_t> ids;
    for (uint64_t i = 0; i < tables[Move].second; ++i) ++ids[Get<uint16_t>(image, record(Move, i) + offsetof(G::Move, moveId_val2))];
    const auto characterId = std::max_element(ids.begin(), ids.end(), [](auto& a, auto& b) { return a.second < b.second; })->first;
    ImageProcess process;
    process.Add(Root, std::move(image)); process.Add(Arc, std::move(arc)); process.Add(Common, common); if (hasMovelist) process.Add(Movelist, std::move(movelist));
    ExtractSettings settings = ExtractSettings_DisplayableMovelist;
    for (int i = 0; i < 12; ++i) settings |= ExtractSettings(1) << i;
    auto snapshot = T7Export::ReadSnapshot(process, Root, settings, hasMovelist ? Movelist : 0);
    TKMovesetHeader header;
    const auto name = DisplayName(snapshot.characterName);
    header.flags = 0; header.game_specific_flags = 0;
    header.gameId = GameId_T7; header.minorVersion = GameVersions::T7::NONE; header.characterId = characterId;
    memset(header.gameVersionHash, 0, sizeof(header.gameVersionHash));
    strcpy_s(header.version_string, sizeof(header.version_string), MOVESET_VERSION_STRING);
    strncpy_s(header.origin, sizeof(header.origin), "Tekken 7", _TRUNCATE);
    strncpy_s(header.target_character, sizeof(header.target_character), name.c_str(), _TRUNCATE);
    strncpy_s(header.orig_character_name, sizeof(header.orig_character_name), name.c_str(), _TRUNCATE);
    header.date = header.extraction_date = Helpers::getCurrentTimestamp();
    if (report) *report = {name, characterId, tables[Move].second, used.size()};
    return T7Export::Serialize(snapshot, header);
}

std::string FileName(const std::string& displayName) { return "T7_" + displayName + ".tkmvst"; }

Report WriteMovesetFile(const std::filesystem::path& tkdata, const std::string& code, const std::vector<Byte>& common, const std::filesystem::path& directory,
    TKMovesetCompressionType_ codec) {
    Report report;
    const auto file = BuildFile(tkdata, code, common, &report);
    report.path = directory / Helpers::string_to_wstring(FileName(report.name));
    // Two codes can share a display name (jac/ja4 "Jack", hei/yhe "Heihachi"): when the file already there holds another
    // character, this one is written as "T7_<name> (<code>).tkmvst" instead of being skipped.
    if (std::filesystem::exists(report.path) && file.size() >= sizeof(TKMovesetHeader)) {
        const auto& ours = *reinterpret_cast<const TKMovesetHeader*>(file.data());
        TKMovesetHeader theirs{};
        std::ifstream existing(report.path, std::ios::binary);
        const bool read = existing.read(reinterpret_cast<char*>(&theirs), sizeof(theirs)).good();
        if (!read || theirs.gameId != ours.gameId || theirs.characterId != ours.characterId)
            report.path = directory / Helpers::string_to_wstring(FileName(report.name + " (" + code + ")"));
    }
    if (std::filesystem::exists(report.path)) { report.existed = true; return report; }
    const auto& path = report.path;
    uint64_t size = 0;
    std::unique_ptr<Byte[]> compressed(CompressionUtils::RAW::Moveset::Compress(file.data(), file.size(), codec, size));
    if (!compressed) throw std::runtime_error("Moveset compression failed");
    if (!FileIO::WriteAtomically(path, {compressed.get(), static_cast<size_t>(size)})) throw std::runtime_error("Cannot write the moveset file");
    return report;
}
}

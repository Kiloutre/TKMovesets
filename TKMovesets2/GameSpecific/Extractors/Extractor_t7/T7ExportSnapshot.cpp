#include "ImportProfile.hpp"
#include "T7ExportSnapshot.hpp"
#include "Animations.hpp"
#include "AnimationReader.hpp"
#include "Importer_t7/Importer_t7_Utils.hpp"
#include "constants.h"
#include "Helpers.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <map>
#include <span>
#include <set>
#include <stdexcept>

namespace T7Export {
namespace {
namespace G = StructsT7_gameAddr;
using namespace StructsT7;
constexpr uint64_t Limit = 256ULL << 20;
// The Python importer allocated exactly 0x2e0: do not read the unknown thirteenth MOTA pointer.
constexpr size_t SourceInfoSize = offsetof(G::MovesetInfo, motas) + 12 * sizeof(gameAddr);
static_assert(SourceInfoSize == 0x2e0);
size_t Aligned(size_t value) { return (value + 7) & ~size_t(7); }
gameAddr Add(gameAddr address, uint64_t offset) {
    if (offset > UINT64_MAX - address) throw std::runtime_error("Remote address overflow");
    return address + offset;
}
gameAddr Relative(gameAddr address, int32_t offset) {
    if (offset >= 0) return Add(address, static_cast<uint32_t>(offset));
    auto distance = static_cast<uint64_t>(-static_cast<int64_t>(offset));
    if (distance > address) throw std::runtime_error("Remote relative address underflow");
    return address - distance;
}
bool Missing(gameAddr address) { return address == 0 || address == UINT64_MAX; }
bool ValidMotaAnimations(const std::vector<Byte>& bytes) {
    TAnimUtils::AnimationReader reader(bytes);
    const auto count = reader.Read<uint32_t>(12, bytes[5] == 1);
    std::set<uint32_t> offsets;
    for (uint32_t i = 0; i < count; ++i) offsets.insert(reader.Read<uint32_t>(20 + i * 4ULL, bytes[5] == 1));
    for (auto it = offsets.begin(); it != offsets.end(); ++it) {
        const auto following = std::next(it);
        const auto end = following == offsets.end() ? bytes.size() : *following;
        if (*it >= end || end > bytes.size() ||
            !TAnimUtils::FromMemory::Safe::GetAnimSize(bytes.data() + *it, end - *it)) return false;
    }
    return true;
}

struct Budget {
    size_t used = 0;
    void Take(uint64_t bytes) {
        if (bytes > Limit - used) throw std::runtime_error("Export exceeds the 256 MiB budget");
        used += static_cast<size_t>(bytes);
    }
};
// Cache small animation-header/track probes: one RPM per scalar/channel would
// turn a few thousand animations into hundreds of thousands of system calls.
class AnimationProbe {
    const GameProcess& process;
    gameAddr address;
    std::array<Byte, 4096> cache{};
    size_t start = 0, length = 0;
    DWORD pageSize;
public:
    AnimationProbe(const GameProcess& value, gameAddr base) : process(value), address(base) {
        if (Missing(base)) throw std::runtime_error("Missing animation pointer");
        SYSTEM_INFO system; GetSystemInfo(&system); pageSize = system.dwPageSize;
    }
    size_t Size() const { return static_cast<size_t>((std::min)(Limit, UINT64_MAX - address)); }
    void Require(size_t offset, size_t count) const {
        if (offset > Size() || count > Size() - offset) throw std::runtime_error("Animation probe exceeds export budget");
    }
    template<class T> T Read(size_t offset, bool swapped = false) {
        Require(offset, sizeof(T));
        if (offset < start || offset - start > length || sizeof(T) > length - (offset - start)) {
            start = offset;
            auto cursor = Add(address, offset);
            length = (std::min)({cache.size(), Size() - offset, size_t(pageSize - cursor % pageSize)});
            // Exact-size test providers and partially readable allocations can reject look-ahead.
            while (!process.readBytes(cursor, cache.data(), length)) {
                if (length <= sizeof(T)) {
                    length = sizeof(T);
                    if (!process.readBytes(cursor, cache.data(), length)) throw std::runtime_error("Unreadable animation metadata");
                    break;
                }
                length = (std::max)(sizeof(T), length / 2);
            }
            if (length < sizeof(T)) {
                length = sizeof(T);
                if (!process.readBytes(cursor, cache.data(), length)) throw std::runtime_error("Unreadable animation metadata");
            }
        }
        T value; memcpy(&value, cache.data() + offset - start, sizeof(value));
        if (swapped) { auto bytes = reinterpret_cast<Byte*>(&value); std::reverse(bytes, bytes + sizeof(value)); }
        return value;
    }
};
class Reader {
    const GameProcess& process;
public:
    explicit Reader(const GameProcess& value) : process(value) {}
    void Read(gameAddr address, void* data, size_t size) const {
        if (Missing(address) || size > UINT64_MAX - address || !process.readBytes(address, data, size))
            throw std::runtime_error("Unreadable moveset region at " + std::to_string(address));
    }
    std::vector<Byte> Bytes(gameAddr address, size_t size) const {
        if (size > Limit) throw std::runtime_error("Remote region exceeds export budget");
        std::vector<Byte> result(size);
        if (size) Read(address, result.data(), size);
        return result;
    }
    std::string String(gameAddr address, size_t remaining) const {
        if (Missing(address)) throw std::runtime_error("Missing moveset string");
        std::string result;
        // Never cross a Windows page boundary just to look ahead for a terminator.
        // Fall back for exact-size read providers or partially readable regions.
        SYSTEM_INFO system; GetSystemInfo(&system);
        while (result.size() < remaining) {
            auto cursor = Add(address, result.size());
            size_t count = (std::min)({size_t(256), size_t(system.dwPageSize - cursor % system.dwPageSize), remaining - result.size()});
            std::array<char, 256> buffer;
            while (!process.readBytes(cursor, buffer.data(), count)) {
                if (count == 1) throw std::runtime_error("Unreadable moveset string");
                count /= 2;
            }
            auto end = static_cast<const char*>(memchr(buffer.data(), 0, count));
            result.append(buffer.data(), end ? end - buffer.data() : count);
            if (end) return result;
        }
        throw std::runtime_error("Unterminated moveset string exceeds export budget");
    }
    uint64_t AnimationSize(gameAddr address) const {
        AnimationProbe probe(process, address);
        try {
            const uint64_t size = TAnimUtils::ReadAnimationLayoutFrom(probe, false).size;
            if (!size || size > Limit) throw std::runtime_error("Invalid remote animation size");
            return size;
        } catch (const std::exception& error) {
            throw std::runtime_error("Invalid animation at " + std::to_string(address) + ": " + error.what());
        }
    }
};
class Strings {
    const Reader& reader;
    Budget& budget;
    std::map<gameAddr, size_t> offsets;
    std::map<gameAddr, std::string> source;
public:
    std::vector<Byte> bytes;
    Strings(const Reader& read, Budget& limit) : reader(read), budget(limit) {}
    const std::string& Source(gameAddr address) {
        auto found = source.find(address);
        if (found != source.end()) return found->second;
        auto value = reader.String(address, Limit - budget.used);
        return source.emplace(address, std::move(value)).first->second;
    }
    size_t Text(const std::string& value) {
        budget.Take(value.size() + 1);
        auto offset = bytes.size();
        bytes.insert(bytes.end(), value.begin(), value.end()); bytes.push_back(0);
        return offset;
    }
    size_t AddString(gameAddr address) {
        auto found = offsets.find(address);
        if (found != offsets.end()) return found->second;
        auto offset = Text(Source(address));
        offsets.emplace(address, offset);
        return offset;
    }
};
template<class T> std::span<T> Records(std::vector<Byte>& bytes, uint64_t start, uint64_t count) {
    return {reinterpret_cast<T*>(bytes.data() + start), static_cast<size_t>(count)};
}
uint64_t Index(gameAddr pointer, gameAddr start, uint64_t count, size_t stride) {
    if (Missing(pointer)) return UINT64_MAX;
    if (Missing(start) || pointer < start || (pointer - start) % stride || (pointer - start) / stride >= count)
        throw std::runtime_error("Record reference does not point to an entry in its target list");
    return (pointer - start) / stride;
}
std::vector<Byte> ReadTraining(const Reader& reader, gameAddr address, Budget& budget) {
    if (!address) { budget.Take(8); return std::vector<Byte>(8); }
    MvlHead head{};
    constexpr size_t prefix = offsetof(MvlHead, translation_strings);
    reader.Read(address, &head, prefix);
    if (memcmp(head.mvlString, "MVLT", 4) || head.displayables_count < 0 || head.playables_count < 0)
        throw std::runtime_error("Invalid training movelist header");
    const uint64_t rowBytes = uint64_t(head.displayables_count) * sizeof(MvlDisplayable);
    const uint64_t playableBytes = uint64_t(head.playables_count) * sizeof(MvlPlayable);
    budget.Take(prefix); budget.Take(rowBytes); budget.Take(playableBytes);
    auto rowAddress = Add(address, head.displayables_offset), playableAddress = Add(address, head.playables_offset);
    auto rows = reader.Bytes(rowAddress, static_cast<size_t>(rowBytes));
    auto playables = reader.Bytes(playableAddress, static_cast<size_t>(playableBytes));
    Strings strings(reader, budget);
    for (size_t i = 0; i < static_cast<size_t>(head.displayables_count); ++i) {
        auto& row = reinterpret_cast<MvlDisplayable*>(rows.data())[i];
        for (auto& offset : row.all_translation_offsets)
            if (offset) offset = static_cast<int32_t>(prefix + strings.AddString(Relative(Add(rowAddress, i * sizeof(row)), offset)));
    }
    std::vector<Byte> inputs;
    struct Sequence { gameAddr address; uint64_t count, index; };
    std::vector<Sequence> sequences;
    for (size_t i = 0; i < static_cast<size_t>(head.playables_count); ++i) {
        auto& row = reinterpret_cast<MvlPlayable*>(playables.data())[i];
        if (!row.input_count) { row.input_sequence_offset = 0; continue; }
        auto start = Add(Add(playableAddress, i * sizeof(row)), row.input_sequence_offset);
        auto bytes = uint64_t(row.input_count) * sizeof(MvlInput);
        auto found = std::find_if(sequences.begin(), sequences.end(), [&](const auto& entry) {
            return start >= entry.address && (start - entry.address) % sizeof(MvlInput) == 0 &&
                start - entry.address <= entry.count * sizeof(MvlInput) &&
                bytes <= entry.count * sizeof(MvlInput) - (start - entry.address);
        });
        if (found != sequences.end()) {
            row.input_sequence_offset = static_cast<uint32_t>(found->index + (start - found->address) / sizeof(MvlInput));
        } else {
            budget.Take(bytes);
            auto data = reader.Bytes(start, static_cast<size_t>(bytes));
            auto index = inputs.size() / sizeof(MvlInput);
            sequences.push_back({start, row.input_count, index});
            row.input_sequence_offset = static_cast<uint32_t>(index);
            inputs.insert(inputs.end(), data.begin(), data.end());
        }
    }
    auto displayablesOffset = Aligned(prefix + strings.bytes.size());
    budget.Take(displayablesOffset - prefix - strings.bytes.size());
    head.displayables_offset = static_cast<uint32_t>(displayablesOffset);
    head.playables_offset = static_cast<uint32_t>(displayablesOffset + rows.size());
    head.inputs_offset = static_cast<uint32_t>(head.playables_offset + playables.size());
    std::vector<Byte> result(head.inputs_offset + inputs.size());
    memcpy(result.data(), &head, prefix);
    if (!strings.bytes.empty()) memcpy(result.data() + prefix, strings.bytes.data(), strings.bytes.size());
    if (!rows.empty()) memcpy(result.data() + head.displayables_offset, rows.data(), rows.size());
    if (!playables.empty()) memcpy(result.data() + head.playables_offset, playables.data(), playables.size());
    if (!inputs.empty()) memcpy(result.data() + head.inputs_offset, inputs.data(), inputs.size());
    MvlHead check{}; reader.Read(address, &check, prefix);
    // Compare source header fields rather than our rewritten destination offsets.
    if (check.displayables_count != head.displayables_count || check.playables_count != head.playables_count ||
        Add(address, check.displayables_offset) != rowAddress || Add(address, check.playables_offset) != playableAddress)
        throw std::runtime_error("Training movelist changed during export");
    return result;
}
}
std::string ReadCharacterName(const GameProcess& process, gameAddr address) {
    Reader reader(process);
    gameAddr name = 0;
    reader.Read(Add(address, offsetof(G::MovesetInfo, character_name_addr)), &name, sizeof(name));
    return reader.String(name, Limit);
}
Snapshot ReadSnapshot(const GameProcess& process, gameAddr address, ExtractSettings settings, gameAddr training) {
    Reader reader(process); Budget budget; budget.Take(sizeof(G::MovesetInfo));
    G::MovesetInfo source{};
    reader.Read(address, &source, SourceInfoSize);
    const auto remote = source.table;
    if (!remote.moveCount) throw std::runtime_error("Moveset has no moves");
    G::MovesetTable table = remote;
    std::vector<Byte> records;
    for (size_t i = 0; i < RecordSizes.size(); ++i) {
        const auto& entry = remote.entries[i];
        if (entry.listCount > Limit / RecordSizes[i]) throw std::runtime_error("Record count exceeds export budget");
        const auto bytes = entry.listCount * RecordSizes[i];
        const auto start = Aligned(records.size());
        budget.Take(start - records.size()); budget.Take(bytes);
        records.resize(start + bytes);
        table.entries[i].listAddr = start;
        if (bytes) reader.Read(entry.listAddr, records.data() + start, static_cast<size_t>(bytes));
    }
    Strings names(reader, budget);
    Snapshot result;
    result.characterName = names.Source(reinterpret_cast<gameAddr>(source.character_name_addr));
    std::string marked = result.characterName;
    if (!marked.starts_with(MOVESET_EXTRACTED_NAME_PREFIX)) {
        if (marked.starts_with("[")) marked.erase(0, 1);
        marked.insert(0, MOVESET_EXTRACTED_NAME_PREFIX);
    }
    const auto characterOffset = names.Text(marked);
    auto metadata = [&](char* pointer) -> uint64_t { return Missing(reinterpret_cast<gameAddr>(pointer)) ? UINT64_MAX : names.AddString(reinterpret_cast<gameAddr>(pointer)); };
    const auto creator = metadata(source.character_creator_addr), date = metadata(source.date_addr), fullDate = metadata(source.fulldate_addr);
    std::vector<Byte> animations;
    std::map<gameAddr, size_t> animationOffsets;
    for (auto& move : Records<G::Move>(records, table.move, table.moveCount)) {
        move.name_addr = names.AddString(move.name_addr);
        move.anim_name_addr = names.AddString(move.anim_name_addr);
        auto found = animationOffsets.find(move.anim_addr);
        if (found == animationOffsets.end()) {
            auto size = reader.AnimationSize(move.anim_addr); budget.Take(size);
            auto bytes = reader.Bytes(move.anim_addr, static_cast<size_t>(size));
            if (!TAnimUtils::FromMemory::Safe::GetAnimSize(bytes.data(), bytes.size())) throw std::runtime_error("Animation changed during export");
            if (!TAnimUtils::FromMemory::IsLittleEndian(bytes.data()) && !TAnimUtils::FromMemory::ByteswapAnimation(bytes.data(), bytes.size()))
                throw std::runtime_error("Animation byte-order conversion failed");
            auto offset = animations.size(); animationOffsets.emplace(move.anim_addr, offset);
            animations.insert(animations.end(), bytes.begin(), bytes.end());
            move.anim_addr = offset;
        } else move.anim_addr = found->second;
#define REF(field, target, Type) field = Index(field, remote.target, remote.target##Count, sizeof(Type))
        REF(move.cancel_addr, cancel, Cancel); REF(move._0x28_cancel_addr, cancel, Cancel);
        REF(move._0x38_cancel_addr, cancel, Cancel); REF(move._0x48_cancel_addr, cancel, Cancel);
        REF(move.hit_condition_addr, hitCondition, HitCondition); REF(move.voicelip_addr, voiceclip, Voiceclip);
        REF(move.extra_move_property_addr, extraMoveProperty, ExtraMoveProperty);
        REF(move.move_start_extraprop_addr, moveBeginningProp, OtherMoveProperty);
        REF(move.move_end_extraprop_addr, moveEndingProp, OtherMoveProperty);
    }
    for (auto& item : Records<G::Projectile>(records, table.projectile, table.projectileCount)) {
        REF(item.cancel_addr, cancel, Cancel); REF(item.hit_condition_addr, hitCondition, HitCondition);
    }
    for (auto list : {Records<G::Cancel>(records, table.cancel, table.cancelCount), Records<G::Cancel>(records, table.groupCancel, table.groupCancelCount)})
        for (auto& item : list) { REF(item.requirements_addr, requirement, Requirement); REF(item.extradata_addr, cancelExtradata, CancelExtradata); }
    for (auto& item : Records<G::Reactions>(records, table.reactions, table.reactionsCount))
        for (auto& pointer : item.pushbacks) { REF(pointer, pushback, Pushback); }
    for (auto& item : Records<G::InputSequence>(records, table.inputSequence, table.inputSequenceCount)) {
        if (!item.input_amount) item.input_addr = UINT64_MAX;
        else { REF(item.input_addr, input, Input); }
    }
    for (auto& item : Records<G::ThrowCamera>(records, table.throwCameras, table.throwCamerasCount)) { REF(item.cameradata_addr, cameraData, CameraData); }
    for (auto& item : Records<G::HitCondition>(records, table.hitCondition, table.hitConditionCount)) {
        REF(item.requirements_addr, requirement, Requirement); REF(item.reactions_addr, reactions, Reactions);
    }
    for (auto& item : Records<G::Pushback>(records, table.pushback, table.pushbackCount)) { REF(item.extradata_addr, pushbackExtradata, PushbackExtradata); }
    for (auto list : {Records<G::OtherMoveProperty>(records, table.moveBeginningProp, table.moveBeginningPropCount),
        Records<G::OtherMoveProperty>(records, table.moveEndingProp, table.moveEndingPropCount)})
        for (auto& item : list) { REF(item.requirements_addr, requirement, Requirement); }
#undef REF
    G::MotaList motas; memset(&motas, 0xff, sizeof(motas));
    std::vector<Byte> motaBytes;
    std::map<gameAddr, size_t> motaOffsets;
    std::set<gameAddr> discardedMotas;
    for (size_t i = 0; i < 12; ++i) {
        auto pointer = source.motas.motas[i];
        if (!(settings & (uint64_t(1) << i)) || Missing(pointer)) continue;
        if (discardedMotas.contains(pointer)) { result.discardedMotas.push_back(static_cast<uint8_t>(i)); continue; }
        if (auto found = motaOffsets.find(pointer); found != motaOffsets.end()) { motas.motas[i] = found->second; continue; }
        auto header = reader.Bytes(pointer, 20);
        if (memcmp(header.data(), "MOTA", 4)) {
            if (settings & ExtractSettings_UnknownMotas) throw std::runtime_error("Opaque MOTA has no safe length; cannot infer it from adjacent pointers");
            continue;
        }
        TAnimUtils::AnimationReader input(header); const bool swapped = header[5] == 1;
        const auto count = input.Read<uint32_t>(12, swapped);
        if (count > (Limit - 20) / 4) throw std::runtime_error("MOTA count exceeds export budget");
        auto offsets = reader.Bytes(Add(pointer, 20), static_cast<size_t>(count) * 4);
        uint64_t last = 0;
        TAnimUtils::AnimationReader entries(offsets);
        for (uint32_t n = 0; n < count; ++n) {
            auto offset = entries.Read<uint32_t>(n * 4ULL, swapped);
            if (offset < 20ULL + count * 4ULL || offset >= Limit) throw std::runtime_error("Invalid MOTA animation offset");
            last = (std::max)(last, uint64_t(offset));
        }
        const auto size = count ? last + reader.AnimationSize(Add(pointer, last)) : 20;
        budget.Take(size);
        auto bytes = reader.Bytes(pointer, static_cast<size_t>(size));
        if (!ValidMotaAnimations(bytes)) {
            discardedMotas.insert(pointer);
            result.discardedMotas.push_back(static_cast<uint8_t>(i));
            continue;
        }
        if (swapped && !TAnimUtils::FromMemory::ByteswapMota(bytes.data(), bytes.size())) throw std::runtime_error("Invalid big-endian MOTA");
        motas.motas[i] = motaBytes.size(); motaOffsets.emplace(pointer, motaBytes.size());
        motaBytes.insert(motaBytes.end(), bytes.begin(), bytes.end());
    }
    auto trainingBytes = ReadTraining(reader, (settings & ExtractSettings_DisplayableMovelist) ? training : 0, budget);
    auto& blocks = result.blocks;
    blocks.tableBlock = offsetof(G::MovesetInfo, table); blocks.motalistsBlock = offsetof(G::MovesetInfo, motas);
    blocks.nameBlock = sizeof(G::MovesetInfo);
    blocks.movesetBlock = Aligned(blocks.nameBlock + names.bytes.size());
    blocks.animationBlock = Aligned(blocks.movesetBlock + records.size());
    blocks.motaBlock = Aligned(blocks.animationBlock + animations.size());
    blocks.movelistBlock = Aligned(blocks.motaBlock + motaBytes.size());
    auto size = blocks.movelistBlock + trainingBytes.size();
    if (size > Limit) throw std::runtime_error("Export exceeds file budget after alignment");
    result.payload.resize(size);
    auto info = source; info.table = table; info.motas = motas;
    info.character_name_addr = reinterpret_cast<char*>(blocks.nameBlock + characterOffset);
    info.character_creator_addr = creator == UINT64_MAX ? nullptr : reinterpret_cast<char*>(blocks.nameBlock + creator);
    info.date_addr = date == UINT64_MAX ? nullptr : reinterpret_cast<char*>(blocks.nameBlock + date);
    info.fulldate_addr = fullDate == UINT64_MAX ? nullptr : reinterpret_cast<char*>(blocks.nameBlock + fullDate);
    memcpy(result.payload.data(), &info, sizeof(info));
    auto copy = [&](size_t offset, const auto& bytes) { if (!bytes.empty()) memcpy(result.payload.data() + offset, bytes.data(), bytes.size()); };
    copy(blocks.nameBlock, names.bytes); copy(blocks.movesetBlock, records); copy(blocks.animationBlock, animations);
    copy(blocks.motaBlock, motaBytes); copy(blocks.movelistBlock, trainingBytes);
    std::string error;
    if (!T7ImportUtils::ValidateT7Semantics(blocks, result.payload.data(), result.payload.size(), &error))
        throw std::runtime_error("Invalid extracted moveset: " + error);
    G::MovesetInfo check{}; reader.Read(address, &check, SourceInfoSize);
    if (memcmp(&check.table, &source.table, sizeof(source.table)) ||
        memcmp(&check.motas, &source.motas, 12 * sizeof(gameAddr)) ||
        check.character_name_addr != source.character_name_addr || check.character_creator_addr != source.character_creator_addr ||
        check.date_addr != source.date_addr || check.fulldate_addr != source.fulldate_addr ||
        check.isInitialized != source.isInitialized)
        throw std::runtime_error("Moveset pointers or counts changed during export");
    return result;
}
std::vector<Byte> Serialize(const Snapshot& snapshot, TKMovesetHeader header, const std::vector<TKMovesetProperty>* properties) {
    TKMovesetProperty end{.id = TKMovesetProperty_END, .value = 0};
    // Custom property list (the import profile): the given one, or the default profile for a new file.
    auto list = properties ? *properties : ImportProfile::ToProperties({});
    list.push_back(end);
    const auto listBytes = Aligned(list.size() * sizeof(TKMovesetProperty));
    header.header_size = static_cast<uint32_t>(Aligned(sizeof(header)));
    header.block_list = static_cast<uint32_t>(header.header_size + listBytes);
    header.block_list_size = 8;
    header.moveset_data_start = header.block_list + sizeof(snapshot.blocks);
    header.compressionType = TKMovesetCompressionType_None; header.moveset_data_size = 0;
    const auto size = header.moveset_data_start + Aligned(snapshot.payload.size());
    if (size > Limit) throw std::runtime_error("Serialized export exceeds the 256 MiB budget");
    std::vector<Byte> bytes(size);
    memcpy(bytes.data() + header.header_size, list.data(), list.size() * sizeof(TKMovesetProperty));
    memcpy(bytes.data() + header.block_list, &snapshot.blocks, sizeof(snapshot.blocks));
    memcpy(bytes.data() + header.moveset_data_start, snapshot.payload.data(), snapshot.payload.size());
    auto payload = bytes.data() + header.moveset_data_start;
    const auto& b = snapshot.blocks;
    // The checksum identifies the moveset data: it covers an empty property list whatever the import profile says.
    std::vector<std::pair<Byte*, uint64_t>> hashed = {
        {reinterpret_cast<Byte*>(&end), sizeof(end)},
        {payload, b.tableBlock}, {payload + b.tableBlock, b.motalistsBlock - b.tableBlock},
        {payload + b.motalistsBlock, b.nameBlock - b.motalistsBlock},
        {payload + b.movesetBlock, b.animationBlock - b.movesetBlock},
        {payload + b.animationBlock, b.motaBlock - b.animationBlock},
        {payload + b.motaBlock, b.movelistBlock - b.motaBlock}};
    header.orig_crc32 = header.crc32 = Helpers::CalculateCrc32(hashed);
    memcpy(bytes.data(), &header, sizeof(header));
    return bytes;
}

}

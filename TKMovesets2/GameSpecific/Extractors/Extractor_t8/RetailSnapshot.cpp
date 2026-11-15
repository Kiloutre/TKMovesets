#include "ImportProfile.hpp"
#include "RetailSnapshot.hpp"
#include "GameIDs.hpp"
#include "Helpers.hpp"
#include "RetailRuntime.hpp"
#include "RetailNames.hpp"
#include "debug.hpp"
#include <set>
#include <algorithm>
#include <map>
#include <cstring>
#include <stdexcept>

namespace RetailT8Snapshot {
namespace {
constexpr uint64_t Limit = 1024ULL << 20;
constexpr uint64_t TableLimit = 256ULL << 20;
constexpr uint64_t PanmLimit = 32ULL << 20;
size_t Align(size_t value) { return (value + 7) & ~size_t(7); }
void Check(bool value, const char* error) { if (!value) throw std::runtime_error(error); }
template<class T> T Read(std::span<const Byte> bytes, size_t offset) {
    Check(offset <= bytes.size() && sizeof(T) <= bytes.size() - offset, "Truncated retail snapshot field");
    T result; memcpy(&result, bytes.data() + offset, sizeof(result)); return result;
}
void Remote(const GameProcess& process, gameAddr address, void* data, size_t size) {
    Check(address && address != UINT64_MAX && size <= UINT64_MAX - address &&
        process.readBytes(address, data, size), "Cannot read retail moveset region");
}
void CheckSource(std::span<const Byte> source) {
    const auto date = Read<uint32_t>(source, 4);
    Check(date >= 20240101 && date <= 20991231, "Unsupported retail header compilation date");
    Check(Read<uint64_t>(source, 0x238) != 0, "Retail moveset has no moves");
    uint64_t total = sizeof(Header) + SourceHeaderSize + Tables.size() * sizeof(Entry);
    for (const auto& spec : Tables) {
        const auto count = Read<uint64_t>(source, spec.countOffset);
        Check(count <= (TableLimit - total) / spec.stride, "Retail table count exceeds snapshot budget");
        total += count * spec.stride;
        const auto address = Read<uint64_t>(source, spec.pointerOffset);
        Check(!count || (address && address != UINT64_MAX && count * spec.stride <= UINT64_MAX - address),
            "Invalid retail table pointer");
    }
}
}
namespace {
// Validates a T8RX block (it must span to the end of `block`'s first `size` bytes) and optionally decodes it.
size_t ParseExtension(std::span<const Byte> block, NativeData* out) {
    const auto x = Read<ExtensionHeader>(block, 0);
    Check(!memcmp(x.signature, "T8RX", 4) && x.version == 1 && !x.reserved && x.size >= sizeof(x) && x.size <= block.size() &&
        x.size % 8 == 0, "Invalid retail snapshot extension");
    const auto ext = block.first(static_cast<size_t>(x.size));
    // Each offset is checked against `size` before it is subtracted from it (no unsigned wrap-around).
    Check(x.moveCount <= 0x100000 && x.stringsOffset == Align(sizeof(x) + x.moveCount * 8) && x.stringsOffset <= x.size &&
        x.stringsSize <= x.size - x.stringsOffset && x.cancelsOffset == Align(x.stringsOffset + x.stringsSize) &&
        x.cancelsOffset <= x.size && x.cancelCount <= 0x100000 &&
        x.cancelCount * sizeof(ExternalCancelEntry) <= x.size - x.cancelsOffset,
        "Invalid retail snapshot extension layout");
    Check(Helpers::CalculateCrc32(ext.data() + sizeof(x), x.size - sizeof(x)) == x.crc32, "Retail snapshot extension checksum mismatch");
    auto string = [&](uint32_t at) -> std::optional<std::string> {
        if (at == UINT32_MAX) return std::nullopt;
        Check(at < x.stringsSize, "Invalid retail snapshot string");
        const auto* text = reinterpret_cast<const char*>(ext.data() + x.stringsOffset + at);
        const auto length = strnlen(text, static_cast<size_t>((std::min)(x.stringsSize - at, uint64_t(1025))));
        Check(length < x.stringsSize - at && length <= 1024, "Unterminated retail snapshot string");
        return std::string(text, length);
    };
    if (out) { out->names.clear(); out->animations.clear(); out->cancels.clear(); }
    for (uint64_t row = 0; row < x.moveCount; ++row) {
        auto name = string(Read<uint32_t>(ext, sizeof(x) + row * 8)), animation = string(Read<uint32_t>(ext, sizeof(x) + row * 8 + 4));
        if (out) { out->names.push_back(std::move(name)); out->animations.push_back(std::move(animation)); }
    }
    auto cursor = Align(x.cancelsOffset + x.cancelCount * sizeof(ExternalCancelEntry));
    std::set<std::pair<uint32_t, uint32_t>> keys;
    for (uint64_t i = 0; i < x.cancelCount; ++i) {
        const auto entry = Read<ExternalCancelEntry>(ext, x.cancelsOffset + i * sizeof(ExternalCancelEntry));
        Check(entry.row < x.moveCount && std::count(std::begin(ExternalCancelFields), std::end(ExternalCancelFields), entry.field) &&
            keys.emplace(entry.row, entry.field).second && entry.offset == cursor && entry.size && entry.size % 40 == 0 &&
            entry.size <= 4096 * 40 && entry.size <= x.size - entry.offset &&
            Read<uint64_t>(ext, entry.offset + entry.size - 40) == 0x8000, "Invalid retail external cancel list");
        if (out) out->cancels[{entry.row, entry.field}].assign(ext.begin() + entry.offset, ext.begin() + entry.offset + entry.size);
        cursor = Align(entry.offset + entry.size);
    }
    Check(cursor == x.size, "Unexpected retail snapshot extension bytes");
    return static_cast<size_t>(x.size);
}
std::span<const Byte> ExtensionAt(std::span<const Byte> payload) {
    const auto header = Read<Header>(payload, 0);
    if (header.version < 4) return {};
    auto last = Read<Entry>(payload, header.directoryOffset + (Tables.size() - 1) * sizeof(Entry));
    auto cursor = Align(last.offset + last.count * last.stride);
    const auto directory = Read<AssetDirectory>(payload, cursor);
    cursor = directory.dataOffset;
    for (size_t i = 0; i < directory.count; ++i) {
        const auto asset = Read<AssetEntry>(payload, directory.entriesOffset + i * sizeof(AssetEntry));
        cursor = Align(cursor); cursor = asset.offset + asset.size;
    }
    cursor = Align(cursor);
    return payload.subspan(cursor, static_cast<size_t>(Read<ExtensionHeader>(payload, cursor).size));
}
}
std::vector<Byte> BuildExtension(const NativeData& data) {
    Check(data.names.size() == data.animations.size() && data.names.size() <= 0x100000, "Invalid retail snapshot extension rows");
    ExtensionHeader x{{'T','8','R','X'}, 1, 0, data.names.size(), 0, 0, 0, data.cancels.size(), 0, 0};
    std::vector<Byte> refs(data.names.size() * 8), strings;
    std::map<std::string, uint32_t> pooled;
    auto add = [&](const std::optional<std::string>& text) -> uint32_t {
        if (!text) return UINT32_MAX;
        Check(text->size() <= 1024 && text->find('\0') == std::string::npos, "Invalid retail snapshot string");
        auto [it, inserted] = pooled.try_emplace(*text, static_cast<uint32_t>(strings.size()));
        if (inserted) { strings.insert(strings.end(), text->begin(), text->end()); strings.push_back(0); }
        Check(strings.size() < UINT32_MAX, "Retail snapshot strings exceed budget");
        return it->second;
    };
    for (size_t row = 0; row < data.names.size(); ++row) {
        const uint32_t pair[2] = {add(data.names[row]), add(data.animations[row])};
        memcpy(refs.data() + row * 8, pair, 8);
    }
    x.stringsOffset = Align(sizeof(x) + refs.size()); x.stringsSize = strings.size();
    x.cancelsOffset = Align(x.stringsOffset + x.stringsSize);
    std::vector<Byte> block(Align(x.cancelsOffset + data.cancels.size() * sizeof(ExternalCancelEntry)));
    memcpy(block.data() + sizeof(x), refs.data(), refs.size());
    if (!strings.empty()) memcpy(block.data() + x.stringsOffset, strings.data(), strings.size());
    size_t index = 0;
    for (const auto& [key, list] : data.cancels) {
        ExternalCancelEntry entry{key.first, key.second, block.size(), list.size()};
        memcpy(block.data() + x.cancelsOffset + index++ * sizeof(entry), &entry, sizeof(entry));
        block.insert(block.end(), list.begin(), list.end());
        block.resize(Align(block.size()));
    }
    x.size = block.size();
    Check(x.size <= Limit, "Retail snapshot extension exceeds budget");
    x.crc32 = Helpers::CalculateCrc32(block.data() + sizeof(x), x.size - sizeof(x));
    memcpy(block.data(), &x, sizeof(x));
    ParseExtension(block, nullptr);
    return block;
}
std::optional<NativeData> ReadExtension(std::span<const Byte> payload) {
    Validate(payload);
    const auto block = ExtensionAt(payload);
    if (block.empty()) return std::nullopt;
    NativeData data; ParseExtension(block, &data); return data;
}
std::span<const Byte> ExtensionBytes(std::span<const Byte> payload) { Validate(payload); return ExtensionAt(payload); }
void AppendExtension(std::vector<Byte>& payload, std::span<const Byte> extension) {
    if (extension.empty()) return;
    const std::vector<Byte> copy(extension.begin(), extension.end()); // `extension` may alias `payload`
    auto header = Read<Header>(payload, 0);
    Check(header.version >= 2, "Retail snapshot extension needs an animation directory");
    payload.resize(Align(payload.size()));
    Check(payload.size() <= Limit && copy.size() <= Limit - payload.size(), "Retail snapshot exceeds budget");
    payload.insert(payload.end(), copy.begin(), copy.end());
    header.version = 4; header.size = payload.size(); memcpy(payload.data(), &header, sizeof(header));
    Validate(payload);
}
namespace {
// Null, unreadable or unterminated strings are recorded as not captured; import then lends the target's own.
std::optional<std::string> RemoteString(const GameProcess& process, gameAddr address) {
    if (!RetailT8Runtime::Pointer(address)) return std::nullopt;
    std::string text;
    while (text.size() <= 1024) {
        char chunk[16];
        size_t length = sizeof(chunk);
        if (!process.probeBytes(address + text.size(), chunk, sizeof(chunk))) { // near a region end: byte by byte
            if (!process.probeBytes(address + text.size(), chunk, 1)) return std::nullopt;
            length = 1;
        }
        for (size_t i = 0; i < length; ++i) { if (!chunk[i]) return text; text.push_back(chunk[i]); }
    }
    return std::nullopt;
}
}
NativeData CaptureNative(const GameProcess& process, std::span<const Byte> snapshot) {
    NativeData data;
    const auto moves = Table(snapshot, 12);
    const auto header = Read<Header>(snapshot, 0);
    const auto cancels = Read<Entry>(snapshot, header.directoryOffset + 6 * sizeof(Entry));
    for (size_t row = 0; row < moves.size() / 0x448; ++row) {
        data.names.push_back(RemoteString(process, Read<uint64_t>(moves, row * 0x448 + 0x40)));
        data.animations.push_back(RemoteString(process, Read<uint64_t>(moves, row * 0x448 + 0x48)));
        for (auto field : ExternalCancelFields) {
            const auto pointer = Read<uint64_t>(moves, row * 0x448 + field);
            // Same gating as import: optional banks are inactive when their signed gate is <= 0.
            if (!pointer || (field != 0x98 && Read<int32_t>(moves, row * 0x448 + field + 8) <= 0)) continue;
            if (pointer >= cancels.address && pointer - cancels.address < cancels.count * cancels.stride &&
                (pointer - cancels.address) % cancels.stride == 0) continue; // inside the cancel table
            // An unreadable or unterminated list is not captured; import of that row then needs the live source.
            std::vector<Byte> list;
            for (size_t index = 0; index < 4096; ++index) {
                Byte entry[40];
                if (!process.probeBytes(pointer + index * 40, entry, sizeof(entry))) break;
                list.insert(list.end(), entry, entry + sizeof(entry));
                uint64_t command; memcpy(&command, entry, 8);
                if (command == 0x8000) { data.cancels[{static_cast<uint32_t>(row), field}] = std::move(list); break; }
            }
        }
    }
    return data;
}
void Validate(std::span<const Byte> bytes) {
    Check(bytes.size() <= Limit, "Retail snapshot exceeds budget");
    const auto header = Read<Header>(bytes, 0);
    Check(!memcmp(header.signature, "T8RS", 4) && header.version >= 1 && header.version <= 4, "Unknown retail snapshot format");
    Check(header.size == bytes.size() && header.sourceHeaderOffset == sizeof(Header) &&
        header.directoryOffset == Align(sizeof(Header) + SourceHeaderSize), "Invalid retail snapshot directory");
    Check(header.sourceAddress && header.sourceAddress != UINT64_MAX, "Missing retail source address");
    Check(bytes.size() >= header.directoryOffset + Tables.size() * sizeof(Entry), "Truncated retail snapshot directory");
    const auto source = bytes.subspan(header.sourceHeaderOffset, SourceHeaderSize);
    CheckSource(source);
    size_t cursor = header.directoryOffset + Tables.size() * sizeof(Entry);
    for (size_t i = 0; i < Tables.size(); ++i) {
        const auto spec = Tables[i];
        const auto entry = Read<Entry>(bytes, header.directoryOffset + i * sizeof(Entry));
        Check(entry.stride == spec.stride && !entry.reserved && entry.count == Read<uint64_t>(source, spec.countOffset) &&
            entry.address == Read<uint64_t>(source, spec.pointerOffset), "Retail table metadata differs from source header");
        const auto size = entry.count * entry.stride;
        cursor = Align(cursor);
        Check(entry.offset == cursor && cursor <= bytes.size() && size <= bytes.size() - cursor,
            "Invalid retail table extent or order");
        cursor += size;
    }
    if (header.version >= 2) {
        cursor = Align(cursor);
        auto directory = Read<AssetDirectory>(bytes, cursor);
        // v4 snapshots always carry a directory, possibly empty (tables-only selection).
        Check(!memcmp(directory.signature, "T8AN", 4) && (directory.count || header.version >= 4) && directory.count <= 100000 && !directory.reserved &&
            directory.entriesOffset == cursor + sizeof(directory) && directory.dataOffset == Align(directory.entriesOffset + directory.count * sizeof(AssetEntry)),
            "Invalid retail animation directory");
        cursor = directory.dataOffset;
        std::set<std::pair<uint32_t, uint32_t>> identities;
        for (size_t i = 0; i < directory.count; ++i) {
            auto asset = Read<AssetEntry>(bytes, directory.entriesOffset + i * sizeof(AssetEntry));
            cursor = Align(cursor);
            Check(asset.type < 6 && identities.emplace(asset.type, asset.instance).second &&
                asset.offset == cursor && asset.size <= PanmLimit && cursor <= bytes.size() && asset.size <= bytes.size() - cursor,
                "Invalid retail animation extent or identity");
            if (!asset.size) {
                Check(header.version >= 3 && !asset.address, "Missing animation marker in incompatible retail snapshot");
                continue;
            }
            Check(asset.size >= 8 && RetailT8Runtime::Pointer(asset.address), "Invalid available retail animation");
            Check(PanmSize(bytes.subspan(cursor, asset.size)) == asset.size, "Invalid retail PANM extent");
            cursor += asset.size;
        }
    }
    if (header.version >= 4) {
        cursor = Align(cursor);
        Check(cursor <= bytes.size() && bytes.size() - cursor >= sizeof(ExtensionHeader), "Missing retail snapshot extension");
        cursor += ParseExtension(bytes.subspan(cursor), nullptr);
    }
    Check(cursor == bytes.size(), "Unexpected trailing retail snapshot bytes");
}
std::vector<Byte> Capture(const GameProcess& process, gameAddr address) {
    std::array<Byte, SourceHeaderSize> source{};
    Remote(process, address, source.data(), source.size());
    CheckSource(source);
    Header header{{'T','8','R','S'}, 1, address, sizeof(Header), Align(sizeof(Header) + SourceHeaderSize), 0};
    std::vector<Byte> bytes(header.directoryOffset + Tables.size() * sizeof(Entry));
    memcpy(bytes.data() + header.sourceHeaderOffset, source.data(), source.size());
    for (size_t i = 0; i < Tables.size(); ++i) {
        const auto spec = Tables[i];
        Entry entry{Read<uint64_t>(source, spec.pointerOffset), Read<uint64_t>(source, spec.countOffset), Align(bytes.size()), spec.stride, 0};
        const auto size = entry.count * entry.stride;
        Check(entry.offset <= Limit && size <= Limit - entry.offset, "Aligned retail snapshot exceeds budget");
        bytes.resize(entry.offset + size);
        if (size) Remote(process, entry.address, bytes.data() + entry.offset, static_cast<size_t>(size));
        memcpy(bytes.data() + header.directoryOffset + i * sizeof(Entry), &entry, sizeof(entry));
    }
    std::array<Byte, SourceHeaderSize> check{};
    Remote(process, address, check.data(), check.size());
    Check(check == source, "Retail moveset header changed during capture");
    header.size = bytes.size(); memcpy(bytes.data(), &header, sizeof(header));
    Validate(bytes); return bytes;
}
uint64_t PanmSize(std::span<const Byte> prefix) {
    Check(prefix.size() >= 8 && !memcmp(prefix.data() + 4, "PANM", 4), "Invalid PANM signature");
    size_t furthest = 8;
    auto u32 = [&](size_t offset) { furthest = (std::max)(furthest, offset + 4); return Read<uint32_t>(prefix, offset); };
    auto field = [&](size_t table, size_t index) -> size_t {
        Check(table <= PanmLimit - 4, "Invalid PANM table offset");
        auto distance = static_cast<int32_t>(u32(table));
        int64_t vt = static_cast<int64_t>(table) - distance;
        Check(vt >= 8 && vt <= PanmLimit - 4, "Invalid PANM vtable");
        auto size = Read<uint16_t>(prefix, static_cast<size_t>(vt));
        auto objectSize = Read<uint16_t>(prefix, static_cast<size_t>(vt) + 2);
        Check(size >= 4 && size <= 0x1000 && size % 2 == 0 && objectSize >= 4 && 4 + index * 2 < size,
            "Missing PANM extent field");
        auto offset = Read<uint16_t>(prefix, static_cast<size_t>(vt) + 4 + index * 2);
        furthest = (std::max)(furthest, static_cast<size_t>(vt) + 6 + index * 2);
        Check(offset >= 4 && offset <= objectSize - 4, "Invalid PANM field offset");
        return table + offset;
    };
    auto root = u32(0);
    auto animationRef = field(root, 3);
    auto relative = u32(animationRef);
    Check(relative && relative <= PanmLimit - animationRef, "Invalid PANM animation reference");
    auto animation = animationRef + relative;
    const uint64_t block = u32(field(animation, 5)), end = u32(field(animation, 6));
    auto size = block + end;
    Check(size >= furthest && size <= PanmLimit, "Invalid PANM file extent");
    return size;
}

std::vector<Byte> CaptureWithAnimations(const GameProcess& process, gameAddr address, const std::vector<GivenAnimation>& animations) {
    auto bytes = Capture(process, address);
    const auto native = CaptureNative(process, bytes);
    auto header = Read<Header>(bytes, 0);
    const auto directoryOffset = Align(bytes.size());
    AssetDirectory directory{{'T','8','A','N'}, static_cast<uint32_t>(animations.size()), directoryOffset + sizeof(AssetDirectory), 0, 0};
    directory.dataOffset = Align(directory.entriesOffset + animations.size() * sizeof(AssetEntry));
    bytes.resize(directory.dataOffset);
    memcpy(bytes.data() + directoryOffset, &directory, sizeof(directory));
    for (size_t i = 0; i < animations.size(); ++i) {
        const auto& animation = animations[i];
        Check(animation.type < 6, "Invalid retail animation category");
        const uint64_t size = animation.panm.size();
        if (size) Check(PanmSize(animation.panm) == size, "Given retail animation has an invalid extent");
        AssetEntry asset{size ? ArchiveAssetAddress : 0, Align(bytes.size()), size, animation.instance, animation.type};
        Check(asset.offset <= Limit && size <= Limit - asset.offset, "Retail animation capture exceeds budget");
        bytes.resize(asset.offset + size);
        if (size) memcpy(bytes.data() + asset.offset, animation.panm.data(), animation.panm.size());
        memcpy(bytes.data() + directory.entriesOffset + i * sizeof(asset), &asset, sizeof(asset));
    }
    header.version = 3; header.size = bytes.size(); memcpy(bytes.data(), &header, sizeof(header));
    Validate(bytes);
    AppendExtension(bytes, BuildExtension(native));
    return bytes;
}

std::vector<Byte> CaptureComplete(const GameProcess& process, gameAddr address, gameAddr manager, gameAddr container, uint32_t character, uint8_t categories,
    const ArchiveReader& archive) {
    namespace RT = RetailT8Runtime;
    auto q = [&](gameAddr a) { return RT::Read<uint64_t>(process, a); };
    auto u = [&](gameAddr a) { return RT::Read<uint32_t>(process, a); };
    auto lookup = [&](gameAddr head, gameAddr buckets, uint64_t mask, uint32_t key) { return RT::Lookup(process, head, buckets, mask, key); };
    auto bytes = Capture(process, address);
    Check(!(categories & ~63), "Invalid retail animation category selection");
    // v4: strings and external cancel lists, so a memory-only import needs no live source root.
    const auto native = CaptureNative(process, bytes);
    auto finish = [&](std::vector<Byte> result) {
        auto header = Read<Header>(result, 0);
        if (header.version < 2) { // tables-only selection: an empty animation directory
            const auto at = Align(result.size());
            AssetDirectory empty{{'T','8','A','N'}, 0, at + sizeof(AssetDirectory), Align(at + sizeof(AssetDirectory)), 0};
            result.resize(empty.dataOffset); memcpy(result.data() + at, &empty, sizeof(empty));
            header.version = 3; header.size = result.size(); memcpy(result.data(), &header, sizeof(header));
        }
        std::array<Byte, SourceHeaderSize> check{};
        Remote(process, address, check.data(), check.size());
        Check(!memcmp(check.data(), result.data() + header.sourceHeaderOffset, check.size()), "Retail moveset changed during capture");
        AppendExtension(result, BuildExtension(native));
        return result;
    };
    if (!categories) return finish(std::move(bytes));
    character = RT::AnimationCharacter(Table(bytes, 12), character);
    Check(RT::Pointer(manager) && RT::Pointer(container), "Retail animation resources are not loaded");
    const auto charNode = lookup(q(manager + 0x2090), q(manager + 0x20a0), q(manager + 0x20b8), character);
    Check(charNode != 0, "Missing retail character animation map");
    const auto head = q(charNode + 0x20), count = q(charNode + 0x28);
    Check(count && count <= 100000, "Invalid retail animation slot count");
    struct Slot { gameAddr node; uint32_t packed; uint64_t handle; uint32_t type; };
    std::vector<Slot> slots;
    std::set<gameAddr> nodes;
    std::set<std::pair<uint32_t, uint32_t>> identities;
    for (auto node = q(head + 8); node != head; node = q(node + 8)) {
        Check(nodes.insert(node).second && nodes.size() <= count, "Cyclic retail animation slot map");
        auto packed = u(node + 0x10);
        auto bits = (packed >> 20) & 63;
        if (!bits || (bits & (bits - 1))) continue;
        uint32_t type = 0; while (!(bits & (1U << type))) ++type;
        if (!(categories & (1U << type))) continue;
        auto handle = q(node + 0x18);
        if (identities.emplace(type, static_cast<uint32_t>(handle)).second) slots.push_back({node, packed, handle, type});
    }
    Check(nodes.size() == count, "Incomplete retail animation slot map");
    if (slots.empty()) return finish(std::move(bytes));
    auto idMap = [&](uint32_t key) -> gameAddr {
        auto node = lookup(q(container + 8), q(container + 0x18), q(container + 0x30), key);
        if (!node) return 0;
        auto begin = q(node + 0x18), end = q(node + 0x20);
        if (begin == end) return 0;
        Check(end > begin && end - begin <= 0x10000 && (end - begin) % 8 == 0, "Invalid retail resource vector");
        return q(begin);
    };
    const auto globalMap = idMap(0x81), characterMap = idMap(character);
    auto resolve = [&](gameAddr map, uint32_t instance, uint32_t type, uint64_t& available, bool& unsupported) -> gameAddr {
        if (!map) return 0;
        auto node = lookup(q(map + 0x10), q(map + 0x20), q(map + 0x38), instance);
        if (!node) return 0;
        auto entry = q(node + 0x18);
        if (!entry) return 0;
        Check(RT::Pointer(entry), "Invalid retail animation entry");
        auto resource = q(entry + 8);
        if (!resource) return 0;
        Check(RT::Pointer(resource), "Invalid retail animation resource");
        auto panm = q(resource + 0x1a0);
        Check(!panm || RT::Pointer(panm), "Invalid retail animation data pointer");
        // Prefer the active raw resource, preserving in-memory animation edits.
        // An obsolete cache pointer can be replaced by its retained archive in
        // the same process. No file lookup or resource request occurs here.
        uint32_t magic = 0;
        if (panm && process.probeBytes(panm + 4, &magic, sizeof(magic)) && magic == 0x4d4e4150) return panm;
        if (auto archived = RT::ArchivePanm(process, map, instance, type, 8, &available)) return archived;
        unsupported |= panm != 0;
        return 0;
    };
    auto header = Read<Header>(bytes, 0);
    const auto directoryOffset = Align(bytes.size());
    AssetDirectory directory{{'T','8','A','N'}, static_cast<uint32_t>(slots.size()), directoryOffset + sizeof(AssetDirectory), 0, 0};
    directory.dataOffset = Align(directory.entriesOffset + slots.size() * sizeof(AssetEntry));
    bytes.resize(directory.dataOffset);
    memcpy(bytes.data() + directoryOffset, &directory, sizeof(directory));
    std::vector<std::string> notFound; // every animation neither in memory nor in the game's archives (the export stops)
    for (size_t i = 0; i < slots.size(); ++i) {
        const auto& slot = slots[i];
        const auto instance = static_cast<uint32_t>(slot.handle);
        gameAddr panm = 0;
        uint64_t available = 0;
        bool unsupported = false;
        for (auto map : {globalMap, characterMap}) {
            auto candidate = resolve(map, instance, slot.type, available, unsupported);
            if (!candidate) continue;
            panm = candidate; break;
        }
        if (!panm) {
            Check(!unsupported, "Unsupported loaded retail animation resource");
            Check(u(slot.node + 0x10) == slot.packed && q(slot.node + 0x18) == slot.handle, "Unloaded retail animation slot changed during capture");
            // Not loaded by the game: the export still carries it, from the game's own archive (same bytes as the game
            // loads). One the archives don't have is exported as unavailable, like an export without archive reading;
            // the rest of the moveset is not refused for it (owner, 2026-11-15).
            std::optional<std::vector<Byte>> filled;
            if (archive) try { filled = archive(character, slot.type, instance); } // com.anmbin first, then the character's
                catch (const std::exception& error) {
                    if (notFound.size() < 1000) notFound.push_back(error.what()); else notFound.back() = "...";
                    filled.reset();
                }
            if (filled) {
                auto& panm = *filled;
                const auto size = PanmSize(panm);
                Check(size == panm.size(), "Archived retail animation has an invalid extent");
                AssetEntry asset{ArchiveAssetAddress, Align(bytes.size()), size, instance, slot.type};
                Check(asset.offset <= Limit && size <= Limit - asset.offset, "Retail animation capture exceeds budget");
                bytes.resize(asset.offset + size);
                memcpy(bytes.data() + asset.offset, panm.data(), panm.size());
                memcpy(bytes.data() + directory.entriesOffset + i * sizeof(asset), &asset, sizeof(asset));
                continue;
            }
            AssetEntry unavailable{0, Align(bytes.size()), 0, instance, slot.type};
            bytes.resize(unavailable.offset);
            memcpy(bytes.data() + directory.entriesOffset + i * sizeof(unavailable), &unavailable, sizeof(unavailable));
            continue;
        }
        std::vector<Byte> prefix;
        uint64_t size = 0;
        for (size_t length = 128; length <= 1 << 20; length *= 4) {
            prefix.resize(available ? static_cast<size_t>((std::min)(available, uint64_t(length))) : length);
            Remote(process, panm, prefix.data(), prefix.size());
            try { size = PanmSize(prefix); break; } catch (const std::runtime_error&) { }
            if (available && length >= available) break;
        }
        Check(size != 0, "Cannot establish bounded PANM size");
        Check(!available || size <= available, "PANM exceeds its retained memory archive");
        AssetEntry asset{panm, Align(bytes.size()), size, instance, slot.type};
        Check(asset.offset <= Limit && size <= Limit - asset.offset, "Retail animation capture exceeds budget");
        bytes.resize(asset.offset + size);
        Remote(process, panm, bytes.data() + asset.offset, size);
        Check(PanmSize({bytes.data() + asset.offset, static_cast<size_t>(size)}) == size, "Retail animation changed during capture");
        Check(u(slot.node + 0x10) == slot.packed && q(slot.node + 0x18) == slot.handle, "Retail animation handle changed during capture");
        memcpy(bytes.data() + directory.entriesOffset + i * sizeof(asset), &asset, sizeof(asset));
    }
    if (!notFound.empty()) {
        // exported as unavailable (see above), not a failure
        std::string message = std::to_string(notFound.size()) + " selected animation(s) are neither loaded by the game nor in its "
            "tkdata.bin archives: exported without them: ";
        for (size_t i = 0; i < notFound.size() && i < 20; ++i) message += (i ? "; " : "") + notFound[i];
        if (notFound.size() > 20) message += "; ...";
        DEBUG_LOG("%s\n", message.c_str());
    }
    Check(q(charNode + 0x20) == head && q(charNode + 0x28) == count, "Retail animation map changed during capture");
    std::array<Byte, SourceHeaderSize> check{};
    Remote(process, address, check.data(), check.size());
    Check(!memcmp(check.data(), bytes.data() + header.sourceHeaderOffset, check.size()), "Retail moveset changed during animation capture");
    header.version = 3; header.size = bytes.size(); memcpy(bytes.data(), &header, sizeof(header));
    Validate(bytes); return finish(std::move(bytes));
}

std::vector<AssetEntry> Assets(std::span<const Byte> payload) {
    Validate(payload);
    const auto header = Read<Header>(payload, 0);
    if (header.version == 1) return {};
    auto last = Read<Entry>(payload, header.directoryOffset + (Tables.size() - 1) * sizeof(Entry));
    auto directory = Read<AssetDirectory>(payload, Align(last.offset + last.count * last.stride));
    std::vector<AssetEntry> result(directory.count);
    memcpy(result.data(), payload.data() + directory.entriesOffset, result.size() * sizeof(AssetEntry));
    return result;
}

namespace {
struct OuterSectionHeader { char signature[4]; uint32_t version, size, crc; };
static_assert(sizeof(OuterSectionHeader) == 16);
constexpr uint64_t OuterSectionsLimit = 64ULL << 20;
}
std::vector<OuterSection> ReadOuterSections(std::span<const Byte> file) {
    Check(file.size() >= sizeof(TKMovesetHeader), "Truncated moveset header");
    const auto outer = Read<TKMovesetHeader>(file, 0);
    Check(outer.header_size <= file.size() && outer.header_size <= OuterSectionsLimit + 4096, "Invalid moveset header size");
    std::vector<OuterSection> sections;
    for (size_t at = Align(sizeof(TKMovesetHeader)); at + sizeof(OuterSectionHeader) <= outer.header_size;) {
        const auto section = Read<OuterSectionHeader>(file, at);
        Check(section.size <= outer.header_size - at - sizeof(section), "Moveset header section exceeds the header");
        const auto data = file.subspan(at + sizeof(section), section.size);
        Check(Helpers::CalculateCrc32(data.data(), data.size()) == section.crc, "Moveset header section checksum differs");
        OuterSection result; memcpy(result.tag.data(), section.signature, 4); result.version = section.version;
        // Only known tags: a damaged tag must not turn editor provenance into an ignored section.
        Check(result.tag == EditorTag || result.tag == NotesTag || result.tag == NamesTag, "Unknown moveset header section");
        result.data.assign(data.begin(), data.end());
        sections.push_back(std::move(result));
        at = Align(at + sizeof(section) + section.size);
    }
    return sections;
}
std::string Notes(std::span<const Byte> file) {
    for (const auto& section : ReadOuterSections(file))
        if (section.tag == NotesTag && section.version == 1) return {section.data.begin(), section.data.end()};
    return {};
}
OuterSection NotesSection(std::string_view text) {
    Check(text.size() <= (1u << 20), "Moveset notes are too long");
    OuterSection section; section.tag = NotesTag; section.data.assign(text.begin(), text.end());
    return section;
}
OuterSection NamesSection(const NameMap& names) {
    Check(names.size() <= (1u << 20), "Too many moveset names");
    OuterSection section; section.tag = NamesTag;
    auto put = [&](uint32_t value) { const auto* p = reinterpret_cast<const Byte*>(&value); section.data.insert(section.data.end(), p, p + 4); };
    put(static_cast<uint32_t>(names.size()));
    for (const auto& [hash, name] : names) {
        Check(name.size() <= 4096, "Moveset name is too long");
        put(hash); put(static_cast<uint32_t>(name.size()));
        section.data.insert(section.data.end(), name.begin(), name.end());
    }
    Check(section.data.size() <= OuterSectionsLimit, "Moveset names section is too large");
    return section;
}
NameMap Names(std::span<const Byte> file) {
    NameMap names;
    for (const auto& section : ReadOuterSections(file)) {
        if (section.tag != NamesTag) continue;
        Check(section.version == 1, "Unsupported moveset names section");
        const std::span<const Byte> data(section.data);
        size_t at = 4;
        const auto count = Read<uint32_t>(data, 0);
        for (uint32_t i = 0; i < count; ++i) {
            const auto hash = Read<uint32_t>(data, at), length = Read<uint32_t>(data, at + 4);
            at += 8;
            Check(length <= 4096 && length <= data.size() - at, "Truncated moveset names section");
            names[hash].assign(reinterpret_cast<const char*>(data.data() + at), length);
            at += length;
        }
        Check(at == data.size(), "Moveset names section has trailing bytes");
    }
    return names;
}
std::vector<MoveNameHashes> MoveHashes(std::span<const Byte> payload) {
    const auto moves = Table(payload, 12);
    std::vector<MoveNameHashes> result(moves.size() / 0x448);
    for (size_t row = 0; row < result.size(); ++row) {
        const auto at = row * 0x448;
        result[row].name = RetailNames::Decode(Read<uint64_t>(moves, at), Read<uint64_t>(moves, at + 8));
        result[row].animation = RetailNames::Decode(Read<uint64_t>(moves, at + 0x20), Read<uint64_t>(moves, at + 0x28));
    }
    return result;
}
NameMap CollectNames(std::span<const Byte> payload, const std::function<const std::string*(uint32_t)>& lookup) {
    NameMap names;
    const auto native = ReadExtension(payload);
    const auto hashes = MoveHashes(payload);
    auto add = [&](std::optional<uint32_t> hash, const std::optional<std::string>* captured) {
        if (!hash || names.contains(*hash)) return;
        // A captured string is the game's own; use it only when it is the string the record hashes.
        if (captured && *captured && !(*captured)->empty() && RetailNames::Hash(**captured) == *hash) { names[*hash] = **captured; return; }
        if (lookup) if (const auto* found = lookup(*hash); found && !found->empty()) names[*hash] = *found;
    };
    for (size_t row = 0; row < hashes.size(); ++row) {
        add(hashes[row].name, native && row < native->names.size() ? &native->names[row] : nullptr);
        add(hashes[row].animation, native && row < native->animations.size() ? &native->animations[row] : nullptr);
    }
    return names;
}
std::vector<Byte> Serialize(std::span<const Byte> payload, TKMovesetHeader header, const std::vector<OuterSection>& sections,
    const std::vector<TKMovesetProperty>* properties) {
    Validate(payload);
    header.gameId = GameId_T8; header.minorVersion = GameVersions::T8::RETAIL;
    header.game_specific_flags = Read<Header>(payload, 0).version >= 2 ? RuntimeAssetsFlag : TablesOnlyFlag;
    size_t extension = 0;
    for (const auto& section : sections) {
        Check(section.data.size() <= OuterSectionsLimit, "Moveset header section exceeds budget");
        extension = Align(extension + sizeof(OuterSectionHeader) + section.data.size());
    }
    Check(extension <= OuterSectionsLimit, "Moveset header sections exceed budget");
    header.header_size = static_cast<uint32_t>(Align(sizeof(header)) + extension);
    // Custom property list (the import profile): the given one, or the default profile for a new file.
    auto list = properties ? *properties : ImportProfile::ToProperties({});
    list.push_back({TKMovesetProperty_END, 0});
    header.block_list = static_cast<uint32_t>(header.header_size + Align(list.size() * sizeof(TKMovesetProperty)));
    header.block_list_size = 1; header.moveset_data_start = header.block_list + sizeof(uint64_t);
    header.compressionType = TKMovesetCompressionType_None; header.moveset_data_size = 0;
    Check(payload.size() <= Limit - header.moveset_data_start, "Retail container exceeds budget");
    std::vector<Byte> bytes(header.moveset_data_start + payload.size());
    memcpy(bytes.data() + header.header_size, list.data(), list.size() * sizeof(TKMovesetProperty));
    memcpy(bytes.data() + header.moveset_data_start, payload.data(), payload.size());
    for (size_t at = Align(sizeof(header)); const auto& section : sections) {
        OuterSectionHeader h{{}, section.version, static_cast<uint32_t>(section.data.size()),
            Helpers::CalculateCrc32(section.data.data(), section.data.size())};
        memcpy(h.signature, section.tag.data(), 4);
        memcpy(bytes.data() + at, &h, sizeof(h));
        if (!section.data.empty()) memcpy(bytes.data() + at + sizeof(h), section.data.data(), section.data.size());
        at = Align(at + sizeof(h) + section.data.size());
    }
    header.crc32 = header.orig_crc32 = Helpers::CalculateCrc32(bytes.data() + header.moveset_data_start, payload.size());
    memcpy(bytes.data(), &header, sizeof(header)); return bytes;
}
std::span<const Byte> Table(std::span<const Byte> payload, size_t index) {
    Validate(payload); Check(index < Tables.size(), "Unknown retail table");
    const auto header = Read<Header>(payload, 0);
    const auto entry = Read<Entry>(payload, header.directoryOffset + index * sizeof(Entry));
    return payload.subspan(entry.offset, entry.count * entry.stride);
}
}

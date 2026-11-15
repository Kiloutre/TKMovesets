#include "RetailResourceLoader.hpp"
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <cstring>
#include <string>
#include "Extractor_t8/RetailRuntime.hpp"

namespace RetailT8Resources {
EngineAddresses ReadEngineAddresses(const GameAddressesWrapper& game, gameAddr module) {
    auto at = [&](const char* key) {
        const auto rva = game.GetValue(key);
        if (rva <= 0) throw std::runtime_error(std::string("game_addresses.ini has no t8-retail_") + key + ": update the addresses file");
        return module + static_cast<gameAddr>(rva);
    };
    if (!module) throw std::runtime_error("Retail game module is not known");
    return {at("anim_compiled_vtable_rva"), at("anim_empty_bucket_rva"), at("anim_resource_vtable_rva"),
        at("anim_resource_ref_vtable_rva"), at("anim_noop_rva")};
}

gameAddr FindAnimationResource(const GameProcess& process, gameAddr container, uint32_t character, uint32_t instance) {
    namespace RT = RetailT8Runtime;
    auto q = [&](gameAddr address) { return RT::Read<uint64_t>(process, address); };
    for (uint32_t domain : {uint32_t(0x81), character}) {
        auto node = RT::Lookup(process, q(container + 8), q(container + 0x18), q(container + 0x30), domain);
        if (!node) continue;
        auto begin = q(node + 0x18), end = q(node + 0x20);
        if (begin == end) continue;
        auto map = q(begin);
        if (!map) continue;
        auto entryNode = RT::Lookup(process, q(map + 0x10), q(map + 0x20), q(map + 0x38), instance);
        if (!entryNode) continue;
        auto entry = q(entryNode + 0x18);
        if (!entry) continue;
        auto resource = q(entry + 8);
        if (resource && q(resource + 0x190)) return resource;
    }
    return 0;
}

namespace {
// Carrier log: resource -> (original compiled object, object TKMovesets attached, owning player).
// Imports run in separate processes, so it lives in a file keyed by the game's process ID and start time.
struct Carrier { gameAddr original, attached, owner; };
std::filesystem::path CarrierLog(DWORD pid) {
    FILETIME created{}, exited{}, kernel{}, user{};
    if (auto handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) { GetProcessTimes(handle, &created, &exited, &kernel, &user); CloseHandle(handle); }
    const auto start = (uint64_t(created.dwHighDateTime) << 32) | created.dwLowDateTime;
    return std::filesystem::temp_directory_path() / "TKMovesets2" / ("t8-carriers-" + std::to_string(pid) + "-" + std::to_string(start) + ".bin");
}
std::map<gameAddr, Carrier> ReadCarriers(DWORD pid) {
    std::map<gameAddr, Carrier> result;
    std::ifstream in(CarrierLog(pid), std::ios::binary);
    for (uint64_t row[4]; in.read(reinterpret_cast<char*>(row), sizeof(row));) result[row[0]] = {row[1], row[2], row[3]};
    return result;
}
void WriteCarriers(DWORD pid, const std::map<gameAddr, Carrier>& carriers) {
    const auto path = CarrierLog(pid);
    std::error_code error; std::filesystem::create_directories(path.parent_path(), error);
    auto temporary = path; temporary += ".tmp";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        for (const auto& [resource, carrier] : carriers) {
            const uint64_t row[4] = {resource, carrier.original, carrier.attached, carrier.owner};
            out.write(reinterpret_cast<const char*>(row), sizeof(row));
        }
        if (!out) throw std::runtime_error("Cannot record carrier animations");
    }
    std::filesystem::rename(temporary, path);
}
}

namespace {
// Local copy of the engine's PANM compile (0x1419DB2A0), see .docs/retail-t8-compiled-anim-re.md.
// Bounded FlatBuffers reads; absent fields read as 0 like the engine's.
struct Fb {
    std::span<const Byte> b;
    template<class T> T Get(size_t at) const {
        if (at > b.size() || sizeof(T) > b.size() - at) throw std::runtime_error("Truncated custom PANM");
        T value; memcpy(&value, b.data() + at, sizeof(value)); return value;
    }
    size_t Field(size_t table, unsigned index) const { // 0 when absent
        const auto vt = int64_t(table) - Get<int32_t>(table);
        if (vt < 0 || uint64_t(vt) >= b.size()) throw std::runtime_error("Invalid custom PANM vtable");
        const auto size = Get<uint16_t>(size_t(vt));
        if (4 + 2 * index >= size) return 0;
        const auto offset = Get<uint16_t>(size_t(vt) + 4 + 2 * index);
        return offset ? table + offset : 0;
    }
    template<class T> T Value(size_t table, unsigned index) const { auto at = Field(table, index); return at ? Get<T>(at) : T{}; }
    size_t Object(size_t table, unsigned index) const {
        const auto at = Field(table, index);
        if (!at) return 0;
        const auto target = at + Get<uint32_t>(at);
        if (target >= b.size()) throw std::runtime_error("Invalid custom PANM reference");
        return target;
    }
    std::vector<size_t> Vector(size_t table, unsigned index) const {
        std::vector<size_t> result;
        const auto start = Object(table, index);
        if (!start) return result;
        const auto count = Get<uint32_t>(start);
        if (count > 0x10000) throw std::runtime_error("Invalid custom PANM vector");
        for (uint32_t i = 0; i < count; ++i) {
            const auto at = start + 4 + size_t(i) * 4;
            const auto target = at + Get<uint32_t>(at);
            if (target >= b.size()) throw std::runtime_error("Invalid custom PANM reference");
            result.push_back(target);
        }
        return result;
    }
    std::string String(size_t table, unsigned index) const { // the engine copies it as a C string
        const auto start = Object(table, index);
        if (!start) throw std::runtime_error("Missing custom PANM name");
        const auto length = Get<uint32_t>(start);
        if (length > 0x1000 || start + 4 > b.size() || length > b.size() - start - 4) throw std::runtime_error("Invalid custom PANM name");
        const auto* text = reinterpret_cast<const char*>(b.data() + start + 4);
        return {text, strnlen(text, length)};
    }
};
// Enum fields the engine keeps only inside their valid range.
uint8_t Enum(uint32_t value, uint32_t last) { return value >= 1 && value <= last ? uint8_t(value) : 0; }
// Track entry (track node +0x38).
struct CompiledTrack { uint8_t type = 0, encoding = 0; uint32_t samples = 0, flags = 0, offset = 0, size = 0; };
using CompiledBones = std::map<std::string, std::map<std::string, CompiledTrack>>;

// Writes EASTL containers into an image whose byte 0 lives at the remote address `base`.
struct Image {
    std::vector<Byte> bytes; gameAddr base;
    size_t Alloc(size_t size, size_t align = 16) {
        const auto at = (bytes.size() + align - 1) & ~(align - 1);
        bytes.resize(at + size); return at;
    }
    gameAddr Remote(size_t at) const { return base + at; }
    template<class T> void Put(size_t at, T value) { memcpy(bytes.data() + at, &value, sizeof(value)); }
    // EASTL basic_string (0x18 bytes): in place up to 23 characters, last byte = 23 - size; otherwise heap
    // {pointer, size, capacity | 1<<63}.
    void String(size_t at, const std::string& text) {
        if (text.size() <= 23) { memcpy(bytes.data() + at, text.data(), text.size()); bytes[at + 23] = Byte(23 - text.size()); return; }
        const auto heap = Alloc(text.size() + 1, 8);
        memcpy(bytes.data() + heap, text.data(), text.size());
        Put<uint64_t>(at, Remote(heap)); Put<uint64_t>(at + 8, text.size()); Put<uint64_t>(at + 16, text.size() | (1ULL << 63));
    }
    // EASTL rbtree. Node: +0 right (greater), +8 left, +0x10 parent, +0x18 color (0 red, 1 black).
    // Anchor: +0 rightmost, +8 leftmost, +0x10 root, +0x18 color 0, +0x20 size. `nodes` are in key order.
    // Balanced build from the sorted keys; the last, incomplete level is red, so every path has the same black count.
    void Tree(size_t anchor, const std::vector<size_t>& nodes) {
        const auto self = Remote(anchor);
        Put<uint64_t>(anchor, self); Put<uint64_t>(anchor + 8, self); Put<uint64_t>(anchor + 0x10, 0);
        Put<uint8_t>(anchor + 0x18, 0); Put<uint64_t>(anchor + 0x20, nodes.size());
        if (nodes.empty()) return;
        size_t full = 0; while ((size_t(2) << full) - 1 <= nodes.size()) ++full; // complete levels
        auto build = [&](auto& self_, size_t lo, size_t hi, gameAddr parent, size_t depth) -> gameAddr {
            if (lo >= hi) return 0;
            const auto mid = lo + (hi - lo) / 2, node = nodes[mid];
            Put<uint64_t>(node + 0x08, self_(self_, lo, mid, Remote(node), depth + 1));
            Put<uint64_t>(node + 0x00, self_(self_, mid + 1, hi, Remote(node), depth + 1));
            Put<uint64_t>(node + 0x10, parent); Put<uint8_t>(node + 0x18, depth < full ? 1 : 0);
            return Remote(node);
        };
        Put<uint64_t>(anchor + 0x10, build(build, 0, nodes.size(), self, 0));
        Put<uint64_t>(anchor, Remote(nodes.back())); Put<uint64_t>(anchor + 8, Remote(nodes.front()));
    }
    // Empty EASTL hash_map<u32,u32> (0x30 bytes): the engine's static single-bucket sentinel.
    void EmptyHash(size_t at, gameAddr sentinel) {
        Put<uint64_t>(at + 0x08, sentinel); Put<uint64_t>(at + 0x10, 1); Put<uint64_t>(at + 0x18, 0);
        Put<float>(at + 0x20, 1.0f); Put<float>(at + 0x24, 2.0f); Put<uint32_t>(at + 0x28, 0);
    }
};
}

namespace {
// Appends the PANM and its compiled animation to `image`; returns the object's offset (the PANM is at offset 0).
size_t BuildCompiledInto(Image& image, std::span<const Byte> panm, const EngineAddresses& engine) {
    if (panm.size() < 8 || memcmp(panm.data() + 4, "PANM", 4)) throw std::invalid_argument("Invalid custom PANM");
    if (image.base % 16 || !image.bytes.empty()) throw std::invalid_argument("Unaligned compiled animation address");
    Fb fb{panm};
    const auto root = size_t(fb.Get<uint32_t>(0));
    if (root >= panm.size() || fb.Value<uint8_t>(root, 2) != 1) throw std::runtime_error("The game would reject this PANM (root kind)");
    const auto anim = fb.Object(root, 3);
    if (!anim) throw std::runtime_error("The game would reject this PANM (no animation)");
    const auto sampleBase = fb.Value<uint32_t>(anim, 5);
    if (sampleBase > panm.size()) throw std::runtime_error("Invalid custom PANM sample base");
    if (fb.Value<uint32_t>(anim, 8)) throw std::runtime_error("Keyframed custom PANMs are not supported");
    // Mode table (field 10): flag, rate, and a u32 list the engine indexes in two hash maps (only empty lists supported).
    uint8_t modeFlag = 0; float modeRate = 1.0f;
    if (const auto mode = fb.Object(anim, 10)) {
        modeFlag = fb.Value<uint8_t>(mode, 1) != 0; modeRate = fb.Value<float>(mode, 2);
        if (const auto list = fb.Object(mode, 3); list && fb.Get<uint32_t>(list)) throw std::runtime_error("Custom PANM mode lists are not supported");
    }
    // group type -> bone -> track. Like the engine: groups and bones merge by key, a repeated track overwrites.
    std::map<uint8_t, CompiledBones> groups;
    // Nested vectors may alias each other (65536^3 visits from one small file): bound the total walk.
    size_t visited = 0;
    auto visit = [&] { if (++visited > MaxCompiledNodes) throw std::runtime_error("Custom PANM has too many bones or tracks"); };
    for (auto group : fb.Vector(anim, 4)) {
        auto& bones = groups[Enum(fb.Value<uint32_t>(group, 1), 5)];
        for (auto bone : fb.Vector(group, 2)) {
            visit();
            auto& tracks = bones[fb.String(bone, 1)];
            for (auto track : fb.Vector(bone, 2)) {
                visit();
                CompiledTrack entry;
                entry.type = Enum(fb.Value<uint32_t>(track, 2), 9); entry.encoding = Enum(fb.Value<uint32_t>(track, 3), 3);
                entry.samples = fb.Value<uint32_t>(track, 4);
                if (const auto options = fb.Object(track, 5)) {
                    const std::pair<unsigned, uint32_t> bits[] = {{1, 1}, {2, 2}, {3, 4}, {4, 8}, {8, 0x80}};
                    for (auto [field, bit] : bits) if (fb.Value<uint8_t>(options, field)) entry.flags |= bit;
                }
                entry.offset = fb.Value<uint32_t>(track, 6); entry.size = fb.Value<uint32_t>(track, 7);
                // The engine trusts these; a bad extent would make the game read outside our upload.
                if (entry.offset > panm.size() - sampleBase || entry.size > panm.size() - sampleBase - entry.offset)
                    throw std::runtime_error("Custom PANM track samples lie outside the file");
                tracks[fb.String(track, 1)] = entry;
            }
        }
    }
    // Image: the PANM itself (samples are read in place, as from the engine's own buffer), then the object and its nodes.
    const auto source = image.Alloc(panm.size());
    memcpy(image.bytes.data() + source, panm.data(), panm.size());
    const auto object = image.Alloc(0xF0);
    image.Put<uint64_t>(object, engine.compiledVtable);
    image.Put<uint32_t>(object + 0x08, fb.Value<uint32_t>(anim, 1)); // frames - 1
    image.Put<float>(object + 0x0C, fb.Value<float>(anim, 2));       // fps
    image.Put<uint8_t>(object + 0x10, Enum(fb.Value<uint32_t>(anim, 3), 3));
    image.Put<uint8_t>(object + 0x18, modeFlag); image.Put<float>(object + 0x1C, modeRate);
    image.EmptyHash(object + 0x20, engine.emptyBucket); image.EmptyHash(object + 0x50, engine.emptyBucket);
    const auto samples = image.Remote(source) + sampleBase;
    image.Put<uint64_t>(object + 0xB0, samples);
    for (unsigned field = 6; field <= 9; ++field) image.Put<uint64_t>(object + 0xB8 + (field - 6) * 8, fb.Value<uint32_t>(anim, field));
    image.Put<uint64_t>(object + 0xD8, samples); // data base every sampler uses; +0xE0/+0xE8 (data owner) stay 0
    std::vector<size_t> groupNodes;
    for (const auto& [type, bones] : groups) {
        const auto groupNode = image.Alloc(0x58);
        image.Put<uint8_t>(groupNode + 0x20, type);
        groupNodes.push_back(groupNode);
        std::vector<size_t> boneNodes;
        for (const auto& [name, tracks] : bones) {
            const auto boneNode = image.Alloc(0x68);
            image.String(boneNode + 0x20, name);
            boneNodes.push_back(boneNode);
            std::vector<size_t> trackNodes;
            for (const auto& [trackName, entry] : tracks) {
                const auto node = image.Alloc(0x68);
                image.String(node + 0x20, trackName);
                image.Put<uint8_t>(node + 0x38, entry.type); image.Put<uint8_t>(node + 0x39, entry.encoding);
                image.Put<uint32_t>(node + 0x3C, entry.samples); image.Put<uint32_t>(node + 0x40, entry.flags);
                image.Put<uint32_t>(node + 0x44, entry.offset); image.Put<uint32_t>(node + 0x48, entry.size);
                trackNodes.push_back(node); // +0x50..+0x67: empty keyframe vector
            }
            image.Tree(boneNode + 0x38, trackNodes);
        }
        image.Tree(groupNode + 0x28, boneNodes);
    }
    image.Tree(object + 0x80, groupNodes);
    return object;
}
}

CompiledAnimation BuildCompiledAnimation(std::span<const Byte> panm, gameAddr base, const EngineAddresses& engine) {
    Image image{{}, base};
    const auto object = BuildCompiledInto(image, panm, engine);
    return {std::move(image.bytes), image.Remote(object)};
}

RegisteredAnimation BuildRegisteredAnimation(std::span<const Byte> panm, gameAddr base, const EngineAddresses& engine, uint32_t id) {
    Image image{{}, base};
    const auto object = BuildCompiledInto(image, panm, engine);
    // Shared-pointer control block that never reaches zero: readers take weak/strong references through it, and
    // its vtable (only reached at zero) points at a side-effect-free game function. Nothing here is ever freed.
    const auto vtable = image.Alloc(0x30, 8);
    for (size_t slot = 0; slot < 6; ++slot) image.Put<uint64_t>(vtable + slot * 8, engine.noOp);
    const auto ctrl = image.Alloc(0x20, 8);
    image.Put<uint64_t>(ctrl, image.Remote(vtable)); image.Put<uint32_t>(ctrl + 8, 0x40000000); image.Put<uint32_t>(ctrl + 0x0C, 0x40000000);
    image.Put<uint64_t>(ctrl + 0x10, image.Remote(object));
    // Finished load request: IsDone (AnimResource vtable +0x28) reads request+0x30 == 2.
    const auto request = image.Alloc(0x40, 8);
    image.Put<uint32_t>(request + 0x30, 2);
    // AnimResource with the game's own vtable; only its IsDone slot is ever called (it is in no manager or list).
    const auto resource = image.Alloc(0x1A8, 16);
    image.Put<uint64_t>(resource, engine.animResourceVtable); image.Put<uint64_t>(resource + 0x08, id);
    image.Put<uint32_t>(resource + 0x10, 3); image.Put<uint64_t>(resource + 0x140, image.Remote(request));
    image.Put<uint64_t>(resource + 0x190, image.Remote(object)); image.Put<uint64_t>(resource + 0x198, image.Remote(ctrl));
    image.Put<uint64_t>(resource + 0x1A0, image.Remote(0)); // the PANM copy, for byte checks and exports
    const auto ref = image.Alloc(0x20, 8);
    image.Put<uint64_t>(ref, engine.resourceRefVtable); image.Put<uint64_t>(ref + 8, image.Remote(resource));
    image.Put<uint64_t>(ref + 0x18, id);
    const auto node = image.Alloc(0x20, 8); // next/prev are written by PublishAnimations
    image.Put<uint32_t>(node + 0x10, id); image.Put<uint64_t>(node + 0x18, image.Remote(ref));
    return {std::move(image.bytes), image.Remote(object), image.Remote(resource), image.Remote(node)};
}

AnimationId ChooseAnimationId(const GameProcess& process, gameAddr container, uint32_t domain, std::span<const Byte> panm,
    const EngineAddresses& engine, const std::set<uint32_t>& taken) {
    namespace RT = RetailT8Runtime;
    uint32_t hash = 0x811c9dc5;
    for (auto byte : panm) { hash ^= byte; hash *= 0x01000193; }
    const gameAddr maps[2] = {RT::DomainMap(process, container, 0x81), RT::DomainMap(process, container, domain)};
    if (!maps[1]) throw std::runtime_error("Target animation domain has no instance map");
    for (uint32_t probe = 0; probe < 4096; ++probe) {
        const auto id = 0x80000000U | ((hash + probe) & 0x7fffffffU);
        if (taken.count(id)) continue;
        gameAddr node = 0;
        for (auto map : maps) if (map && !node) node = RT::MapNode(process, map, id);
        if (!node) return {id, false};
        // Reuse only our own earlier entry for the same bytes (its resource keeps a copy of the PANM).
        try {
            const auto entry = RT::Read<uint64_t>(process, node + 0x18);
            const auto resource = entry ? RT::Read<uint64_t>(process, entry + 8) : 0;
            if (!resource || RT::Read<uint64_t>(process, resource) != engine.animResourceVtable ||
                !RT::Read<uint64_t>(process, resource + 0x190)) continue;
            const auto copy = RT::Read<uint64_t>(process, resource + 0x1A0);
            bool same = copy != 0;
            for (size_t at = 0; same && at < panm.size();) {
                const auto length = (std::min)(size_t(1) << 20, panm.size() - at);
                std::vector<Byte> bytes(length);
                same = process.probeBytes(copy + at, bytes.data(), length) && !memcmp(bytes.data(), panm.data() + at, length);
                at += length;
            }
            if (same) return {id, true};
        } catch (const std::exception&) {}
    }
    throw std::runtime_error("No free retail animation id");
}

size_t PublishAnimations(GameProcess& process, gameAddr map, std::span<const PublishedAnimation> entries) {
    namespace RT = RetailT8Runtime;
    if (entries.empty()) return 0;
    // Readers only see a fully populated map; a later rebuild is a new map (guarded by the caller's container read).
    const auto populated = RT::Read<uint8_t>(process, map + 0x48);
    const auto head = RT::Read<uint64_t>(process, map + 0x10), buckets = RT::Read<uint64_t>(process, map + 0x20);
    const auto mask = RT::Read<uint64_t>(process, map + 0x38);
    if (populated != 1) throw std::runtime_error("Retail animation map is not populated yet");
    if (!process.ExpectBytes(map + 0x48, &populated, 1) || !process.ExpectBytes(map + 0x10, &head, 8) ||
        !process.ExpectBytes(map + 0x20, &buckets, 8) || !process.ExpectBytes(map + 0x38, &mask, 8))
        throw std::runtime_error("Cannot guard retail animation map");
    // Bucket-only links: the node goes on top of its bucket's prev chain (bucket.hi), never into the list. Every
    // reader walks hi -> prev -> ... -> lo; teardown, rehash and readiness walk the list by next and never reach it,
    // so the engine never frees our memory. One 8-byte store per bucket publishes.
    std::map<gameAddr, std::pair<RT::MapBucket, std::vector<gameAddr>>> chains;
    for (const auto& entry : entries) {
        if (RT::MapNode(process, map, entry.id)) throw std::runtime_error("Retail animation id is already registered");
        const auto bucket = RT::Bucket(process, map, entry.id);
        auto& chain = chains.try_emplace(bucket.address, bucket, std::vector<gameAddr>{}).first->second;
        for (auto node : chain.second) if (RT::Read<uint32_t>(process, node + 0x10) == entry.id) throw std::runtime_error("Duplicate retail animation id");
        chain.second.push_back(entry.node);
    }
    for (auto& [address, chain] : chains) {
        const auto& [bucket, nodes] = chain;
        const uint64_t original[2] = {bucket.lo, bucket.hi};
        if (!process.ExpectBytes(address, original, sizeof(original))) throw std::runtime_error("Cannot guard retail animation bucket");
        auto previous = bucket.Empty() ? head : bucket.hi;
        for (auto node : nodes) {
            const uint64_t links[2] = {head, previous}; // next (never followed), prev
            if (!process.writeBytes(node, links, sizeof(links))) throw std::runtime_error("Cannot link custom animation");
            previous = node;
        }
        // hi first: with hi = ours and lo = head, a miss on our node still ends at head.
        if (!process.writeUInt64(address + 8, nodes.back())) throw std::runtime_error("Cannot publish custom animation");
        if (bucket.Empty() && !process.writeUInt64(address, nodes.front())) throw std::runtime_error("Cannot publish custom animation");
    }
    return entries.size();
}

gameAddr AttachCustomAnimation(GameProcess& source, const EngineAddresses& engine, gameAddr resource, std::span<const Byte> panm,
    gameAddr owner) {
    if (!resource) throw std::invalid_argument("Missing carrier resource");
    // Everything goes through `source`: inside the import's WriteBatch the upload is a batch allocation (released if
    // the import fails or rolls back) and the +0x190 store is staged with the moveset, guarded and published at Commit.
    auto& process = source;
    uint64_t current = 0;
    if (!process.readBytes(resource + 0x190, &current, sizeof(current))) throw std::runtime_error("Cannot read carrier animation");
    // Route 1b: build the compiled animation locally instead of running the engine decoder. Only for the profiled
    // build: the carrier must hold a compiled animation with the expected vtable, and the empty-bucket sentinel
    // must look like EASTL's.
    uint64_t vtable = 0, sentinel[2]{};
    if (!engine.compiledVtable || !current || !process.readBytes(current, &vtable, sizeof(vtable)) || vtable != engine.compiledVtable ||
        !process.readBytes(engine.emptyBucket, sentinel, sizeof(sentinel)) || sentinel[0] != 0 || sentinel[1] != UINT64_MAX)
        throw std::runtime_error("Carrier is not a compiled animation of the supported retail build");
    const auto size = BuildCompiledAnimation(panm, 0, engine).image.size(); // layout size does not depend on the address
    const auto region = process.allocateMem(size);
    if (!region) throw std::runtime_error("Cannot allocate converted animation");
    // Never freed once committed: players keep pointers into it after binding (the engine's own compile leaked the same way).
    const auto built = BuildCompiledAnimation(panm, region, engine);
    if (built.image.size() != size || !process.writeBytes(region, built.image.data(), built.image.size()))
        throw std::runtime_error("Cannot upload converted animation");
    const auto compiled = built.object;
    // Keep the first original: re-attaching replaces only our own previous object. The log is written now; if the
    // batch rolls back, the resource still holds `current`, never `compiled`, so the entry is stale and harmless:
    // RestoreCarriers drops it and a later attach takes the resource's object as the original again.
    auto carriers = ReadCarriers(source.processId);
    auto found = carriers.find(resource);
    const auto original = found != carriers.end() && found->second.attached == current ? found->second.original : current;
    carriers[resource] = {original, compiled, owner};
    WriteCarriers(source.processId, carriers);
    uint64_t value = compiled;
    if (!process.ExpectBytes(resource + 0x190, &current, sizeof(current)) ||
        !process.writeBytes(resource + 0x190, &value, sizeof(value))) throw std::runtime_error("Cannot attach converted animation");
    return compiled;
}

size_t RestoreCarriers(GameProcess& source, gameAddr owner, std::span<const gameAddr> keep) {
    auto carriers = ReadCarriers(source.processId);
    if (carriers.empty()) return 0;
    // Through `source`: inside the import's WriteBatch each restore is staged and guarded, so it is undone with a
    // failed import. Its log entry is kept until a later call sees the resource no longer holds the attached object.
    auto& process = source;
    size_t restored = 0;
    for (auto it = carriers.begin(); it != carriers.end();) {
        const auto& [resource, carrier] = *it;
        if (carrier.owner != owner || std::find(keep.begin(), keep.end(), resource) != keep.end()) { ++it; continue; }
        // Only undo our own write: a rebuilt or freed resource no longer holds the attached object.
        // probeBytes: an unreadable (freed) resource must not fail the caller's batch.
        uint64_t current = 0;
        if (process.probeBytes(resource + 0x190, &current, sizeof(current)) && current == carrier.attached) {
            uint64_t value = carrier.original;
            if (!process.ExpectBytes(resource + 0x190, &current, sizeof(current)) ||
                !process.writeBytes(resource + 0x190, &value, sizeof(value))) throw std::runtime_error("Cannot restore carrier animation");
            ++restored; ++it; continue;
        }
        it = carriers.erase(it);
    }
    WriteCarriers(source.processId, carriers);
    return restored;
}

}

#pragma once
#include "GameProcess.hpp"
#include <array>
#include <set>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <algorithm>
#include <span>

namespace RetailT8Runtime {
// Player-struct offsets, from game_addresses.ini (t8-retail_*).
struct PlayerOffsets {
    uint64_t moveset = 0;          // motbin_offset: moveset root; the four context roots follow at +8..+0x20
    uint64_t character = 0;        // chara_id_offset: character id; the secondary identity follows at +4
    uint64_t currentMove = 0;      // currmove: pointer to the current move
    uint64_t currentMoveTimer = 0; // currmove_timer: current move frame
    uint64_t nextMove = 0;         // nextmove_addr: queued next move pointer
    uint64_t nextMoveId = 0;       // nextmove_id: queued next move id
};
inline bool Pointer(uint64_t p) { return p >= 0x10000 && p < 0x800000000000ULL; }
template<class T> T Read(const GameProcess& process, gameAddr address) {
    T value{};
    if (!Pointer(address) || sizeof(T) > 0x800000000000ULL - address || !process.probeBytes(address, &value, sizeof(value)))
        throw std::runtime_error("Unreadable retail runtime field");
    return value;
}
inline uint64_t Hash(uint32_t key) {
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (unsigned i = 0; i < 4; ++i) { hash ^= (key >> (8 * i)) & 255; hash *= 0x100000001b3ULL; }
    return hash;
}
// Native body handles encode their character domain independently of the
// player's visual/requirement identity: EF00 CC00 IIIIIIII.
inline uint32_t AnimationCharacter(std::span<const Byte> moves, uint32_t fallback) {
    if (moves.size() % 0x448) throw std::runtime_error("Invalid retail move resource dimensions");
    uint32_t character = UINT32_MAX;
    for (size_t row = 0; row < moves.size() / 0x448; ++row) {
        uint64_t handle; memcpy(&handle, moves.data() + row * 0x448 + 0x50, sizeof(handle));
        if (!(handle >> 32)) continue; // Legacy unqualified handle.
        if ((handle & 0xffff00ff00000000ULL) != 0xef00000000000000ULL)
            throw std::runtime_error("Unsupported retail animation handle domain");
        const auto domain = static_cast<uint32_t>((handle >> 40) & 255);
        if (domain == 255 || (character != UINT32_MAX && character != domain))
            throw std::runtime_error("Mixed retail animation domains cannot be represented by this snapshot");
        character = domain;
    }
    return character == UINT32_MAX ? fallback : character;
}
// MSVC unordered-map bucket endpoints; finite traversal, including corrupt cycles.
inline gameAddr Lookup(const GameProcess& p, gameAddr head, gameAddr buckets, uint64_t mask, uint32_t key, bool hashed = true) {
    if (!Pointer(head) || !Pointer(buckets) || mask > 0x1fffff || ((mask + 1) & mask))
        throw std::runtime_error("Invalid retail resource map");
    auto bucket = buckets + ((hashed ? Hash(key) : key) & mask) * 16;
    const auto first = Read<uint64_t>(p, bucket);
    auto node = Read<uint64_t>(p, bucket + 8);
    std::set<gameAddr> seen;
    while (node != head) {
        if (!Pointer(node) || !seen.insert(node).second || seen.size() > 16384)
            throw std::runtime_error("Invalid retail map chain");
        if (Read<uint32_t>(p, node + 0x10) == key) return node;
        if (node == first) return 0;
        node = Read<uint64_t>(p, node + 8);
    }
    return 0;
}
// Retail 26155785: slots are separately allocated, never a fixed player stride.
// The native resource cache can retain a compiled animation after its original
// raw PANM buffer has been replaced. The owning ANMBIN map retains the current
// archive; its pool pointers have already been relocated by the engine.
inline gameAddr ArchivePanm(const GameProcess& process, gameAddr map, uint32_t instance, uint32_t type, uint64_t bytes = 8, uint64_t* remaining = nullptr) {
    if (type >= 6) throw std::runtime_error("Invalid retail animation category");
    auto q = [&](gameAddr address) { return Read<uint64_t>(process, address); };
    auto job = q(map);
    if (!job) return 0;
    auto resource = q(job + 8);
    if (!resource) return 0;
    auto owner = q(resource + 0x118);
    if (!owner) return 0;
    auto data = q(owner), size = q(owner + 8);
    if (!data || size < 0x98 || size > 0x80000000ULL || !Pointer(data) || size > 0x800000000000ULL - data) return 0;
    auto inside = [&](uint64_t pointer, uint64_t length) { return pointer >= data && pointer - data <= size && length <= size - (pointer - data); };
    auto count = Read<uint32_t>(process, data + 4 + type * 4);
    if (!count) return 0;
    auto pool = q(data + 0x38 + type * 8);
    if (count > 100000 || !inside(pool, uint64_t(count) * 0x38)) throw std::runtime_error("Invalid native ANMBIN pool extent");
    std::vector<Byte> records(size_t(count) * 0x38);
    if (!records.empty() && !process.probeBytes(pool, records.data(), records.size())) throw std::runtime_error("Unreadable native ANMBIN pool");
    for (size_t row = 0; row < count; ++row) {
        uint32_t key; memcpy(&key, records.data() + row * 0x38, sizeof(key));
        if (key != instance) continue;
        uint64_t panm; memcpy(&panm, records.data() + row * 0x38 + 8, sizeof(panm));
        if (!panm) continue; // Shared reference; try the common archive.
        if (!inside(panm, (std::max)(bytes, uint64_t(8)))) throw std::runtime_error("Invalid native ANMBIN animation pointer");
        if (Read<uint32_t>(process, panm + 4) == 0x4d4e4150) {
            if (remaining) *remaining = size - (panm - data);
            return panm;
        }
    }
    return 0;
}
inline bool HasCharacterAnimations(const GameProcess& process, gameAddr container, uint32_t character) {
    auto q=[&](gameAddr p){return Read<uint64_t>(process,p);};
    auto node=Lookup(process,q(container+8),q(container+0x18),q(container+0x30),character);
    if(!node)return false;
    auto begin=q(node+0x18),end=q(node+0x20);
    if(end<begin || end-begin>0x10000 || (end-begin)%8)throw std::runtime_error("Invalid retail resource vector");
    if(begin==end)return false;
    auto map=q(begin);
    if(!map)return false;
    auto count=q(map+0x18);
    return count && count<=100000 && Pointer(q(map+0x10));
}
// The animation map readers use for a domain: vector[0] of the container's domain entry (0x81 shared, else the
// character's animation domain). 0 when the domain has no map.
// Address of that vector[0] slot (to guard it), 0 when the domain has no map.
inline gameAddr DomainMapSlot(const GameProcess& process, gameAddr container, uint32_t domain) {
    auto q = [&](gameAddr address) { return Read<uint64_t>(process, address); };
    const auto node = Lookup(process, q(container + 8), q(container + 0x18), q(container + 0x30), domain);
    if (!node) return 0;
    const auto begin = q(node + 0x18), end = q(node + 0x20);
    if (end < begin || end - begin > 0x10000 || (end - begin) % 8) throw std::runtime_error("Invalid retail resource vector");
    return begin == end ? 0 : begin;
}
inline gameAddr DomainMap(const GameProcess& process, gameAddr container, uint32_t domain) {
    const auto slot = DomainMapSlot(process, container, domain);
    return slot ? Read<uint64_t>(process, slot) : 0;
}
// Instance-map bucket of `key` (MSVC unordered_map: list head +0x10, buckets +0x20 as {lo, hi}, mask +0x38).
struct MapBucket { gameAddr address = 0, lo = 0, hi = 0, head = 0; bool Empty() const { return lo == head && hi == head; } };
inline MapBucket Bucket(const GameProcess& process, gameAddr map, uint32_t key) {
    auto q = [&](gameAddr address) { return Read<uint64_t>(process, address); };
    const auto head = q(map + 0x10), buckets = q(map + 0x20), mask = q(map + 0x38);
    if (!Pointer(head) || !Pointer(buckets) || mask > 0x1fffff || ((mask + 1) & mask)) throw std::runtime_error("Invalid retail resource map");
    MapBucket bucket; bucket.head = head; bucket.address = buckets + (Hash(key) & mask) * 16;
    bucket.lo = q(bucket.address); bucket.hi = q(bucket.address + 8);
    return bucket;
}
// Node for `key` in an instance map, the way the game's readers search (bucket high end, then prev links).
inline gameAddr MapNode(const GameProcess& process, gameAddr map, uint32_t key) {
    return Lookup(process, Read<uint64_t>(process, map + 0x10), Read<uint64_t>(process, map + 0x20), Read<uint64_t>(process, map + 0x38), key);
}
// Resource object an instance resolves to for the game's readers: shared domain first, then `domain`. 0 if none.
inline gameAddr ResolveAnimation(const GameProcess& process, gameAddr container, uint32_t domain, uint32_t id) {
    for (uint32_t key : {uint32_t(0x81), domain}) {
        const auto map = DomainMap(process, container, key);
        if (!map) continue;
        if (const auto node = MapNode(process, map, id)) {
            const auto entry = Read<uint64_t>(process, node + 0x18);
            return entry ? Read<uint64_t>(process, entry + 8) : 0;
        }
    }
    return 0;
}
inline gameAddr Panm(const GameProcess& process, gameAddr container, uint32_t character, uint32_t instance, uint32_t type, uint64_t bytes = 8) {
    auto q = [&](gameAddr address) { return Read<uint64_t>(process, address); };
    auto idMap = [&](uint32_t key) -> gameAddr {
        auto node = Lookup(process, q(container + 8), q(container + 0x18), q(container + 0x30), key);
        if (!node) return 0;
        auto begin = q(node + 0x18), end = q(node + 0x20);
        if (begin == end) return 0;
        if (end < begin || end - begin > 0x10000 || (end - begin) % 8) throw std::runtime_error("Invalid retail resource vector");
        return q(begin);
    };
    for (auto map : {idMap(0x81), idMap(character)}) {
        if (!map) continue;
        // Auxiliary PANMs can be resident in the retained native archive before
        // the engine creates their lazy compiled-cache entries. Byte identity
        // depends on the bounded archive, not on that cache having been touched.
        if (auto archived = ArchivePanm(process, map, instance, type, bytes)) return archived;
        auto node = Lookup(process, q(map + 0x10), q(map + 0x20), q(map + 0x38), instance);
        if (!node) continue;
        auto entry = q(node + 0x18);
        if (!entry) continue;
        auto resource = q(entry + 8);
        if (!resource) continue;
        auto panm = q(resource + 0x1a0);
        if (panm && Read<uint32_t>(process, panm + 4) == 0x4d4e4150) return panm;
    }
    return 0;
}
inline std::array<gameAddr, 4> Players(const GameProcess& p, gameAddr registrySlot, gameAddr keySlot) {
    std::array<gameAddr, 4> players{};
    const auto registry = Read<uint64_t>(p, registrySlot);
    if (!registry) return players;
    const auto map = Read<uint64_t>(p, registry + 0x10);
    if (!map) return players;
    auto node = Lookup(p, Read<uint64_t>(p, map + 0x100), Read<uint64_t>(p, map + 0x110),
        Read<uint64_t>(p, map + 0x128), Read<uint32_t>(p, keySlot), false);
    if (!node) return players;
    const auto round = Read<uint64_t>(p, node + 0x18);
    if (!round) return players;
    const auto list = Read<uint64_t>(p, round);
    if (!list) return players;
    const auto begin = Read<uint64_t>(p, list + 8), end = Read<uint64_t>(p, list + 0x10);
    if (end < begin || (end - begin) % 8 || end - begin > 64 * 8)
        throw std::runtime_error("Invalid retail player list");
    for (auto at = begin; at < end; at += 8) {
        const auto entry = Read<uint64_t>(p, at);
        if (!entry) continue;
        const auto player = entry + 0x90;
        const auto slot = Read<uint32_t>(p, player + 4);
        if (slot >= players.size()) continue;
        if (players[slot]) throw std::runtime_error("Duplicate retail player slot");
        players[slot] = player;
    }
    return players;
}
}

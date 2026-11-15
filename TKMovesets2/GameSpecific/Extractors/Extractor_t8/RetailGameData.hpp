#pragma once
#include "MovesetStructs.h"
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The game's own animation archives, read from disk: the source for every animation a running game has not loaded.
// tkdata.bin (`__TEKKEN8FILES__`) holds mothead/bin/<code>.anmbin and com.anmbin (the shared domain 0x81). Its format is
// documented in G:/Dev/Retroingenierie/Tekken8 cnt/docs/formats/tkdata.md; on retail 26155785 the PANMs inside are
// byte-identical to the ones the game holds in memory (verify-anmbin-t8 on a Kazuya export: 1764/1764).
namespace RetailT8GameData {
// <install>/Polaris/Binaries/Win64/<exe> -> <install>/Polaris/Content/Binary/pak/tkdata.bin
std::filesystem::path TkdataPath(const std::filesystem::path& gameExecutable);
// One file of a tkdata.bin, decrypted and decompressed. Throws when the archive or the file is missing or invalid.
std::vector<Byte> ReadTkdataFile(const std::filesystem::path& tkdata, std::string_view vfsPath);
// The VFS paths among `candidates` that the archive holds (its table of contents stores name hashes only).
std::vector<std::string> TkdataContains(const std::filesystem::path& tkdata, const std::vector<std::string>& candidates);
// Path of the running retail game executable, found by process name (no handle with memory access is opened).
std::optional<std::filesystem::path> RunningGameExecutable();
// Every character moveset in the archive: all 3-character codes ([a-z0-9]) probed as mothead/bin/<code>.motbin (the
// table of contents holds name hashes only), each with its in-match fighter id from the motbin header identity
// (+0x164 = id << 16 | (0x10000 - id)). The archive itself is the authority for which characters exist and their ids;
// display names come from t8-retail_names.json. Cached per archive path (same size and time). Sorted by id.
struct TkdataFighter { std::string code; uint32_t id = 0; };
const std::vector<TkdataFighter>& TkdataRoster(const std::filesystem::path& tkdata);
// Archive code of an in-match fighter id from the roster above; empty when unknown or the archive is unreadable
std::string TkdataCodeFor(const std::filesystem::path& tkdata, uint32_t fighterId);

// A parsed .anmbin: PANMs by (category, instance).
class Anmbin {
    std::vector<Byte> m_bytes;
    std::map<std::pair<uint32_t, uint32_t>, std::vector<uint64_t>> m_entries;
    std::set<std::pair<uint32_t, uint32_t>> m_declared; // every identity listed, with data or not
    std::vector<std::pair<uint32_t, uint32_t>> m_order;  // the same, in record order
public:
    explicit Anmbin(std::vector<Byte> bytes);
    // The PANM of (category, instance), bounded by its own extent. nullopt when the archive has none. Throws when the
    // archive holds several different blobs for it (no way to tell which one the game uses).
    std::optional<std::span<const Byte>> Panm(uint32_t type, uint32_t instance) const;
    size_t Count() const { return m_entries.size(); }
    // Listed by the archive (a record exists), whether or not it has data here.
    bool Declared(uint32_t type, uint32_t instance) const { return m_declared.count({type, instance}) != 0; }
    // Every (category, instance) this archive holds data for, in order.
    std::vector<std::pair<uint32_t, uint32_t>> Identities() const;
    // Every (category, instance) the archive declares, with data or not, in record order.
    std::vector<std::pair<uint32_t, uint32_t>> DeclaredIdentities() const { return m_order; }
    // Reference list `type` (count at header +0x1c + 4*type, offsets at +0x68 + 8*type): list 0 maps a move's animation
    // index to its body animation id.
    std::vector<uint32_t> List(uint32_t type) const;
};

// Lazily opened com.anmbin + <code>.anmbin of one install. Lookup order is the game's (ResolveBodyAnim 0x145A9ADD0):
// the shared domain first, then the character's.
class Source {
    std::filesystem::path m_tkdata;
    std::function<std::string(uint32_t)> m_code;   // animation character (domain) -> motbin code ("grl")
    std::map<std::string, std::shared_ptr<Anmbin>> m_open;
    std::shared_ptr<Anmbin> Open(const std::string& code);
public:
    Source(std::filesystem::path tkdata, std::function<std::string(uint32_t)> code)
        : m_tkdata(std::move(tkdata)), m_code(std::move(code)) {}
    // Copy of the PANM the game would load for (domain, category, instance). nullopt when the archives declare it but
    // hold no data for it anywhere (the game itself has no bytes to load: ~9 such slots per character on 26155785).
    // Throws when no archive declares it at all.
    std::optional<std::vector<Byte>> Panm(uint32_t domain, uint32_t type, uint32_t instance);
    const std::filesystem::path& Tkdata() const { return m_tkdata; }
};
}

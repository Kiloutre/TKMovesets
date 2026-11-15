#pragma once
#include "GameProcess.hpp"
#include "MovesetStructs.h"
#include <array>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <span>
#include <vector>

namespace RetailT8Snapshot {
inline constexpr uint64_t TablesOnlyFlag = 1ULL << 32;
inline constexpr uint64_t RuntimeAssetsFlag = 1ULL << 33;
inline constexpr size_t SourceHeaderSize = 0x318;
// Address recorded for an animation the game had not loaded at export, read from the game's own tkdata.bin instead
// (RetailGameData.hpp). Not a game address: never dereferenced, only told apart from captured ones.
inline constexpr uint64_t ArchiveAssetAddress = 0x00007FFFFFFE0000ULL;
struct TableSpec { const char* name; uint32_t pointerOffset, countOffset, stride; };
// Retail source-header pointer/count offsets and record strides, checked against live exports.
// The reactions pointer has an extra qword before its count; it is not a uniform pair.
inline constexpr std::array<TableSpec, 20> Tables = {{
    {"reactions",0x168,0x178,0x70}, {"requirements",0x180,0x188,0x14},
    {"hit_conditions",0x190,0x198,0x18}, {"projectiles",0x1a0,0x1a8,0xe0},
    {"pushbacks",0x1b0,0x1b8,0x10}, {"pushback_extra",0x1c0,0x1c8,2},
    {"cancels",0x1d0,0x1d8,0x28}, {"group_cancels",0x1e0,0x1e8,0x28},
    {"cancel_extra",0x1f0,0x1f8,4}, {"extra_properties",0x200,0x208,0x28},
    {"start_properties",0x210,0x218,0x20}, {"end_properties",0x220,0x228,0x20},
    {"moves",0x230,0x238,0x448}, {"voiceclips",0x240,0x248,0xc},
    {"input_sequences",0x250,0x258,0x10}, {"inputs",0x260,0x268,8},
    {"parry",0x270,0x278,4}, {"cameras",0x280,0x288,0xc},
    {"throws",0x290,0x298,0x10}, {"dialogues",0x2a0,0x2a8,0x18}
}};
struct Header {
    char signature[4]; uint32_t version;
    uint64_t sourceAddress, sourceHeaderOffset, directoryOffset, size;
};
struct Entry { uint64_t address, count, offset; uint32_t stride, reserved; };
struct AssetDirectory { char signature[4]; uint32_t count; uint64_t entriesOffset, dataOffset, reserved; };
struct AssetEntry { uint64_t address, offset, size; uint32_t instance, type; };
static_assert(sizeof(Header) == 40 && sizeof(Entry) == 32);
static_assert(sizeof(AssetDirectory) == 32 && sizeof(AssetEntry) == 32);
// Version 4 appends a T8RX extension after the animation data: what a memory-only import otherwise reads from a
// live source root. Offsets are relative to the extension start, so editors can carry the block verbatim.
// Layout: ExtensionHeader, moveCount x {u32 name, u32 animation} string offsets (UINT32_MAX = null), the
// NUL-terminated string blob, cancelCount x ExternalCancelEntry, then the 40-byte cancel lists. crc32 covers
// every byte after the header.
struct ExtensionHeader {
    char signature[4]; uint32_t version;
    uint64_t size, moveCount, stringsOffset, stringsSize, cancelsOffset, cancelCount;
    uint32_t crc32, reserved;
};
struct ExternalCancelEntry { uint32_t row, field; uint64_t offset, size; };
static_assert(sizeof(ExtensionHeader) == 64 && sizeof(ExternalCancelEntry) == 24);
// Per captured move row: its name (+0x40) and animation (+0x48) strings; external cancel lists keyed by
// (captured row, move field 0x98/0xa0/0xb0/0xc0), entries verbatim (pointers are capture-time table addresses).
struct NativeData {
    std::vector<std::optional<std::string>> names, animations;
    std::map<std::pair<uint32_t, uint32_t>, std::vector<Byte>> cancels;
};
std::vector<Byte> BuildExtension(const NativeData& data);
// Validated extension of a v4 payload; nullopt for v1-v3.
std::optional<NativeData> ReadExtension(std::span<const Byte> payload);
// The raw extension block (empty for v1-v3), and appending one to a rebuilt payload (sets version 4).
std::span<const Byte> ExtensionBytes(std::span<const Byte> payload);
void AppendExtension(std::vector<Byte>& payload, std::span<const Byte> extension);
// Move fields that may point at a cancel list outside the cancel table.
inline constexpr uint32_t ExternalCancelFields[] = {0x98, 0xa0, 0xb0, 0xc0};
// Strings and external cancel lists of a captured snapshot's source, read from the live process (v4 export).
NativeData CaptureNative(const GameProcess& process, std::span<const Byte> snapshot);
void Validate(std::span<const Byte> payload);
std::vector<Byte> Capture(const GameProcess& process, gameAddr address);
// Animations the game has not loaded come from `archive(domain, category, instance)` (the game's tkdata.bin). It returns
// nullopt for an animation the game's archives declare without any data (recorded as a size-0 marker: there are no bytes
// to carry) and throws when none declares it (the export stops). Without a reader (tests, no-archive) they are markers.
using ArchiveReader = std::function<std::optional<std::vector<Byte>>(uint32_t domain, uint32_t type, uint32_t instance)>;
std::vector<Byte> CaptureComplete(const GameProcess& process, gameAddr address, gameAddr manager, gameAddr container, uint32_t character, uint8_t categories = 63,
    const ArchiveReader& archive = {});
// A v4 snapshot of a moveset image plus explicitly given animations (a moveset built from the game's files rather than
// captured from its animation maps). An empty `panm` records an animation without data (size-0 marker).
struct GivenAnimation { uint32_t type, instance; std::vector<Byte> panm; };
std::vector<Byte> CaptureWithAnimations(const GameProcess& process, gameAddr address, const std::vector<GivenAnimation>& animations);
uint64_t PanmSize(std::span<const Byte> prefix);
std::vector<AssetEntry> Assets(std::span<const Byte> payload);
// Tagged sections in the outer header's extension area (after TKMovesetHeader, up to header_size): each is a 16-byte
// {signature, version, size, crc32 of the data} header, then the data, 8-aligned. Known tags: "T8ED" (editor
// provenance, RetailEditorData) and "T8NT" (notes: UTF-8 text about the file, e.g. animations it could not carry;
// written only when there is something to say). Unknown tags are rejected (a damaged tag must not be ignored).
struct OuterSection { std::array<char, 4> tag{}; uint32_t version = 1; std::vector<Byte> data; };
inline constexpr std::array<char, 4> EditorTag{'T','8','E','D'}, NotesTag{'T','8','N','T'}, NamesTag{'T','8','N','M'};
// "T8NM" (names): retail records hold only Kamui hashes of move and animation names (move fields 0x00 and 0x20), so
// the file carries the strings itself: hash -> name. Data: u32 count, then count x {u32 hash, u32 length, bytes}
// sorted by hash. Written by memory/disk export (captured strings, else the shipped t8-retail name table) and by
// T7 -> T8 conversion (the T7 names that were hashed).
using NameMap = std::map<uint32_t, std::string>;
OuterSection NamesSection(const NameMap& names);
// The names of a file (empty when it has no names section).
NameMap Names(std::span<const Byte> file);
// Per move row of a payload: decoded name hash (0x00) and animation hash (0x20).
struct MoveNameHashes { std::optional<uint32_t> name, animation; };
std::vector<MoveNameHashes> MoveHashes(std::span<const Byte> payload);
// Names for every move/animation hash of `payload`: the v4 extension's captured strings first, then `lookup`.
NameMap CollectNames(std::span<const Byte> payload, const std::function<const std::string*(uint32_t)>& lookup);
// `file` starts at the TKMovesetHeader and holds at least header_size bytes. Throws on a malformed section.
std::vector<OuterSection> ReadOuterSections(std::span<const Byte> file);
// The notes of a file (empty when it has none).
std::string Notes(std::span<const Byte> file);
OuterSection NotesSection(std::string_view text);
// `properties`: the custom property list (import profile); nullptr writes the default profile.
std::vector<Byte> Serialize(std::span<const Byte> payload, TKMovesetHeader header, const std::vector<OuterSection>& sections = {},
    const std::vector<TKMovesetProperty>* properties = nullptr);
std::span<const Byte> Table(std::span<const Byte> payload, size_t index);
}

#pragma once
#include "ImportProfile.hpp"
#include "Extractor_t8/RetailSnapshot.hpp"
#include <string>
#include <functional>
#include <set>
#include <map>
#include <optional>
#include <vector>
#include "Editor_t8Retail/RetailEditorData.hpp"

// First import stage: an offline table graph, not a gameplay-ready moveset.
// No process access, allocation, publication, or animation registration occurs here.
namespace RetailT8Import {
struct PointerField { size_t table; uint32_t offset; size_t target; };
inline constexpr PointerField PointerFields[] = {
    {0,0,4},{0,8,4},{0,16,4},{0,24,4},{0,32,4},{0,40,4},{0,48,4},
    {2,0,1},{2,16,0}, {3,0x90,2},{3,0x98,6}, {4,8,5},
    {6,8,1},{6,16,8},{7,8,1},{7,16,8},
    {9,8,1},{10,0,1},{11,0,1},
    {12,0x98,6},{12,0xa0,6},{12,0xb0,6},{12,0xc0,6},
    {12,0x110,2},{12,0x130,13},{12,0x138,9},{12,0x140,10},{12,0x148,11},
    {14,8,15},{18,8,17},{19,8,1}
};
struct Relocation { uint64_t field, target; };
struct Dependency {
    uint64_t field, source;
    std::string kind;
};
struct Issue { size_t table; uint64_t record; uint32_t field; uint64_t source; size_t targetTable; std::string reason; };
struct Plan {
    std::vector<Byte> tables;
    std::vector<Relocation> relocations;
    std::vector<Dependency> dependencies;
    std::vector<Issue> issues;
    uint64_t issueCount = 0, availableAnimations = 0, unavailableAnimations = 0;
    std::array<uint64_t, RetailT8Snapshot::Tables.size()> offsets{}, counts{};
    bool Ok() const { return issueCount == 0; }
    // Resolves only the documented table graph. External slots remain zero;
    // animation handles and encrypted values are retained verbatim. Never publish
    // this buffer to a player until runtime dependencies/lifetimes are implemented.
    std::vector<Byte> RelocateTables(gameAddr base) const;
};
using ExternalCancels = std::function<std::vector<Byte>(uint64_t record, uint32_t field)>;
// One 40-byte cancel of an external list (read from a live source or the file's capture), rebased onto the export's
// tables. `source(table)` gives the base address and row count the entry's pointers refer to; `place(table, id)` the
// address to write for an export row. A pointer outside the source tables throws (the source is not a moveset).
// A cancel whose requirements, extradata, move or input sequence the edit deleted is dropped (nullopt); the list's
// terminator is never dropped, a deleted dependency of it is cleared instead.
std::optional<std::vector<Byte>> RebaseExternalCancel(std::vector<Byte> entry,
    const std::function<std::pair<uint64_t, uint64_t>(size_t table)>& source,
    std::span<const RetailT8Snapshot::Entry> entries, const RetailEditorData::Metadata* metadata,
    const std::function<uint64_t(size_t table, uint64_t id)>& place);
// An empty external cancel list: one terminator (command 0x8000). It copies the first terminator of the file's own cancel
// table, rebased with `place`, so its requirement pointer is a valid imported row; a bare 0x8000 entry when the table
// has none. Used when no loaded moveset can provide a list the file does not carry.
std::vector<Byte> TerminatorCancel(std::span<const Byte> payload, std::span<const RetailT8Snapshot::Entry> entries,
    const std::function<uint64_t(size_t table, uint64_t id)>& place);
// Move identities stay at their original index. Animation resources may be
// shared by another row after native runtime substitutions.
// How far a loaded moveset is from the export: rows whose identity differs, and moves whose animation it does not
// play (those are bound to `fallbackRow`, its idle move).
struct BindStats { uint64_t identityMismatches = 0, missingAnimations = 0; };
std::vector<size_t> BindAnimations(std::span<const Byte> captured, std::span<const Byte> loaded, bool convertedT7 = false,
    const RetailEditorData::Metadata* metadata = nullptr, size_t fallbackRow = 0, BindStats* stats = nullptr);
// A converted T7 move whose carrier resource is not loaded would play the carrier's unrelated native
// animation. Rebind those rows (handle instance in `missing`) to the idle move's animation. Returns the count.
// The resource the game's body-animation resolver (0x145A9ADD0) returns for `id`: the shared domain 0x81 entry when it
// is ready, else the `domain` entry when ready; 0 when neither is. Ready = finished compiled animation (request done,
// compiled object with the game's vtable). Binding and the image audit both use it, so a move is never bound to an
// entry the game would skip.
// `compiledVtable`: the game's compiled animation vtable (RetailT8Resources::EngineAddresses).
gameAddr ReadyAnimation(const GameProcess& process, gameAddr container, uint32_t domain, uint32_t id, gameAddr compiledVtable);
uint64_t FallBackMissingCarriers(std::vector<size_t>& rows, std::span<const Byte> payload, const std::set<uint32_t>& missing);
Plan Prepare(const TKMovesetHeader& header, std::span<const Byte> payload, const ExternalCancels& external = {});
struct Audit { uint64_t bytes = 0, externalLists = 0, animations = 0; };
// Memory-only imports: the expected body handle per move, the target row lending move strings, and where the
// handles must resolve (each to a finished AnimResource with a compiled animation and its PANM copy).
struct MemoryOnlyAudit {
    gameAddr container = 0; uint32_t domain = 0; uint64_t stringRow = 0; std::vector<uint64_t> handles;
    // v4 native snapshots: localised move strings per row (nullopt or empty: the target row's), and the captured
    // external cancel lists (keyed by captured row and field) instead of the live source's.
    std::vector<std::optional<std::string>> names, animations;
    const std::map<std::pair<uint32_t, uint32_t>, std::vector<Byte>>* cancels = nullptr;
    // Ids the game's archives declare without data: the only handles allowed not to resolve.
    const std::set<uint32_t>* dataless = nullptr;
    // The game's compiled animation vtable (ImporterT8::Engine()); required.
    gameAddr compiledVtable = 0;
};
// Swaps the source and target identities in self-character checks: primary checks (220/221, 499/500) use `target`
// (player +0x168), secondary checks (224/225, 503/504) use `targetSecondary` (player +0x16c; UINT32_MAX = `target`).
uint64_t AdaptCharacterRequirements(std::span<Byte> records, uint32_t source, uint32_t target, uint32_t targetSecondary = UINT32_MAX);
// Import profile (docs/import-profile.md): the identity the moveset acts as (its own, or a fixed character), and the
// requirement rows the profile decides (gender 689/690, story flag 777 when set) rewritten to always / never true.
uint32_t ProfileIdentity(const ImportProfile::Profile& profile, uint32_t own);
uint64_t ApplyProfileConditions(std::span<Byte> records, const ImportProfile::Profile& profile);
// Stripped extra properties (ImportProfile::StripsProperty) move to a frame no move reaches.
uint64_t ApplyProfileProperties(std::span<Byte> records, const ImportProfile::Profile& profile);
// Converted T7 files carry a placeholder character identity; give the header (+0x160 code(id - 1), +0x164 code(id))
// and every move (+0xd0, encrypted code(id - 1)) the target's, with code(n) = n << 16 | (0x10000 - n), as in the
// target's native moveset. `header` is the 0x318-byte source header, `moves` the 0x448-byte move records.
void ApplyConvertedIdentity(std::span<Byte> header, std::span<Byte> moves, uint32_t target);
Audit AuditImage(std::span<const Byte> payload, const GameProcess& image, gameAddr base, uint64_t size,
    const GameProcess& live, gameAddr originalRoot, uint32_t sourceCharacter = UINT32_MAX, uint32_t targetCharacter = UINT32_MAX,
    bool convertedT7 = false, const RetailEditorData::Metadata* metadata = nullptr, const std::set<uint32_t>* missingCarriers = nullptr,
    const MemoryOnlyAudit* memoryOnly = nullptr, const ImportProfile::Profile* profile = nullptr, uint32_t targetSecondary = UINT32_MAX,
    const std::map<std::pair<uint32_t, uint32_t>, std::vector<Byte>>* fallbackCancels = nullptr);
}

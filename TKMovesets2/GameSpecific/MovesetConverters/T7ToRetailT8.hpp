#pragma once
#include "RetailT8Records.hpp"
#include "MovesetStructs.h"
#include "ImportProfile.hpp"
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <nlohmann/json_fwd.hpp>

// Shared by offline conversion and the retail import preflight. This intermediate
// uses T7 row indices (UINT64_MAX = null), never process addresses. It is not T8RS.
namespace T7ToRetailT8 {
// Writes `value` as the game holds an encrypted retail move field in memory (value, key, four check slots), for move
// `row` at record offset `offset`. Shared with the moveset built from the game's files (RetailDiskMoveset).
void EncodeRetailField(std::vector<Byte>& out, uint64_t record, uint64_t row, uint32_t offset, uint32_t value);
struct Table {
    uint64_t count = 0, sourceOffset = 0;
    uint32_t sourceStride = 0, targetStride = 0;
};
struct Reference {
    size_t table; uint64_t row; uint32_t sourceField, targetField;
    size_t targetTable; uint64_t index;
};
struct MoveResource {
    uint64_t row, animationOffset;
    std::string name, animation;
};
struct GuardedProperty {
    uint64_t requirement;
    uint32_t id;
    std::array<uint32_t, 4> parameters{};
};
struct MappingIssue {
    size_t table; uint64_t row; uint32_t id, parameter;
    std::string reason;
};
struct IdAlignment {
    size_t table; uint64_t row; uint32_t source, target;
    bool parametersConverted;
    const char* confidence;
    const char* provenance;
};
struct Plan {
    std::array<Table, 20> tables{};
    std::vector<Reference> references;
    std::vector<MoveResource> moves;
    std::vector<RetailT8::Requirement> requirements;
    std::vector<RetailT8::Property> properties;
    std::vector<GuardedProperty> startProperties, endProperties;
    std::vector<MappingIssue> issues;
    std::vector<IdAlignment> idAlignments;
    uint64_t issueCount = 0, character = 0, animationBytes = 0, motaBytes = 0, movelistBytes = 0;
    bool RecordsConverted() const { return issueCount == 0; }
    // A record plan alone has neither a materialized image nor native bindings.
    bool Loadable() const { return false; }
    std::string ImportBlocker() const;
};
Plan Build(const TKMovesetHeader& header, const StructsT7::TKMovesetHeaderBlocks& blocks,
    std::span<const Byte> payload);
nlohmann::json Report(const Plan& plan);
// Emits a retail container using native T8 animation names. Unsupported record
// semantics fail before output; import independently checks every native binding.
std::vector<Byte> Convert(const TKMovesetHeader& header, const StructsT7::TKMovesetHeaderBlocks& blocks,
    std::span<const Byte> payload);

// Full self-contained conversion (T7ToRetailT8Standalone.cpp): every record is built from the T7 moveset and fixed
// retail tables shipped with the tool; every animation is converted from T7 onto the built-in skeleton. No other
// moveset is read, so the output does not depend on the import target. The file's character is a placeholder
// identity (124) that the importer swaps for the target's. Converted animations need the memory-only import.
// Never set by the standalone conversion (always 0): pairedMoves, animationsByName, animationsByMap,
// carriersAvailable, embeddedNativeAnimations, voiceclipsDropped.
struct ConversionReport {
    uint64_t moves = 0, pairedMoves = 0, animationsByName = 0, animationsByMap = 0, animationsFallback = 0;
    uint64_t requirementsVoted = 0, requirementsCrossVoted = 0, requirementsCatalog = 0, requirementsUnresolved = 0, requirementsAssumedTrue = 0;
    uint64_t propertiesVoted = 0, propertiesCrossVoted = 0, propertiesCatalog = 0, propertiesDropped = 0;
    uint64_t walkCancelsRetimed = 0, voiceclipsConverted = 0, voiceclipsDropped = 0, packedHitboxes = 0;
    uint64_t projectilesDropped = 0, projectileSpawns = 0, throwsDropped = 0, votedRequirementIds = 0, votedPropertyIds = 0;
    uint64_t animationsConverted = 0, carriersAvailable = 0, animationsUnconverted = 0, animationsConversionFailed = 0, customAnimationBytes = 0, animationTemplateBytes = 0, embeddedNativeAnimations = 0;
    uint64_t animationsLengthRejected = 0, animationsSynthetic = 0;
    // Hand animations (T7 hand MOTAs): distinct (hand, animation) pairs converted, ones that did not convert (their
    // properties are dropped), and the hand-animation property rows kept.
    uint64_t handAnimationsConverted = 0, handAnimationsFailed = 0, handAnimationProperties = 0;
    // Camera animations (T7 camera MOTAs), counted the same way.
    uint64_t cameraAnimationsConverted = 0, cameraAnimationsFailed = 0, cameraAnimationProperties = 0;
    // Wing animations (T7 wing MOTAs), counted the same way.
    uint64_t wingAnimationsConverted = 0, wingAnimationsFailed = 0, wingAnimationProperties = 0;
    // Face animations (T7 face MOTAs, onto the legacy 43-bone face rig), counted the same way.
    uint64_t faceAnimationsConverted = 0, faceAnimationsFailed = 0, faceAnimationProperties = 0;
    uint64_t requirementPropertiesDropped = 0; // unmapped property calls (IDs >= 0x8000) in requirement lists, dropped
    bool idleTemplate = false;
    uint32_t convertedCharacter = 0, sourceCharacter = 0; // convertedCharacter: the output's placeholder identity
    std::vector<std::pair<uint32_t, uint64_t>> unresolvedRequirementIds, droppedPropertyIds;
    std::vector<std::string> convertedMoves;
};
nlohmann::json ConversionReportJson(const ConversionReport& report);
// Placeholder character of ConvertStandalone output (unused by retail characters); the importer swaps it for the target.
inline constexpr uint32_t ConvertedCharacter = 124;
// `profile`: the source file's import profile (ImportProfile::Read): the rules' setting(...) values, and it travels to
// the output (a fixed character in T8 numbering).
std::vector<Byte> ConvertStandalone(const TKMovesetHeader& header, const StructsT7::TKMovesetHeaderBlocks& blocks,
    std::span<const Byte> payload, ConversionReport* report = nullptr, bool convertAnimations = true,
    const ImportProfile::Profile& profile = {});

// Incremental conversion for live editing a T7 moveset imported into retail T8. Built once with the full conversion
// (File() holds the same bytes ConvertStandalone returns, animations included); then UpdateRow converts a single
// edited T7 row with the same rules and writes its records into File(), with no full conversion. Most tables convert
// row for row. Property lists (9, 10, 11) and voiceclips (13) drop the rows without a retail equivalent, so their
// rows map through what the conversion emitted; a move's names, animation and list starts must stay as converted.
// An edit outside those bounds returns the reason: it needs a full conversion and a new import.
class LiveConversion {
public:
    LiveConversion(const TKMovesetHeader& header, const StructsT7::TKMovesetHeaderBlocks& blocks,
        std::span<const Byte> payload, const ImportProfile::Profile& profile);
    ~LiveConversion();
    LiveConversion(LiveConversion&&) noexcept;
    LiveConversion& operator=(LiveConversion&&) noexcept;
    // The converted retail container (TKM2), updated by UpdateRow. Its CRC is the full conversion's.
    const std::vector<Byte>& File() const;
    const ConversionReport& Report() const;
    // `t7Row`: the edited row of T7 table `table` (0..18) in file layout (row indices, UINT64_MAX = null).
    std::optional<std::string> UpdateRow(size_t table, uint64_t row, std::span<const Byte> t7Row);
private:
    struct State;
    std::unique_ptr<State> m;
};
}

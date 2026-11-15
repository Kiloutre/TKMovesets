#pragma once
#include "MovesetStructs.h"
#include "ImportProfile.hpp"
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include <nlohmann/json_fwd.hpp>

// Self-contained retail Tekken 8 -> Tekken 7 conversion (T8ToT7.cpp). Every T7 record is built from the retail
// snapshot plus fixed tables shipped with the tool: the inverse of T7ToRetailT8's record rules, the pooled T8 -> T7 ID
// votes measured on 31 characters exported from both games (T8ToT7Votes.inc), the reversed alias catalog. Body
// animations are converted from their PANM (raw, static and packed tracks) to T7 0x64. No other moveset is read, so
// the output does not depend on the import target. See docs/conversion.md.
namespace T8ToT7 {
struct Report {
    uint64_t moves = 0, externalCancels = 0, danglingReferences = 0;
    uint64_t requirementsFixed = 0, requirementsVoted = 0, requirementsCatalog = 0, requirementsUnresolved = 0;
    uint64_t requirementsAssumedTrue = 0, requirementPropertiesDropped = 0;
    uint64_t propertiesFixed = 0, propertiesVoted = 0, propertiesCatalog = 0, propertiesDropped = 0;
    // Extra-property rows written whose retail requirement list has a condition: T7 extra properties have no
    // requirement field, so the row fires unconditionally in T7.
    uint64_t extraPropertyRequirementsDropped = 0;
    uint64_t specialCancelsDisabled = 0, walkCancelsRetimed = 0, voiceclipsConverted = 0, voiceclipsDropped = 0;
    uint64_t animationsConverted = 0, animationsMissing = 0, animationsConversionFailed = 0, animationsFallback = 0;
    uint64_t animationBytes = 0;
    uint64_t ruleFixesChanged = 0, ruleFixesDropped = 0; // t8_to_t7_fix rules: values rewritten, rows dropped / disabled
    uint64_t ruleGuards = 0; // t8_to_t7_guard rules: condition-less requirement lists given a condition row
    uint32_t sourceCharacter = 0, convertedCharacter = 0;
    bool placeholderCharacter = false;
    std::vector<std::pair<uint32_t, uint64_t>> unresolvedRequirementIds, droppedPropertyIds;
};
nlohmann::json ReportJson(const Report& report);
// T7 character identity of a converted moveset whose T8 character has no T7 counterpart: t7_t8_conversion_rules.ini
// val:t8_to_t7_placeholder_character (built-in 120, no T7 character uses it). The T7 importer swaps self checks for the
// target's identity, as for any import.
// Retail T8 character id -> T7 character id (UINT32_MAX when the character is not in T7).
uint32_t CharacterT8ToT7(uint32_t id);
// `header` and `payload` are a decompressed retail T8 file (TKM2 header, T8RS payload). Returns a complete,
// uncompressed T7 moveset file. `convertAnimations` false gives every move a one-frame rest pose (tests, planning).
// `profile`: the source file's import profile (ImportProfile::Read): the rules' setting(...) values, and it travels to
// the output (a fixed character in T7 numbering).
std::vector<Byte> Convert(const TKMovesetHeader& header, std::span<const Byte> payload, Report* report = nullptr,
    bool convertAnimations = true, const ImportProfile::Profile& profile = {});
}

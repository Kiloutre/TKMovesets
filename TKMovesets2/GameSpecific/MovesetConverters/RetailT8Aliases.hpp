#pragma once
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <utility>

namespace RetailT8 {
// Unlike Rules, these are research ID alignments, not parameter conversion rules.
struct AliasEvidence {
    bool property;
    uint32_t source, target; // UINT32_MAX means no resolved selection.
    const char* confidence;
    const char* provenance;
};
inline constexpr AliasEvidence Aliases[] = {
#include "RetailT8Aliases.inc"
};
inline const AliasEvidence* FindAlias(bool property, uint32_t id) {
    const auto key = std::pair{property,id};
    const auto found = std::lower_bound(std::begin(Aliases),std::end(Aliases),key,
        [](const AliasEvidence& row, auto value) { return std::pair{row.property,row.source} < value; });
    return found != std::end(Aliases) && found->property == property && found->source == id ? found : nullptr;
}
}

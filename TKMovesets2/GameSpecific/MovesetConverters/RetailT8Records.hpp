#pragma once
#include <cstdlib>
#include <cstring>
#include "Structs_t7.h"
#include <array>
#include <span>
#include <string>
#include <vector>
#include <stdexcept>
#include <utility>

// Logical intermediate records, deliberately independent of CNT/retail binary layouts.
namespace RetailT8 {
struct Requirement { uint32_t id; std::array<uint32_t, 4> parameters{}; };
struct Property { uint32_t frame, id; std::array<uint32_t, 5> parameters{}; };
struct Issue { bool property; size_t index; uint32_t id, parameter; std::string reason; };
struct Result {
    std::vector<Requirement> requirements;
    std::vector<Property> properties;
    std::vector<Issue> issues;
    size_t issueCount = 0;
    void Reject(Issue issue) {
        ++issueCount;
        if (issues.size() < 256) issues.push_back(std::move(issue));
    }
    bool Ok() const { return issueCount == 0; }
};
struct Rule { bool property; uint32_t source, target, minimum, maximum; };
inline constexpr Rule Rules[] = {
#include "RetailT8Rules.inc"
};
inline bool ConvertId(bool property, uint32_t id, uint32_t value, uint32_t& target, std::string& reason) {
    for (const auto& rule : Rules) {
        if (rule.property != property || rule.source != id) continue;
        if (value < rule.minimum || value > rule.maximum) {
            reason = "Parameter outside documented conversion domain"; return false;
        }
        target = rule.target; return true;
    }
    reason = "No supported offline conversion rule"; return false;
}
inline Result ConvertRecords(std::span<const StructsT7::Requirement> requirements,
                            std::span<const StructsT7::ExtraMoveProperty> properties) {
    constexpr size_t Budget = 256ULL << 20;
    if (requirements.size() > Budget / sizeof(Requirement) ||
        properties.size() > (Budget - requirements.size() * sizeof(Requirement)) / sizeof(Property))
        throw std::length_error("Retail record conversion exceeds allocation budget");
    Result result;
    auto convert = [&](bool property, size_t index, uint32_t id, uint32_t value, uint32_t& target) {
        std::string reason;
        if (ConvertId(property,id,value,target,reason)) return true;
        result.Reject({property,index,id,value,std::move(reason)}); return false;
    };
    for (size_t i = 0; i < requirements.size(); ++i) {
        const auto& source = requirements[i]; Requirement target{};
        if (convert(false, i, source.condition, source.param_unsigned, target.id)) {
            target.parameters[0] = source.param_unsigned;
            result.requirements.push_back(target);
        }
    }
    for (size_t i = 0; i < properties.size(); ++i) {
        const auto& source = properties[i]; Property target{source.starting_frame, 0};
        if (convert(true, i, source.id, source.value_unsigned, target.id)) {
            // Property id 0 ends a T7 list whatever its frame; a frame-0 row with an id is an ordinary property.
            if (source.id == 0) target.frame = 0;
            target.parameters[0] = source.value_unsigned;
            result.properties.push_back(target);
        }
    }
    // No partial converted arrays escape after an unsupported or malformed record.
    if (!result.Ok()) { result.requirements.clear(); result.properties.clear(); }
    return result;
}
}

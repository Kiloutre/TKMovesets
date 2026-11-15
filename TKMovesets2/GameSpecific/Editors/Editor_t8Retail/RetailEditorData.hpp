#pragma once
#include "Extractor_t8/RetailSnapshot.hpp"
#include <map>
#include <string>

// Editor provenance lives in the extensible outer header, keeping native table
// bytes and captured PANMs in the existing T8RS payload format.
namespace RetailEditorData
{
struct Metadata
{
    bool stored = false;
    uint64_t sourceAddress = 0;
    std::array<Byte, RetailT8Snapshot::SourceHeaderSize> source{};
    std::vector<uint32_t> moveNames;
    std::array<std::vector<uint32_t>, 20> origins;
    std::array<std::vector<uint32_t>, 20> remaps;
    std::map<uint32_t, std::string> names;
    // The file's notes section (RetailT8Snapshot::NotesTag), carried through editor saves unchanged.
    std::string notes;
    // The file's names section (RetailT8Snapshot::NamesTag): hash -> move/animation name. Rebuilt on save for every
    // hash the moves use: custom names, then these, then the shipped t8-retail name table.
    RetailT8Snapshot::NameMap fileNames;
};
Metadata Read(const TKMovesetHeader &outer, std::span<const Byte> payload);
std::vector<Byte> Serialize(const TKMovesetHeader &outer, std::span<const Byte> payload, const Metadata &metadata,
    const std::vector<TKMovesetProperty> *properties = nullptr);
} // namespace RetailEditorData

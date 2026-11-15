#include "RetailEditorData.hpp"
#include "Editor_t8Retail.hpp"
#include "RetailNames.hpp"
#include "GameStaticData.hpp"
#include <nlohmann/json.hpp>
#include <numeric>

namespace RetailEditorData
{
namespace R = RetailT8Snapshot;
namespace
{
constexpr size_t Limit = 64ULL << 20;
struct Extension
{
    char signature[4];
    uint32_t version, size, crc;
};
template <class T> T At(std::span<const Byte> b, size_t at)
{
    if (at > b.size() || sizeof(T) > b.size() - at)
        throw std::runtime_error("Truncated retail editor metadata");
    T v;
    memcpy(&v, b.data() + at, sizeof(v));
    return v;
}
size_t Align(size_t size)
{
    return (size + 7) & ~size_t(7);
}
void Validate(const Metadata &m, std::span<const Byte> payload)
{
    auto h = At<R::Header>(payload, 0);
    if (!m.sourceAddress || m.sourceAddress != h.sourceAddress)
        throw std::runtime_error("Editor provenance source differs");
    if (m.moveNames.size() != At<uint64_t>(m.source, 0x238))
        throw std::runtime_error("Editor provenance move count differs");
    for (size_t i = 0; i < 20; ++i)
    {
        auto e = At<R::Entry>(payload, h.directoryOffset + i * sizeof(R::Entry));
        if (m.origins[i].size() != e.count)
            throw std::runtime_error("Editor provenance row count differs");
        const auto original = At<uint64_t>(m.source, R::Tables[i].countOffset);
        if (m.remaps[i].size() != original)
            throw std::runtime_error("Editor provenance remap count differs");
        for (auto id : m.remaps[i])
            if (id != UINT32_MAX && id >= e.count)
                throw std::runtime_error("Editor remap outside current table");
        for (auto id : m.origins[i])
            if (id != UINT32_MAX && id >= original)
                throw std::runtime_error("Editor provenance row outside original table");
    }
    for (const auto &[hash, name] : m.names)
        if (name.empty() || name.size() > 255 || name.find('\0') != name.npos || RetailNames::Hash(name) != hash)
            throw std::runtime_error("Invalid custom retail name");
}
} // namespace
Metadata Read(const TKMovesetHeader &outer, std::span<const Byte> payload)
{
    R::Validate(payload);
    Metadata result;
    auto h = At<R::Header>(payload, 0);
    const auto prefix = std::span<const Byte>(reinterpret_cast<const Byte *>(&outer), outer.header_size);
    std::optional<std::vector<Byte>> editor;
    for (auto &section : R::ReadOuterSections(prefix))
    {
        if (section.tag == R::NotesTag && section.version == 1)
            result.notes.assign(section.data.begin(), section.data.end());
        if (section.tag != R::EditorTag)
            continue; // other sections are not editor provenance
        if (section.version != 1 || section.data.size() > Limit || editor)
            throw std::runtime_error("Unsupported retail editor metadata");
        editor = std::move(section.data);
    }
    result.fileNames = R::Names(prefix);
    if (editor)
    {
        const std::span<const Byte> data(*editor);
        auto j = nlohmann::json::from_cbor(data.begin(), data.end());
        result.sourceAddress = j.at("source_address").get<uint64_t>();
        auto source = j.at("source").get<std::vector<Byte>>();
        if (source.size() != result.source.size())
            throw std::runtime_error("Invalid provenance header size");
        std::copy(source.begin(), source.end(), result.source.begin());
        result.moveNames = j.at("move_names").get<std::vector<uint32_t>>();
        result.origins = j.at("origins").get<decltype(result.origins)>();
        result.remaps = j.at("remaps").get<decltype(result.remaps)>();
        for (const auto &pair : j.at("names"))
            if (!result.names.emplace(pair.at(0).get<uint32_t>(), pair.at(1).get<std::string>()).second)
                throw std::runtime_error("Duplicate custom name hash");
        result.stored = true;
    }
    else
    {
        result.sourceAddress = h.sourceAddress;
        memcpy(result.source.data(), payload.data() + h.sourceHeaderOffset, result.source.size());
        for (size_t i = 0; i < 20; ++i)
        {
            auto e = At<R::Entry>(payload, h.directoryOffset + i * sizeof(R::Entry));
            result.origins[i].resize(e.count);
            std::iota(result.origins[i].begin(), result.origins[i].end(), 0U);
            result.remaps[i] = result.origins[i];
            if (i == 12)
                for (size_t row = 0; row < e.count; ++row)
                {
                    auto hash = EditorT8Retail::Decode(At<uint64_t>(payload, e.offset + row * e.stride),
                                                       At<uint64_t>(payload, e.offset + row * e.stride + 8));
                    if (!hash)
                        throw std::runtime_error("Invalid original move identity");
                    result.moveNames.push_back(*hash);
                }
        }
    }
    Validate(result, payload);
    return result;
}
std::vector<Byte> Serialize(const TKMovesetHeader &outer, std::span<const Byte> payload, const Metadata &m,
    const std::vector<TKMovesetProperty> *properties)
{
    R::Validate(payload);
    Validate(m, payload);
    nlohmann::json names = nlohmann::json::array();
    for (const auto &[hash, name] : m.names)
        names.push_back({hash, name});
    nlohmann::json j{{"source_address", m.sourceAddress},
                     {"source", m.source},
                     {"move_names", m.moveNames},
                     {"origins", m.origins},
                     {"remaps", m.remaps},
                     {"names", names}};
    auto data = nlohmann::json::to_cbor(j);
    if (data.size() > Limit)
        throw std::length_error("Retail editor metadata exceeds budget");
    std::vector<R::OuterSection> sections(1);
    sections[0].tag = R::EditorTag;
    sections[0].data = std::move(data);
    if (!m.notes.empty())
        sections.push_back(R::NotesSection(m.notes));
    static const GameStaticData shippedNames = GameStaticData::LoadOptional("t8-retail");
    const auto fileNames = R::CollectNames(payload, [&](uint32_t hash) -> const std::string * {
        if (auto found = m.names.find(hash); found != m.names.end()) return &found->second;
        if (auto found = m.fileNames.find(hash); found != m.fileNames.end()) return &found->second;
        return shippedNames.FindName(hash);
    });
    if (!fileNames.empty())
        sections.push_back(R::NamesSection(fileNames));
    auto result = R::Serialize(payload, outer, sections, properties);
    auto header = At<TKMovesetHeader>(result, 0);
    header.orig_crc32 = outer.orig_crc32;
    memcpy(result.data(), &header, sizeof(header));
    return result;
}
} // namespace RetailEditorData

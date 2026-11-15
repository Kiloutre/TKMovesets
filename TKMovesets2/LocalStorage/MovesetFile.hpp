#pragma once
#include <memory>
#include <filesystem>
#include <span>
#include "MovesetStructs.h"

class MovesetFile
{
    std::unique_ptr<Byte, decltype(&free)> m_bytes{nullptr, free};
    uint64_t m_size = 0;
    TKMovesetCompressionType_ m_originalCompression{};
public:
    explicit MovesetFile(const std::filesystem::path& path);
    TKMovesetCompressionType_ OriginalCompression() const { return m_originalCompression; }
    const TKMovesetHeader& Header() const { return *reinterpret_cast<const TKMovesetHeader*>(m_bytes.get()); }
    std::span<const Byte> Bytes() const { return {m_bytes.get(), static_cast<size_t>(m_size)}; }
    std::span<const Byte> Payload() const { return Bytes().subspan(Header().moveset_data_start); }
    // Full record/relocation validation for supported native or convertible formats.
    void Validate() const;
    std::vector<Byte> ConvertToT7() const;
    std::vector<Byte> ConvertToT8() const;
};

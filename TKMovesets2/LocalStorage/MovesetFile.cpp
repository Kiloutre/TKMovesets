#include "Editor_t8Retail/RetailEditorData.hpp"
#include "MovesetFile.hpp"
#include "Compression.hpp"
#include "Helpers.hpp"
#include "MovesetConverters.hpp"
#include "Importer_t7/Importer_t7_Utils.hpp"
#include "GameIDs.hpp"
#include "T7ToRetailT8.hpp"
#include "T8ToT7.hpp"
#include "ImportProfile.hpp"
#include "Extractor_t8/RetailSnapshot.hpp"
#include <stdexcept>

MovesetFile::MovesetFile(const std::filesystem::path& path)
{
    Byte* bytes = Helpers::ReadMovesetFile(path.wstring(), m_size);
    m_bytes.reset(bytes);
    m_originalCompression = static_cast<TKMovesetCompressionType_>(Header().compressionType);
    if (!CompressionUtils::RAW::Moveset::ExpandFileBuffer(bytes, m_size))
        throw std::runtime_error("Cannot decompress moveset");
    m_bytes.release();
    m_bytes.reset(bytes);
}

static void ValidateT7(const StructsT7::TKMovesetHeaderBlocks& blocks, std::span<const Byte> payload)
{
    std::string reason;
    if (!T7ImportUtils::ValidateT7Semantics(blocks, payload.data(), payload.size(), &reason))
        throw std::runtime_error(reason);
}

std::vector<Byte> MovesetFile::ConvertToT7() const
{
    auto& header = Header();
    Byte* converted = const_cast<Byte*>(Payload().data());
    uint64_t size = Payload().size();
    StructsT7::TKMovesetHeaderBlocks blocks{};
    uint16_t origin = 0;
    bool success = false;
    switch (header.gameId) {
    case GameId_T7:
        Validate();
        return {Bytes().begin(), Bytes().end()};
    case GameId_T8:
        // Retail only (CNT/CBT have other layouts): self-contained, the same file for every target.
        if (header.minorVersion != GameVersions::T8::RETAIL) throw std::runtime_error("Only retail Tekken 8 movesets convert to Tekken 7");
        Validate();
        return T8ToT7::Convert(header, Payload(), nullptr, true, ImportProfile::Read(Bytes()));
    case GameId_TTT2:
        success = MovesetConverter::TTT2ToT7().Convert(&header, converted, size, blocks);
        origin = GameVersions::T7::FROM_TTT2;
        break;
    case GameId_TREV:
        success = MovesetConverter::TREVToT7().Convert(&header, converted, size, blocks);
        origin = GameVersions::T7::FROM_TREV;
        break;
    case GameId_T6:
        success = MovesetConverter::T6ToT7().Convert(&header, converted, size, blocks);
        origin = GameVersions::T7::FROM_T6;
        break;
    case GameId_T5:
        if (header.minorVersion == GameVersions::T5::DR_ONLINE) {
            success = MovesetConverter::T5DRToT7().Convert(&header, converted, size, blocks);
            origin = GameVersions::T7::FROM_T5DR;
        } else {
            success = MovesetConverter::T5ToT7().Convert(&header, converted, size, blocks);
            origin = GameVersions::T7::FROM_T5;
        }
        break;
    default:
        throw std::runtime_error("This game format has no validated T7 conversion");
    }
    if (!success) throw std::runtime_error("Moveset conversion failed");
    std::unique_ptr<Byte[]> owner(converted);
    ValidateT7(blocks, {converted, static_cast<size_t>(size)});
    TKMovesetHeader output = header;
    output.header_size = Helpers::align8Bytes(sizeof(output));
    output.gameId = GameId_T7;
    output.minorVersion = GameVersions::T7::NONE;
    output.conversion_origin = origin;
    strcpy_s(output.origin, "Tekken 7"); // the game the file now imports into; conversion_origin keeps the source
    output.compressionType = TKMovesetCompressionType_None;
    output.moveset_data_size = 0;
    // Normalize legacy optional header fields before emitting a current header.
    strcpy_s(output.orig_character_name, header.GetOrigCharacterName());
    output.game_specific_flags = header.GetGameSpecificFlags();
    TKMovesetProperty end{.id = TKMovesetProperty_END, .value = 0};
    // The source's import profile travels with the moveset (defaults for sources without one).
    auto properties = ImportProfile::ToProperties(ImportProfile::Read(Bytes()));
    properties.push_back(end);
    output.block_list = output.header_size + Helpers::align8Bytes(properties.size() * sizeof(TKMovesetProperty));
    output.block_list_size = std::size(blocks.blocks);
    output.moveset_data_start = output.block_list + sizeof(blocks);
    std::vector<Byte> file(output.moveset_data_start + size, 0);
    memcpy(file.data() + output.header_size, properties.data(), properties.size() * sizeof(TKMovesetProperty));
    memcpy(file.data() + output.block_list, &blocks, sizeof(blocks));
    memcpy(file.data() + output.moveset_data_start, converted, size);
    std::vector<std::pair<Byte*, uint64_t>> hashed{{reinterpret_cast<Byte*>(&end), sizeof(end)}};
    for (size_t i = 0; i + 1 < std::size(blocks.blocks); ++i) {
        if (i == StructsT7::TKMovesetHeaderBlocks_Name) continue;
        hashed.emplace_back(converted + blocks.blocks[i], blocks.blocks[i + 1] - blocks.blocks[i]);
    }
    output.crc32 = Helpers::CalculateCrc32(hashed);
    memcpy(file.data(), &output, sizeof(output));
    return file;
}

std::vector<Byte> MovesetFile::ConvertToT8() const
{
    if (Header().gameId != GameId_T7) throw std::runtime_error("T8 conversion requires a T7 moveset");
    Validate();
    StructsT7::TKMovesetHeaderBlocks blocks;
    memcpy(&blocks, Bytes().data() + Header().block_list, sizeof(blocks));
    // Same self-contained conversion as import: no donor, the same file for every target.
    return T7ToRetailT8::ConvertStandalone(Header(), blocks, Payload(), nullptr, true, ImportProfile::Read(Bytes()));
}

void MovesetFile::Validate() const
{
    auto& header = Header();
    if (header.gameId == GameId_T7) {
        if (header.block_list_size < sizeof(StructsT7::TKMovesetHeaderBlocks) / sizeof(uint64_t))
            throw std::runtime_error("T7 block table is truncated");
        auto& blocks = *reinterpret_cast<const StructsT7::TKMovesetHeaderBlocks*>(Bytes().data() + header.block_list);
        ValidateT7(blocks, Payload());
    } else if (header.gameId == GameId_T8 && header.minorVersion == GameVersions::T8::RETAIL) {
        const auto flag = header.GetGameSpecificFlags();
        if ((flag != RetailT8Snapshot::TablesOnlyFlag && flag != RetailT8Snapshot::RuntimeAssetsFlag) || header.block_list_size != 1 ||
            *reinterpret_cast<const uint64_t*>(Bytes().data() + header.block_list) != 0)
            throw std::runtime_error("Unsupported retail T8 container");
        RetailT8Snapshot::Validate(Payload());
        if(header.header_size>((sizeof(TKMovesetHeader)+7)&~size_t(7)))RetailEditorData::Read(header,Payload());
        uint32_t version; memcpy(&version, Payload().data() + 4, sizeof(version));
        if ((version >= 2) != (flag == RetailT8Snapshot::RuntimeAssetsFlag)) throw std::runtime_error("Retail payload/flags mismatch");
    } else {
        ConvertToT7();
    }
}

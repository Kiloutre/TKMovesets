#include "Extractor_t8.hpp"
#include "RetailSnapshot.hpp"
#include "RetailGameData.hpp"
#include "RetailDiskMoveset.hpp"
#include "Compression.hpp"
#include "FileIO.hpp"
#include "RetailRuntime.hpp"
#include <stdexcept>
#include <array>

gameAddr ExtractorT8::GetCharacterAddress(uint8_t player) {
    if (player >= 2) return 0;
    try {
        const auto base = m_process.mainModule.address;
        return RetailT8Runtime::Players(m_process, base + m_game.GetValue("registry_rva"), base + m_game.GetValue("round_key_rva"))[player];
    } catch (const std::exception&) { return 0; }
}
std::vector<gameAddr> ExtractorT8::GetCharacterAddresses() {
    std::vector<gameAddr> result;
    for (uint8_t slot = 0; slot < 2; ++slot) if (auto player = GetCharacterAddress(slot)) result.push_back(player);
    return result;
}
uint32_t ExtractorT8::GetCharacterID(gameAddr player) {
    uint32_t id = UINT32_MAX;
    if (!player || player == UINT64_MAX || !m_process.readBytes(player + m_game.GetValue("chara_id_offset"), &id, sizeof(id))) return UINT32_MAX;
    return id;
}
std::string ExtractorT8::GetPlayerCharacterName(gameAddr player) {
    const auto id = GetCharacterID(player);
    return id == UINT32_MAX ? std::string{} : m_names.CharacterName(id);
}
bool ExtractorT8::CanExtract() {
    const auto player = GetCharacterAddress(0);
    gameAddr root = 0;
    return player && player != UINT64_MAX && GetCharacterID(player) != UINT32_MAX &&
        m_process.readBytes(player + m_game.GetValue("motbin_offset"), &root, sizeof(root)) && root && root != UINT64_MAX;
}
ExtractionErrcode_ ExtractorT8::Extract(gameAddr player, ExtractSettings settings, uint8_t& progress) {
    progress = 0;
    m_lastError.clear();
    try {
        if (!player || player == UINT64_MAX) return ExtractionErrcode_MovesetDataError;
        const auto pointer = player + m_game.GetValue("motbin_offset");
        gameAddr root = 0;
        if (!m_process.readBytes(pointer, &root, sizeof(root)) || !root || root == UINT64_MAX)
            return ExtractionErrcode_MovesetDataError;
        const auto id = GetCharacterID(player);
        if (id == UINT32_MAX) return ExtractionErrcode_MovesetDataError;
        const auto categories=RetailAnimationSelection(settings);
        // v4 snapshot in every case (tables-only selections need no animation manager).
        // Animations the game has not loaded are read from its own tkdata.bin, so the export always carries all of them.
        const auto tkdataPath = RetailT8GameData::TkdataPath(m_process.mainModule.path);
        RetailT8GameData::Source disk(tkdataPath, [this, tkdataPath](uint32_t domain) {
            // the archive's own roster (motbin header identities) first: a names file without a fighter's runtime id
            // gave no code (Azucena 29, Victor 30 before 2026-11-15)
            if (auto code = RetailT8GameData::TkdataCodeFor(tkdataPath, domain); !code.empty()) return code;
            const auto character = m_names.FindCharacter(domain);
            return character ? character->code : std::string();
        });
        auto payload = RetailT8Snapshot::CaptureComplete(m_process, root, categories ? m_game.ReadPtrPath("anim_manager") : 0,
            categories ? m_game.ReadPtrPath("anim_container") : 0, id, categories,
            (settings & ExtractSettings_T8_NO_ARCHIVE) ? RetailT8Snapshot::ArchiveReader{} :
            RetailT8Snapshot::ArchiveReader([&](uint32_t domain, uint32_t type, uint32_t instance) { return disk.Panm(domain, type, instance); }));
        progress = 65;
        gameAddr check = 0;
        if (!m_process.readBytes(pointer, &check, sizeof(check)) || check != root || GetCharacterID(player) != id)
            return ExtractionErrcode_MovesetDataError;
        TKMovesetHeader header;
        auto name = m_names.CharacterName(id);
        // Display metadata must also fit the existing header and one filename component.
        if (name.size() >= sizeof(header.target_character) || name.find_first_of("<>:\"/\\|?*") != name.npos)
            name = "Character " + std::to_string(id);
        header.characterId = id; header.flags = 0;
        header.date = header.extraction_date = Helpers::getCurrentTimestamp();
        strcpy_s(header.origin, "Tekken 8"); strcpy_s(header.version_string, MOVESET_VERSION_STRING);
        strcpy_s(header.target_character, name.c_str()); strcpy_s(header.orig_character_name, name.c_str());
        // Notes section: said only when there is something to say (animations this file does not carry).
        std::vector<RetailT8Snapshot::OuterSection> sections;
        {
            std::array<uint64_t, 6> missing{};
            for (const auto& asset : RetailT8Snapshot::Assets(payload)) if (!asset.size && asset.type < 6) ++missing[asset.type];
            // With tkdata.bin read, the only size-0 slots are ids the game itself has no data for: nothing is missing.
            // Unselected categories are not in the file at all and are never counted.
            if (const auto note = (settings & ExtractSettings_T8_NO_ARCHIVE) ? RetailT8Disk::MissingAnimationsNote(missing, false) : std::string(); !note.empty())
                sections.push_back(RetailT8Snapshot::NotesSection(note));
        }
        // Names section: records only hold name hashes, so the file carries the strings (captured, else shipped table).
        if (const auto names = RetailT8Snapshot::CollectNames(payload, [&](uint32_t hash) { return m_names.FindName(hash); }); !names.empty())
            sections.push_back(RetailT8Snapshot::NamesSection(names));
        auto file = RetailT8Snapshot::Serialize(payload, header, sections);
        uint64_t size = 0;
        std::unique_ptr<Byte[]> compressed(CompressionUtils::RAW::Moveset::Compress(file.data(), file.size(),
            ExtractorUtils::GetCompressionAlgorithm(settings), size));
        if (!compressed) return ExtractionErrcode_CompressionFailure;
        progress = 90;
        std::wstring path, temporary;
        GetFilepath(name.c_str(), path, temporary, (settings & ExtractSettings_OVERWRITE_SAME_FILENAME) != 0);
        if (!FileIO::WriteAtomically(path, {compressed.get(), static_cast<size_t>(size)})) return ExtractionErrcode_FileCreationErr;
        progress = 100; return ExtractionErrcode_Successful;
    } catch (const std::bad_alloc&) { return ExtractionErrcode_AllocationErr; }
    catch (const std::exception& error) {
        m_lastError = error.what();
        DEBUG_ERR("Retail T8 extraction failed: %s", error.what());
        return ExtractionErrcode_MovesetDataError;
    }
}

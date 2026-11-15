#include "Extractor_t7.hpp"
#include "T7ExportSnapshot.hpp"
#include "Compression.hpp"
#include "FileIO.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <stdexcept>

using namespace StructsT7;
namespace {
std::string DisplayName(std::string value) {
    if (value.starts_with(MOVESET_EXTRACTED_NAME_PREFIX)) value.erase(0, strlen(MOVESET_EXTRACTED_NAME_PREFIX));
    if (value.starts_with("[")) value.erase(0, 1);
    if (value.ends_with("]")) value.pop_back();
    bool start = true;
    for (auto& character : value) {
        const auto byte = static_cast<unsigned char>(character);
        if (!std::isprint(byte) || strchr("<>:\\\"/\\\\|?*", byte) || character == '_') character = ' ';
        else if (std::isalpha(byte)) character = static_cast<char>(start ? std::toupper(byte) : std::tolower(byte));
        start = strchr(" -.", character) != nullptr;
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '.')) value.pop_back();
    if (value.empty()) value = "Unknown";
    if (value.size() > 31) value.resize(31);
    return value;
}
}
void ExtractorT7::FillHeaderInfos(TKMovesetHeader& header, gameAddr player, const std::string& name)
{
    header.flags = 0; header.game_specific_flags = 0;
    header.gameId = m_gameInfo.gameId; header.minorVersion = m_gameInfo.minorVersion;
    header.characterId = GetCharacterID(player);
    memset(header.gameVersionHash, 0, sizeof(header.gameVersionHash));
    strcpy_s(header.version_string, sizeof(header.version_string), MOVESET_VERSION_STRING);
    strncpy_s(header.origin, sizeof(header.origin), m_gameInfo.name, _TRUNCATE);
    strncpy_s(header.target_character, sizeof(header.target_character), name.c_str(), _TRUNCATE);
    strncpy_s(header.orig_character_name, sizeof(header.orig_character_name), name.c_str(), _TRUNCATE);
    header.date = header.extraction_date = Helpers::getCurrentTimestamp();
#ifdef BUILD_TYPE_DEBUG
    header.flags |= MovesetFlags_Debug;
    strcat_s(header.version_string, sizeof(header.version_string), "d");
#endif
}
ExtractionErrcode_ ExtractorT7::Extract(gameAddr player, ExtractSettings settings, uint8_t& progress)
{
    progress = 0;
    try {
        gameAddr moveset = 0, training = 0;
        const auto pointerAddress = player + m_game.GetValue("motbin_offset");
        if (!m_process.readBytes(pointerAddress, &moveset, sizeof(moveset)) || !moveset || moveset == UINT64_MAX)
            throw std::runtime_error("Cannot read player moveset pointer");
        if (settings & ExtractSettings_DisplayableMovelist) {
            const auto manager = m_game.ReadPtrPath("movelist_manager_addr");
            if (manager == UINT64_MAX) throw std::runtime_error("Cannot read training manager");
            if (manager) {
                uint32_t playerId = 0;
                if (!m_process.readBytes(player + m_game.GetValue("playerid_offset"), &playerId, sizeof(playerId)) || playerId > 1 ||
                    !m_process.readBytes(manager + playerId * sizeof(MvlManager) + offsetof(MvlManager, mvlHead), &training, sizeof(training)))
                    throw std::runtime_error("Cannot read training movelist pointer");
            }
        }
        progress = 10;
        auto snapshot = T7Export::ReadSnapshot(m_process, moveset, settings, training);
        progress = 75;
        const auto character = DisplayName(snapshot.characterName);
        TKMovesetHeader header; FillHeaderInfos(header, player, character);
        auto file = T7Export::Serialize(snapshot, header);
        gameAddr current = 0;
        if (!m_process.readBytes(pointerAddress, &current, sizeof(current)) || current != moveset)
            throw std::runtime_error("Player changed moveset during export");
        progress = 85;
        const auto codec = ExtractorUtils::GetCompressionAlgorithm(settings);
        uint64_t encodedSize = 0;
        std::unique_ptr<Byte[]> encoded(CompressionUtils::RAW::Moveset::Compress(file.data(), file.size(), codec, encodedSize));
        if (!encoded) return ExtractionErrcode_CompressionFailure;
        std::wstring destination, unusedTemporary;
        GetFilepath(character.c_str(), destination, unusedTemporary, (settings & ExtractSettings_OVERWRITE_SAME_FILENAME) != 0);
        if (!FileIO::WriteAtomically(destination, {encoded.get(), static_cast<size_t>(encodedSize)}))
            return ExtractionErrcode_FileCreationErr;
        progress = 100;
        return ExtractionErrcode_Successful;
    } catch (const std::bad_alloc&) { return ExtractionErrcode_AllocationErr; }
    catch (const std::exception& error) {
        DEBUG_ERR("T7 export failed: %s", error.what());
        return ExtractionErrcode_MovesetDataError;
    }
}

bool ExtractorT7::CanExtract()
{
	// todo: this is invalid, because when we import our own moveset and leave back to main menu, it will return true
	// yes we can import in that case but it will serve zero purpose
	gameAddr playerAddress = m_game.ReadPtrPath("p1_addr");

	// We'll just read through a bunch of values that wouldn't be valid if a moveset wasn't loaded
	// readInt64() may return -1 if the read fails so we have to check for this value as well.

	if (playerAddress == 0 || playerAddress == -1) {
		return false;
	}

	for (int i = 0; i < 1; ++i)
	{
		gameAddr player = playerAddress + i * m_game.GetValue("playerstruct_size");

		// Read into current moveset to see if it has been initialized
		{
			gameAddr movesetAddr = m_process.readUInt64(playerAddress + m_game.GetValue("motbin_offset"));
			if (movesetAddr == 0 || movesetAddr == -1) {
				return false;
			}

			int8_t isInitialized = m_process.readInt8(movesetAddr + offsetof(MovesetInfo, isInitialized));
			if (isInitialized != 1) {
				return false;
			}

		}

		// Read into current move to see if it is valid
		{
			gameAddr currentMove = m_process.readUInt64(player + m_game.GetValue("currmove"));
			if (currentMove == 0 || currentMove == -1) {
				return false;
			}

			gameAddr animAddr = m_process.readInt64(currentMove + 0x10);
			if (animAddr == 0 || animAddr == -1) {
				return false;
			}

			uint8_t animType = m_process.readInt8(animAddr);
			if (animType != 0x64 && animType != 0xC8) {
				return false;
			}
		}
	}
	return true;
}


std::string ExtractorT7::GetPlayerCharacterName(gameAddr playerAddress)
{
    try {
        gameAddr moveset = 0;
        if (!m_process.readBytes(playerAddress + m_game.GetValue("motbin_offset"), &moveset, sizeof(moveset))) return {};
        return DisplayName(T7Export::ReadCharacterName(m_process, moveset));
    } catch (const std::exception&) { return {}; }
}

uint32_t ExtractorT7::GetCharacterID(gameAddr playerAddress)
{
	return m_process.readInt16(playerAddress + m_game.GetValue("chara_id_offset"));
}

gameAddr ExtractorT7::GetCharacterAddress(uint8_t playerId)
{
	gameAddr playerAddress = m_game.ReadPtrPath("p1_addr");
	if (playerId > 0) {
		playerAddress += playerId * m_game.GetValue("playerstruct_size");
	}
	return playerAddress;
}

std::vector<gameAddr> ExtractorT7::GetCharacterAddresses()
{
	gameAddr playerAddress = m_game.ReadPtrPath("p1_addr");
	uint64_t playerstructSize = m_game.GetValue("playerstruct_size");
	std::vector<gameAddr> addresses;

	for (uint8_t i = 0; i < m_gameInfo.characterCount; ++i) {
		addresses.push_back(playerAddress + i * playerstructSize);
	}

	return addresses;
}
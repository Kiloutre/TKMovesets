#pragma once

#include <stdint.h>

#include "Helpers.hpp"

typedef uint8_t TKMovesetCompressionType;
enum TKMovesetCompressionType_
{
	TKMovesetCompressionType_None = 0,

	TKMovesetCompressionType_LZ4,
	TKMovesetCompressionType_LZMA,
	TKMovesetCompressionType_CompactZstd,
};

enum TKMovesetProperty_
{
	TKMovesetProperty_END = 0x0
};

// Custom moveset properties used when loading into a game
struct TKMovesetProperty
{
	int id;
	int value;
};

typedef uint64_t MovesetFlags;
enum MovesetFlags_
{
	// Used to show which movesets the user has modified (knowing if it's an original or not can be useful)
	MovesetFlags_MovesetModified = (1 << 0),
	// Moveset was extracted in debug mode
	MovesetFlags_Debug           = (1 << 1),
};

// This structure should really not change or it would break compatibility with all previous movesets
// If you have any change in mind that might be important, do it now
// Useful infos regarding the moveset : extracting date, game origin, game id, extractor version
struct TKMovesetHeader
{
	// Should always be TKM2
	const char _signature[4] = { 'T', 'K', 'M', '2' };
	// Version string of the extractor at the time of the extraction
	char version_string[28] = "";
	// Array of bytes where you can store a unique identifier/hash for the game version, allowing you to detect which version the moveset was extracted form
	Byte gameVersionHash[32] = {};
	// ID of the game that the moveset was extracted from
	uint16_t gameId = (uint16_t)-1;
	// Minor version of the game that the moveset was extracted from
	uint16_t minorVersion = (uint16_t)-1;
	// Contains the size of the header, aligned on 8 bytes
	uint32_t header_size = (uint32_t)-1;
	// Offset to the list of blocks
	uint32_t block_list = (uint32_t)-1;
	// Amount of items in the list of blocks
	uint32_t block_list_size = (uint32_t)-1;
	// Absolute offset of the block offset list
	uint32_t moveset_data_start = (uint32_t)-1;
	// Size of the moveset data. 0 if not compressed.
	uint64_t moveset_data_size = 0;
	// Type of the compression
	TKMovesetCompressionType compressionType = TKMovesetCompressionType_None;
	// Stores a hash of the moveset data 
	uint32_t crc32 = (uint32_t)-1;
	// Original crc32 of the moveset data at the time of extraction
	uint32_t orig_crc32 = (uint32_t)-1;
	// Date of last modification
	uint64_t date = (uint64_t)-1;
	// Date of initial file extraction
	uint64_t extraction_date = (uint64_t)-1;
	// ID of the extracted character, used internally to make some moves that require it work
	uint32_t characterId = (uint32_t)-1;
	// Flags used for storing useful data. Currently unused.
	MovesetFlags flags = (MovesetFlags)-1;
	// Origin (Game name)
	char origin[32] = "";
	// Target character to play on
	char target_character[32] = "";
	// Original name of the character
	char orig_character_name[32] = "";
	// Flags used for storing useful game-specific infos. Access using GetGameSpecificFlags()
	uint64_t game_specific_flags = -1;
	// Origin of the conversion, if the moveset was converted
	uint16_t conversion_origin = 0;

	// -- Functions -- //

	// Function to validate some of the header content, first step to ensuring the file isn't badly formated
	bool ValidateHeader(uint64_t fileSize) const
	{
		if (strncmp(_signature, "TKM2", 4) != 0) return false;
		if (!memchr(version_string, 0, sizeof(version_string)) || !memchr(origin, 0, sizeof(origin)) ||
			!memchr(target_character, 0, sizeof(target_character))) return false;
		if (header_size >= offsetof(TKMovesetHeader, game_specific_flags) &&
			!memchr(orig_character_name, 0, sizeof(orig_character_name))) return false;

		if (header_size < offsetof(TKMovesetHeader, orig_character_name)) return false;
		if (block_list_size == 0) return false;
		if (block_list < header_size) return false;
		if (moveset_data_start <= block_list) return false;
		if (moveset_data_start >= fileSize) return false;
		if (block_list_size > (moveset_data_start - block_list) / sizeof(uint64_t)) return false;
		if (compressionType > TKMovesetCompressionType_CompactZstd) return false;
		if (isCompressed() && (moveset_data_size == 0 || moveset_data_size > INT32_MAX)) return false;

		return true;
	}

	bool isCompressed() const { return compressionType != TKMovesetCompressionType_None; }

	// -- Version specific -- //

	// A version-gated field is only read when the file's header actually contains it:
	// past header_size the bytes belong to the property list or block list.
	bool HeaderContains(size_t fieldOffset, size_t fieldSize) const
	{
		return header_size >= fieldOffset + fieldSize;
	}

	uint64_t GetConversionOrigin() const
	{
		// conversion_origin was introduced in 0.6
		if (Helpers::VersionGreater(version_string, "0.6") &&
			HeaderContains(offsetof(TKMovesetHeader, conversion_origin), sizeof(conversion_origin))) return conversion_origin;
		return 0;
	}

	// todo: convert movesets to new versions to avoid having these variables missing
	uint64_t GetGameSpecificFlags() const
	{
		// game_specific_flags was introduced in 0.6
		if (Helpers::VersionGreater(version_string, "0.5") &&
			HeaderContains(offsetof(TKMovesetHeader, game_specific_flags), sizeof(game_specific_flags))) return game_specific_flags;
		return 0;
	}

	const char* GetOrigCharacterName() const
	{
		// Orig_character_name was introduced in 0.6. Same condition as ValidateHeader's NUL check.
		if (Helpers::VersionGreater(version_string, "0.5") &&
			HeaderContains(offsetof(TKMovesetHeader, orig_character_name), sizeof(orig_character_name))) return orig_character_name;
		return target_character;
	}
};

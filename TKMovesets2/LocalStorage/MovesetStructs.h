#pragma once

#include <stdint.h>

typedef uint8_t TKMovesetCompressionType;
enum TKMovesetCompressionType_
{
	TKMovesetCompressionType_None = 0,

	TKMovesetCompressionType_LZ4,
	TKMovesetCompressionType_LZMA,
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
	MovesetFlags_MOVESET_MODIFIED = (1 << 0),
	MovesetFlags_Compressed = (1 << 1),
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
	// Size of the list of blocks
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
	// Flags used for storing useful data. Currently unused. Todo : see what we can do with this?
	MovesetFlags flags = (MovesetFlags)-1;
	// Origin (Game name)
	char origin[32] = "";
	// Target character to play on
	char target_character[32] = "";

	// Function to validate some of the header content, first step to ensuring the file isn't badly formated
	bool ValidateHeader() const
	{
		if (strncmp(_signature, "TKM2", 4) != 0) return false;

		if (block_list_size == 0) return false;
		if (block_list < header_size) return false;
		if (moveset_data_start <= block_list) return false;
		if (moveset_data_size < 0) return false;

		return true;
	}


	bool isCompressed() const { return compressionType != TKMovesetCompressionType_None; }
};

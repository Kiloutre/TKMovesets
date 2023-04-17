#pragma once

#include <stdint.h>

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
};

// This structure should really not change or it would break compatibility with all previous movesets
// If you have any change in mind that might be important, do it now
// Useful infos regarding the moveset : extracting date, game origin, game id, extractor version
struct TKMovesetHeader
{
	const char _signature[4] = { 'T', 'K', 'M', '2' };
	// Version string of the extracted moveset
	char version_string[28];
	// ID of the game that the moveset was extracted from
	uint32_t gameId;
	// ID of the extracted character, used internally to make some moves that require it work
	uint32_t characterId;
	// Contains the size of the header, aligned on 8 bytes
	uint32_t header_size;
	// Offset to the list of blocks
	uint32_t block_list;
	// Size of the list of blocks
	uint32_t block_list_size;
	// Absolute offset of the block offset list
	uint32_t moveset_data_start;
	// Stores a hash of the moveset data (everything past our TKMovesetHeader_infos structure)
	uint32_t crc32;
	// Flags used for storing useful data. Currently unused. Todo : see what we can do with this?
	MovesetFlags flags;
	// Date of last modification
	uint64_t date;
	// Origin (Game name + author if you want)
	char origin[32];
	// Target character to play on
	char target_character[32];
};

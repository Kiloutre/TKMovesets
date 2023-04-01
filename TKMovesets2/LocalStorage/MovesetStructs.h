#pragma once

#include <stdint.h>

// Never resize this
typedef uint64_t MovesetFlags;
enum MovesetFlags_
{
	// Used to detect whether to not show the "rename moveset?" prompt when editing a moveset
	MovesetFlags_MOVESET_MODIFIED = (1 << 0),
};

// This structure should really not change or it would break compatibility with all previous movesets
// If you have any change in mind that might be important, do it now
struct TKMovesetHeader_infos
{
	// Version string of the extracted moveset
	char version_string[32];
	// ID of the game that the moveset was extracted from
	uint32_t gameId;
	// ID of the extracted character, used internally to make some moves that require it work
	uint32_t characterId;
	// Small cost for ensuring future compatibility and less annoying cost
	uint32_t header_size;
	// Flags used for storing useful data. Currently unused. Todo : see what we can do with this?
	MovesetFlags flags;
	// Date of last modification
	uint64_t date;
	// Stores a hash of the moveset data (everything past our TKMovesetHeader_infos structure)
	uint32_t crc32;
	// Origin (Game name + author if you want)
	char origin[32];
	// Target character to play on
	char target_character[32];
};

// Offsets of the various blocks contained within the moveset, in the same order they will appear in
// You can append new members to this structure without breaking compatibility
struct TKMovesetHeader_offsets
{
	// Store moveid aliases and some more i don't understand
	uint64_t movesetInfoBlock;
	// Store a list of offsets toward various structure lists paired with their size
	uint64_t tableBlock;
	// Store a list of offets pointing to mota files
	uint64_t motalistsBlock;
	// Stores an offset to the block containing move names & anim names (they are the same)
	uint64_t nameBlock;
	// Stores an offset to the block containing stucture lists
	uint64_t movesetBlock;
	// Store an offset to the block containing every regular animation used in the moveset. (customly-built)
	uint64_t animationBlock;
	// Store an offset to the block containing the multiple mota files for those that are extracted . (customly-built)
	uint64_t motaBlock;
	// Stores the .mvl file in order to show the custom movelist in training mode
	uint64_t movelistBlock;
};


// Custom data contained at the head of every extracted movesets
struct TKMovesetHeader
{
	// Useful infos regarding the moveset : extracting date, game origin, game id, extractor version
	TKMovesetHeader_infos infos;
	// Absolute offsets of the moveset-related data blocks within the file
	TKMovesetHeader_offsets offsets;
};

#pragma once

#include <stdint.h>

enum MOVESET_FLAGS
{
	// Used to detect whether to not show the "rename moveset?" prompt when editing a moveset
	MOVESET_MODIFIED = (1 << 0),
};

// This structure should really not change or it would break compatibility with all previous movesets
// If you have any change in mind that might be important, do it now
struct MovesetHeader_infos
{
	// Version string of the extracted moveset
	char version_string[32];
	// ID of the game that the moveset was extracted from
	uint32_t gameId;
	// ID of the extracted character, used internally to make some moves that require it work
	uint32_t characterId;
	// Flags used for storing useful data. Currently unused. Todo : see what we can do with this?
	uint64_t flags;
	// Origin (Game name + author if you want)
	char origin[32];
	// Target character to play on
	char target_character[32];
	// Date of last modification
	uint64_t date;
	// Small cost for ensuring future compatibility and less annoying cost
	uint32_t header_size;
};

// Offsets of the various blocks contained within the moveset, in the same order they will appear in
// You can append new members to this structure without breaking compatibility
struct MovesetHeader_offsets
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
	// Store an offset to the block containing every regular animation used in the moveset
	uint64_t animationBlock; // Custom block
	// Store an offset to the block containing the multiple mota files for those that are extracted
	uint64_t motaBlock; // Custom block
};


// Data contained at the head of every extracted movesets
struct MovesetHeader
{
	// Useful infos regarding the moveset : extracting date, game origin, game id, extractor version
	MovesetHeader_infos infos;
	// Absolute offsets of the moveset-related data blocks within the file
	MovesetHeader_offsets offsets;
};

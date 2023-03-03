#pragma once

#include <stdint.h>

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

// You can append new members to this structure without breaking compatibility
struct MovesetHeader_offsets
{
	// Offsets of the various blocks contained within the moveset, in the same order they will appear in
	int64_t movesetInfoBlock;
	int64_t listsBlock;
	int64_t motalistsBlock;
	int64_t nameBlock;
	int64_t movesetBlock;
	int64_t animationBlock; // Custom block
	int64_t motaBlock; // Custom block
};


// Data contained at the head of every extracted movesets
struct MovesetHeader
{
	// Useful infos regarding the moveset : extracting date, game origin, game id, extractor version
	MovesetHeader_infos infos;
	// Absolute offsets of the moveset-related data blocks within the file
	MovesetHeader_offsets offsets;
};

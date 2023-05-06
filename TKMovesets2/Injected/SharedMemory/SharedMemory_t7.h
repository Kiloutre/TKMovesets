#pragma once

#include "SharedMemory.h"

struct SharedMemT7_Player
{
	// CRC32 of the moveset, currently unused
	uint32_t crc32;
	// Can be null, in which case no moveset is to be loaded
	uint64_t custom_moveset_addr;
	// Size of the uncompressed moveset. Todo: send the compressed moveset.
	uint64_t custom_moveset_size = 0;
	// Nth bit set = Nth bit mota missing
	uint16_t missingMotas;
	// True if moveset is ready to apply, false if moveset needs initialization
	bool is_initialized;
	// Original character ID of the loaded moveset
	uint8_t moveset_character_id;
	// Character ID of the last loaded char
	uint8_t previous_character_id;

	void SetMotaMissing(int id) {
		missingMotas |= (1 << id);
	}
	bool isMotaMissing(int id) {
		return missingMotas & (1 << id);
	}
};

struct SharedMemT7_Extraprop
{
	uint32_t playerid;
	uint32_t id;
	uint32_t value;
};

struct SharedMemT7 : SharedMemBase
{
	// List of players to send custom movesets to
	SharedMemT7_Player players[2];
	// Property to play when the exported ExecuteExtraprop() function is being called
	SharedMemT7_Extraprop propToPlay;
};
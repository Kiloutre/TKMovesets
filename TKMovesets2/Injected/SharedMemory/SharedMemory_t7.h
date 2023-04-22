#pragma once

# include <cstdint>

// Used to determine if a player was loaded before or not
# define SHARED_MEM_MOVESET_NO_CHAR (0xFF)

// todo: make this entirely game specific

struct SharedMemT7_Player
{
	// CRC32 of the moveset, currently unused
	uint32_t crc32;
	// Can be null, in which case no moveset is to be loaded
	uint64_t custom_moveset_addr;
	// Nth bit set = Nth bit mota missing
	uint16_t missingMotas;
	// True if moveset is ready to apply, false if moveset needs initialization
	bool is_initialized;
	// Original character ID of the loaded moveset
	uint8_t moveset_character_id;
	// Character ID of the last loaded char, or SHARED_MEM_MOVESET_NO_CHAR is none
	uint8_t previous_character_id = SHARED_MEM_MOVESET_NO_CHAR;

	void SetMotaMissing(int id) {
		missingMotas |= (1 << id);
	}
	bool isMotaMissing(int id) {
		return missingMotas & (1 << id);
	}
};

struct SharedMemT7
{
	// List of players to send custom movesets to
	SharedMemT7_Player players[2];
	// Only load custom movesets if locked in
	bool locked_in;
};
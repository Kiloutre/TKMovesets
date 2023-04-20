#pragma once

# include <cstdint>

// Used to determine if a player was loaded before or not
# define SHARED_MEM_MOVESET_NO_CHAR (0xFF)

struct SharedMemoryPlayer
{
	// Character ID of the loaded moveset
	uint8_t moveset_character_id;
	// Character ID of the last loaded char, or SHARED_MEM_MOVESET_NO_CHAR is none
	uint8_t previous_character_id = SHARED_MEM_MOVESET_NO_CHAR;
	// Can be null, in which case no moveset is to be loaded
	uint64_t custom_moveset_addr;
};

struct SharedMemory
{
	// Only load custom movesets if locked in
	bool locked_in;
	// List of players to send custom movesets to
	SharedMemoryPlayer players[6];
};
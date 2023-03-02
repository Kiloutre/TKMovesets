#pragma once

#include <stdint.h>

#include "constants.h"

enum GameFlag
{
	// Game can be opened for character extraction
	GameExtractable = (1 << 0),
	// Game can be open for character importation
	GameImportable = (1 << 1),
	// Game can be opened for camera animation
	GameCamera = (1 << 2),
};

// Contains a unique ID for every supported game. Every list should respect the index order here
enum GameId
{
	GameId_t7 = 0,
	GameId_t8 = 1,
	GameId_ttt2 = 2,
};

struct GameInfo
{
	const char* name;
	const char* processName;
	uint8_t characterCount;
	uint8_t flags;
};

namespace Games
{
	// Returns a pointer to a struct containing a game's name, process name, character count
	DLLCONTENT GameInfo* GetGameInfo(uint8_t idx);

	// Returns the amount of supported games
	DLLCONTENT size_t GetGamesCount();
}
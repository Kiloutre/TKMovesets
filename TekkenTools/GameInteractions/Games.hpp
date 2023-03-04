#pragma once

#include <stdint.h>

#include "GameData.hpp"
#include "GameProcess.hpp"
#include "Importer_t7.hpp"
#include "Extractor_t7.hpp"

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

// Contains a unique ID for every supported game. GameInfo list should respect the index order here
enum GameId
{
	GameId_t7 = 0,
	GameId_t8 = 1,
	GameId_ttt2 = 2,
};

struct GameInfo
{
	// Displayed name of the game
	const char* name;
	// Process that will be opened and main module name
	const char* processName;
	// Amount of characters at once the game is able to load
	uint8_t characterCount;
	// Determines what can be done with the same. See GameFlag enum for more details
	uint16_t flags;
};

namespace Games
{
	// Returns a pointer to a struct containing a game's name, process name, character count
	DLLCONTENT GameInfo* GetGameInfo(uint8_t idx);

	// Returns the amount of supported games
	DLLCONTENT size_t GetGamesCount();

	// Istantiate a new game-dependant extractor
	DLLCONTENT Extractor* FactoryGetExtractor(uint8_t gameId, GameProcess* process, GameData* game);

	// Istantiate a new game-dependant importer
	DLLCONTENT Importer* FactoryGetImporter(uint8_t gameId, GameProcess* process, GameData* game);
}
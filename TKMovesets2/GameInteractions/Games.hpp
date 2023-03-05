#pragma once

#include <stdint.h>

#include "GameData.hpp"
#include "GameProcess.hpp"
#include "Importer_t7.hpp"
#include "Extractor_t7.hpp"

#include "constants.h"

class FactoryType_Base
{
public:
	virtual ~FactoryType_Base() {}
	virtual void* allocate(GameProcess* process, GameData* game) const = 0;
	virtual void* cast(void* obj) const = 0;
};

template<typename T> class FactoryType : public FactoryType_Base
{
public:
	virtual void* allocate(GameProcess* process, GameData* game) const { return new T(process, game); }
	virtual void* cast(void* obj) const { return static_cast<T*>(obj); }
};

enum GameFlag
{
	// Game can be opened for character extraction
	GameExtractable = (1 << 0),
	// Game can be open for character importation
	GameImportable = (1 << 1),
	// Game can be opened for camera animations
	GameMovesetEdition = (1 << 2),
};

struct GameInfo
{
	// Displayed name of the game
	const char* name;
	// Process that will be opened and main module name
	const char* processName;
	// Amount of characters at once the game is able to load
	uint8_t characterCount;
	// Determines what can be done with the game. See GameFlag enum for more details
	uint16_t flags;
	// Dynamic type allocator to store the game's extractor. Can be nullptr for no available extractor.
	FactoryType_Base* extractor;
	// Dynamic type allocator to store the game's importer. Can be nullptr for no available importer.
	FactoryType_Base* importer;
};

namespace Games
{
	// Returns a pointer to a struct containing a game's name, process name, character count
	DLLCONTENT GameInfo* GetGameInfo(uint8_t idx);

	// Returns the amount of supported games
	DLLCONTENT uint8_t GetGamesCount();

	// Istantiate a new game-dependant extractor
	DLLCONTENT Extractor* FactoryGetExtractor(uint8_t gameId, GameProcess* process, GameData* game);

	// Istantiate a new game-dependant importer
	DLLCONTENT Importer* FactoryGetImporter(uint8_t gameId, GameProcess* process, GameData* game);
}
#pragma once

#include <stdint.h>

#include "GameData.hpp"
#include "GameProcess.hpp"
#include "Importer.hpp"
#include "Extractor.hpp"
#include "Editor.hpp"

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

typedef uint8_t GameFlag;
enum GameFlag_
{
	// Game can be opened for moveset edition
	GameFlag_MovesetEditable = (1 << 0),
	// Game can be opened for live moveset edition : This requires some extra methods to be implemented in the Importer
	GameFlag_MovesetLiveEditable = (1 << 1),
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
	GameFlag flags;
	// Dynamic type allocator to store the game's extractor. Can be nullptr for no available extractor.
	FactoryType_Base* extractor;
	// Dynamic type allocator to store the game's importer. Can be nullptr for no available importer.
	FactoryType_Base* importer;
	// Dynamic type allocator to store the game's editor. Can be nullptr for no available editor.
	FactoryType_Base* editor;
};

namespace Games
{
	// Returns a pointer to a struct containing a game's name, process name, character count
	DLLCONTENT GameInfo* GetGameInfo(uint8_t idx);

	// Returns the amount of supported games
	DLLCONTENT uint8_t GetGamesCount();
	// Returns the amount of games that upport extraction
	DLLCONTENT size_t GetExtractableGamesCount();
	// Returns the amount of games that upport importation
	DLLCONTENT size_t GetImportableGamesCount();

	// Returns true if moveset edition on the moveset is allowed
	bool IsGameEditable(uint8_t gameId);
	// Returns true if live moveset edition on the moveset is allowed
	bool IsGameLiveEditable(uint8_t gameId);

	// Istantiate a new game-dependant extractor
	DLLCONTENT Extractor* FactoryGetExtractor(uint8_t gameId, GameProcess* process, GameData* game);

	// Istantiate a new game-dependant importer
	DLLCONTENT Importer* FactoryGetImporter(uint8_t gameId, GameProcess* process, GameData* game);

	// Istantiate a new game-dependant importer
	DLLCONTENT Editor* FactoryGetEditor(uint8_t gameId, GameProcess* process, GameData* game);
}
#pragma once

#include <stdint.h>

#include "GameData.hpp"
#include "GameProcess.hpp"
#include "Importer.hpp"
#include "Extractor.hpp"
#include "Editor.hpp"
#include "Online.hpp"

#include "constants.h"

enum GameId_
{
	GameId_T7,
	GameId_T8,
	GameId_TTT2,
};

class FactoryType_Base
{
public:
	virtual ~FactoryType_Base() {}
	virtual void* allocate(GameProcess* process, GameData* game, uint16_t gameId, uint16_t minorVersion) const = 0;
	virtual void* cast(void* obj) const = 0;
};

template<typename T> class FactoryType : public FactoryType_Base
{
public:
	virtual void* allocate(GameProcess* process, GameData* game, uint16_t gameId, uint16_t minorVersion) const { return new T(process, game, gameId, minorVersion); }
	virtual void* cast(void* obj) const { return static_cast<T*>(obj); }
};

typedef uint8_t GameFlag;
enum GameFlag_
{
	// Game can be opened for live moveset edition : This requires some extra methods to be implemented in the Importer
	GameFlag_MovesetLiveEditable = (1 << 0),
};

struct GameInfo
{
	// Displayed name of the game
	const char* name;
	// Process that will be opened and main module name
	const char* processName;
	// ID of the game, used to decide which extractor/importer/etc to use
	uint16_t gameId;
	// Minor version, used to implement behaviour variations inside of the extractor/importer/etc
	uint16_t minorVersion;
	// Amount of characters at once the game is able to load
	uint8_t characterCount;
	// Determines what can be done with the game. See GameFlag enum for more details
	GameFlag flags;
	// Used to build path for label files
	const char* dataString;
	// Dynamic type allocator to store the game's extractor. Can be nullptr for no available extractor.
	FactoryType_Base* extractor;
	// Dynamic type allocator to store the game's importer. Can be nullptr for no available importer.
	FactoryType_Base* importer;
	// Dynamic type allocator to store the game's editor. Can be nullptr for no available editor.
	FactoryType_Base* editor;
	// Dynamic type allocator to store the game's online handler. Can be nullptr for no available handler.
	FactoryType_Base* onlineHandler;
};

namespace Games
{
	// Returns a pointer to a struct containing a game's name, process name, character count
	DLLCONTENT const GameInfo* GetGameInfoFromIndex(unsigned int index);
	DLLCONTENT const GameInfo* GetGameInfoFromIdentifier(uint16_t gameId);
	DLLCONTENT const GameInfo* GetGameInfoFromIdentifier(uint16_t gameId, uint16_t minorVersion);

	// Returns the amount of supported games
	DLLCONTENT unsigned int GetGamesCount();
	// Returns the amount of games that upport extraction
	DLLCONTENT unsigned int GetExtractableGamesCount();
	// Returns the amount of games that upport importation
	DLLCONTENT unsigned int GetImportableGamesCount();

	// Returns true if moveset edition on the moveset is allowed
	bool IsGameEditable(uint16_t gameId, uint16_t minorVersion);
	// Returns true if live moveset edition on the moveset is allowed
	bool IsGameLiveEditable(uint16_t gameId, uint16_t minorVersion);

	// Instantiate a new game-specific extractor
	DLLCONTENT Extractor* FactoryGetExtractor(const GameInfo* gameInfo, GameProcess* process, GameData* game);

	// Instantiate a new game-specific importer
	DLLCONTENT Importer* FactoryGetImporter(const GameInfo* gameInfo, GameProcess* process, GameData* game);

	// Instantiate a new game-specific importer
	DLLCONTENT Editor* FactoryGetEditor(const GameInfo* gameInfo, GameProcess* process, GameData* game);

	// Instantiate a new game-specific online handler
	DLLCONTENT Online* FactoryGetOnline(const GameInfo* gameInfo, GameProcess* process, GameData* game);
}
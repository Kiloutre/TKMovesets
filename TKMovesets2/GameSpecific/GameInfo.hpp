#pragma once

#include <stdint.h>
#include <set>

#include "GameData.hpp"
#include "GameProcess.hpp"

#include "constants.h"

struct GameInfo;

class FactoryType_Base
{
public:
	virtual ~FactoryType_Base() {}
	virtual void* allocate(GameProcess* process, GameData* game, const GameInfo* gameInfo) const = 0;
	virtual void* cast(void* obj) const = 0;
};

template<typename T> class FactoryType : public FactoryType_Base
{
public:
	virtual void* allocate(GameProcess* process, GameData* game, const GameInfo* gameInfo) const { return new T(process, game, gameInfo); }
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
public:
	// Displayed name of the game
	const char* name;
	// Process that will be opened and main module name
	const char* processName;
	// Prefix of the extracted files of this game
	const char* movesetNamePrefix;
	// ID of the game, used to decide which extractor/importer/etc to use
	uint16_t gameId;
	// Minor version, used to implement behaviour variations inside of the extractor/importer/etc
	uint16_t minorVersion;
	// Amount of characters at once the game is able to load
	uint8_t characterCount;
	// Determines what can be done with the game. See GameFlag enum for more details
	GameFlag flags;
	// Used to build path for label files and game addresses, lower priority than .minorDataString
	const char* dataString;
	// Used to build path for label files and game addresses, higher priority than .dataString
	const char* minorDataString;
	// List of movesets' games we can import from, doesn't have to include ourselves
	std::set<uint16_t> supportedImports;
	// Dynamic type allocator to store the game's extractor. Can be nullptr for no available extractor.
	FactoryType_Base* extractor;
	// Dynamic type allocator to store the game's importer. Can be nullptr for no available importer.
	FactoryType_Base* importer;
	// Dynamic type allocator to store the game's editor. Can be nullptr for no available editor.
	FactoryType_Base* editor;
	// Dynamic type allocator to store the game's online handler. Can be nullptr for no available handler.
	FactoryType_Base* onlineHandler;


	bool SupportsGameImport(uint16_t t_gameId) const
	{
		return this != nullptr && (gameId == t_gameId || supportedImports.contains(t_gameId));
	}
};
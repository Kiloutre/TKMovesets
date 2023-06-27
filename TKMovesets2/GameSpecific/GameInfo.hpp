#pragma once

#include <stdint.h>
#include <set>

#include "GameData.hpp"
#include "GameProcess.hpp"
#include "LocalStorage.hpp"

#include "constants.h"

#pragma warning (push)
#pragma warning (disable:4099)
struct GameInfo;
#pragma warning (pop)

class GameFactory_Base
{
public:
	virtual ~GameFactory_Base() {}
	virtual void* allocate(GameProcess& process, GameData& game, const GameInfo* gameInfo) const = 0;
	virtual void* cast(void* obj) const = 0;
};

template<typename T> class GameFactory : public GameFactory_Base
{
public:
	virtual void* allocate(GameProcess& process, GameData& game, const GameInfo* gameInfo) const { return new T(process, game, gameInfo); }
	virtual void* cast(void* obj) const { return static_cast<T*>(obj); }
};


class EditorFactory_Base
{
public:
	virtual ~EditorFactory_Base() {}
	virtual void* allocate(const movesetInfo* movesetInfos, GameAddressesFile* addrFile, LocalStorage* storage) const = 0;
	virtual void* cast(void* obj) const = 0;
};

template<typename T> class EditorFactory : public EditorFactory_Base
{
public:
	virtual void* allocate(const movesetInfo* movesetInfos, GameAddressesFile* addrFile, LocalStorage* storage) const { return new T(movesetInfos, addrFile, storage); }
	virtual void* cast(void* obj) const { return static_cast<T*>(obj); }
};

typedef uint8_t GameFlag;
enum GameFlag_
{
	// Game can be opened for live moveset edition : This requires some extra methods to be implemented in the Importer
	GameFlag_MovesetLiveEditable = (1 << 0),
};

#pragma warning (push)
#pragma warning (disable:4099)
struct GameInfo
{
	// Separator text to display above the game
	const char* separatorText = nullptr;

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
	// Size of PTRs in the game
	uint8_t ptrSize;
	// Determines what can be done with the game. See GameFlag enum for more details
	GameFlag flags;
	// Used to build path for label files and game addresses, lower priority than .minorDataString
	const char* dataString;
	// Used to build path for label files and game addresses, higher priority than .dataString
	const char* minorDataString;
	// List of movesets' games we can import from, doesn't have to include ourselves
	std::set<uint16_t> supportedImports;
	// List of movesets' games we can import from, doesn't have to include ourselves
	std::set<uint16_t> supportedOnlineImports;
	// Dynamic type allocator to store the game's extractor. Can be nullptr for no available extractor.
	GameFactory_Base* extractor;
	// Dynamic type allocator to store the game's importer. Can be nullptr for no available importer.
	GameFactory_Base* importer;
	// Dynamic type allocator to store the game's editor. Can be nullptr for no available editor.
	GameFactory_Base* editorLogic;
	// Dynamic type allocator to store the game's editor visuals. Can be nullptr for no available editor.
	EditorFactory_Base* editorVisuals;
	// Dynamic type allocator to store the game's online handler. Can be nullptr for no available handler.
	GameFactory_Base* onlineHandler;
	// Address called to get the base address of the game
	uint64_t(*GetBaseAddressFunc)(const GameInfo* game, const GameAddressesFile* addrFile, const GameProcess* process) = nullptr;
	// True if game values must be interpreted as big endian
	bool bigEndian;
	// Substrings that will be searched in process window. Leave empty to match everything.
	std::vector<const char*> matchingProcessSubstrings;


	bool SupportsGameImport(uint16_t t_gameId) const
	{
		return this != nullptr && (gameId == t_gameId || supportedImports.contains(t_gameId));
	}


	bool SupportsOnlineGameImport(uint16_t t_gameId) const
	{
		return this != nullptr && (gameId == t_gameId || supportedOnlineImports.contains(t_gameId));
	}

	bool IsEditable() const
	{
		return editorLogic != nullptr && editorVisuals != nullptr;
	}

	bool MatchesProcessWindowName(DWORD pid) const
	{
		return matchingProcessSubstrings.size() == 0 || GameProcessUtils::FindWindowText(pid, matchingProcessSubstrings);
	}
};
#pragma warning (pop)
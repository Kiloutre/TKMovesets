#pragma once

#include <windows.h>

#include "GameProcess.hpp"
#include "GameData.hpp"
#include "LocalStorage.hpp"

#include "constants.h"
#include "SharedMemory.h"

class DLLCONTENT Online
{
protected:
	// Stores a handle to the shared memory
	HANDLE m_memoryHandle = nullptr;
	// Stores the process
	GameProcess* m_process;
	// Stores a helper class to read the game's memory from strings in game_addresses.txt
	GameData* m_game;
	// Contains whether or not we have injected our DLL
	bool m_injectedDll = false;
	// Ptr to the shared memory
	SharedMemory* m_sharedMemPtr = nullptr;

	// Calls a function of the MovesetLoader inside of the remote process. Returns false if failure was encountered somewhere.
	bool CallMovesetLoaderFunction(const char* functionName, bool waitEnd=false);
	// Returns the name of the shared memory to look after
	virtual const TCHAR* GetSharedMemoryName() = 0;
public:
	// Contains a copy of the basic moveset informations we have loaded
	std::vector<movesetInfo>* movesetInfos;

	Online(GameProcess* process, GameData* game);
	~Online();

	// Load the shared memory handle
	bool LoadSharedMemory();
	// Returns true if the shared memory has been found and loaded
	bool IsMemoryLoaded();
	// Injects the DLL into the current process
	bool InjectDll();
	// Called when a moveset is successfully loaded in the game's memory by the importer
	void OnMovesetImport(movesetInfo* moveset, gameAddr movesetAddr, unsigned int playerId);
};
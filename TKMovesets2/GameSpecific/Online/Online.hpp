#pragma once

#include <windows.h>

#include "GameProcess.hpp"
#include "GameData.hpp"

#include "constants.h"

class DLLCONTENT Online
{
protected:
	// Stores a handle to the shared memory
	HANDLE m_memoryHandle = nullptr;
	// Stores the process
	GameProcess* m_process;
	// Stores a helper class to read the game's memory from strings in game_addresses.txt
	GameData* m_game;

	// Returns the name of the shared memory to look after
	virtual const TCHAR* GetSharedMemoryName() = 0;
public:
	Online(GameProcess* process, GameData* game) : m_process(process), m_game(game) {}
	~Online();

	// Load the shared memory handle
	bool LoadSharedMemory();
	// Returns true if the shared memory has been found and loaded
	bool IsMemoryLoaded();
	// Injects the DLL into the current process
	bool InjectDll();
};
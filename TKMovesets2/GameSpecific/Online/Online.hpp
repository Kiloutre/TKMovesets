#pragma once

#include <windows.h>
#include <thread>

#include "GameProcess.hpp"
#include "GameData.hpp"
#include "LocalStorage.hpp"
#include "BaseGameSpecificClass.hpp"

#include "constants.h"
#include "SharedMemory.h"

#ifndef LAST_LOADED_DEFINED
# define LAST_LOADED_DEFINED
struct s_lastLoaded {
	// Last loaded moveset's CRC32 ID, used right after import by online importer
	uint8_t charId = 0;
	// Last loaded moveset's character ID, used right after import by online importer
	uint32_t crc32 = 0;
	// Stores the in-game address of the moveset successfully loaded by the last Import() call
	gameAddr address = 0;
};
#endif

class DLLCONTENT Online : public BaseGameSpecificClass
{
protected:
	// Stores a handle to the shared memory
	HANDLE m_memoryHandle = nullptr;
	// Ptr to the shared memory
	SharedMemBase* m_orig_sharedMemPtr = nullptr;
	// Thread that will run the DLL injection
	std::thread* m_dllInjectionThread;
	// True if dllInjection thread has been started at least once and must be joined
	bool m_startedThread = false;

	// Calls a function of the MovesetLoader inside of the remote process. Returns false if failure was encountered somewhere.
	bool CallMovesetLoaderFunction(const char* functionName, bool waitEnd=false);
	// Returns the name of the shared memory to look after
	virtual const TCHAR* GetSharedMemoryName() = 0;
	// Called after the shared memory has been successfully initialized
	virtual void Init() = 0;
public:
	// Contains a copy of the basic moveset informations we have loaded
	std::vector<movesetInfo>* displayedMovesets;
	// True if we are currently injecting a DLL
	bool isInjecting = false;
	// Contains whether or not we have injected our DLL
	bool injectedDll = false;
	// If the shared memory is to be reseted && the injected DLL freed when this instance gets deleted.
	// If you have multiple instances of sharedMem, it might be good to only have one reset the shared memory & free the dll
	bool resetMemOnDestroy = false;
	// Becomes true if the injected DLL version does not match the current program's one
	bool versionMismatch = false;

	Online(GameProcess* process, GameData* game, const GameInfo* gameInfo);
	virtual ~Online();

	// Check if the DLL's version does not match the current one, sets .versionMismatch and returns its value
	bool VerifyDllVersionMismatch();
	// Load the shared memory handle
	bool LoadSharedMemory();
	// Returns true if the shared memory has been found and loaded
	bool IsMemoryLoaded();
	// Injects the DLL into the current process, not waiting for it to be completed
	void InjectDll();
	// Injects the DLL into the current process, waiting for it to be completed. Returns true/false if the library was successfully loaded or not.
	bool InjectDllAndWaitEnd();
	// Called when a moveset is successfully loaded in the game's memory by the importer
	virtual void OnMovesetImport(movesetInfo* displayedMoveset, unsigned int playerId, const s_lastLoaded& lastLoadedMoveset) = 0;
	// Called when the user wants to de-select a moveset for a given player
	virtual void ClearMovesetSelection(unsigned int playerid) = 0;
	// Enable or disable custom moveset loading for the game
	virtual void SetLockIn(bool locked) = 0;
};
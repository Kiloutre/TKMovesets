#pragma once

#include <windows.h>

#include "Online.hpp"
#include "SharedMemory_t8.h"

class OnlineT8 : public Online
{
private:
	// Ptr to the shared memory
	SharedMemT8* m_sharedMemPtr = nullptr;

	const TCHAR* GetSharedMemoryName() {
		return TEXT("Local\\TKMovesets2T8Mem");
	}
public:
	using Online::Online; // Inherit constructor too
	~OnlineT8() override;

	void Init() override;
	void OnMovesetImport(movesetInfo* displayedMoveset, unsigned int playerId, const s_lastLoaded& lastLoadedMoveset) override;	// Called when the user wants to de-select a moveset for a given player
	void ClearMovesetSelection(unsigned int playerid) override;
	void SetLockIn(bool locked, MovesetLoaderMode_ movesetLoaderMode) override;
	void ExecuteExtraprop(uint32_t playerid, uint32_t id, uint32_t value);
	// The loader's shared memory (Tag commands, Tools > Extra), null until LoadSharedMemory() succeeded
	SharedMemT8* SharedMemory() const { return m_orig_sharedMemPtr ? m_sharedMemPtr : nullptr; }
	// Asks the injected loader to stop and unload itself
	bool StopLoader() { return CallMovesetLoaderFunction(MOVESET_LOADER_STOP_FUNC, true); }
};

#pragma once

#include <windows.h>

#include "Online.hpp"
#include "SharedMemory_t7.h"

class DLLCONTENT OnlineT7 : public Online
{
private:
	// Ptr to the shared memory
	SharedMemT7* m_sharedMemPtr = nullptr;

	const TCHAR* GetSharedMemoryName() {
		return TEXT("Local\\TKMovesets2T7Mem");
	}
public:
	using Online::Online; // Inherit constructor too
	~OnlineT7() override;

	void Init() override;
	void OnMovesetImport(movesetInfo* displayedMoveset, unsigned int playerId, const s_lastLoaded& lastLoadedMoveset) override;	// Called when the user wants to de-select a moveset for a given player
	void ClearMovesetSelection(unsigned int playerid) override;
	void SetLockIn(bool locked) override;
	void ExecuteExtraprop(uint32_t playerid, uint64_t id, uint64_t value) override;
};

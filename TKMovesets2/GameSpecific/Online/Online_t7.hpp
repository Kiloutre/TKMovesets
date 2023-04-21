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

	void Init() override;
	void OnMovesetImport(movesetInfo* displayedMoveset, gameAddr movesetAddr, unsigned int playerId, uint8_t characterId);
};

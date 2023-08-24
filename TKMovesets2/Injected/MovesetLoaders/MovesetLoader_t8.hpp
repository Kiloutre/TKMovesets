#pragma once

#include "MovesetLoader.hpp"

#include "Structs_t8.h"
#include "SharedMemory_t8.h"
#include "steam_api.h"

// -- Moveset loader -- //

class MovesetLoaderT8 : public MovesetLoader
{
private:

	const TCHAR* GetSharedMemoryName() override {
		return TEXT("Local\\TKMovesets2T8Mem");
	}

	void SetAddressesGameKey() override
	{
		addresses.gameKey = "t8";
		addresses.minorGameKey = "t8";
	}

public:
	MovesetLoaderT8()
	{
		m_loadSteam = false;
	}

	// Sale ptr to the shared memory as base class but casted
	SharedMemT8* sharedMemPtr = nullptr;

	void InitHooks() override;
	void OnInitEnd() override;
	void Mainloop() override;
	void ExecuteExtraprop() override {};

	void Debug() override;
};

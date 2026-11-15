#pragma once

#include "MovesetLoader.hpp"

#include "Structs_t8cnt.h"
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
		addresses.gameKey = "t8cbt";
		addresses.minorGameKey = "t8cbt";
	}

public:
	MovesetLoaderT8()
	{
		m_loadSteam = false;
	}

	// Sale ptr to the shared memory as base class but casted
	SharedMemT8* sharedMemPtr = nullptr;
	// Retail build (no closed beta signatures): only the Tag mode runs
	bool isRetail = false;
	// Why TagT8::Init() failed, shown in Tools > Extra
	std::string tagInitError;
	int32_t lastTriggerId = 0;

	// Takes the Tag commands written by Tools > Extra and reports the Tag state back
	void HandleTagCommands();

	void InitHooks() override;
	void OnInitEnd() override;
	void Mainloop() override;
	void ExecuteExtraprop() override {};

	void Debug() override;
};

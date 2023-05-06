#pragma once

#include "MovesetLoader.hpp"

#include "Structs_t7.h"
#include "SharedMemory_t7.h"

struct MovesetLoaderT7_IncomingMovesetInfo
{
	const char packetMagicBytes[4] = { 'T', 'K', 'M', '2' };
	// Size of the moveset to accept. Will not read any more than .size bytes.
	// If size is zero, that means the opponent has purposefully not chosen a moveset
	uint64_t size = 0;
};

class MovesetLoaderT7 : public MovesetLoader
{
private:

	const TCHAR* GetSharedMemoryName() override {
		return TEXT("Local\\TKMovesets2T7Mem");
	}

	void SetAddressesGameKey() override
	{
		addresses.gameKey = "t7";
		addresses.minorGameKey = "t7";
	}

public:
	// Sale ptr to the shared memory as base class but casted
	SharedMemT7* sharedMemPtr = nullptr;

	// Online opponent to send moveset to / receive moveset from. Only used in OnlinePlay mode.
	CSteamID opponent;
	// Size in bytes expected of the coming moveset
	uint64_t incomingMovesetSize;
	// Ptr to the incoming moveset
	Byte* incomingMoveset;
	// Ptr to the next area to write to in the incoming moveset
	Byte* incomingMovesetCursor;

	void InitHooks() override;
	void PostInit() override;
	void Mainloop() override;
	void ExecuteExtraprop() override;

	// Called when a moveset is expected and a packet is received
	void OnPacketReceive(CSteamID senderId, Byte* packetBuf, uint32_t packetSize);
	// Inits the variable that tell that a moveset is expected (if locked in, opponent valid etc), and send our own moveset to the opponent
	void InitMovesetSyncing();

	void Debug() override;
};
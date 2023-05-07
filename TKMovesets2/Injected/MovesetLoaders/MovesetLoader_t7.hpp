#pragma once

#include "MovesetLoader.hpp"

#include "Structs_t7.h"
#include "SharedMemory_t7.h"
#include "steam_api.h"

// -- Packet -- //

enum PacketT7Type_
{
	PacketT7Type_INVALID,
	PacketT7Type_SyncMoveset,
	PacketT7Type_AnswerMovesetSync,
};

struct PacketT7
{
	const char packetMagicBytes[11] = "TKM2PACKET";
	// Type of the packet, will determine the size of the structure
	PacketT7Type_ packetType = PacketT7Type_INVALID;
};

struct PacketT7_SyncMoveset : PacketT7
{
	struct {
		// Size of the moveset to send (0 if no moveset to send)
		uint64_t size;
	} local_moveset;

	struct {
		// CRC32 of the previously received moveset
		uint32_t crc32;
	} remote_moveset;

	PacketT7_SyncMoveset() {
		packetType = PacketT7Type_SyncMoveset;
	};
};

struct PacketT7_AnswerMovesetSync: PacketT7
{
	// If true, request the opponent to send their moveset to us
	// If false, tell the opponent that we already have his moveset
	bool requesting_download;

	PacketT7_AnswerMovesetSync() {
		packetType = PacketT7Type_AnswerMovesetSync;
	};
};

struct MovesetLoaderT7_IncomingMovesetInfo
{
	const char packetMagicBytes[4] = { 'T', 'K', 'M', '2' };
	// Size of the moveset to accept. Will not read any more than .size bytes.
	// If size is zero, that means the opponent has purposefully not chosen a moveset
	uint64_t size = 0;
};

// -- Other -- //

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
	// Ptr to the incoming moveset
	Byte* incomingMoveset = nullptr;
	// Size in bytes expected of the coming moveset
	uint64_t incomingMovesetSize = 0;
	// Ptr to the next area to write to in the incoming moveset
	Byte* incomingMovesetCursor = nullptr;

	void InitHooks() override;
	void PostInit() override;
	void Mainloop() override;
	void ExecuteExtraprop() override;

	// Called when a moveset is expected and a packet is received
	void OnPacketReceive(CSteamID senderId, Byte* packetBuf, uint32_t packetSize);
	// Inits the variable that tell that a moveset is expected (if locked in, opponent valid etc), and send our own moveset to the opponent
	void InitMovesetSyncing();

	// Moveset importing function
	Byte* ImportForOnline(SharedMemT7_Player& player, Byte* moveset, uint64_t s_moveset);
	// Game-specific Moveset importing functions, returns false on failure
	bool ImportForOnline_FromT7(const TKMovesetHeader* header, Byte* moveset, uint64_t s_moveset);

	void Debug() override;
};
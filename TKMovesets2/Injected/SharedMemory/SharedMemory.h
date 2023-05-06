#pragma once

enum MovesetLoaderMode_
{
	MovesetLoaderMode_Invalid = 0,
	MovesetLoaderMode_OnlineMode,
	MovesetLoaderMode_PersistentPlay,
};

enum MovesetSyncStatus_
{
	MovesetSyncStatus_NotStarted,
	MovesetSyncStatus_Syncing,
	MovesetSyncStatus_Discard,
	MovesetSyncStatus_Synced,
};

struct SharedMemBase
{
	// Contains the version of the moveset loader
	char moveset_loader_version[32];
	// Only load custom movesets if locked in
	bool locked_in;
	// Status of the automatic moveset syncing
	MovesetSyncStatus_ moveset_sync_status;
	// Depending on the mode (persistent / online play), files might be sent over, movesets might be applied to different players, etc
	MovesetLoaderMode_ moveset_loader_mode;

	bool OnlinePlayMovesetsNotUseable()
	{
		return moveset_loader_mode == MovesetLoaderMode_OnlineMode && moveset_sync_status != MovesetSyncStatus_Synced;
	}

	bool AcceptingPackets()
	{
		return moveset_sync_status == MovesetSyncStatus_Discard || moveset_sync_status == MovesetSyncStatus_Syncing;
	}
};
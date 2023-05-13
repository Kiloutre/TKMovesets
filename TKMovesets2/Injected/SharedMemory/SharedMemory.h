#pragma once

enum MovesetLoaderMode_
{
	MovesetLoaderMode_Invalid = 0,
	MovesetLoaderMode_OnlineMode,
	MovesetLoaderMode_PersistentPlay,
};

enum MovesetSyncStatus_
{
	// Packets will not be received (and be buffered) as long as the NotStarted state is on
	MovesetSyncStatus_NotStarted,

	// Packets will be received
	MovesetSyncStatus_AcceptPackets,

	// Stage 1
	MovesetSyncStatus_RequestSync,
	// Stage 2
	MovesetSyncStatus_DownloadingMoveset,
	MovesetSyncStatus_AwaitingReady,
	// Stage 3
	MovesetSyncStatus_Ready,
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
	// Stores the path to the program, used in order to tell the moveset loader where to look for movesets if needed
	wchar_t program_path[MAX_PATH];

	bool IsAttemptingOnlinePlay() const {
		return locked_in && moveset_loader_mode && (moveset_sync_status != MovesetSyncStatus_NotStarted && moveset_sync_status != MovesetSyncStatus_AcceptPackets);
	}
	bool OnlinePlayMovesetsNotUseable() const
	{
		return locked_in && moveset_loader_mode == MovesetLoaderMode_OnlineMode && (moveset_sync_status != MovesetSyncStatus_Ready && moveset_sync_status != MovesetSyncStatus_AcceptPackets);
	}
};
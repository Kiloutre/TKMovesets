#pragma once

#include <fstream>

#include "GameData.hpp"
#include "GameProcess.hpp"

#include "constants.h"

// Turns an index into an absolute address, or NULL (0) if index is -1
# define FROM_INDEX(field, listStartAddr, type) (field = (field == -1 ? 0 : listStartAddr + (field * sizeof(type))))

typedef uint64_t ImportSettings; // 64 flags
enum ImportSettings_
{
	// By default, orig aliases overwrites the current ones. This does the opposite.
	ImportSettings_EnforceCurrentAliasesAsDefault = (1 << 0),
	// Force the current move to end early to switch to the new moveset asap
	ImportSettings_ApplyInstantly = (1 << 1),
	// Free old movesets if they are unused after successfully importing the new one
	ImportSettings_FreeUnusedMovesets = (1 << 2),
	// Don't write the new moveset address to the current player
	ImportSettings_BasicLoadOnly = (1 << 3),

	ImportSettings_DEFAULT = ImportSettings_ApplyInstantly | ImportSettings_FreeUnusedMovesets,
};

enum ImportationErrcode_
{
	ImportationErrcode_Successful,
	ImportationErrcode_AllocationErr,
	ImportationErrcode_GameAllocationErr,
	ImportationErrcode_FileReadErr,
};

// Base class for extracting from a game
class DLLCONTENT Importer
{
protected:
	// Stores the process to read on
	GameProcess* m_process;
	// Stores a helper class to read the game's memory from strings in game_addresses.txt
	GameData* m_game;

public:
	// Stores the number of character we are expected be able to import to
	// You shouldn't set this here but in the game list file (Games.cpp). The 1 here should get overwritten or something has gone wrong.
	uint8_t characterCount = 1;
	// Stores the in-game address of the moveset successfully loaded by the last Import() call
	gameAddr lastLoadedMoveset = 0;

	Importer(GameProcess* process, GameData* game) : m_process(process), m_game(game) {}
	// Import moveset from filename. Just a wrapper for Import(Byte*...). Does not need to be overriden, will send the entire file to the Import() function.
	ImportationErrcode_ Import(const wchar_t* filename, gameAddr playerAddress, ImportSettings settings, uint8_t& progress);
	// Secondary import method taking bytes instead of a file
	virtual ImportationErrcode_ Import(const Byte* orig_moveset, uint64_t s_moveset, gameAddr playerAddress, ImportSettings settings, uint8_t& progress) = 0;
	// Returns true if importation is possible
	virtual bool CanImport() = 0;
	// Look through movesets that we previously allocated in the game and free the unused ones
	virtual void CleanupUnusedMovesets() = 0;

	// Returns a character address depending on the provided playerid
	virtual gameAddr GetCharacterAddress(uint8_t playerId) = 0;
	// Returns the moveset address of the given playerid. Used by live editor.
	virtual gameAddr GetMovesetAddress(uint8_t playerId) = 0;
};
#pragma once

#include <fstream>

#include "GameData.hpp"
#include "GameProcess.hpp"
#include "BaseGameSpecificClass.hpp"

#include "constants.h"
#include "MovesetStructs.h"
#include "lastLoadedMoveset.h"

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
	// The original moveset data will also be loaded, used by for online moveset syncing
	ImportSettings_ImportOriginalData = (1 << 4),

	ImportSettings_DEFAULT = ImportSettings_ApplyInstantly | ImportSettings_FreeUnusedMovesets,
};

enum ImportationErrcode_
{
	ImportationErrcode_Successful,
	ImportationErrcode_AllocationErr,
	ImportationErrcode_GameAllocationErr,
	ImportationErrcode_FileReadErr,
	ImportationErrcode_DecompressionError,
	ImportationErrcode_UnsupportedGameVersion,
	ImportationErrcode_MovesetDataError,
};

// Base class for extracting from a game
class Importer : public BaseGameSpecificClass
{
protected:
	// Internal import method that will decompress the moveset (if needed) and get a ptr to the moveset data before calling ImportMOvesetData
	ImportationErrcode_ _Import(Byte* moveset, uint64_t s_moveset, gameAddr playerAddress, ImportSettings settings, uint8_t& progress);

	virtual ImportationErrcode_ ImportMovesetData(const TKMovesetHeader* header, Byte* moveset, uint64_t s_moveset, gameAddr playerAddress, ImportSettings settings, uint8_t& progress) = 0;
public:
	// Store informations about the last loaded moveset such as its address, crc32, character ID. Used by online shared memory.
	s_lastLoaded lastLoaded;

	// Inherit copy constructor
	using BaseGameSpecificClass::BaseGameSpecificClass;

	// Import moveset from filename. Does not need to be overriden, will send the entire file to the _Import() function.
	ImportationErrcode_ Import(const wchar_t* filename, gameAddr playerAddress, ImportSettings settings, uint8_t& progress);
	// Import moveset from bytes. Does not need to be overriden. Will make a copy of the passed moveset and send it to _Import()
	ImportationErrcode_ Import(const Byte* orig_moveset, uint64_t s_moveset, gameAddr playerAddress, ImportSettings settings, uint8_t& progress);

	// Returns true if importation is possible
	virtual bool CanImport() = 0;
	// Look through movesets that we previously allocated in the game and free the unused ones
	virtual void CleanupUnusedMovesets() = 0;

	// Returns a character address depending on the provided playerid
	virtual gameAddr GetCharacterAddress(uint8_t playerId) = 0;
	// Returns the moveset address of the given playerid. Used by live editor.
	virtual gameAddr GetMovesetAddress(uint8_t playerId) = 0;
};
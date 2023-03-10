#pragma once

#include <fstream>

#include "GameData.hpp"
#include "GameProcess.hpp"

#include "constants.h"

// Turns an index into an absolute address, or NULL (0) if index is -1
# define FROM_INDEX(field, listStartAddr, type) (field = (field == -1 ? 0 : listStartAddr + (field * sizeof(type))))

// todo: actually handle these error codes and display an err message
enum ImportationErrcode_
{
	ImportationErrcode_Successful = 0,
	ImportationErrcode_AllocationErr = 1,
	ImportationErrcode_GameAllocationErr = 2,
	ImportationErrcode_FileReadErr = 3,
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

	Importer(GameProcess* process, GameData* game) : m_process(process), m_game(game) {}
	// Pure virtual base method meant to do the heavy lifting
	virtual ImportationErrcode_ Import(const char* filename, gameAddr playerAddress, bool applyInstantly, uint8_t& progress) = 0;
	// Secondary import method taking bytes instead ofa file
	virtual ImportationErrcode_ Import(byte* moveset, uint64_t s_moveset, gameAddr playerAddress, bool applyInstantly, uint8_t& progress) = 0;
	// Returns true if extraction is possible (characters have been loaded)...
	virtual bool CanImport() = 0;
	// Look through movesets that we previously allocated in the game and free the unused ones
	virtual void CleanupUnusedMovesets() = 0;
	// Returns a character address depending on the provided playerid
	virtual gameAddr GetCharacterAddress(uint8_t playerId) = 0;
};
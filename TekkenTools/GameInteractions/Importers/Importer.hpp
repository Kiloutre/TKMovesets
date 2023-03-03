#pragma once

#include <fstream>

#include "GameData.hpp"
#include "GameProcess.hpp"
#include "GameAddressesFile.hpp"
#include "LocalStorage.hpp"

#include "constants.h"
#include "GameAddresses.h"

// Base class for extracting from a game
class DLLCONTENT Importer
{
protected:
	// Stores the extraction directory
	// Stores the process to read on
	GameProcess* m_process;
	// Stores a helper class to read the game's memory from strings in game_addresses.txt
	GameData* m_game;

public:
	Importer(GameProcess* process, GameData* game) : m_process(process), m_game(game) {}
	// Pure virtual base method meant to do the heavy lifting
	virtual void Import(const char* filename, gameAddr playerAddress, float* progress) = 0;
	// Returns true if extraction is possible (characters have been loaded)...
	virtual bool CanImport() = 0;
};
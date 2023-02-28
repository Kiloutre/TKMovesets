#pragma once

#include "GameData.hpp"
#include "GameProcess.hpp"
#include "GameAddresses.h"
#include "GameAddressesFile.hpp"

// Base class for extracting from a game
class Extractor
{
public:
	Extractor(GameProcess* process, GameData* game) : m_process(process), m_game(game) {}
	// Pure virtual base method meant to do the heavy lifting
	virtual void Extract(gameAddr playerAddress, float* progress) = 0;

protected:
	// Extraction directory
	const char* extractionDirt = "./extracted_chars";
	// Stores the process to read on
	GameProcess* m_process;
	// Stores a helper class to read the game's memory from strings in game_addresses.txt
	GameData* m_game;
	
	// Calculates a block size from start to end, writes it to &size_out and return a pointer pointing to a new allocated space containing the data in the block
	void* allocateAndReadBlock(gameAddr blockStart, gameAddr blockEnd, uint64_t& size_out);
};
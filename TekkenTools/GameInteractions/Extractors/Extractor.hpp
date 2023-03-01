#pragma once

#include <fstream>

#include "GameData.hpp"
#include "GameProcess.hpp"
#include "GameAddressesFile.hpp"

#include "GameAddresses.h"

// Base class for extracting from a game
class Extractor
{
public:
	Extractor(GameProcess* process, GameData* game) : m_process(process), m_game(game) {}
	// Pure virtual base method meant to do the heavy lifting
	virtual void Extract(gameAddr playerAddress, float* progress) = 0;
	// Returns a string containing the character name of the provided playerId
	virtual std::string GetPlayerCharacterName(gameAddr playerAddress) = 0;
	virtual bool CanExtract() = 0;
protected:
	// Stores the extraction directory
	const char* cm_extractionDir = "./extracted_chars";
	// Stores the process to read on
	GameProcess* m_process;
	// Stores a helper class to read the game's memory from strings in game_addresses.txt
	GameData* m_game;
	// Stores a handle to the file to write on
	std::ofstream m_file;
	
	// Calculates a block size from start to end, writes it to &size_out and return a pointer pointing to a new allocated space containing the data in the block
	void* allocateAndReadBlock(gameAddr blockStart, gameAddr blockEnd, uint64_t& size_out);
	// Creates the file to later write on
	void CreateMovesetFile(const char* characterName, const char* gameIdentifierstring);
	// Close the moveset file. Must be called once the extraction is over.
	void CloseMovesetFile();
};
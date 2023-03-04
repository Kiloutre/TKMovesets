#pragma once

#include <fstream>

#include "GameData.hpp"
#include "GameProcess.hpp"

#include "constants.h"

enum ExtractionErrcode
{
	ExtractionSuccessful = 0,
	ExtractionAllocationErr = 1,
	ExtractionFileCreationErr = 2,
};

namespace ExtractorUtils
{
	uint64_t getC8AnimSize(GameProcess* process, gameAddr anim);
};

// Base class for extracting from a game
class DLLCONTENT Extractor
{
protected:
	// Stores the extraction directory
	const char* cm_extractionDir = MOVESET_DIRECTORY;
	// Stores the process to read on
	GameProcess* m_process;
	// Stores a helper class to read the game's memory from strings in game_addresses.txt
	GameData* m_game;
	// Stores a handle to the file to write on
	std::ofstream m_file;


	// Calculates a block size from start to end, writes it to &size_out and return a pointer pointing to a new allocated space containing the data in the block
	char* allocateAndReadBlock(gameAddr blockStart, gameAddr blockEnd, uint64_t& size_out);

	// Creates the file to later write on. Return true if it succeeded.
	bool CreateMovesetFile(const char* characterName, const char* gameIdentifierstring, bool overwriteSameFilename);
	// Close the moveset file. Must be called once the extraction is over.
	void CloseMovesetFile();
	// Returns a string containing what the filename of a character to extract will be
	std::string GetFilename(const char* characterName, const char* gameIdentifierString, unsigned int suffixId = 0);
public:
	Extractor(GameProcess* process, GameData* game) : m_process(process), m_game(game) {}
	// Pure virtual base method meant to do the heavy lifting
	virtual ExtractionErrcode Extract(gameAddr playerAddress, uint8_t gameId, bool overwriteSameFilename, float& progress) = 0;
	// Returns true if extraction is possible (characters have been loaded)...
	virtual bool CanExtract() = 0;
	// Returns a string containing the character name of the provided playerId.
	virtual std::string GetPlayerCharacterName(gameAddr playerAddress) = 0;
	// Returns the ID of the character, actually important toward making movesets work properly
	virtual uint32_t GetCharacterID(gameAddr playerAddress) = 0;

	// Returns the game identifier string, used for moveset file pefix
	virtual const char* GetGameIdentifierString() = 0;
	// Returns the game name, used in moveset headers
	virtual const char* GetGameOriginString() = 0;
};
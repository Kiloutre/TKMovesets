#pragma once

#include <fstream>

#include "GameData.hpp"
#include "GameProcess.hpp"

#include "constants.h"
#include "GameAddresses.h"

#pragma warning( disable: 4251 )

// Converts a ptr to an index, -1 if the address is null
# define TO_INDEX(field, listStartAddr, type) (field = (field == 0 ? -1 : (field - listStartAddr) / sizeof(type)))

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
private:
	// Generates the filename to write, with a suffix (or not if [suffixId] is 0)
	std::string GenerateFilename(const char* characterName, const char* gameIdentifierstring, unsigned int suffixId);
protected:
	// Stores the extraction directory
	const char* cm_extractionDir = MOVESET_DIRECTORY;
	// Stores the process to read on
	GameProcess* m_process;
	// Stores a helper class to read the game's memory from strings in game_addresses.txt
	GameData* m_game;

	// Calculates a block size from start to end, writes it to &size_out and return a pointer pointing to a new allocated space containing the data in the block
	char* allocateAndReadBlock(gameAddr blockStart, gameAddr blockEnd, uint64_t& size_out);
	// Generates the full filepath to write to
	std::string GetFilepath(const char* characterName, bool overwriteSameFilename);

	// Returns the game identifier string, used for moveset file pefix
	virtual const char* GetGameIdentifierString() = 0;
	// Returns the game name, used in moveset headers
	virtual const char* GetGameOriginString() = 0;
public:
	Extractor(GameProcess* process, GameData* game) : m_process(process), m_game(game) {}
	// Pure virtual base method meant to do the heavy lifting
	virtual ExtractionErrcode Extract(gameAddr playerAddress, uint8_t gameId, bool overwriteSameFilename, uint8_t& progress) = 0;
	// Returns true if extraction is possible (characters have been loaded)...
	virtual bool CanExtract() = 0;
	// Returns a string containing the character name of the provided playerId.
	virtual std::string GetPlayerCharacterName(gameAddr playerAddress) = 0;
	// Returns the ID of the character, actually important toward making movesets work properly
	virtual uint32_t GetCharacterID(gameAddr playerAddress) = 0;
};
#pragma once

#include <fstream>

#include "GameData.hpp"
#include "GameProcess.hpp"
#include "BaseGameSpecificClass.hpp"
#include "ExtractorSettings.hpp"

#include "constants.h"
#include "GameTypes.h"
#include "MovesetStructs.h"

// Converts a ptr to an index, -1 if the address is null
# define TO_INDEX(field, listStartAddr, type) (field = (field == 0 ? -1 : (field - listStartAddr) / sizeof(type)))

enum ExtractionErrcode_
{
	ExtractionErrcode_Successful,
	ExtractionErrcode_AllocationErr,
	ExtractionErrcode_FileCreationErr,
	ExtractionErrcode_CompressionFailure,
};

namespace ExtractorUtils
{
	// Write a list of blocks, each aligned on a 8 bytes basis
	void WriteFileData(std::ofstream& file, const std::vector<std::pair<Byte*, uint64_t>>& blocks, uint8_t& progress, uint8_t progress_max);
	// Returns the compression algorithm depending on extraction settings
	TKMovesetCompressionType_ GetCompressionAlgorithm(ExtractSettings settings);
};

// Base class for extracting from a game
class Extractor : public BaseGameSpecificClass
{
private:
	// Generates the filename to write, with a suffix (or not if [suffixId] is 0)
	std::wstring GenerateFilename(const char* characterName, const char* gameIdentifierString, const char* extension, unsigned int suffixId=0);
protected:
	// Stores the extraction directory
	const char* cm_extractionDir = MOVESET_DIRECTORY;

	// Calculates a block size from start to end, writes it to &size_out and return a pointer pointing to a new allocated space containing the data in the block
	Byte* allocateAndReadBlock(gameAddr blockStart, gameAddr blockEnd, uint64_t& size_out);
	// Generates the full filepath and the full tmp filepath to write to
	void GetFilepath(const char* characterName, std::wstring& out, std::wstring& out_tmp, bool overwriteSameFilename);

	// Returns the amount of bytes an animation contains
	uint64_t GetAnimationSize(gameAddr anim);
public:
	// Inherit copy constructor
	using BaseGameSpecificClass::BaseGameSpecificClass;

	// Pure virtual base method meant to do the heavy lifting
	virtual ExtractionErrcode_ Extract(gameAddr playerAddress, ExtractSettings settings, uint8_t& progress) = 0;
	// Returns true if extraction is possible (characters have been loaded)...
	virtual bool CanExtract() = 0;
	// Returns a string containing the character name of the provided player address
	virtual std::string GetPlayerCharacterName(gameAddr playerAddress) = 0;
	// Returns the ID of the character, actually important toward making movesets work properly
	virtual uint32_t GetCharacterID(gameAddr playerAddress) = 0;
	// Returns a character address depending on the provided playerid
	virtual gameAddr GetCharacterAddress(uint8_t playerId) = 0;
	// Returns every character addresses we can extract
	virtual std::vector<gameAddr> GetCharacterAddresses() = 0;
};
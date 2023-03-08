#pragma once

#include <fstream>

#include "GameData.hpp"
#include "GameProcess.hpp"

#include "constants.h"
#include "GameAddresses.h"

namespace ExtractionOptions
{
	typedef uint64_t Settings; // 64 flags

	enum SettingEnum
	{
		MOTA_0_ANIM = (1 << 0),
		MOTA_1_ANIM = (1 << 1),
		MOTA_2_HAND = (1 << 2),
		MOTA_3_HAND = (1 << 3),
		MOTA_4_FACE = (1 << 4),
		MOTA_5_FACE = (1 << 5),
		MOTA_6_WINGS = (1 << 6),
		MOTA_7_WINGS = (1 << 7),
		MOTA_8_CAMERA = (1 << 8),
		MOTA_9_CAMERA = (1 << 9),
		MOTA_10_UNKNOWN = (1 << 10),
		MOTA_11_UNKNOWN = (1 << 11),

		MOTA_ANIM = MOTA_0_ANIM | MOTA_1_ANIM,
		MOTA_HANDS = MOTA_2_HAND | MOTA_3_HAND,
		MOTA_FACES = MOTA_4_FACE | MOTA_5_FACE,
		MOTA_CAMERAS = MOTA_8_CAMERA | MOTA_9_CAMERA,
		MOTA_WINGS = MOTA_6_WINGS | MOTA_7_WINGS,
		MOTA_UNKNOWNS = MOTA_10_UNKNOWN | MOTA_11_UNKNOWN,

		OVERWRITE_SAME_FILENAME = (1 << 12),
	};
}


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
	// Returns the size in bytes of a 0xC8 in 
	uint64_t getC8AnimSize(GameProcess* process, gameAddr anim);
	// Compress
	void CompressFile(std::string dest_filename, std::string src_filename);
};

// Base class for extracting from a game
class DLLCONTENT Extractor
{
private:
	// Generates the filename to write, with a suffix (or not if [suffixId] is 0)
	std::string GenerateFilename(const char* characterName, const char* gameIdentifierString, const char* extension, unsigned int suffixId=0);
protected:
	// Stores the extraction directory
	const char* cm_extractionDir = MOVESET_DIRECTORY;
	// Stores the process to read on
	GameProcess* m_process;
	// Stores a helper class to read the game's memory from strings in game_addresses.txt
	GameData* m_game;

	// Calculates a block size from start to end, writes it to &size_out and return a pointer pointing to a new allocated space containing the data in the block
	byte* allocateAndReadBlock(gameAddr blockStart, gameAddr blockEnd, uint64_t& size_out);
	// Generates the full filepath and the full tmp filepath to write to
	void GetFilepath(const char* characterName, std::string& out, std::string& out_tmp, bool overwriteSameFilename);

	// Returns the game identifier string, used for moveset file pefix
	virtual const char* GetGameIdentifierString() = 0;
	// Returns the game name, used in moveset headers
	virtual const char* GetGameOriginString() = 0;
public:
	Extractor(GameProcess* process, GameData* game) : m_process(process), m_game(game) {}
	// Pure virtual base method meant to do the heavy lifting
	virtual ExtractionErrcode Extract(gameAddr playerAddress, ExtractionOptions::Settings settings, uint8_t gameId, uint8_t& progress) = 0;
	// Returns true if extraction is possible (characters have been loaded)...
	virtual bool CanExtract() = 0;
	// Returns a string containing the character name of the provided playerId.
	virtual std::string GetPlayerCharacterName(gameAddr playerAddress) = 0;
	// Returns the ID of the character, actually important toward making movesets work properly
	virtual uint32_t GetCharacterID(gameAddr playerAddress) = 0;
};
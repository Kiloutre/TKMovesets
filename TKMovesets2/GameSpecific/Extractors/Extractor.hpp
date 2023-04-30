#pragma once

#include <fstream>

#include "GameData.hpp"
#include "GameProcess.hpp"
#include "BaseGameSpecificClass.hpp"

#include "constants.h"
#include "GameTypes.h"

// Converts a ptr to an index, -1 if the address is null
# define TO_INDEX(field, listStartAddr, type) (field = (field == 0 ? -1 : (field - listStartAddr) / sizeof(type)))

typedef uint64_t ExtractSettings; // 64 flags
enum ExtractSettings_
{
	// These 12 must occupy the first 12 bits
	ExtractSettings_MOTA_0_ANIM = (1 << 0),
	ExtractSettings_MOTA_1_ANIM = (1 << 1),
	ExtractSettings_MOTA_2_HAND = (1 << 2),
	ExtractSettings_MOTA_3_HAND = (1 << 3),
	ExtractSettings_MOTA_4_FACE = (1 << 4),
	ExtractSettings_MOTA_5_FACE = (1 << 5),
	ExtractSettings_MOTA_6_WINGS = (1 << 6),
	ExtractSettings_MOTA_7_WINGS = (1 << 7),
	ExtractSettings_MOTA_8_CAMERA = (1 << 8),
	ExtractSettings_MOTA_9_CAMERA = (1 << 9),
	ExtractSettings_MOTA_10_UNKNOWN = (1 << 10),
	ExtractSettings_MOTA_11_UNKNOWN = (1 << 11),

	// Shortcuts for matching pairs
	ExtractSettings_MOTA_ANIM = ExtractSettings_MOTA_0_ANIM | ExtractSettings_MOTA_1_ANIM,
	ExtractSettings_MOTA_HANDS = ExtractSettings_MOTA_2_HAND | ExtractSettings_MOTA_3_HAND,
	ExtractSettings_MOTA_FACES = ExtractSettings_MOTA_4_FACE | ExtractSettings_MOTA_5_FACE,
	ExtractSettings_MOTA_CAMERAS = ExtractSettings_MOTA_8_CAMERA | ExtractSettings_MOTA_9_CAMERA,
	ExtractSettings_MOTA_WINGS = ExtractSettings_MOTA_6_WINGS | ExtractSettings_MOTA_7_WINGS,
	ExtractSettings_MOTA_UNKNOWNS = ExtractSettings_MOTA_10_UNKNOWN | ExtractSettings_MOTA_11_UNKNOWN,


	ExtractSettings_OVERWRITE_SAME_FILENAME = (1 << 12),
	ExtractSettings_DisplayableMovelist = (1 << 13),
	ExtractSettings_Compress = (1 << 14),
};

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
	// Returns the size in bytes of a 0xC8 animation 
	uint64_t getC8AnimSize(GameProcess* process, gameAddr anim);
	// Wrapper that calculates the size in bytes of a 0x64 animation (big or little endian)
	uint64_t get64AnimSize(GameProcess* process, gameAddr anim);
	// Returns the size in bytes of a 0x64 animation (little endian anim)
	uint64_t get64AnimSize_LittleEndian(GameProcess* process, gameAddr anim);
	// Returns the size in bytes of a 0x64 animation (big endian anim)
	uint64_t get64AnimSize_BigEndian(GameProcess* process, gameAddr anim);
	// Compress a moveset. Returns false on failure. Modifies header->moveset_data_size automatically.
	bool CompressFile(int32_t moveset_data_start, const std::wstring& dest_filename, const std::wstring& src_filename);
	// Byteswaps a MOTA block and its animations (little endian <-> big endian). Animations will be made to match the MOTA's endian.
	void ByteswapMota(Byte* motaAddr); 
	// Byteswaps an animation (little endian <-> big endian)
	void ByteswapAnimation(Byte* animAddr);
};

// Base class for extracting from a game
class DLLCONTENT Extractor : public BaseGameSpecificClass
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
	// Stores the number of character we are expected to extract if asked to extract every char
	// You shouldn't set this here but in the game list file (Games.cpp). The 1 here should get overwritten or something has gone wrong.
	uint8_t characterCount = 1;

	// Inherit copy constructor
	using BaseGameSpecificClass::BaseGameSpecificClass;

	// Pure virtual base method meant to do the heavy lifting
	virtual ExtractionErrcode_ Extract(gameAddr playerAddress, ExtractSettings settings, uint8_t& progress) = 0;
	// Returns true if extraction is possible (characters have been loaded)...
	virtual bool CanExtract() = 0;
	// Returns a string containing the character name of the provided playerId.
	virtual std::string GetPlayerCharacterName(gameAddr playerAddress) = 0;
	// Returns the ID of the character, actually important toward making movesets work properly
	virtual uint32_t GetCharacterID(gameAddr playerAddress) = 0;
	// Returns a character address depending on the provided playerid
	virtual gameAddr GetCharacterAddress(uint8_t playerId) = 0;
	// Returns every character addresses we can extract
	virtual std::vector<gameAddr> GetCharacterAddresses() = 0;
};
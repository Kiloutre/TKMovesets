#include <fstream>

#include "MovesetConverters.hpp"
#include "MovesetFileConverter.hpp"
#include "Helpers.hpp"
#include "Compression.hpp"

#include "GameTypes.h"

#include "Structs_t7.h"

// -- Static helpers -- //

static void CopyHeader(TKMovesetHeader* dest, const TKMovesetHeader* src)
{
	memcpy((void*)dest->_signature, (void*)src->_signature, 4);
	strcpy_s(dest->version_string, sizeof(dest->version_string), src->version_string);
	memcpy(dest->gameVersionHash, src->gameVersionHash, sizeof(src->gameVersionHash));
	dest->gameId = src->gameId;
	dest->minorVersion = src->minorVersion;
	dest->header_size = src->header_size;
	dest->block_list = src->block_list;
	dest->block_list_size = src->block_list_size;
	dest->moveset_data_start = src->moveset_data_start;
	dest->moveset_data_size = src->moveset_data_size;
	dest->compressionType = TKMovesetCompressionType_None;
	dest->crc32 = src->crc32;
	dest->orig_crc32 = src->orig_crc32;
	dest->date = src->date;
	dest->extraction_date = src->extraction_date;
	dest->characterId = src->characterId;
	dest->flags = src->flags;
	strcpy_s(dest->origin, sizeof(dest->origin), src->origin);
	strcpy_s(dest->target_character, sizeof(dest->origin), src->target_character);
	strcpy_s(dest->orig_character_name, sizeof(dest->origin), src->GetOrigCharacterName());
	dest->game_specific_flags = src->GetGameSpecificFlags();
}

// Reads the the moveset size, allocate the moveset in our own memory and write to it
static Byte* getMovesetInfos(std::ifstream& file, uint64_t& size_out)
{
	file.seekg(0, std::ios::end);
	size_out = file.tellg();
	Byte* moveset = (Byte*)malloc(size_out);
	if (moveset != nullptr) {
		file.seekg(0, std::ios::beg);
		file.read((char*)moveset, size_out);
	}
	file.close();
	return moveset;
}

// -- -- //

static std::wstring GenerateNewName(const std::wstring& currentName, const wchar_t* oldPrefix, const wchar_t* newPrefix, const wchar_t* newSuffix)
{
	std::wstring filepath = currentName;
	std::wstring name = currentName;

	auto lastSlash = name.find_last_of(L"/\\");
	if (lastSlash != std::wstring::npos) {
		filepath.erase(lastSlash + 1);
		name.erase(0, lastSlash + 1);
		name.erase(name.find_last_of(L"."));
	}

	if (Helpers::startsWith<std::wstring>(name.c_str(), oldPrefix)) {
		name.erase(name.begin(), name.begin() + wcslen(oldPrefix));
	}

	std::wstring newName = filepath + newPrefix + name + newSuffix + L"" MOVESET_FILENAME_EXTENSION;
	unsigned int counter = 2;
	while (Helpers::fileExists(newName.c_str())) {
		newName = filepath + newPrefix + name + newSuffix + L" (" + std::to_wstring(counter) + L")" MOVESET_FILENAME_EXTENSION;
		counter++;
	}

	return newName;
}

// -- Converters -- //


static bool ConvertTREVToT7(const movesetInfo& mInfo, const TKMovesetHeader* orig_header, Byte* orig_moveset, uint64_t s_moveset)
{
	StructsT7::TKMovesetHeaderBlocks blocks;

	std::vector<TKMovesetProperty> propertyList;
	propertyList.push_back({ .id = TKMovesetProperty_END, .value = 0 });

	{
		if (!MovesetConverter::TREVToT7().Convert(orig_header, orig_moveset, s_moveset, blocks)) {
			return false;
		}
	}

	// Calculate new moveset size 
	uint64_t new_size = 0;

	new_size += Helpers::align8Bytes(sizeof(TKMovesetHeader));
	new_size += Helpers::align8Bytes(propertyList.size() * sizeof(TKMovesetProperty));
	new_size += Helpers::align8Bytes(sizeof(blocks));
	new_size += s_moveset;

	Byte* new_moveset;
	TKMovesetHeader* new_header;
	try {
		new_moveset = new Byte[new_size];
		new_header = (TKMovesetHeader*)new_moveset;
	}
	catch (std::bad_alloc& _) {
		(void)_;
		return false;
	}

	// Copy header
	CopyHeader(new_header, orig_header);
	new_header->gameId = GameId_T7;
	new_header->minorVersion = GameVersions::T7::NONE;
	new_header->conversion_origin = GameVersions::T7::FROM_TREV;

	// Copy property list
	TKMovesetProperty* dest_propertyList = (TKMovesetProperty*)(new_moveset + Helpers::align8Bytes(sizeof(TKMovesetHeader)));
	memcpy(dest_propertyList, propertyList.data(), propertyList.size() * sizeof(TKMovesetProperty));

	// Copy block list
	StructsT7::TKMovesetHeaderBlocks* dest_blocks = (StructsT7::TKMovesetHeaderBlocks*)((Byte*)dest_propertyList + Helpers::align8Bytes(propertyList.size() * sizeof(TKMovesetProperty)));
	memcpy(dest_blocks, &blocks, sizeof(blocks));

	// Get moveset data ptr
	Byte* movesetData = (Byte*)dest_blocks + Helpers::align8Bytes(sizeof(blocks));

	// Correct block list ptr & moveset data start ptr
	new_header->moveset_data_start = (uint32_t)((Byte*)movesetData - new_moveset);
	new_header->moveset_data_size = 0;
	new_header->block_list = (uint32_t)((Byte*)dest_blocks - new_moveset);
	new_header->block_list_size = _countof(blocks.blocks);

	// Copy moveset data
	memcpy(movesetData, orig_moveset, s_moveset);

	// Re-calculate CRC32
	std::vector<std::pair<Byte*, uint64_t>> hashedFileBlocks{
		{(Byte*)propertyList.data(), propertyList.size() * sizeof(TKMovesetProperty)},
		{orig_moveset + blocks.movesetInfoBlock, blocks.tableBlock - blocks.movesetInfoBlock },
		{orig_moveset + blocks.tableBlock, blocks.motalistsBlock - blocks.tableBlock },
		{orig_moveset + blocks.motalistsBlock, blocks.nameBlock - blocks.motalistsBlock },
		{orig_moveset + blocks.movesetBlock, blocks.animationBlock - blocks.movesetBlock },
		{orig_moveset + blocks.animationBlock, blocks.motaBlock - blocks.animationBlock },
		{orig_moveset + blocks.motaBlock, blocks.movelistBlock - blocks.motaBlock },
	};
	new_header->crc32 = Helpers::CalculateCrc32(hashedFileBlocks);

	// Generate new name & write to file
	std::wstring new_filename = GenerateNewName(mInfo.filename, L"TREV", L"T7", L" (TREV)");
	{
		std::ofstream new_moveset_file(new_filename, std::ios::binary);
		new_moveset_file.write((char*)new_moveset, new_size);
	}

	// Compress if needed
	if (orig_header->compressionType != TKMovesetCompressionType_None) {
		CompressionUtils::FILE::Moveset::Compress(new_filename, (TKMovesetCompressionType_)orig_header->compressionType);
	}

	return true;
}


static bool ConvertTTT2ToT7(const movesetInfo& mInfo, const TKMovesetHeader* orig_header, Byte* orig_moveset, uint64_t s_moveset)
{
	StructsT7::TKMovesetHeaderBlocks blocks;

	std::vector<TKMovesetProperty> propertyList;
	propertyList.push_back({ .id = TKMovesetProperty_END, .value = 0 });

	{
		if (!MovesetConverter::TTT2ToT7().Convert(orig_header, orig_moveset, s_moveset, blocks)) {
			return false;
		}
	}

	// Calculate new moveset size 
	uint64_t new_size = 0;

	new_size += Helpers::align8Bytes(sizeof(TKMovesetHeader));
	new_size += Helpers::align8Bytes(propertyList.size() * sizeof(TKMovesetProperty));
	new_size += Helpers::align8Bytes(sizeof(blocks));
	new_size += s_moveset;

	Byte* new_moveset;
	TKMovesetHeader* new_header;
	try {
		new_moveset = new Byte[new_size];
		new_header = (TKMovesetHeader*)new_moveset;
	}
	catch (std::bad_alloc& _) {
		(void)_;
		return false;
	}

	// Copy header
	CopyHeader(new_header, orig_header);
	new_header->gameId = GameId_T7;
	new_header->minorVersion = GameVersions::T7::NONE;
	new_header->conversion_origin = GameVersions::T7::FROM_TTT2;

	// Copy property list
	TKMovesetProperty* dest_propertyList = (TKMovesetProperty*)(new_moveset + Helpers::align8Bytes(sizeof(TKMovesetHeader)));
	memcpy(dest_propertyList, propertyList.data(), propertyList.size() * sizeof(TKMovesetProperty));

	// Copy block list
	StructsT7::TKMovesetHeaderBlocks* dest_blocks = (StructsT7::TKMovesetHeaderBlocks * )((Byte*)dest_propertyList + Helpers::align8Bytes(propertyList.size() * sizeof(TKMovesetProperty)));
	memcpy(dest_blocks, &blocks, sizeof(blocks));

	// Get moveset data ptr
	Byte* movesetData = (Byte*)dest_blocks + Helpers::align8Bytes(sizeof(blocks));

	// Correct block list ptr & moveset data start ptr
	new_header->moveset_data_start = (uint32_t)((Byte*)movesetData - new_moveset);
	new_header->moveset_data_size = 0;
	new_header->block_list = (uint32_t)((Byte*)dest_blocks - new_moveset);
	new_header->block_list_size = _countof(blocks.blocks);

	// Copy moveset data
	memcpy(movesetData, orig_moveset, s_moveset);

	// Re-calculate CRC32
	std::vector<std::pair<Byte*, uint64_t>> hashedFileBlocks{
		{(Byte*)propertyList.data(), propertyList.size() * sizeof(TKMovesetProperty)},
		{orig_moveset + blocks.movesetInfoBlock, blocks.tableBlock - blocks.movesetInfoBlock },
		{orig_moveset + blocks.tableBlock, blocks.motalistsBlock - blocks.tableBlock },
		{orig_moveset + blocks.motalistsBlock, blocks.nameBlock - blocks.motalistsBlock },
		{orig_moveset + blocks.movesetBlock, blocks.animationBlock - blocks.movesetBlock },
		{orig_moveset + blocks.animationBlock, blocks.motaBlock - blocks.animationBlock },
		{orig_moveset + blocks.motaBlock, blocks.movelistBlock - blocks.motaBlock },
	};
	new_header->crc32 = Helpers::CalculateCrc32(hashedFileBlocks);

	// Generate new name & write to file
	std::wstring new_filename = GenerateNewName(mInfo.filename, L"TTT2", L"T7", L" (TTT2)");
	{
		std::ofstream new_moveset_file(new_filename, std::ios::binary);
		new_moveset_file.write((char*)new_moveset, new_size);
	}

	// Compress if needed
	if (orig_header->compressionType != TKMovesetCompressionType_None) {
		CompressionUtils::FILE::Moveset::Compress(new_filename, (TKMovesetCompressionType_)orig_header->compressionType);
	}

	return true;
}


static bool ConvertT6ToT7(const movesetInfo& mInfo, const TKMovesetHeader* orig_header, Byte* orig_moveset, uint64_t s_moveset)
{
	StructsT7::TKMovesetHeaderBlocks blocks;

	std::vector<TKMovesetProperty> propertyList;
	propertyList.push_back({ .id = TKMovesetProperty_END, .value = 0 });

	{
		if (!MovesetConverter::T6ToT7().Convert(orig_header, orig_moveset, s_moveset, blocks)) {
			return false;
		}
	}

	// Calculate new moveset size 
	uint64_t new_size = 0;

	new_size += Helpers::align8Bytes(sizeof(TKMovesetHeader));
	new_size += Helpers::align8Bytes(propertyList.size() * sizeof(TKMovesetProperty));
	new_size += Helpers::align8Bytes(sizeof(blocks));
	new_size += s_moveset;

	Byte* new_moveset;
	TKMovesetHeader* new_header;
	try {
		new_moveset = new Byte[new_size];
		new_header = (TKMovesetHeader*)new_moveset;
	}
	catch (std::bad_alloc& _) {
		(void)_;
		return false;
	}

	// Copy header
	CopyHeader(new_header, orig_header);
	new_header->gameId = GameId_T7;
	new_header->minorVersion = GameVersions::T7::NONE;
	new_header->conversion_origin = GameVersions::T7::FROM_T6;

	// Copy property list
	TKMovesetProperty* dest_propertyList = (TKMovesetProperty*)(new_moveset + Helpers::align8Bytes(sizeof(TKMovesetHeader)));
	memcpy(dest_propertyList, propertyList.data(), propertyList.size() * sizeof(TKMovesetProperty));

	// Copy block list
	StructsT7::TKMovesetHeaderBlocks* dest_blocks = (StructsT7::TKMovesetHeaderBlocks * )((Byte*)dest_propertyList + Helpers::align8Bytes(propertyList.size() * sizeof(TKMovesetProperty)));
	memcpy(dest_blocks, &blocks, sizeof(blocks));

	// Get moveset data ptr
	Byte* movesetData = (Byte*)dest_blocks + Helpers::align8Bytes(sizeof(blocks));

	// Correct block list ptr & moveset data start ptr
	new_header->moveset_data_start = (uint32_t)((Byte*)movesetData - new_moveset);
	new_header->moveset_data_size = 0;
	new_header->block_list = (uint32_t)((Byte*)dest_blocks - new_moveset);
	new_header->block_list_size = _countof(blocks.blocks);

	// Copy moveset data
	memcpy(movesetData, orig_moveset, s_moveset);

	// Re-calculate CRC32
	std::vector<std::pair<Byte*, uint64_t>> hashedFileBlocks{
		{(Byte*)propertyList.data(), propertyList.size() * sizeof(TKMovesetProperty)},
		{orig_moveset + blocks.movesetInfoBlock, blocks.tableBlock - blocks.movesetInfoBlock },
		{orig_moveset + blocks.tableBlock, blocks.motalistsBlock - blocks.tableBlock },
		{orig_moveset + blocks.motalistsBlock, blocks.nameBlock - blocks.motalistsBlock },
		{orig_moveset + blocks.movesetBlock, blocks.animationBlock - blocks.movesetBlock },
		{orig_moveset + blocks.animationBlock, blocks.motaBlock - blocks.animationBlock },
		{orig_moveset + blocks.motaBlock, blocks.movelistBlock - blocks.motaBlock },
	};
	new_header->crc32 = Helpers::CalculateCrc32(hashedFileBlocks);

	// Generate new name & write to file
	std::wstring new_filename = GenerateNewName(mInfo.filename, L"T6", L"T7", L" (T6)");
	{
		std::ofstream new_moveset_file(new_filename, std::ios::binary);
		new_moveset_file.write((char*)new_moveset, new_size);
	}

	// Compress if needed
	if (orig_header->compressionType != TKMovesetCompressionType_None) {
		CompressionUtils::FILE::Moveset::Compress(new_filename, (TKMovesetCompressionType_)orig_header->compressionType);
	}

	return true;
}


static bool ConvertT5ToT7(const movesetInfo& mInfo, const TKMovesetHeader* orig_header, Byte* orig_moveset, uint64_t s_moveset)
{
	StructsT7::TKMovesetHeaderBlocks blocks;

	std::vector<TKMovesetProperty> propertyList;
	propertyList.push_back({ .id = TKMovesetProperty_END, .value = 0 });

	{
		if (!MovesetConverter::T5ToT7().Convert(orig_header, orig_moveset, s_moveset, blocks)) {
			return false;
		}
	}

	// Calculate new moveset size 
	uint64_t new_size = 0;

	new_size += Helpers::align8Bytes(sizeof(TKMovesetHeader));
	new_size += Helpers::align8Bytes(propertyList.size() * sizeof(TKMovesetProperty));
	new_size += Helpers::align8Bytes(sizeof(blocks));
	new_size += s_moveset;

	Byte* new_moveset;
	TKMovesetHeader* new_header;
	try {
		new_moveset = new Byte[new_size];
		new_header = (TKMovesetHeader*)new_moveset;
	}
	catch (std::bad_alloc& _) {
		(void)_;
		return false;
	}

	// Copy header
	CopyHeader(new_header, orig_header);
	new_header->gameId = GameId_T7;
	new_header->minorVersion = GameVersions::T7::NONE;
	new_header->conversion_origin = GameVersions::T7::FROM_T5;

	// Copy property list
	TKMovesetProperty* dest_propertyList = (TKMovesetProperty*)(new_moveset + Helpers::align8Bytes(sizeof(TKMovesetHeader)));
	memcpy(dest_propertyList, propertyList.data(), propertyList.size() * sizeof(TKMovesetProperty));

	// Copy block list
	StructsT7::TKMovesetHeaderBlocks* dest_blocks = (StructsT7::TKMovesetHeaderBlocks*)((Byte*)dest_propertyList + Helpers::align8Bytes(propertyList.size() * sizeof(TKMovesetProperty)));
	memcpy(dest_blocks, &blocks, sizeof(blocks));

	// Get moveset data ptr
	Byte* movesetData = (Byte*)dest_blocks + Helpers::align8Bytes(sizeof(blocks));

	// Correct block list ptr & moveset data start ptr
	new_header->moveset_data_start = (uint32_t)((Byte*)movesetData - new_moveset);
	new_header->moveset_data_size = 0;
	new_header->block_list = (uint32_t)((Byte*)dest_blocks - new_moveset);
	new_header->block_list_size = _countof(blocks.blocks);

	// Copy moveset data
	memcpy(movesetData, orig_moveset, s_moveset);

	// Re-calculate CRC32
	std::vector<std::pair<Byte*, uint64_t>> hashedFileBlocks{
		{(Byte*)propertyList.data(), propertyList.size() * sizeof(TKMovesetProperty)},
		{orig_moveset + blocks.movesetInfoBlock, blocks.tableBlock - blocks.movesetInfoBlock },
		{orig_moveset + blocks.tableBlock, blocks.motalistsBlock - blocks.tableBlock },
		{orig_moveset + blocks.motalistsBlock, blocks.nameBlock - blocks.motalistsBlock },
		{orig_moveset + blocks.movesetBlock, blocks.animationBlock - blocks.movesetBlock },
		{orig_moveset + blocks.animationBlock, blocks.motaBlock - blocks.animationBlock },
		{orig_moveset + blocks.motaBlock, blocks.movelistBlock - blocks.motaBlock },
	};
	new_header->crc32 = Helpers::CalculateCrc32(hashedFileBlocks);

	// Generate new name & write to file
	std::wstring new_filename = GenerateNewName(mInfo.filename, L"T5", L"T7", L" (T5)");
	{
		std::ofstream new_moveset_file(new_filename, std::ios::binary);
		new_moveset_file.write((char*)new_moveset, new_size);
	}

	// Compress if needed
	if (orig_header->compressionType != TKMovesetCompressionType_None) {
		CompressionUtils::FILE::Moveset::Compress(new_filename, (TKMovesetCompressionType_)orig_header->compressionType);
	}

	return true;
}


// -- -- //

void ConvertMoveset(const movesetInfo& mInfo, GameId_ targetGameId)
{
	DEBUG_LOG("ConvertMoveset '%S' - Game ID %u -> %u\n", mInfo.filename.c_str(), mInfo.gameId, targetGameId);

	// Read moveset data
	std::ifstream file(mInfo.filename, std::ios::binary);
	if (file.fail()) {
		DEBUG_ERR("Failed to open file for moveset conversion.");
		return;
	}

	uint64_t s_moveset;
	Byte* orig_moveset = getMovesetInfos(file, s_moveset);
	Byte* moveset;
	TKMovesetHeader* header = (TKMovesetHeader*)orig_moveset;

	if (orig_moveset == nullptr) {
		DEBUG_ERR("Failed to parse moveset data");
		return;
	}

	// Make sure it is a valid moveset
	if (!header->ValidateHeader()) {
		delete[] orig_moveset;
		DEBUG_ERR("Failed to validate moveset header");
		return;
	}
	
	if (header->isCompressed())
	{
		// Decompress moveset and free old copy if needed
		moveset = CompressionUtils::RAW::Moveset::Decompress(orig_moveset, s_moveset - header->moveset_data_start, s_moveset);
		if (moveset == nullptr) {
			return;
		}
	}
	else {
		moveset = orig_moveset + header->moveset_data_start;
		s_moveset -= header->moveset_data_start;
	}

	// Convert the moveset
	switch (targetGameId)
	{
		case GameId_T7:
		{
			switch (mInfo.gameId)
			{
				case GameId_TTT2:
				{
					ConvertTTT2ToT7(mInfo, header, moveset, s_moveset);
					break;
				}
				case GameId_TREV:
				{
					ConvertTREVToT7(mInfo, header, moveset, s_moveset);
					break;
				}
				case GameId_T6:
				{
					ConvertT6ToT7(mInfo, header, moveset, s_moveset);
					break;
				}
				case GameId_T5:
				{
					ConvertT5ToT7(mInfo, header, moveset, s_moveset);
					break;
				}
				default:
					DEBUG_ERR("Conversion from game id '%u' to game id '%u' not found", mInfo.gameId, targetGameId);
					break;
			}

		}
		break;

		default:
			DEBUG_ERR("Unsupported target game id conversion: %u\n", targetGameId);
			break;
	}

	// Cleanup
	if (header->isCompressed()) {
		delete[] moveset;
	}
	delete[] orig_moveset;
}
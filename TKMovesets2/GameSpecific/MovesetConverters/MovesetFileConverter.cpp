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
	strcpy(dest->version_string, src->version_string);
	memcpy(dest->gameVersionHash, src->gameVersionHash, sizeof(src->gameVersionHash));
	dest->gameId = src->gameId;
	dest->minorVersion = src->minorVersion;
	dest->header_size = src->header_size;
	dest->block_list = src->block_list;
	dest->block_list_size = src->block_list_size;
	dest->moveset_data_start = src->moveset_data_start;
	dest->moveset_data_size = src->moveset_data_size;
	dest->compressionType = src->compressionType;
	dest->crc32 = src->crc32;
	dest->orig_crc32 = src->orig_crc32;
	dest->date = src->date;
	dest->extraction_date = src->extraction_date;
	dest->characterId = src->characterId;
	dest->flags = src->flags;
	strcpy(dest->origin, src->origin);
	strcpy(dest->target_character, src->target_character);
	strcpy(dest->orig_character_name, src->GetOrigCharacterName());
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

static bool ConvertTTT2ToT7(const TKMovesetHeader* orig_header, Byte* orig_moveset, uint64_t s_moveset)
{
	StructsT7::TKMovesetHeaderBlocks blocks;

	std::vector<TKMovesetProperty> propertyList;
	propertyList.push_back({ .id = TKMovesetProperty_END, .value = 0 });

	if (!MovesetConverter::TTT2ToT7(orig_header, orig_moveset, s_moveset, blocks)) {
		return false;
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

	// todo: compress
	// Write to file
	std::wstring filename = L"test.out";
	std::ofstream new_moveset_file(filename, std::ios::binary);
	new_moveset_file.write((char*)new_moveset, new_size);
	return true;
}

// -- -- //

void ConvertMoveset(const movesetInfo& mInfo, GameId_ targetGameId)
{
	DEBUG_LOG("ConvertMoveset '%S' - %u -> %u\n", mInfo.filename.c_str(), mInfo.gameId, targetGameId);

	// Read moveset data
	std::ifstream file(mInfo.filename, std::ios::binary);
	if (file.fail()) {
		return;
	}

	uint64_t s_moveset;
	Byte* orig_moveset = getMovesetInfos(file, s_moveset);
	Byte* moveset;
	TKMovesetHeader* header = (TKMovesetHeader*)orig_moveset;

	if (orig_moveset == nullptr) {
		return;
	}

	// Make sure it is a valid moveset
	if (!header->ValidateHeader()) {
		delete[] orig_moveset;
		return;
	}
	
	if (header->isCompressed())
	{
		// Decompress moveset and free old copy if needed
		moveset = CompressionUtils::RAW::Moveset::Decompress(orig_moveset, header->moveset_data_size, s_moveset);
		delete[] orig_moveset;
		if (moveset == nullptr) {
			return;
		}
		orig_moveset = moveset;
	}
	else {
		moveset = orig_moveset;
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
					ConvertTTT2ToT7(header, moveset, s_moveset);
					break;
				}
			}

		}
		break;

		default:
			DEBUG_ERR("Unsupported target game id conversion: %u\n", targetGameId);
			break;
	}

	// Cleanup
	delete[] orig_moveset;
}
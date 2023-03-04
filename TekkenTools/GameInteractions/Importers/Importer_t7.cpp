#include <string>

#include "helpers.hpp"
#include "Importer_t7.hpp"

#include "Structs_t7.h"

// Contains the same structure as StructsT7 but with gameAddr types instead of ptrs types
// Defined here because i don't want any other file to have access to this shortcut
#define gAddr StructsT7Helpers

// -- Static helpers -- //

static char* getMovesetInfos(std::ifstream& file, MovesetHeader* header, uint64_t& movesetSize)
{
	file.read((char*)header, sizeof(MovesetHeader));
	file.seekg(0, std::ios::end);
	movesetSize = file.tellg();
	char* movesetData = (char*)malloc(movesetSize);
	if (movesetData) {
		file.seekg(header->offsets.movesetInfoBlock + header->infos.header_size, std::ios::beg);
		file.read(movesetData, movesetSize);
	}
	file.close();
	return movesetData;
}

void ImporterT7::CorrectMotaList(MovesetHeader& header, char* movesetData, gameAddr gameMoveset, gameAddr playerAddress)
{
	MotaList currentMotasList{};
	gameAddr currentMovesetAddr = m_process->readInt64(playerAddress + m_game->addrFile->GetSingleValue("val_motbin_offset"));
	m_process->readBytes(currentMovesetAddr + 0x280, &currentMotasList, sizeof(MotaList));

	MotaList* motaList = (MotaList*)(movesetData + header.offsets.motalistsBlock);

	uint64_t* gameMotaCursor = (uint64_t*)&currentMotasList;
	uint64_t* fileMotaCursor = (uint64_t*)motaList;

	// This is just a list of uint64_t anyway so might as well do this
	for (size_t i = 0; i <= 12; ++i)
	{
		if (fileMotaCursor[i] == 0 || true) {
			// Moveset block was not included in the file: copy the currently used one
			fileMotaCursor[i] = gameMotaCursor[i];
		}
		else
		{
			// Todo: check if this works. I believe we are off.
			fileMotaCursor[i] += gameMoveset;
		}
	}
}

void ImporterT7::CorrectPtrList(MovesetHeader& header, char* movesetData, gameAddr gameMoveset)
{
	gAddr::MovesetLists* lists = (gAddr::MovesetLists*)(movesetData + header.offsets.listsBlock);

	gameAddr offset = gameMoveset + header.offsets.movesetBlock;

	// LOL
	*(uint64_t*)(&lists->reactions) += offset;
	*(uint64_t*)&lists->requirement += offset;
	*(uint64_t*)&lists->hitCondition += offset;
	*(uint64_t*)&lists->projectile += offset;
	*(uint64_t*)&lists->pushback += offset;
	*(uint64_t*)&lists->pushbackExtradata += offset;
	*(uint64_t*)&lists->cancel += offset;
	*(uint64_t*)&lists->groupCancel += offset;
	*(uint64_t*)&lists->cancelExtradata += offset;
	*(uint64_t*)&lists->extraMoveProperty += offset;
	*(uint64_t*)&lists->unknown_0x1f0 += offset;
	*(uint64_t*)&lists->unknown_0x200 += offset;
	*(uint64_t*)&lists->move += offset;
	*(uint64_t*)&lists->voiceclip += offset;
	*(uint64_t*)&lists->inputSequence += offset;
	*(uint64_t*)&lists->input += offset;
	*(uint64_t*)&lists->unknownParryRelated += offset;
	*(uint64_t*)&lists->cameraData += offset;
	*(uint64_t*)&lists->throws += offset;
}

// -- Public methods -- //

ImportationErrcode ImporterT7::Import(const char* filename, gameAddr playerAddress, bool applyInstantly, float* progress)
{
	// Read file data
	std::ifstream file;
	file.open(filename, std::ios::binary);

	if (file.fail()) {
		return ImportationFileReadErr;
	}

	MovesetHeader header{ 0 };

	// Allocate a copy of the moveset locally. This is NOT in the game's memory
	uint64_t movesetSize;
	char *movesetData = getMovesetInfos(file, &header, movesetSize);
	if (movesetData == nullptr) {
		return ImportationAllocationErr;
	}
	
	// Allocate our moveset in the game's memory, but we aren't gonna write on that for a while.
	// The idea is to write on our moveset in our own memory (should be faster), then write it all at once on gameMoveset with a single m_process->writeBytes()
	gameAddr gameMoveset = m_process->allocateMem(movesetSize);
	if (gameMoveset == 0) {
		free(movesetData);
		return ImportationGameAllocationErr;
	}

	// Correct/Convert everything that needs to be corrected/converted first

	// Turn our mota offsets into mota ptrs, or copy the currently loaded character's mota for each we didn't provide
	CorrectMotaList(header, movesetData, gameMoveset, playerAddress);
	// Turn our moveset structures lists offsets into ptrs
	CorrectPtrList(header, movesetData, gameMoveset);


	// Finally lrite our moveset to the game's memory
	printf("Moveset allocated at %llx\n", gameMoveset);
	m_process->writeBytes(gameMoveset, movesetData, movesetSize);

	// Todo: write moveset to player
	// Todo: Write move 32769 to player

	free(movesetData);
	return ImportationSuccessful;
}

bool ImporterT7::CanImport()
{
	gameAddr playerAddress = m_game->ReadPtr("p1_addr");
	// We'll just read through a bunch of values that wouldn't be valid if a moveset wasn't loaded
	// readInt64() may return -1 if the read fails so we have to check for this value as well.

	if (playerAddress == 0 || playerAddress == -1) {
		return false;
	}

	gameAddr currentMove = m_process->readInt64(playerAddress + 0x220);
	if (currentMove == 0 || currentMove == -1) {
		return false;
	}

	gameAddr animAddr = m_process->readInt64(currentMove + 0x10);
	if (animAddr == 0 || animAddr == -1) {
		return false;
	}

	uint8_t animType = m_process->readInt8(animAddr);
	return animType == 0x64 || animType == 0xC8;
}

#include <string>

#include "helpers.hpp"
#include "Importer_t7.hpp"

#include "Structs_t7.h"
#include "GameAddresses.h"
#include "MovesetStructs.h"


// -- Static helpers -- //


static void* GetMovesetInfos(std::ifstream& file, MovesetHeader* out, uint64_t& movesetSize)
{
	file.read((char*)out, sizeof(MovesetHeader));
	file.seekg(0, std::ios::end);
	movesetSize = file.tellg();
	char* movesetData = (char*)malloc(movesetSize);
	if (movesetData) {
		file.seekg(out->offsets.movesetInfoBlock, std::ios::beg);
		file.read(movesetData, movesetSize);
	}
	file.close();
	return movesetData;
}

// -- Public methods -- //

ImportationErrcode ImporterT7::Import(const char* filename, gameAddr playerAddress, bool applyInstantly, float* progress)
{
	std::ifstream file;
	file.open(filename, std::ios::binary);

	if (file.fail()) {
		return ImportationFileReadErr;
	}

	MovesetHeader header{ 0 };

	uint64_t movesetSize;
	void *movesetData = GetMovesetInfos(file, &header, movesetSize);
	if (movesetData == nullptr) {
		return ImportationAllocationErr;
	}
	;

	gameAddr allocatedMovesetArea = m_process->allocateMem(movesetSize);
	if (allocatedMovesetArea == 0) {
		free(movesetData);
		return ImportationGameAllocationErr;
	}

	printf("Moveset allocated at %llx\n", allocatedMovesetArea);
	m_process->writeBytes(allocatedMovesetArea, movesetData, movesetSize);

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

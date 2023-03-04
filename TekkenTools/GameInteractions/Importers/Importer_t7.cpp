#include <string>

#include "helpers.hpp"
#include "Importer_t7.hpp"

#include "Structs_t7.h"

// Contains the same structure as StructsT7 but with gameAddr types instead of ptrs types
// Defined here because i don't want any other file to have access to this shortcut
#define gAddr StructsT7_gameAddr

// -- Static helpers -- //

static char* getMovesetInfos(std::ifstream& file, MovesetHeader* header, uint64_t& movesetSize)
{
	file.read((char*)header, sizeof(MovesetHeader));
	file.seekg(0, std::ios::end);
	movesetSize = file.tellg();
	char* movesetData = (char*)malloc(movesetSize);
	if (movesetData != nullptr) {
		file.seekg(header->offsets.movesetInfoBlock + header->infos.header_size, std::ios::beg);
		file.read(movesetData, movesetSize);
	}
	file.close();
	return movesetData;
}

// -- Private methods -- //

void ImporterT7::ConvertMotaListOffsets(MovesetHeader_offsets& offsets, char* movesetData, gameAddr gameMoveset, gameAddr playerAddress)
{
	MotaList currentMotasList{};
	gameAddr currentMovesetAddr = m_process->readInt64(playerAddress + m_game->addrFile->GetSingleValue("val_motbin_offset"));
	m_process->readBytes(currentMovesetAddr + 0x280, &currentMotasList, sizeof(MotaList));

	MotaList* motaList = (MotaList*)(movesetData + offsets.motalistsBlock);

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

void ImporterT7::ConvertMovesetTableOffsets(MovesetHeader_offsets& offsets, char* moveset, gameAddr gameMoveset)
{
	gAddr::MovesetTable* table = (gAddr::MovesetTable*)(moveset + offsets.tableBlock);
	gameAddr offset = gameMoveset + offsets.movesetBlock;

	printf("moveset block at %llx\n", offsets.movesetBlock + sizeof(MovesetHeader_infos));

	table->reactions += offset;
	table->requirement += offset;
	table->hitCondition += offset;
	table->projectile += offset;
	table->pushback += offset;
	table->pushbackExtradata += offset;
	table->cancel += offset;
	table->groupCancel += offset;
	table->cancelExtradata += offset;
	table->extraMoveProperty += offset;
	table->unknown_0x1f0 += offset;
	table->unknown_0x200 += offset;
	table->move += offset;
	table->voiceclip += offset;
	table->inputSequence += offset;
	table->input += offset;
	table->unknownParryRelated += offset;
	table->cameraData += offset;
	table->throws += offset;
}

void ImporterT7::ConvertMovesOffsets(char* moveset, gameAddr gameMoveset, gAddr::MovesetTable* offsets, MovesetHeader_offsets& blockOffsets)
{
	// todo:: this isnt correct
	gAddr::Move* move = (gAddr::Move*)(moveset + blockOffsets.movesetBlock + offsets->move);
	uint64_t moveCount = offsets->moveCount;

	for (size_t i = 0; i < moveCount; ++i)
	{
		break;
		move->name_addr += gameMoveset + blockOffsets.nameBlock;
		move->anim_addr += gameMoveset + blockOffsets.nameBlock;
		move->anim_addr += gameMoveset + blockOffsets.animationBlock;
		move->cancel_addr += gameMoveset + offsets->cancel;
		move->hit_condition_addr += gameMoveset + offsets->hitCondition;
		move->voicelip_addr += gameMoveset + offsets->voiceclip;
		move->extra_move_property_addr += gameMoveset + offsets->extraMoveProperty;

		++move;
	}
}

// -- Public methods -- //

ImportationErrcode ImporterT7::Import(const char* filename, gameAddr playerAddress, bool applyInstantly, float& progress)
{
	// Read file data
	std::ifstream file(filename, std::ios::binary);

	if (file.fail()) {
		return ImportationFileReadErr;
	}

	// Variables that will store the moveset size & the moveset itself in our own memory
	uint64_t s_moveset;
	char* moveset;

	// Header of the moveset that will contain our own information about it
	MovesetHeader header;

	// Table that contains offsets and amount of cancels, move, requirements, etc...
	gAddr::MovesetTable* offsets;

	// Moveset allocated IN-GAME. 
	gameAddr gameMoveset;

	// -- File reading & allocations -- //


	// Allocate a copy of the moveset locally. This is NOT in the game's memory
	moveset = getMovesetInfos(file, &header, s_moveset);
	if (moveset == nullptr) {
		return ImportationAllocationErr;
	}


	// Allocate our moveset in the game's memory, but we aren't gonna write on that for a while.
	// The idea is to write on our moveset in our own memory (should be faster), then write it all at once on gameMoveset with a single m_process->writeBytes()
	gameMoveset = m_process->allocateMem(s_moveset);
	if (gameMoveset == 0) {
		free(moveset);
		return ImportationGameAllocationErr;
	}

	// -- Conversions -- //


	// Get the table address
	offsets = (gAddr::MovesetTable*)(moveset + header.offsets.tableBlock);


	//Convert move offets into ptrs
	//ConvertMovesOffsets(moveset, gameMoveset, offsets, header.offsets);

	// Turn our table offsets into ptrs. Do this only at the end because we actually need those offsets above
	ConvertMovesetTableOffsets(header.offsets, moveset, gameMoveset);

	// Turn our mota offsets into mota ptrs, or copy the currently loaded character's mota for each we didn't provide
	ConvertMotaListOffsets(header.offsets, moveset, gameMoveset, playerAddress);


	// -- Allocation &Conversion finished -- //

	// Finally write our moveset to the game's memory
	printf("Moveset allocated at %llx\n", gameMoveset);
	m_process->writeBytes(gameMoveset, moveset, s_moveset);

	// Todo: write moveset to player
	// Todo: Write move 32769 to player

	// -- Cleanup -- //

	free(moveset);
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

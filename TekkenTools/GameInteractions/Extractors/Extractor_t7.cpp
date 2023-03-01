#include <stddef.h>

#include "Extractor_t7.hpp"
#include "GameAddressesFile.hpp"

#include "Structs_t7.h"
#include "GameAddresses.h"
#include "helpers.hpp"

//#include <utility>
//#include <vector>

// Todo: remove
#include <chrono>
#include <stdio.h>
#include <iostream>

void getAnimationBlockBounds(t7structs::Move* move, uint64_t moveCount, gameAddr movesetAddr, gameAddr& start, gameAddr& end)
{
	// Todo: fix. This does not extract everything
	uint64_t smallest = (int64_t)move[0].anim_addr;
	uint64_t highest = smallest;

	for (size_t moveId = 1; moveId < moveCount; ++moveId)
	{
		uint64_t anim_addr = (uint64_t)move[moveId].anim_addr;

		if (anim_addr < smallest) {
			smallest = anim_addr;
		}
		else if (anim_addr > highest) {
			if (anim_addr < movesetAddr) {
				highest = anim_addr;
			}
			else {
				// Anim is in MOTAs for some reason??
			}
		}
	}

	start = smallest;
	end = highest;
}

void ConvertPtrsToOffset(void *listAddr, uint64_t to_substract, uint64_t struct_size, uint64_t amount)
{
	uint64_t listCursor = (uint64_t)listAddr;
	while (amount-- > 0)
	{
		uint64_t *structMember = (uint64_t*)listCursor;
		*structMember -= to_substract;
		listCursor += struct_size;
	}
}

void FixMovesetOffsets(char* movesetBlock, const t7structs::movesetLists* lists)
{
	// Fix move ptrs
	char* moveListStart = movesetBlock + (uint64_t)lists->move;
	ConvertPtrsToOffset(moveListStart + offsetof(t7structs::Move, cancel_addr), (gameAddr)lists->cancel, sizeof(t7structs::Move), lists->moveCount);
	ConvertPtrsToOffset(moveListStart + offsetof(t7structs::Move, hit_condition_addr), (gameAddr)lists->hitCondition, sizeof(t7structs::Move), lists->moveCount);
	ConvertPtrsToOffset(moveListStart + offsetof(t7structs::Move, voicelip_addr), (gameAddr)lists->voiceclip, sizeof(t7structs::Move), lists->moveCount);
	ConvertPtrsToOffset(moveListStart + offsetof(t7structs::Move, extra_move_property_addr), (gameAddr)lists->extraMoveProperty, sizeof(t7structs::Move), lists->moveCount);

	// Todo: rest
}

void ExtractorT7::Extract(gameAddr playerAddress, float* progress)
{
	using std::chrono::high_resolution_clock;
	auto t1 = high_resolution_clock::now();
	using std::chrono::duration_cast;
	using std::chrono::duration;
	using std::chrono::milliseconds;

	gameAddr movesetAddr = m_process->readInt64(playerAddress + GameAddressesFile::GetSingleValue("val_motbin_offset"));

	// Get the size of the various lists in the moveset, will need that later
	t7structs::movesetLists lists;
	m_process->readBytes(movesetAddr + 0x150, &lists, sizeof(t7structs::movesetLists));

	gameAddr reactionListAddr = (gameAddr)lists.reactions; // First list
	gameAddr throwListAddr = (gameAddr)lists.throws; // Last list

	ConvertPtrsToOffset(&lists, reactionListAddr, (sizeof(void*) * 2), sizeof(lists) / sizeof(void*) / 2);


	uint64_t movesetInfoBlockSize = 0;
	void* movesetInfoBlock = allocateAndReadBlock(movesetAddr + 0x8, movesetAddr + 0x150, movesetInfoBlockSize);

	uint64_t movesetBlockSize = 0;
	void* movesetBlock = allocateAndReadBlock(reactionListAddr, throwListAddr + (sizeof(t7structs::Throw) * lists.throwsCount), movesetBlockSize);
	FixMovesetOffsets((char*)movesetBlock, &lists);;

	gameAddr animationStart = 0;
	gameAddr animationEnd = 0;
	getAnimationBlockBounds((t7structs::Move*)((int64_t)movesetBlock + (int64_t)lists.move), lists.moveCount, movesetAddr, animationStart, animationEnd);
	//printf("%llx to %llx\n", animationStart, animationEnd);
	//uint64_t animationBlockSize = 0;
	//void* animationBlock = allocateAndReadBlock(animationStart, animationEnd, animationBlockSize);

	


	printf("movesetInfo size: %lld\n", movesetInfoBlockSize);
	printf("movesetBlockSize size: %lld\n", movesetBlockSize);
	printf("Anim block size: %lld - %llx %llx\n", animationEnd - animationStart, animationStart, animationEnd);

	T7MovesetHeader header{0};
	std::string characterName = GetPlayerCharacterName(playerAddress);

	strcpy(header.origin, cm_gameOriginString);
	strcpy(header.target_character, characterName.c_str());
	strcpy(header.date, Helpers::currentDateTime().c_str());

	CreateMovesetFile(characterName.c_str(), cm_gameIdentifierString);

	m_file.write((char*)&header, sizeof(header));
	m_file.write((char*)movesetInfoBlock, movesetInfoBlockSize);
	m_file.write((char*)&lists, sizeof(lists));
	m_file.write((char*)movesetBlock, movesetBlockSize);
	//m_file.write((char*)animationBlock, animationBlockSize);

	free(movesetInfoBlock);
	free(movesetBlock);
	//free(animationBlock);

	CloseMovesetFile();

	auto t2 = high_resolution_clock::now();
	auto ms_int = duration_cast<milliseconds>(t2 - t1);
	duration<double, std::milli> ms_double = t2 - t1;
	std::cout << ms_int.count() << "ms - " << ms_double.count() << "ms\n";
}

std::string ExtractorT7::GetPlayerCharacterName(gameAddr playerAddress)
{
	gameAddr movesetAddr = m_process->readInt64(playerAddress + GameAddressesFile::GetSingleValue("val_motbin_offset"));

	if (movesetAddr == 0) {
		return std::string();
	}

	std::string characterName;

	{
		char buf[32]{ 0 };
		m_process->readBytes(movesetAddr + 0x2E8, buf, 31);
		characterName = std::string(buf);
	}

	if (characterName.front() == '[') {
		characterName.erase(0, 1);
	}

	if (characterName.back() == ']') {
		characterName.erase(characterName.size() - 1);
	}

	return characterName;
}

bool ExtractorT7::CanExtract()
{
	// todo
	return true;
}
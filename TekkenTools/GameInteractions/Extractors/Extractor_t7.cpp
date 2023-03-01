
#include "Extractor_t7.hpp"
#include "GameAddressesFile.hpp"
#include "Structs_t7.h"
#include "GameAddresses.h"

//#include <utility>
//#include <vector>

// Todo: remove
#include <chrono>
#include <stdio.h>
#include <iostream>

std::string ExtractorT7::GetPlayerCharacterName(gameAddr playerAddress)
{
	gameAddr movesetAddr = m_process->readInt64(playerAddress + GameAddressesFile::GetSingleValue("val_motbin_offset"));

	if (movesetAddr == 0) {
		return std::string();
	}

	std::string characterName;

	{
		char buf[32 + 1]{};
		m_process->readBytes(movesetAddr + 0x2E8, buf, 32);
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

void getAnimationBlockBounds(t7structs::Move *move, uint64_t moveCount, gameAddr &start, gameAddr &end)
{
	// Todo: fix. This does not extract everything
	uint64_t smallest = (int64_t)move[0].anim_addr;
	uint64_t highest = smallest;

	for (size_t moveId = 1; moveId < moveCount; ++moveId)
	{
		uint64_t anim_addr = (uint64_t)move[moveId].anim_addr;

		if (anim_addr < smallest) {
			smallest = anim_addr;
		} else if (anim_addr > highest) {
			highest = anim_addr;
		}
	}

	start = smallest;
	end = highest;
}

void ExtractorT7::Extract(gameAddr playerAddress, float* progress)
{
	gameAddr movesetAddr = m_process->readInt64(playerAddress + GameAddressesFile::GetSingleValue("val_motbin_offset"));

	// Get the size of the various lists in the moveset, will need that later
	t7structs::movesetLists lists;
	m_process->readBytes(movesetAddr + 0x150, &lists, sizeof(t7structs::movesetLists));

	gameAddr reactionListAddr = (gameAddr)lists.reactions; // First list
	gameAddr throwListAddr = (gameAddr)lists.throws; // Last list

	// Convert the list addresses to offsets
	for (size_t offset = 0; offset < sizeof(lists); offset += (sizeof(void*) * 2))
	{
		uint64_t* listOffset = (uint64_t *)((uint64_t)&lists + offset);
		*listOffset -= (uint64_t)reactionListAddr;
	}

	
	using std::chrono::high_resolution_clock;
	auto t1 = high_resolution_clock::now();
	using std::chrono::duration_cast;
	using std::chrono::duration;
	using std::chrono::milliseconds;


	uint64_t movesetInfoBlockSize = 0;
	void* movesetInfoBlock = allocateAndReadBlock(movesetAddr + 0x8, movesetAddr + 0x150, movesetInfoBlockSize);

	uint64_t movesetBlockSize = 0;
	void* movesetBlock = allocateAndReadBlock(reactionListAddr, throwListAddr + (sizeof(t7structs::Throw) * lists.throwsCount), movesetBlockSize);

	gameAddr animationStart = 0;
	gameAddr animationEnd = 0;
	getAnimationBlockBounds((t7structs::Move*)((int64_t)movesetBlock + (int64_t)lists.move), lists.moveCount, animationStart, animationEnd);
	//printf("%llx to %llx\n", animationStart, animationEnd);
	uint64_t animationBlockSize = 0;
	void* animationBlock = allocateAndReadBlock(animationStart, animationEnd, animationBlockSize);

	
	auto t2 = high_resolution_clock::now();

	auto ms_int = duration_cast<milliseconds>(t2 - t1);

	duration<double, std::milli> ms_double = t2 - t1;

	std::cout << ms_int.count() << "ms - " << ms_double.count() << "ms\n";


	printf("movesetInfo size: %lld\n", movesetInfoBlockSize);
	printf("movesetBlockSize size: %lld\n", movesetBlockSize);
	printf("Anim block size: %lld\n", animationBlockSize);

	free(movesetInfoBlock);
	free(movesetBlock);
	free(animationBlock);

	std::string characterName = GetPlayerCharacterName(playerAddress);
	CreateMovesetFile(characterName.c_str());
	CloseMovesetFile();
}
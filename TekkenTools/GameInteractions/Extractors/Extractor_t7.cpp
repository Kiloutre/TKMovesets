
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

void getAnimationBlockBounds(t7structs::Move *move, int64_t moveCount, gameAddr &start, gameAddr &end)
{
	start = 0;
	end = 0;
}

void ExtractorT7::Extract(gameAddr playerAddress, float* progress)
{
	gameAddr movesetAddr = m_process->readInt64(playerAddress + GameAddressesFile::GetSingleValue("val_motbin_offset"));
	gameAddr reactionListAddr = m_process->readInt64(movesetAddr + 0x150); // First list
	gameAddr throwListAddr = m_process->readInt64(movesetAddr + 0x270); // Last list

	// Get the size of the various lists in the moveset, will need that later
	t7structs::movesetLists lists;
	m_process->readBytes(movesetAddr + 0x150, &lists, sizeof(t7structs::movesetLists));

	/*
	using std::chrono::high_resolution_clock;
	auto t1 = high_resolution_clock::now();
	using std::chrono::duration_cast;
	using std::chrono::duration;
	using std::chrono::milliseconds;
	*/


	int64_t movesetInfoBlockSize = 0;
	void* movesetInfoBlock = allocateAndReadBlock(movesetAddr + 0x8, movesetAddr + 0x150, movesetInfoBlockSize);

	int64_t movesetSize = 0;
	void* movesetBlock = allocateAndReadBlock(reactionListAddr, throwListAddr + (sizeof(t7structs::Throw) * lists.throwsCount), movesetSize);

	gameAddr animationStart = 0;
	gameAddr animationEnd = 0;
	getAnimationBlockBounds(lists.move, lists.moveCount, animationStart, animationEnd);
	int64_t animationBlockSize = 0;
	void* animationBlock = allocateAndReadBlock(animationStart, animationEnd, animationBlockSize);

	/*
	auto t2 = high_resolution_clock::now();

	auto ms_int = duration_cast<milliseconds>(t2 - t1);

	duration<double, std::milli> ms_double = t2 - t1;

	std::cout << ms_int.count() << "ms\n";
	std::cout << ms_double.count() << "ms\n";
	*/
}
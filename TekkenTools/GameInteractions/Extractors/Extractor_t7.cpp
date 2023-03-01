#include <stddef.h>

#include "Extractor_t7.hpp"
#include "GameAddressesFile.hpp"

#include "Structs_t7.h"
#include "GameAddresses.h"
#include "helpers.hpp"

using Helpers::ConvertPtrsToOffset;
using Helpers::ConvertPtrsToOffsetWithMap;

// Todo: remove
#include <chrono>
#include <stdio.h>
#include <iostream>
//

static int64_t getAnimSize(gameAddr anim)
{
	// todo
	return 0;
}

// Stores the lowest and max animation addresses for quick extraction. Returns fringe (out of bounds) addresses in a vector.
static std::vector<gameAddr> getMainAnimationBlockBounds(t7structs::Move* move, uint64_t moveCount, gameAddr movesetAddr, gameAddr& start, gameAddr& end)
{
	// Todo: fix. This does not extract everything
	uint64_t smallest = (int64_t)move[0].anim_addr;
	uint64_t highest = smallest;

	std::vector<gameAddr> outOfBounds;

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
				// Anim is not in the block, we will save it for later
				outOfBounds.push_back(anim_addr);
			}
		}
	}

	start = smallest;
	end = highest + getAnimSize(highest);

	sort(outOfBounds.begin(), outOfBounds.end());

	return outOfBounds;
}

void* ExtractorT7::GetOtherAnimations(std::vector<gameAddr> outOfBounds, uint64_t& size_out, std::map<gameAddr, uint64_t>& offsets)
{
	//remplir la map
	return nullptr;
}

void ExtractorT7::getNamesBlockBounds(t7structs::Move* move, uint64_t moveCount, gameAddr& start, gameAddr& end)
{
	uint64_t smallest = (int64_t)move[0].name;
	uint64_t highest = smallest;

	for (size_t moveId = 0; moveId < moveCount; ++moveId)
	{
		uint64_t nameAddr = (uint64_t)move[moveId].name;
		uint64_t animNameAddr = (uint64_t)move[moveId].anim_name;

		if (nameAddr < smallest) {
			smallest = nameAddr;
		}
		else if (nameAddr > highest) {
			highest = nameAddr;
		}

		if (animNameAddr < smallest) {
			smallest = animNameAddr;
		}
		else if (animNameAddr > highest) {
			highest = animNameAddr;
		}
	}

	// Move to the last string's END instead of staying at the start
	gameAddr lastItemEnd = (gameAddr)highest;
	while (m_process->readInt8(lastItemEnd) != 0) {
		lastItemEnd += 1;
	}

	start = smallest;
	end = lastItemEnd + 1; // Add 1 for extracting the nullbyte too
}

static void fixMovesetOffsets(char* movesetBlock, const t7structs::movesetLists* lists, gameAddr nameStart, gameAddr animStart, std::map<gameAddr, uint64_t> otherAnimOffsets)
{
	char* addr;
	// Fix move ptrs

	// Add movelist offset
	addr = movesetBlock + (uint64_t)lists->move;
	// Convert every ptr into offsets by substracting the list's head address to the address the member contains
	ConvertPtrsToOffset(addr + offsetof(t7structs::Move, name), nameStart, sizeof(t7structs::Move), lists->moveCount);
	ConvertPtrsToOffset(addr + offsetof(t7structs::Move, anim_name), nameStart, sizeof(t7structs::Move), lists->moveCount);
	// This one tries to replace if found in the map but otherwise will substract like the others
	ConvertPtrsToOffsetWithMap(addr + offsetof(t7structs::Move, anim_addr), animStart, otherAnimOffsets, sizeof(t7structs::Move), lists->moveCount);
	ConvertPtrsToOffset(addr + offsetof(t7structs::Move, cancel_addr), (gameAddr)lists->cancel, sizeof(t7structs::Move), lists->moveCount);
	ConvertPtrsToOffset(addr + offsetof(t7structs::Move, hit_condition_addr), (gameAddr)lists->hitCondition, sizeof(t7structs::Move), lists->moveCount);
	ConvertPtrsToOffset(addr + offsetof(t7structs::Move, voicelip_addr), (gameAddr)lists->voiceclip, sizeof(t7structs::Move), lists->moveCount);
	ConvertPtrsToOffset(addr + offsetof(t7structs::Move, extra_move_property_addr), (gameAddr)lists->extraMoveProperty, sizeof(t7structs::Move), lists->moveCount);

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

	// Get the size of the various lists in the moveset, will need that later. We matched the same structure they did so a single read cab fill it.
	t7structs::movesetLists lists;
	m_process->readBytes(movesetAddr + 0x150, &lists, sizeof(t7structs::movesetLists));

	// Keep these two separated cause the list will lose them (on purpose) but need them for covnerting ptr into offsets
	gameAddr firstListAddr = (gameAddr)lists.reactions;
	gameAddr lastListAddr = (gameAddr)lists.throws;

	// Convert the list of ptr into a list of global offsets by substracting the head of the first list. Lists starts after the previous one ends so this works.
	ConvertPtrsToOffset(&lists, firstListAddr, (sizeof(void*) * 2), sizeof(lists) / sizeof(void*) / 2);

	// Reads block containing basic moveset infos and aliases
	uint64_t movesetInfoBlockSize = 0;
	void* movesetInfoBlock = allocateAndReadBlock(movesetAddr + 0x8, movesetAddr + 0x150, movesetInfoBlockSize);

	// Reads block containing the actual moveset data
	uint64_t movesetBlockSize = 0;
	void* movesetBlock = allocateAndReadBlock(firstListAddr, lastListAddr + (sizeof(t7structs::Throw) * lists.throwsCount), movesetBlockSize);

	// Reads block containing most of the animations (not all of them are in it so this is a first step only)
	uint64_t animationBlockSize = 0;
	gameAddr animationStart = 0;
	gameAddr animationEnd = 0;
	std::vector<gameAddr> outOfBounds;
	outOfBounds = getMainAnimationBlockBounds((t7structs::Move*)((int64_t)movesetBlock + (int64_t)lists.move), lists.moveCount, movesetAddr, animationStart, animationEnd);
	//void* mainAnimationBlock = allocateAndReadBlock(animationStart, animationEnd, animationBlockSize);

	// Extract other animations and build a map
	std::map<gameAddr, uint64_t> otherAnimOffsets;
	uint64_t otherAnimationBlockSize = 0;
	void* otherAnimationsBlock = GetOtherAnimations(outOfBounds, otherAnimationBlockSize, otherAnimOffsets);

	// Reads block containing names of moves and animations
	uint64_t nameBlockSize = 0;
	gameAddr nameStart = 0;
	gameAddr nameEnd = 0;
	getNamesBlockBounds((t7structs::Move*)((int64_t)movesetBlock + (int64_t)lists.move), lists.moveCount, nameStart, nameEnd);
	void *nameBlock = allocateAndReadBlock(nameStart, nameEnd, nameBlockSize);

	
	// Now that we extracted everything, we can properly convert pts to offsets
	fixMovesetOffsets((char*)movesetBlock, &lists, nameStart, animationStart, otherAnimOffsets);


	printf("movesetInfo size: %lld\n", movesetInfoBlockSize);
	printf("movesetBlockSize size: %lld\n", movesetBlockSize);
	printf("Anim block size: %lld - %llx %llx\n", animationEnd - animationStart, animationStart, animationEnd);
	printf("Name block size: %lld - %llx %llx\n", nameEnd - nameStart, nameStart, nameEnd);


	// Setup our own header to write in the output file containg useful information
	MovesetHeader header{0};
	std::string characterName = GetPlayerCharacterName(playerAddress);

	header.flags = 0;
	strcpy(header.versionString, MOVESET_VERSION_STRING);
	strcpy(header.origin, cm_gameOriginString);
	strcpy(header.target_character, characterName.c_str());
	strcpy(header.date, Helpers::currentDateTime().c_str());

	// Create the file
	CreateMovesetFile(characterName.c_str(), cm_gameIdentifierString);

	// Start writing what we extracted into the file
	m_file.write((char*)&header, sizeof(header));
	m_file.write((char*)movesetInfoBlock, movesetInfoBlockSize);
	m_file.write((char*)&lists, sizeof(lists));
	m_file.write((char*)nameBlock, nameBlockSize);
	m_file.write((char*)movesetBlock, movesetBlockSize);
	//m_file.write((char*)mainAnimationBlock, animationBlockSize);
	//m_file.write((char*)otherAnimationsBlock, otherAnimationBlockSize);

	free(movesetInfoBlock);
	free(nameBlock);
	free(movesetBlock);
	//free(mainAnimationBlock);
	//free(otherAnimationsBlock);

	// Extraction is over!
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
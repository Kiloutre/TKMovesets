#include <string>
#include <cctype>
#include <stddef.h>
#include <iterator>

#include "helpers.hpp"
#include "Extractor_t7.hpp"

#include "MovesetStructs.h"
#include "Structs_t7.h"

using namespace t7structs;

// Todo: remove
#include <format>
#include <chrono>
#include <stdio.h>
#include <iostream>
#include <algorithm>
#include <functional>

// -- Static helpers -- //

// Returns a list of anim absolute addresses contained within a mota
static void getMotaAnims(gameAddr motaAddr, std::vector<gameAddr> &animAddr)
{
	// read header
	// push motaAddr + offsets
}

// Returns a list of anim absolute addresses contained within every mota
static std::vector<gameAddr> getMotaListAnims(MotaList* mota)
{
	std::vector<gameAddr> animAddrs;

	for (size_t i = 0; i < 12; ++i)
	{
		gameAddr motaAddr = (gameAddr)mota + i * sizeof(void*);
		getMotaAnims(motaAddr, animAddrs);
	}

	return animAddrs;
}

// Converts abslute ptr into relative offsets before saving to file
static void fixMovesetOffsets(char* movesetBlock, const MovesetLists* lists, gameAddr nameStart, std::map<gameAddr, uint64_t> &animOffsetMap)
{
	// Todo: maybe turn this into callback for ease of reading?

	size_t i;

	// Convert move ptrs
	i = 0;
	for (Move* move = (Move*)(movesetBlock + (uint64_t)lists->move); i < lists->moveCount; ++i, ++move)
	{
		move->name -= nameStart;
		move->anim_name -= nameStart;
		*(char*)(&move->anim_addr) = animOffsetMap[(gameAddr)move->anim_addr];
		move->cancel_addr -= (gameAddr)lists->cancel;
		move->hit_condition_addr -= (gameAddr)lists->hitCondition;
		move->voicelip_addr -= (gameAddr)lists->voiceclip;
		move->extra_move_property_addr -= (gameAddr)lists->extraMoveProperty;
	}

	// Convert projectile ptrs
	i = 0;
	for (Projectile* projectile = (Projectile*)(movesetBlock + (uint64_t)lists->projectile); i < lists->projectileCount; ++i, ++projectile)
	{
		projectile->cancel_addr -= (gameAddr)lists->cancel;
		projectile->hit_condition_addr -= (gameAddr)lists->hitCondition;
	}

	// Convert cancel ptrs
	i = 0;
	for (Cancel* cancel = (Cancel*)(movesetBlock + (uint64_t)lists->cancel); i < lists->cancelCount; ++i, ++cancel)
	{
		cancel->requirement_addr -= (gameAddr)lists->requirement;
		cancel->extradata_addr -= (gameAddr)lists->cancelExtradata;
	}

	// Convert reaction ptrs
	i = 0;
	for (Reactions* reaction = (Reactions*)(movesetBlock + (uint64_t)lists->reactions); i < lists->reactionsCount; ++i, ++reaction)
	{
		reaction->front_pushback -= (gameAddr)lists->pushback;
		reaction->backturned_pushback -= (gameAddr)lists->pushback;
		reaction->left_side_pushback -= (gameAddr)lists->pushback;
		reaction->right_side_pushback -= (gameAddr)lists->pushback;
		reaction->front_counterhit_pushback -= (gameAddr)lists->pushback;
		reaction->downed_pushback -= (gameAddr)lists->pushback;
		reaction->block_pushback -= (gameAddr)lists->pushback;
	}

	// Convert input sequence ptrs
	i = 0;
	for (InputSequence* inputSequence = (InputSequence*)(movesetBlock + (uint64_t)lists->inputSequence); i < lists->inputSequenceCount; ++i, ++inputSequence)
	{
		inputSequence->input_addr -= (gameAddr)lists->input;
	}

	// Convert throws ptrs
	i = 0;
	for (ThrowData* throws = (ThrowData*)(movesetBlock + (uint64_t)lists->throws); i < lists->throwsCount; ++i, ++throws)
	{
		throws->cameradata_addr -= (gameAddr)lists->cameraData;
	}

	// Convert hit conditions ptrs
	i = 0;
	for (HitCondition* hitCondition = (HitCondition*)(movesetBlock + (uint64_t)lists->hitCondition); i < lists->hitConditionCount; ++i, ++hitCondition)
	{
		hitCondition->requirement_addr -= (gameAddr)lists->requirement;
		hitCondition->reactions_addr -= (gameAddr)lists->reactions;
	}
}

static int64_t getClosestBoundary(gameAddr animAddr, std::vector<gameAddr> boundaries)
{
	for (gameAddr comp : boundaries)
	{
		if (comp > animAddr) {
			return comp;
		}
	}
	printf("No boundary found for %llx\n", animAddr);
	// Arbitrary 1KB size for anims where we can't find a close enough boundary
	return animAddr + 1000 + 1;
}

char* ExtractorT7::allocateMotaCustomBlock(MotaList* motas, uint64_t& size_out)
{
	size_out = sizeof(MotaList);
	char* customBlock = (char*)malloc(size_out);

	// Custom block: list of 12 uint64_t (mota offsets), followed by the mota themselves
	// Set offset to 0 to indicate that we didn't export the corresponding mota file

	memcpy(customBlock, motas, sizeof(MotaList));
	// Convert mota offsets into ptrs by getting the size of each block

	for (size_t i = 0; i < 12; ++i)
	{
		((int64_t*)customBlock)[i] = 0;
	}

	return customBlock;
}

// -- Private methods -- //

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


void* ExtractorT7::GetAnimations(t7structs::Move* movelist, size_t moveCount, uint64_t &size_out, std::map<gameAddr, uint64_t>& offsets, std::vector<gameAddr> &boundaries)
{
	uint64_t totalSize = 0;
	std::map<gameAddr, uint64_t> animSizes;
	void* animationBlock = nullptr;

	std::vector<gameAddr> addrList;
	// Get animation list and sort it
	for (size_t i = 0; i < moveCount; ++i)
	{
		t7structs::Move* move = &movelist[i];
		gameAddr anim_addr = (gameAddr)move->anim_addr;

		if (std::find(addrList.begin(), addrList.end(), anim_addr) == addrList.end()) {
			// Avoid adding duplicates
			addrList.push_back(anim_addr);
		}

		// List built by mota
		if (std::find(boundaries.begin(), boundaries.end(), anim_addr) == boundaries.end()) {
			// Avoid adding duplicates
			boundaries.push_back(anim_addr);
		}
	}
	// Sort boundaries list
	std::sort(boundaries.begin(), boundaries.end());
	
	// Find anim sizes and establish offsets
	size_t animCount = addrList.size();
	for (size_t i = 0; i < animCount; ++i)
	{
		gameAddr animAddr = addrList[i];
		gameAddr maxAnimEnd = getClosestBoundary(animAddr, boundaries);

		uint64_t animSize = TryFindAnimSize(animAddr, maxAnimEnd - animAddr);


		//printf("%lld,%llx\n", animSize, animAddr);

		offsets[animAddr] = totalSize;
		animSizes[animAddr] = animSize;
		totalSize += animSize;
	}

	printf("Anim total size: %lld (%f MB)\n", totalSize, (float)totalSize / 1000000);

	// Allocate block
	animationBlock = malloc(totalSize);

	// Read animations and write to our block
	unsigned char *animationBlockCursor = (unsigned char*)animationBlock;
	for (gameAddr animAddr : addrList)
	{
		int64_t animSize = animSizes[animAddr];
		m_process->readBytes(animAddr, animationBlockCursor, animSize);
		animationBlockCursor += animSize;
	}

	size_out = totalSize;
	return animationBlock;
}

uint64_t ExtractorT7::TryFindAnimSize(gameAddr anim, size_t maxSize)
{
	// Attempts to try to get an animation's size
	unsigned char animType = m_process->readInt8(anim);

	if (animType == 0xC8) {
		return ExtractorUtils::getC8AnimSize(m_process, anim);
	}
	else {
		// We do not know how to figure out the size of a 0x64 anim yet

		if (maxSize >= 1000000) {
			// Arbitrary 1MB max size of an animation. Not a good idea tbh
			// todo : change this
			return 1000000;
		}

		return maxSize;
	}
}

// -- Public methods -- //

void ExtractorT7::Extract(gameAddr playerAddress, float* progress, uint8_t gameId, bool overwriteSameFilename)
{
	using std::chrono::high_resolution_clock;
	auto t1 = high_resolution_clock::now();
	using std::chrono::duration_cast;
	using std::chrono::duration;
	using std::chrono::milliseconds;

	gameAddr movesetAddr = m_process->readInt64(playerAddress + m_game->addrFile->GetSingleValue("val_motbin_offset"));

	// Get the size of the various lists in the moveset, will need that later. We matched the same structure they did so a single read cab fill it.
	MovesetLists lists{};
	MotaList motasList{};
	m_process->readBytes(movesetAddr + 0x150, &lists, sizeof(MovesetLists));
	m_process->readBytes(movesetAddr + 0x280, &motasList, sizeof(MotaList));

	// Keep these two separated cause the list will lose them (on purpose) but need them for covnerting ptr into offsets
	gameAddr firstListAddr = (gameAddr)lists.reactions;
	gameAddr lastListAddr = (gameAddr)lists.throws;

	// Convert the list of ptr into a list of global offsets by substracting the head of the first list. Lists starts after the previous one ends so this works.
	Helpers::convertPtrsToOffsets(&lists, firstListAddr, (sizeof(void*) * 2), sizeof(lists) / sizeof(void*) / 2);

	// Reads block containing basic moveset infos and aliases
	uint64_t movesetInfoBlockSize = 0;
	void* movesetInfoBlock = allocateAndReadBlock(movesetAddr + 0x8, movesetAddr + 0x150, movesetInfoBlockSize);

	// Reads block containing the actual moveset data
	uint64_t movesetBlockSize = 0;
	void* movesetBlock = allocateAndReadBlock(firstListAddr, lastListAddr + (sizeof(t7structs::ThrowData) * lists.throwsCount), movesetBlockSize);
	t7structs::Move* movelist = (t7structs::Move*)((int64_t)movesetBlock + (int64_t)lists.move);

	// Prepare anim list to properly guess size of anims
	std::vector<gameAddr> animList = getMotaListAnims(&motasList);
	// Extract animations and build a map for their old address -> their new offset
	std::map<gameAddr, uint64_t> animOffsetMap;
	uint64_t animationBlockSize = 0;
	void* animationBlock = GetAnimations(movelist, lists.moveCount, animationBlockSize, animOffsetMap, animList);

	// Reads block containing names of moves and animations
	gameAddr nameStart = 0;
	gameAddr nameEnd = 0;
	getNamesBlockBounds(movelist, lists.moveCount, nameStart, nameEnd);
	uint64_t nameBlockSize = 0;
	void* nameBlock = allocateAndReadBlock(nameStart, nameEnd, nameBlockSize);

	uint64_t motaCustomBlockSize = 0;
	void* motaCustomBlock = allocateMotaCustomBlock(&motasList, motaCustomBlockSize);


	// Now that we extracted everything, we can properly convert pts to offsets
	fixMovesetOffsets((char*)movesetBlock, &lists, nameStart, animOffsetMap);


	printf("movesetInfo size: %lld\n", movesetInfoBlockSize);
	printf("movesetBlockSize size: %lld\n", movesetBlockSize);
	printf("Name block size: %lld - %llx %llx\n", nameEnd - nameStart, nameStart, nameEnd);


	// Setup our own header to write in the output file containg useful information
	MovesetHeader header{0};
	std::string characterName = GetPlayerCharacterName(playerAddress);

	header.infos.flags = 0;
	header.infos.gameId = gameId;
	header.infos.characterId = GetCharacterID(playerAddress);
	strcpy(header.infos.version_string, MOVESET_VERSION_STRING);
	strcpy(header.infos.origin, GetGameOriginString());
	strcpy(header.infos.target_character, characterName.c_str());
	strcpy(header.infos.date, Helpers::currentDateTime().c_str());

	header.offsets.movesetInfoBlockOffset = sizeof(header);
	header.offsets.listsBlockOffset = header.offsets.movesetInfoBlockOffset + movesetInfoBlockSize;
	header.offsets.nameBlockOffset = header.offsets.listsBlockOffset + sizeof(lists);
	header.offsets.movesetBlockOffset = header.offsets.nameBlockOffset + nameBlockSize;
	header.offsets.animationBlockOffset = header.offsets.movesetBlockOffset + movesetBlockSize;
	header.offsets.motaBlockOffset = header.offsets.animationBlockOffset + animationBlockSize;

	// Create the file
	if (CreateMovesetFile(characterName.c_str(), GetGameIdentifierString(), overwriteSameFilename))
	{
		// Start writing what we extracted into the file
		m_file.write((char*)&header, sizeof(header));
		m_file.write((char*)movesetInfoBlock, movesetInfoBlockSize);
		m_file.write((char*)&lists, sizeof(lists));
		m_file.write((char*)nameBlock, nameBlockSize);
		m_file.write((char*)movesetBlock, movesetBlockSize);
		m_file.write((char*)animationBlock, animationBlockSize);
		m_file.write((char*)motaCustomBlock, motaCustomBlockSize);

		// Extraction is over
		CloseMovesetFile();
	}
	else {
		// todo: signal error that will be looked at by GameExtract
	}



	// Cleanup our temp allocated memory blocks
	free(movesetInfoBlock);
	free(nameBlock);
	free(movesetBlock);
	free(animationBlock);


	auto t2 = high_resolution_clock::now();
	auto ms_int = duration_cast<milliseconds>(t2 - t1);
	duration<double, std::milli> ms_double = t2 - t1;
	std::cout << ms_int.count() << "ms - " << ms_double.count() << "ms\n";
}

bool ExtractorT7::CanExtract()
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


std::string ExtractorT7::GetPlayerCharacterName(gameAddr playerAddress)
{
	gameAddr movesetAddr = m_process->readInt64(playerAddress + m_game->addrFile->GetSingleValue("val_motbin_offset"));
	std::string characterName;
	if (movesetAddr == 0) {
		return characterName;
	}

	{
		char buf[32]{ 0 };
		m_process->readBytes(movesetAddr + 0x2E8, buf, 31);
		characterName = std::string(buf);
	}

	if (characterName.size() == 0) {
		return std::string();
	}

	// Turn name into something more readable. '[CHARACTER_NAME.s]' becomes 'Character Name.S'
	if (characterName.front() == '[') {
		characterName.erase(0, 1);
	}
	if (characterName.back() == ']') {
		characterName.erase(characterName.size() - 1);
	}

	std::replace(characterName.begin(), characterName.end(), '_', ' ');
	{
		bool isWordStart = true;
		for (size_t i = 0; i < characterName.size(); ++i)
		{
			// Have to pass an unsigned char or this will throw when an invalid character is found
			if (!isprint((unsigned char)characterName[i])) {
				characterName[i] = ' ';
			} else if (isalpha((unsigned char)characterName[i])) {
				characterName[i] = isWordStart ? toupper(characterName[i]) : tolower(characterName[i]);
			}
			isWordStart = strchr(" -.", characterName[i]) != nullptr;
		}
	}
	return characterName;
}

uint32_t ExtractorT7::GetCharacterID(gameAddr playerAddress)
{
	return m_process->readInt16(playerAddress + 0xD8);
}

const char* ExtractorT7::GetGameIdentifierString()
{
	return "T7_";
}

const char* ExtractorT7::GetGameOriginString()
{
	return "Tekken 7";
}
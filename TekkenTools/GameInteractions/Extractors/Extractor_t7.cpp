#include <string>
#include <cctype>
#include <stddef.h>
#include <iterator>

#include "helpers.hpp"
#include "Extractor_t7.hpp"

#include "MovesetStructs.h"
#include "Structs_t7.h"

// Todo: remove
#include <format>
#include <chrono>
#include <stdio.h>
#include <iostream>
#include <algorithm>
#include <functional>

// Contains the same structure as StructsT7 but with gameAddr types instead of ptrs types
// Defined here because i don't want any other file to have access to this shortcut
#define gAddr StructsT7Helpers

// Contains the same structures as StructsT7 but with no pointers: gameAddr instead

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

	for (size_t i = 0; i <= 12; ++i)
	{
		gameAddr motaAddr = (gameAddr)mota + i * 8;
		getMotaAnims(motaAddr, animAddrs);
	}

	return animAddrs;
}

// Converts absolute ptr into relative offsets before saving to file
// I know Tekken does the same but convertes to indexes instead of ptrs.
// I just don't think it's a good idea.
static void convertMovesetPointers(char* movesetBlock, const gAddr::MovesetLists &lists, const gAddr::MovesetLists& offsets, gameAddr nameStart, std::map<gameAddr, uint64_t> &animOffsetMap)
{
	size_t i;

	// Convert move ptrs
	i = 0;
	for (gAddr::Move* move = (gAddr::Move*)(movesetBlock + offsets.move); i < lists.moveCount; ++i, ++move)
	{
		move->name -= nameStart;
		move->anim_name -= nameStart;
		move->anim_addr = animOffsetMap[move->anim_addr];
		move->cancel_addr -= lists.cancel;
		move->hit_condition_addr -= lists.hitCondition;
		move->voicelip_addr -= lists.voiceclip;
		move->extra_move_property_addr -= lists.extraMoveProperty;
	}

	// Convert projectile ptrs
	i = 0;
	for (gAddr::Projectile* projectile = (gAddr::Projectile*)(movesetBlock + offsets.projectile); i < lists.projectileCount; ++i, ++projectile)
	{
		projectile->cancel_addr -= lists.cancel;
		projectile->hit_condition_addr -= lists.hitCondition;
	}

	// Convert cancel ptrs
	i = 0;
	for (gAddr::Cancel* cancel = (gAddr::Cancel*)(movesetBlock + offsets.cancel); i < lists.cancelCount; ++i, ++cancel)
	{
		cancel->requirement_addr -= lists.requirement;
		cancel->extradata_addr -= lists.cancelExtradata;
	}

	// Convert reaction ptrs
	i = 0;
	for (gAddr::Reactions* reaction = (gAddr::Reactions*)(movesetBlock + offsets.reactions); i < lists.reactionsCount; ++i, ++reaction)
	{
		reaction->front_pushback -= lists.pushback;
		reaction->backturned_pushback -= lists.pushback;
		reaction->left_side_pushback -= lists.pushback;
		reaction->right_side_pushback -= lists.pushback;
		reaction->front_counterhit_pushback -= lists.pushback;
		reaction->downed_pushback -= lists.pushback;
		reaction->block_pushback -= lists.pushback;
	}

	// Convert input sequence ptrs
	i = 0;
	for (gAddr::InputSequence* inputSequence = (gAddr::InputSequence*)(movesetBlock + offsets.inputSequence); i < lists.inputSequenceCount; ++i, ++inputSequence)
	{
		inputSequence->input_addr -= lists.input;
	}

	// Convert throws ptrs
	i = 0;
	for (gAddr::ThrowData* throws = (gAddr::ThrowData*)(movesetBlock + offsets.throws); i < lists.throwsCount; ++i, ++throws)
	{
		throws->cameradata_addr -= lists.cameraData;
	}

	// Convert hit conditions ptrs
	i = 0;
	for (gAddr::HitCondition* hitCondition = (gAddr::HitCondition*)(movesetBlock + offsets.hitCondition); i < lists.hitConditionCount; ++i, ++hitCondition)
	{
		hitCondition->requirement_addr -= lists.requirement;
		hitCondition->reactions_addr -= lists.reactions;
	}
}

// Find the closest anim address in order to establish the current anim's end. A bit hacky, but does the job until we can understand how to get 0x64 anims sizes
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

// -- Private methods - //

char* ExtractorT7::allocateMotaCustomBlock(MotaList* motas, uint64_t& size_out)
{
	// Custom block contains the mota files we want and does not contain the rest

	std::map<gameAddr, uint64_t> offsetMap;

	size_out = 4; // todo: change, obviously. Get the sizeof the blocks we want and allocate that.
	char* customBlock = (char*)malloc(size_out);
	if (customBlock == nullptr) {
		size_out = 0;
		return nullptr;
	}
	
	//11 motas + 1 unknown (still clearly a ptr)
	uint64_t* motaAddr = (uint64_t*)motas;
	for (size_t i = 0; i < 12; ++i)
	{
		if (offsetMap.find(motaAddr[i]) != offsetMap.end()) {
			motaAddr[i] = offsetMap[motaAddr[i]];
		}
		else {
			// Set to 0 for mota block we aren't exporting
			motaAddr[i] = 0;
		}
	}

	return customBlock;
}

void ExtractorT7::getNamesBlockBounds(Move* move, uint64_t moveCount, gameAddr& start, gameAddr& end)
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


char* ExtractorT7::GetAnimations(Move* movelist, size_t moveCount, uint64_t &size_out, std::map<gameAddr, uint64_t>& offsets, std::vector<gameAddr> &boundaries)
{
	uint64_t totalSize = 0;
	std::map<gameAddr, uint64_t> animSizes;
	char* animationBlock = nullptr;

	std::vector<gameAddr> addrList;
	// Get animation list and sort it
	for (size_t i = 0; i < moveCount; ++i)
	{
		Move* move = &movelist[i];
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
	if (!(animationBlock = (char*)malloc(totalSize))) {
		size_out = 0;
		return nullptr;
	}

	// Read animations and write to our block
	char *animationBlockCursor = animationBlock;
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

ExtractionErrcode ExtractorT7::Extract(gameAddr playerAddress, float* progress, uint8_t gameId, bool overwriteSameFilename)
{
	using std::chrono::high_resolution_clock;
	auto t1 = high_resolution_clock::now();
	using std::chrono::duration_cast;
	using std::chrono::duration;
	using std::chrono::milliseconds;

	gameAddr movesetAddr = m_process->readInt64(playerAddress + m_game->addrFile->GetSingleValue("val_motbin_offset"));

	// Get the size of the various lists in the moveset, will need that later. We matched the same structure they did so a single read cab fill it.
	gAddr::MovesetLists lists{};
	gAddr::MovesetLists listsOffsets{}; // Contains the lists but converted to offsets that we will use to convert ptrs into indexes
	MotaList motasList{};
	m_process->readBytes(movesetAddr + 0x150, &lists, sizeof(MovesetLists));
	m_process->readBytes(movesetAddr + 0x280, &motasList, sizeof(MotaList));

	// Keep these two separated cause the list will lose them (on purpose) but need them for covnerting ptr into offsets
	gameAddr firstListAddr = (gameAddr)lists.reactions;
	gameAddr lastListAddr = (gameAddr)lists.throws;

	// Convert the list of ptr into a list of offsets relative to the movesetInfoBlock
	memcpy(&listsOffsets, &lists, sizeof(MovesetLists));
	Helpers::convertPtrsToOffsets(&listsOffsets, firstListAddr, 16, sizeof(lists) / 8 / 2);

	// Reads block containing basic moveset infos and aliases
	uint64_t movesetInfoBlockSize = 0;
	char* movesetInfoBlock = allocateAndReadBlock(movesetAddr, movesetAddr + 0x150, movesetInfoBlockSize);

	// Reads block containing the actual moveset data
	uint64_t movesetBlockSize = 0;
	char* movesetBlock = allocateAndReadBlock(firstListAddr, lastListAddr + (sizeof(ThrowData) * lists.throwsCount), movesetBlockSize);
	Move* movelist = (Move*)((int64_t)movesetBlock + listsOffsets.move);

	// Prepare anim list to properly guess size of anims
	std::vector<gameAddr> animList = getMotaListAnims(&motasList);
	// Extract animations and build a map for their old address -> their new offset
	std::map<gameAddr, uint64_t> animOffsetMap;
	uint64_t animationBlockSize = 0;
	char* animationBlock = GetAnimations(movelist, lists.moveCount, animationBlockSize, animOffsetMap, animList);

	// Reads block containing names of moves and animations
	gameAddr nameStart = 0;
	gameAddr nameEnd = 0;
	getNamesBlockBounds(movelist, lists.moveCount, nameStart, nameEnd);
	uint64_t nameBlockSize = 0;
	char* nameBlock = allocateAndReadBlock(nameStart, nameEnd, nameBlockSize);

	uint64_t motaCustomBlockSize = 0;
	char* motaCustomBlock = allocateMotaCustomBlock(&motasList, motaCustomBlockSize);


	// Now that we extracted everything, we can properly convert pts to offsets
	convertMovesetPointers(movesetBlock, lists, listsOffsets, nameStart, animOffsetMap);


	// Setup our own header to write in the output file containg useful information
	MovesetHeader header{0};
	std::string characterName = GetPlayerCharacterName(playerAddress);

	header.infos.flags = 0;
	header.infos.gameId = gameId;
	header.infos.characterId = GetCharacterID(playerAddress);
	strcpy(header.infos.version_string, MOVESET_VERSION_STRING);
	strcpy(header.infos.origin, GetGameOriginString());
	strcpy(header.infos.target_character, characterName.c_str());
	header.infos.date = duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();;
	header.infos.header_size = sizeof(MovesetHeader);

	// Calculate each offsets according to the previous block offset + its size
	// Offsets are relative to movesetInfoBlock (which is always 0) and not absolute within the file
	// This is bceause you are not suppoed to allocate the header in the game, header that is stored before movesetInfoBlock
	header.offsets.movesetInfoBlock = 0x0;
	header.offsets.listsBlock = header.offsets.movesetInfoBlock + movesetInfoBlockSize;
	header.offsets.motalistsBlock = header.offsets.listsBlock + sizeof(lists);
	header.offsets.nameBlock = header.offsets.motalistsBlock + sizeof(motasList);
	header.offsets.movesetBlock = header.offsets.nameBlock + nameBlockSize;
	header.offsets.animationBlock = header.offsets.movesetBlock + movesetBlockSize;
	header.offsets.motaBlock = header.offsets.animationBlock + animationBlockSize;

	ExtractionErrcode errcode = ExtractionSuccessful;

	if (movesetInfoBlock == nullptr || nameBlock == nullptr || movesetBlock == nullptr
		|| animationBlock == nullptr || motaCustomBlock == nullptr) {
		// Todo: maybe use new() and delete
		// Todo: this still isn't safe right now because read/write on those pointers after allocating them
		errcode = ExtractionAllocationErr;
	}
	else {
		// Create the file
		if (CreateMovesetFile(characterName.c_str(), GetGameIdentifierString(), overwriteSameFilename))
		{
			// Start writing what we extracted into the file
			m_file.write((char*)&header, sizeof(header));
			m_file.write(movesetInfoBlock, movesetInfoBlockSize);
			m_file.write((char*)&listsOffsets, sizeof(lists));
			m_file.write((char*)&motasList, sizeof(motasList));
			m_file.write(nameBlock, nameBlockSize);
			m_file.write(movesetBlock, movesetBlockSize);
			m_file.write(animationBlock, animationBlockSize);
			m_file.write(motaCustomBlock, motaCustomBlockSize);

			// Extraction is over
			CloseMovesetFile();
		}
		else {
			errcode = ExtractionFileCreationErr;
		}
	}




	// Cleanup our temp allocated memory blocks
	free(movesetInfoBlock);
	free(nameBlock);
	free(movesetBlock);
	free(animationBlock);
	free(motaCustomBlock);


	auto t2 = high_resolution_clock::now();
	auto ms_int = duration_cast<milliseconds>(t2 - t1);
	duration<double, std::milli> ms_double = t2 - t1;
	std::cout << ms_int.count() << "ms - " << ms_double.count() << "ms\n";
	return errcode;
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
#include <string>
#include <cctype>
#include <stddef.h>
#include <iterator>
#include <algorithm>

#include "helpers.hpp"
#include "Extractor_t7.hpp"

// Contains the same structure as StructsT7 but with gameAddr types instead of ptrs types
// Defined here because i don't want any other file to have access to this shortcut
#define gAddr StructsT7_gameAddr

// Contains the same structures as StructsT7 but with no pointers: gameAddr instead

// -- Static helpers -- //

// Returns a list of anim absolute addresses contained within a mota
static void getMotaAnims(gameAddr motaAddr, std::vector<gameAddr> &animAddr)
{
	// read header
	// push motaAddr + offsets
}

// Returns a list of anim absolute addresses contained within every mota
static std::vector<gameAddr> getMotaListAnims(MotaList* mota, std::vector<gameAddr>& animAddrs)
{
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
static void convertMovesetPointers(char* movesetBlock, const gAddr::MovesetTable& table, const gAddr::MovesetTable& offsets, gameAddr nameStart, std::map<gameAddr, uint64_t> &animOffsetMap)
{
	size_t i;

	// Convert move ptrs
	i = 0;
	for (gAddr::Move* move = (gAddr::Move*)(movesetBlock + offsets.move); i < table.moveCount; ++i, ++move)
	{
		move->name -= nameStart;
		move->anim_name -= nameStart;
		move->anim_addr = animOffsetMap[move->anim_addr];
		move->cancel_addr -= table.cancel;
		move->hit_condition_addr -= table.hitCondition;
		move->voicelip_addr -= table.voiceclip;
		move->extra_move_property_addr -= table.extraMoveProperty;
	}

	// Convert projectile ptrs
	i = 0;
	for (gAddr::Projectile* projectile = (gAddr::Projectile*)(movesetBlock + offsets.projectile); i < table.projectileCount; ++i, ++projectile)
	{
		projectile->cancel_addr -= table.cancel;
		projectile->hit_condition_addr -= table.hitCondition;
	}

	// Convert cancel ptrs
	i = 0;
	for (gAddr::Cancel* cancel = (gAddr::Cancel*)(movesetBlock + offsets.cancel); i < table.cancelCount; ++i, ++cancel)
	{
		cancel->requirement_addr -= table.requirement;
		cancel->extradata_addr -= table.cancelExtradata;
	}

	// Convert reaction ptrs
	i = 0;
	for (gAddr::Reactions* reaction = (gAddr::Reactions*)(movesetBlock + offsets.reactions); i < table.reactionsCount; ++i, ++reaction)
	{
		reaction->front_pushback -= table.pushback;
		reaction->backturned_pushback -= table.pushback;
		reaction->left_side_pushback -= table.pushback;
		reaction->right_side_pushback -= table.pushback;
		reaction->front_counterhit_pushback -= table.pushback;
		reaction->downed_pushback -= table.pushback;
		reaction->block_pushback -= table.pushback;
	}

	// Convert input sequence ptrs
	i = 0;
	for (gAddr::InputSequence* inputSequence = (gAddr::InputSequence*)(movesetBlock + offsets.inputSequence); i < table.inputSequenceCount; ++i, ++inputSequence)
	{
		inputSequence->input_addr -= table.input;
	}

	// Convert throws ptrs
	i = 0;
	for (gAddr::ThrowData* throws = (gAddr::ThrowData*)(movesetBlock + offsets.throws); i < table.throwsCount; ++i, ++throws)
	{
		throws->cameradata_addr -= table.cameraData;
	}

	// Convert hit conditions ptrs
	i = 0;
	for (gAddr::HitCondition* hitCondition = (gAddr::HitCondition*)(movesetBlock + offsets.hitCondition); i < table.hitConditionCount; ++i, ++hitCondition)
	{
		hitCondition->requirement_addr -= table.requirement;
		hitCondition->reactions_addr -= table.reactions;
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
	printf("No boundary found for anim %llx, extracting only 1KB\n", animAddr);
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


char* ExtractorT7::CopyAnimations(Move* movelist, size_t moveCount, uint64_t &size_out, std::map<gameAddr, uint64_t>& offsets, std::vector<gameAddr> &boundaries)
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
	// Sort boundaries list because we will iterate on it, trying to find the first item higher than what we're comparing with
	std::sort(boundaries.begin(), boundaries.end());
	
	// Find anim sizes and establish offsets
	size_t animCount = addrList.size();
	for (size_t i = 0; i < animCount; ++i)
	{
		gameAddr animAddr = addrList[i];
		gameAddr maxAnimEnd = getClosestBoundary(animAddr, boundaries);

		uint64_t animSize = GetAnimationSize(animAddr, maxAnimEnd - animAddr);

		offsets[animAddr] = totalSize;
		animSizes[animAddr] = animSize;
		totalSize += animSize;
	}

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

uint64_t ExtractorT7::GetAnimationSize(gameAddr anim, size_t maxSize)
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

void ExtractorT7::FillMovesetTables(gameAddr movesetAddr, gAddr::MovesetTable* table, gAddr::MovesetTable* offsets)
{
	// Fill table
	m_process->readBytes(movesetAddr + 0x150, table, sizeof(MovesetTable));
	// Get the address of the first and last list contained within table. This is used to get the bounds of the movesetBlock
	gameAddr tableStartAddr = (gameAddr)table->reactions;
	// Convert the list of ptr into a list of offsets relative to the movesetInfoBlock
	memcpy(offsets, table, sizeof(MovesetTable));
	Helpers::convertPtrsToOffsets(offsets, tableStartAddr, 16, sizeof(MovesetTable) / 8 / 2);
}

char* ExtractorT7::CopyMovesetBlock(gameAddr movesetAddr, uint64_t& size_out, gAddr::MovesetTable& table)
{
	gameAddr blockStart = (gameAddr)table.reactions;
	gameAddr blockEnd = (gameAddr)table.throws + (sizeof(ThrowData) * table.throwsCount);
	return allocateAndReadBlock(blockStart, blockEnd, size_out);
}

char* ExtractorT7::CopyNameBlock(gameAddr movesetAddr, uint64_t& size_out, Move* movelist, uint64_t moveCount, gameAddr& nameBlockStart)
{
	gameAddr nameBlockEnd;
	getNamesBlockBounds(movelist, moveCount, nameBlockStart, nameBlockEnd);
	return allocateAndReadBlock(nameBlockStart, nameBlockEnd, size_out);
}

char* ExtractorT7::CopyMotaBlocks(gameAddr movesetAddr, uint64_t& size_out, MotaList* motasList, std::vector<gameAddr>& animList)
{
	m_process->readBytes(movesetAddr + 0x280, motasList, sizeof(MotaList));
	getMotaListAnims(motasList, animList);
	return allocateMotaCustomBlock(motasList, size_out);
}

void ExtractorT7::FillHeaderInfos(MovesetHeader_infos& infos, uint8_t gameId, gameAddr playerAddress)
{
	infos.flags = 0; // Currently unused
	infos.gameId = gameId;
	infos.characterId = GetCharacterID(playerAddress);
	strcpy(infos.version_string, MOVESET_VERSION_STRING);
	strcpy(infos.origin, GetGameOriginString());
	strcpy(infos.target_character, GetPlayerCharacterName(playerAddress).c_str());
	infos.date = duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();;
	infos.header_size = sizeof(MovesetHeader);
}

// -- Public methods -- //

ExtractionErrcode ExtractorT7::Extract(gameAddr playerAddress, uint8_t gameId, bool overwriteSameFilename, float& progress)
{
	// These are all the blocks we are going to extract. Most of them will be ripped in one big readBytes()
	char* headerBlock;
	char* movesetInfoBlock;
	char* tableBlock;
	char* motasListBlock;
	char* nameBlock;
	char* movesetBlock;
	char* animationBlock; // This is a custom block: we are building it ourselves because animations are not stored contiguously, as opposed to other datas
	char* motaCustomBlock; // This is also a custom block because not contiguously stored

	// The size in bytes of the same blocks
	uint64_t s_headerBlock = sizeof(MovesetHeader);
	uint64_t s_movesetInfoBlock;
	uint64_t s_tableBlock = sizeof(MovesetTable);
	uint64_t s_motasListBlock = sizeof(MotaList);
	uint64_t s_nameBlock;
	uint64_t s_movesetBlock;
	uint64_t s_animationBlock;
	uint64_t s_motaCustomBlock;

	// Will contain our own informations such as date, version, character id
	MovesetHeader header{ 0 };

	// The address of the moveset we will be extracting
	gameAddr movesetAddr;
	movesetAddr = m_process->readInt64(playerAddress + m_game->addrFile->GetSingleValue("val_motbin_offset"));

	// We will read the table containing <list_ptr:list_size>, in here.
	// Offsets will contain the same as table but converted to offsets, in order to convert the absolute pointers contained within the moveset to offsets
	gAddr::MovesetTable table{};
	gAddr::MovesetTable offsets{};

	// Contains absolute addresses of mota file within the game's memory
	MotaList motasList{};

	// Assign these blocks right away because they're fixed-size structures we write into
	headerBlock = (char*)&header;
	tableBlock = (char*)&offsets;
	motasListBlock = (char*)&motasList;

	// Fill table containing lists of move, lists of cancels, etc...
	FillMovesetTables(movesetAddr, &table, &offsets);

	// Reads block containing the actual moveset data
	// Also get a pointer to our movelist in our own allocated memory. Will be needing it for animation & names extraction.
	movesetBlock = CopyMovesetBlock(movesetAddr, s_movesetBlock, table);
	Move* movelist = (Move*)(movesetBlock + offsets.move);

	// Read mota list, allocate & copy desired mota, prepare anim list to properly guess size of anims later
	std::vector<gameAddr> animBoundaries;
	motaCustomBlock = CopyMotaBlocks(movesetAddr, s_motaCustomBlock, &motasList, animBoundaries);

	// Extract animations and build a map for their old address -> their new offset in our blocks
	std::map<gameAddr, uint64_t> animOffsets;
	animationBlock = CopyAnimations(movelist, table.moveCount, s_animationBlock, animOffsets, animBoundaries);

	// Reads block containing names of moves and animations
	gameAddr nameBlockStart;
	nameBlock = CopyNameBlock(movesetAddr, s_nameBlock, movelist, table.moveCount, nameBlockStart);

	// Reads block containing basic moveset infos and aliases
	movesetInfoBlock = allocateAndReadBlock(movesetAddr, movesetAddr + 0x150, s_movesetInfoBlock);


	// Now that we extracted everything, we can properly convert pts to offsets
	convertMovesetPointers(movesetBlock, table, offsets, nameBlockStart, animOffsets);

	// -- Extraction & data conversion finished --

	// Setup our own header to write in the output file containg useful information
	std::string characterName = GetPlayerCharacterName(playerAddress);

	// Fill the header with our own useful informations
	FillHeaderInfos(header.infos, gameId, playerAddress);

	// Calculate each offsets according to the previous block offset + its size
	// Offsets are relative to movesetInfoBlock (which is always 0) and not absolute within the file
	// This is bceause you are not suppoed to allocate the header in the game, header that is stored before movesetInfoBlock
	header.offsets.movesetInfoBlock = 0x0;
	header.offsets.tableBlock = header.offsets.movesetInfoBlock + s_movesetInfoBlock;
	header.offsets.motalistsBlock = header.offsets.tableBlock + s_tableBlock;
	header.offsets.nameBlock = header.offsets.motalistsBlock + s_motasListBlock;
	header.offsets.movesetBlock = header.offsets.nameBlock + s_nameBlock;
	header.offsets.animationBlock = header.offsets.movesetBlock + s_movesetBlock;
	header.offsets.motaBlock = header.offsets.animationBlock + s_animationBlock;

	ExtractionErrcode errcode;

	// -- Writing the file -- 

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
			m_file.write(headerBlock, s_headerBlock);
			m_file.write(movesetInfoBlock, s_movesetInfoBlock);
			m_file.write(tableBlock, s_tableBlock);
			m_file.write(motasListBlock, s_motaCustomBlock);
			m_file.write(nameBlock, s_nameBlock);
			m_file.write(movesetBlock, s_movesetBlock);
			m_file.write(animationBlock, s_animationBlock);
			m_file.write(motaCustomBlock, s_motaCustomBlock);

			CloseMovesetFile();

			// Extraction is over
			errcode = ExtractionSuccessful;
		}
		else {
			errcode = ExtractionFileCreationErr;
		}
	}


	// -- Cleanup -- //

	/// Cleanup our temp allocated memory blocks
	// headerBlock: not allocated, don't free()
	free(movesetInfoBlock);
	// tableBlock: not allocated, don't free()
	// motasListBlock: not allocated, don't free()
	free(nameBlock);
	free(movesetBlock);
	free(animationBlock);
	free(motaCustomBlock);

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

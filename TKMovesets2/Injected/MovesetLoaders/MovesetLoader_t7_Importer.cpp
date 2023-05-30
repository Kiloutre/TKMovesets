#include <string>

#include "helpers.hpp"
#include "Compression.hpp"
#include "MovesetLoader_t7.hpp"

#include "GameIDs.hpp"
#include "MovesetConverters.hpp"

#include "Structs_t7.h"

using namespace StructsT7;

// Contains the same structure as StructsT7 but with gameAddr types instead of ptrs types
// Defined here because i don't want any other file to have access to this shortcut
#define gAddr StructsT7_gameAddr

// Turns an index into an absolute address, or NULL (0) if index is -1
# define FROM_INDEX(field, listStartAddr, type) (field = (field == -1 ? 0 : listStartAddr + (field * sizeof(type))))

// -- -- //

// Correct the lists pointing to the various moveset structure lists
static void ConvertMovesetTableOffsets(const TKMovesetHeaderBlocks* offsets, Byte* moveset, gameAddr gameMoveset)
{
	gAddr::MovesetTable* table = (gAddr::MovesetTable*)(moveset + offsets->tableBlock);
	gameAddr offset = gameMoveset + offsets->movesetBlock;

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
	table->moveBeginningProp += offset;
	table->moveEndingProp += offset;
	table->move += offset;
	table->voiceclip += offset;
	table->inputSequence += offset;
	table->input += offset;
	table->unknownParryRelated += offset;
	table->cameraData += offset;
	table->throwCameras += offset;
}


//  Convert indexes of moves, cancels, hit conditions, etc... into ptrs
static void ConvertMovesetIndexes(Byte* moveset, gameAddr gameMoveset, const gAddr::MovesetTable* table, const TKMovesetHeaderBlocks* offsets)
{
	gameAddr blockOffset = gameMoveset + offsets->movesetBlock;

	for (auto& move : StructIterator<gAddr::Move>(moveset, offsets->movesetBlock + table->move, table->moveCount))
	{
		move.name_addr += gameMoveset + offsets->nameBlock;
		move.anim_name_addr += gameMoveset + offsets->nameBlock;
		move.anim_addr += gameMoveset + offsets->animationBlock;

		FROM_INDEX(move.cancel_addr, blockOffset + table->cancel, Cancel);
		FROM_INDEX(move._0x28_cancel_addr, blockOffset + table->cancel, Cancel);
		FROM_INDEX(move._0x38_cancel_addr, blockOffset + table->cancel, Cancel);
		FROM_INDEX(move._0x48_cancel_addr, blockOffset + table->cancel, Cancel);
		FROM_INDEX(move.hit_condition_addr, blockOffset + table->hitCondition, HitCondition);
		FROM_INDEX(move.voicelip_addr, blockOffset + table->voiceclip, Voiceclip);
		FROM_INDEX(move.extra_move_property_addr, blockOffset + table->extraMoveProperty, ExtraMoveProperty);
		FROM_INDEX(move.move_start_extraprop_addr, blockOffset + table->moveBeginningProp, OtherMoveProperty);
		FROM_INDEX(move.move_end_extraprop_addr, blockOffset + table->moveEndingProp, OtherMoveProperty);
	}

	// Convert projectile ptrs
	for (auto& projectile : StructIterator<gAddr::Projectile>(moveset, offsets->movesetBlock + table->projectile, table->projectileCount))
	{
		// One projectile actually has both at NULL for some reason ? todo : check
		FROM_INDEX(projectile.cancel_addr, blockOffset + table->cancel, Cancel);
		FROM_INDEX(projectile.hit_condition_addr, blockOffset + table->hitCondition, HitCondition);
	}

	// Convert cancel ptrs
	for (auto& cancel : StructIterator<gAddr::Cancel>(moveset, offsets->movesetBlock + table->cancel, table->cancelCount))
	{
		FROM_INDEX(cancel.requirements_addr, blockOffset + table->requirement, Requirement);
		FROM_INDEX(cancel.extradata_addr, blockOffset + table->cancelExtradata, CancelExtradata);
	}

	// Convert groupe dcancel ptrs
	for (auto& groupCancel : StructIterator<gAddr::Cancel>(moveset, offsets->movesetBlock + table->groupCancel, table->groupCancelCount))
	{
		FROM_INDEX(groupCancel.requirements_addr, blockOffset + table->requirement, Requirement);
		FROM_INDEX(groupCancel.extradata_addr, blockOffset + table->cancelExtradata, CancelExtradata);
	}

	// Convert reaction ptrs
	for (auto& reaction : StructIterator<gAddr::Reactions>(moveset, offsets->movesetBlock + table->reactions, table->reactionsCount))
	{
		FROM_INDEX(reaction.front_pushback, blockOffset + table->pushback, Pushback);
		FROM_INDEX(reaction.backturned_pushback, blockOffset + table->pushback, Pushback);
		FROM_INDEX(reaction.left_side_pushback, blockOffset + table->pushback, Pushback);
		FROM_INDEX(reaction.right_side_pushback, blockOffset + table->pushback, Pushback);
		FROM_INDEX(reaction.front_counterhit_pushback, blockOffset + table->pushback, Pushback);
		FROM_INDEX(reaction.downed_pushback, blockOffset + table->pushback, Pushback);
		FROM_INDEX(reaction.block_pushback, blockOffset + table->pushback, Pushback);
	}

	// Convert input sequence ptrs
	for (auto& inputSequence : StructIterator<gAddr::InputSequence>(moveset, offsets->movesetBlock + table->inputSequence, table->inputSequenceCount))
	{
		FROM_INDEX(inputSequence.input_addr, blockOffset + table->input, Input);
	}

	// Convert throwCameras ptrs
	for (auto& throwCameras : StructIterator<gAddr::ThrowCamera>(moveset, offsets->movesetBlock + table->throwCameras, table->throwCamerasCount))
	{
		FROM_INDEX(throwCameras.cameradata_addr, blockOffset + table->cameraData, CameraData);
	}

	// Convert hit conditions ptrs
	for (auto& hitCondition : StructIterator<gAddr::HitCondition>(moveset, offsets->movesetBlock + table->hitCondition, table->hitConditionCount))
	{
		FROM_INDEX(hitCondition.requirements_addr, blockOffset + table->requirement, Requirement);
		FROM_INDEX(hitCondition.reactions_addr, blockOffset + table->reactions, Reactions);
	}

	// Convert pushback ptrs
	for (auto& pushback : StructIterator<gAddr::Pushback>(moveset, offsets->movesetBlock + table->pushback, table->pushbackCount))
	{
		FROM_INDEX(pushback.extradata_addr, blockOffset + table->pushbackExtradata, PushbackExtradata);
	}

	// Convert move-start ptrs
	for (auto& moveBeginProp : StructIterator<gAddr::OtherMoveProperty>(moveset, offsets->movesetBlock + table->moveBeginningProp, table->moveBeginningPropCount))
	{
		FROM_INDEX(moveBeginProp.requirements_addr, blockOffset + table->requirement, Requirement);
	}

	// Convert move-end prop ptrs
	for (auto& moveEndProp : StructIterator<gAddr::OtherMoveProperty>(moveset, offsets->movesetBlock + table->moveEndingProp, table->moveEndingPropCount))
	{
		FROM_INDEX(moveEndProp.requirements_addr, blockOffset + table->requirement, Requirement);
	}
}


// Correct the offsets of the starting pointers of the moveset data
// This doesn't affect gameplay and to my knowledge is not actually used
static void CorrectMovesetInfoValues(MovesetInfo* info, gameAddr gameMoveset)
{
	info->character_name_addr += gameMoveset;
	info->character_creator_addr += gameMoveset;
	info->date_addr += gameMoveset;
	info->fulldate_addr += gameMoveset;
}


// Enforce the alias at the time of the extraction to be the original ones
static void EnforceCurrentAliasesAsDefault(Byte* moveset)
{
	MovesetInfo* infos = (MovesetInfo*)moveset;

	for (int i = 0; i < _countof(infos->current_aliases); ++i) {
		infos->orig_aliases[i] = infos->current_aliases[i];
	}
}


// Enforce the original aliases as the current one
static void EnforceDefaultAliasesAsCurrent(Byte* moveset)
{
	MovesetInfo* infos = (MovesetInfo*)moveset;

	for (int i = 0; i < _countof(infos->current_aliases); ++i) {
		infos->current_aliases[i] = infos->orig_aliases[i];
	}
}

static void ConvertMotaListOffsets(const TKMovesetHeaderBlocks* offsets, Byte* moveset, gameAddr gameMoveset)
{
	MotaList currentMotasList{};

	MotaList* motaList = (MotaList*)(moveset + offsets->motalistsBlock);

	uint64_t* gameMotas = (uint64_t*)&currentMotasList;
	uint64_t* fileMotas = (uint64_t*)motaList;

	// This is just a list of uint64_t anyway so might as well do this
	for (size_t i = 0; i <= 12; ++i)
	{
		if (fileMotas[i] != MOVESET_ADDR_MISSING) {
			fileMotas[i] += gameMoveset + offsets->motaBlock;
		}
	}
}

// -- -- //

Byte* MovesetLoaderT7::ImportForOnline(SharedMemT7_Player& player, Byte* moveset, uint64_t s_moveset)
{
	Byte* orig_moveset = moveset;

	// Header of the moveset that will contain our own information about it
	const TKMovesetHeader* header = (TKMovesetHeader*)moveset;

	if (s_moveset < sizeof(header) || !header->ValidateHeader()) {
		DEBUG_LOG("Failed to validate header of incoming online moveset.\n");
		delete[] moveset;
		return nullptr;
	}

	bool isCompressed = header->isCompressed();
	auto gameId = header->gameId;

	if (isCompressed) {
		uint64_t src_size = s_moveset - header->moveset_data_start;

		moveset = CompressionUtils::RAW::Moveset::Decompress(moveset, src_size, s_moveset);

		if (moveset == nullptr) {
			DEBUG_LOG("Decompression of moveset failed!\n");
			delete[] orig_moveset;
			return nullptr;
		}
	}
	else {
		// Not compressed
		// Go past the header now that we have a ptr to the header. This will be what is sent to the game.
		s_moveset -= header->moveset_data_start;
		moveset += header->moveset_data_start;
	}

	player.crc32 = header->crc32;
	player.moveset_character_id = header->characterId;
	player.previous_character_id = header->characterId;

	Byte* final_moveset = nullptr;

	switch (gameId)
	{
	case GameId_T7:
		final_moveset = ImportForOnline_FromT7(header, moveset, s_moveset);
		break;
	case GameId_TTT2:
		final_moveset = ImportForOnline_FromTTT2(header, moveset, s_moveset);
		break;
	default:
		DEBUG_LOG("No game found for given game id '%u'\n", gameId);
		break;
	}

	if (isCompressed) {
		delete[] orig_moveset;
	}

	if (final_moveset != nullptr && final_moveset != moveset) {
		delete[] moveset;
	}
	else if (final_moveset == nullptr) {
		DEBUG_LOG("ImportForOnline: Result was zero.\n");
		delete[] moveset;
		return nullptr;

	}

	return final_moveset;
}


Byte* MovesetLoaderT7::ImportForOnline_FromT7(const TKMovesetHeader* header, Byte* moveset, uint64_t s_moveset)
{
	DEBUG_LOG("ImportForOnline_FromT7()\n");

	// List of data blocks within the moveset
	const TKMovesetHeaderBlocks* offsets = (const TKMovesetHeaderBlocks*)((char*)header + header->block_list);

	// Table that contains offsets and amount of cancels, move, requirements, etc...
	gAddr::MovesetTable* table;

	// TODO: Validate moveset

	// -- Basic reading & allocations -- //

	const gameAddr gameMoveset = (gameAddr)moveset;

	// -- Conversions -- //

	// Get the table address
	table = (gAddr::MovesetTable*)(moveset + offsets->tableBlock);

	CorrectMovesetInfoValues((MovesetInfo*)moveset, gameMoveset);

	//Convert move offets into ptrs
	ConvertMovesetIndexes(moveset, gameMoveset, table, offsets);

	// Turn our table offsets into ptrs. Do this only at the end because we actually need those offsets above
	ConvertMovesetTableOffsets(offsets, moveset, gameMoveset);

	// Turn our mota offsets into mota ptrs, or copy the currently loaded character's mota for each we didn't provide
	ConvertMotaListOffsets(offsets, moveset, gameMoveset);

	// -- Allocation & Conversion finished -- //

	EnforceDefaultAliasesAsCurrent(moveset);

	DEBUG_LOG("-- Imported moveset at %llx --\n", gameMoveset);

	return moveset;
}


Byte* MovesetLoaderT7::ImportForOnline_FromTTT2(const TKMovesetHeader* header, Byte* moveset, uint64_t s_moveset)
{
	DEBUG_LOG("ImportForOnline_FromTTT2()\n");

	TKMovesetHeaderBlocks t7_offsets;
	MovesetConverter::TTT2ToT7(header, moveset, s_moveset, t7_offsets);

	// List of data blocks within the moveset
	const TKMovesetHeaderBlocks* offsets = &t7_offsets;

	// Table that contains offsets and amount of cancels, move, requirements, etc...
	gAddr::MovesetTable* table;

	// TODO: Validate moveset

	// -- Basic reading & allocations -- //

	const gameAddr gameMoveset = (gameAddr)moveset;

	// -- Conversions -- //

	// Get the table address
	table = (gAddr::MovesetTable*)(moveset + offsets->tableBlock);

	CorrectMovesetInfoValues((MovesetInfo*)moveset, gameMoveset);

	//Convert move offets into ptrs
	ConvertMovesetIndexes(moveset, gameMoveset, table, offsets);

	// Turn our table offsets into ptrs. Do this only at the end because we actually need those offsets above
	ConvertMovesetTableOffsets(offsets, moveset, gameMoveset);

	// Turn our mota offsets into mota ptrs, or copy the currently loaded character's mota for each we didn't provide
	ConvertMotaListOffsets(offsets, moveset, gameMoveset);

	// -- Allocation & Conversion finished -- //

	EnforceDefaultAliasesAsCurrent(moveset);

	DEBUG_LOG("-- Imported moveset at %llx --\n", gameMoveset);

	return moveset;
}
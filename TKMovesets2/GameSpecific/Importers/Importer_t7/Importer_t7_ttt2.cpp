#include <string>

#include "helpers.hpp"
#include "Importer_t7.hpp"
#include "GameIDs.hpp"

#include "Structs_t7.h"

using namespace StructsT7;

// Contains the same structure as StructsT7 but with gameAddr types instead of ptrs types
// Defined here because i don't want any other file to have access to this shortcut
#define gAddr StructsT7_gameAddr

// -- -- //

static void ConvertDisplayableMovelistOffsets(MvlHead* mvlHead)
{
	MvlDisplayable* displayable = (MvlDisplayable*)((uint64_t)mvlHead + mvlHead->displayables_offset);
	for (size_t i = 0; i < mvlHead->displayables_count; ++i)
	{
		int32_t absoluteDisplayableOffset = mvlHead->displayables_offset + (int32_t)(i * sizeof(MvlDisplayable));
		for (int j = 0; j < _countof(displayable->all_translation_offsets); ++j) {
			int32_t correctedOffset = displayable->all_translation_offsets[j] - absoluteDisplayableOffset;
			displayable->all_translation_offsets[j] = correctedOffset;
		}
		++displayable;
	}

	MvlPlayable* playable = (MvlPlayable*)((uint64_t)mvlHead + mvlHead->playables_offset);
	for (size_t i = 0; i < mvlHead->playables_count; ++i)
	{
		uint32_t playable_addr = mvlHead->playables_offset + (int32_t)(sizeof(MvlPlayable) * i);
		uint32_t input_sequence_id = playable->input_sequence_offset;
		uint32_t input_sequence_addr = input_sequence_id * sizeof(MvlInput) + mvlHead->inputs_offset;

		playable->input_sequence_offset = input_sequence_addr - playable_addr;
		++playable;
	}
}


// In our locally allocated movest, correct the lists pointing to the various moveset structure lists
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


// -- Private methods -- //

ImportationErrcode_ ImporterT7::_Import_FromTTT2(const TKMovesetHeader* header, Byte* moveset, uint64_t s_moveset, gameAddr playerAddress, ImportSettings settings, uint8_t& progress)
{
	DEBUG_LOG("_Import_FromTTT2()\n");
	// Basic load is tied to online importing where we might want to import the moveset but not populate empty MOTA and apply character-specific fixes just yet
	const bool BASIC_LOAD = (settings & ImportSettings_BasicLoadOnly) != 0;

	// List of data blocks within the moveset
	const TKMovesetHeaderBlocks* offsets = (const TKMovesetHeaderBlocks*)((char*)header + header->block_list);

	// Table that contains offsets and amount of cancels, move, requirements, etc...
	gAddr::MovesetTable* table;

	// -- Basic reading & allocations -- //

	progress = 20;

	// Allocate our moveset in the game's memory, but we aren't gonna write on that for a while.
	// The idea is to write on our moveset in our own memory (should be faster), then write it all at once on gameMoveset with a single m_process->writeBytes()
	const gameAddr gameMoveset = m_process->allocateMem(s_moveset);
	if (gameMoveset == 0) {
		return ImportationErrcode_GameAllocationErr;
	}
	progress = 40;

	// -- Conversions -- //

	// Get the table address
	table = (gAddr::MovesetTable*)(moveset + offsets->tableBlock);

	CorrectMovesetInfoValues((MovesetInfo*)moveset, gameMoveset);

	//Convert move offets into ptrs
	ConvertMovesetIndexes(moveset, gameMoveset, table, offsets);
	progress = 70;

	if (!BASIC_LOAD) {
		// Fix moves that use characterID conditions to work
		ApplyCharacterIDFixes(moveset, playerAddress, table, header, offsets);
	}
	progress = 85;

	// Turn our table offsets into ptrs. Do this only at the end because we actually need those offsets above
	ConvertMovesetTableOffsets(offsets, moveset, gameMoveset);
	progress = 80;

	// Turn our mota offsets into mota ptrs, or copy the currently loaded character's mota for each we didn't provide
	ConvertMotaListOffsets(offsets, moveset, gameMoveset, playerAddress, !BASIC_LOAD);
	progress = 90;

	MvlHead* mvlHead = (MvlHead*)(moveset + offsets->movelistBlock);
	bool hasDisplayableMovelist = ((header->moveset_data_start + offsets->movelistBlock + 4) < s_moveset) &&
		(strncmp(mvlHead->mvlString, "MVLT", 4) == 0);

	if (hasDisplayableMovelist) {
		ConvertDisplayableMovelistOffsets(mvlHead);
	}

	// -- Allocation & Conversion finished -- //

	if (hasDisplayableMovelist)
	{
		gameAddr game_mvlHead = (gameAddr)gameMoveset + offsets->movelistBlock;
		ImportMovelist(mvlHead, game_mvlHead, playerAddress);
	}

	if (settings & ImportSettings_EnforceCurrentAliasesAsDefault) {
		EnforceCurrentAliasesAsDefault(moveset);
	}
	else {
		EnforceDefaultAliasesAsCurrent(moveset);
	}

	// Finally write our moveset to the game's memory
	m_process->writeBytes(gameMoveset, moveset, s_moveset);
	progress = 99;
	DEBUG_LOG("-- Imported moveset at %llx --\n", gameMoveset);

	if (!BASIC_LOAD) {
		// Then write our moveset address to the current player
		m_process->writeInt64(playerAddress + m_game->GetValue("motbin_offset"), gameMoveset);
	}
	progress = 100;

	if (!BASIC_LOAD) {
		// Also write camera mota offsts to the player structure if those motas have been exported
		WriteCameraMotasToPlayer(gameMoveset, playerAddress);
	}

	if (!BASIC_LOAD) {
		if (settings & ImportSettings_ApplyInstantly) {
			ForcePlayerMove(playerAddress, gameMoveset, 32769);
		}
	}

	lastLoaded.crc32 = header->crc32;
	lastLoaded.charId = header->characterId;
	lastLoaded.address = gameMoveset;
	lastLoaded.size = s_moveset;

	return ImportationErrcode_Successful;
}

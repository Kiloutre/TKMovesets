#include <string>

#include "helpers.hpp"
#include "Importer_t7.hpp"
#include "Importer_t7/Importer_t7_ttt2_Aliases.hpp"

#include "Structs_t7.h"
#include "Structs_ttt2.h"

using namespace StructsT7;

// Contains the same structure as StructsT7 but with gameAddr types instead of ptrs types
// Defined here because i don't want any other file to have access to this shortcut
#define gAddr StructsT7_gameAddr
#define TTT2 StructsTTT2

#define CONVERT_POSSIBLE_MISSING_ADDR(x) (x == MOVESET_ADDR32_MISSING ? MOVESET_ADDR_MISSING : x)

// -- Aliases -- //

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

static Byte* AllocateMovesetArea(const TKMovesetHeader* header, Byte* moveset, uint64_t s_moveset, uint64_t& size_out, TKMovesetHeaderBlocks& blocks_out)
{
	auto offsets = (const TTT2::TKMovesetHeaderBlocks*)((char*)header + header->block_list);

	auto movesetInfo = (TTT2::MovesetInfo*)moveset;

	size_t movesetSize = 0;
	blocks_out.movesetInfoBlock = 0;

	movesetSize += Helpers::align8Bytes(offsetof(MovesetInfo, table));
	blocks_out.tableBlock = movesetSize;
	movesetSize += Helpers::align8Bytes(sizeof(MovesetTable));

	blocks_out.motalistsBlock = movesetSize;
	movesetSize += Helpers::align8Bytes(sizeof(MotaList));

	blocks_out.nameBlock = movesetSize;
	movesetSize += offsets->GetBlockSize(TTT2::TKMovesetHeaderBlocks_Name, s_moveset);

	blocks_out.movesetBlock = movesetSize;
	size_t movesetBlockSize = 0;
	movesetBlockSize += sizeof(Reactions) * movesetInfo->table.reactionsCount;
	movesetBlockSize += sizeof(Requirement) * movesetInfo->table.requirementCount;
	movesetBlockSize += sizeof(HitCondition) * movesetInfo->table.hitConditionCount;
	movesetBlockSize += sizeof(Projectile) * movesetInfo->table.projectileCount;
	movesetBlockSize += sizeof(Pushback) * movesetInfo->table.pushbackCount;
	movesetBlockSize += sizeof(PushbackExtradata) * movesetInfo->table.pushbackExtradataCount;
	movesetBlockSize += sizeof(Cancel) * movesetInfo->table.cancelCount;
	movesetBlockSize += sizeof(Cancel) * movesetInfo->table.groupCancelCount;
	movesetBlockSize += sizeof(CancelExtradata) * movesetInfo->table.cancelExtradataCount;
	movesetBlockSize += sizeof(ExtraMoveProperty) * movesetInfo->table.extraMovePropertyCount;
	movesetBlockSize += sizeof(OtherMoveProperty) * movesetInfo->table.moveBeginningPropCount;
	movesetBlockSize += sizeof(OtherMoveProperty) * movesetInfo->table.moveEndingPropCount;
	movesetBlockSize += sizeof(Move) * movesetInfo->table.moveCount;
	movesetBlockSize += sizeof(Voiceclip) * movesetInfo->table.voiceclipCount;
	movesetBlockSize += sizeof(InputSequence) * movesetInfo->table.inputSequenceCount;
	movesetBlockSize += sizeof(Input) * movesetInfo->table.inputCount;
	movesetBlockSize += sizeof(UnknownParryRelated) * movesetInfo->table.unknownParryRelatedCount;
	movesetBlockSize += sizeof(CameraData) * movesetInfo->table.cameraDataCount;
	movesetBlockSize += sizeof(ThrowCamera) * movesetInfo->table.throwCamerasCount;
	movesetSize += Helpers::align8Bytes(movesetBlockSize);

	blocks_out.animationBlock = movesetSize;
	movesetSize += offsets->GetBlockSize(TTT2::TKMovesetHeaderBlocks_Animation, s_moveset);
	blocks_out.motaBlock = movesetSize;
	movesetSize += offsets->GetBlockSize(TTT2::TKMovesetHeaderBlocks_Mota, s_moveset);
	blocks_out.movelistBlock = movesetSize;

	Byte* new_moveset = new Byte[movesetSize];
	size_out = movesetSize;
	return new_moveset;
}

static void ConvertToT7Moveset(const TKMovesetHeader* header, Byte*& moveset, uint64_t& s_moveset, TKMovesetHeaderBlocks& blocks_out)
{
	uint64_t new_moveset_size;
	Byte* new_moveset = AllocateMovesetArea(header, moveset, s_moveset, new_moveset_size, blocks_out);

	auto old_blocks = (const TTT2::TKMovesetHeaderBlocks*)((char*)header + header->block_list);
	auto old_movesetInfo = (TTT2::MovesetInfo*)moveset;
	auto new_movesetInfo = (MovesetInfo*)new_moveset;

	memcpy(new_movesetInfo, old_movesetInfo, 8);
	new_movesetInfo->character_name_addr = (char*)0 + old_movesetInfo->character_name_addr + sizeof(MovesetInfo) - sizeof(TTT2::MovesetInfo);
	new_movesetInfo->character_creator_addr = (char*)0 + old_movesetInfo->character_creator_addr + sizeof(MovesetInfo) - sizeof(TTT2::MovesetInfo);
	new_movesetInfo->date_addr = (char*)0 + old_movesetInfo->date_addr + sizeof(MovesetInfo) - sizeof(TTT2::MovesetInfo);
	new_movesetInfo->fulldate_addr = (char*)0 + old_movesetInfo->fulldate_addr + sizeof(MovesetInfo) - sizeof(TTT2::MovesetInfo);

	memcpy(new_movesetInfo->orig_aliases, old_movesetInfo->orig_aliases, sizeof(new_movesetInfo->orig_aliases));
	memcpy(new_movesetInfo->current_aliases, old_movesetInfo->current_aliases, sizeof(new_movesetInfo->current_aliases));
	memcpy(new_movesetInfo->unknown_aliases, old_movesetInfo->unknown_aliases, sizeof(new_movesetInfo->unknown_aliases));

	gAddr::MovesetTable& table = (gAddr::MovesetTable&)new_movesetInfo->table;
	for (unsigned int i = 0; i < _countof(new_movesetInfo->table.entries); ++i) {
		table.entries[i].listCount = old_movesetInfo->table.entries[i].listCount;
	}

	table.reactions = 0;
	table.requirement = table.reactions + sizeof(Reactions) * table.reactionsCount;
	table.hitCondition = table.requirement + sizeof(Requirement) * table.requirementCount;
	table.projectile = table.hitCondition + sizeof(HitCondition) * table.hitConditionCount;
	table.pushback = table.projectile + sizeof(Projectile) * table.projectileCount;
	table.pushbackExtradata = table.pushback + sizeof(Pushback) * table.pushbackCount;
	table.cancel = table.pushbackExtradata + sizeof(PushbackExtradata) * table.pushbackExtradataCount;
	table.groupCancel = table.cancel + sizeof(Cancel) * table.cancelCount;
	table.cancelExtradata = table.groupCancel + sizeof(Cancel) * table.groupCancelCount;
	table.extraMoveProperty = table.cancelExtradata + sizeof(CancelExtradata) * table.cancelExtradataCount;
	table.moveBeginningProp = table.extraMoveProperty + sizeof(ExtraMoveProperty) * table.extraMovePropertyCount;
	table.moveEndingProp = table.moveBeginningProp + sizeof(OtherMoveProperty) * table.moveBeginningPropCount;
	table.move = table.moveEndingProp + sizeof(OtherMoveProperty) * table.moveEndingPropCount;
	table.voiceclip = table.move + sizeof(Move) * table.moveCount;
	table.inputSequence = table.voiceclip + sizeof(Voiceclip) * table.voiceclipCount;
	table.input = table.inputSequence + sizeof(InputSequence) * table.inputSequenceCount;
	table.unknownParryRelated = table.input + sizeof(Input) * table.inputCount;
	table.cameraData = table.unknownParryRelated + sizeof(UnknownParryRelated) * table.unknownParryRelatedCount;
	table.throwCameras = table.cameraData + sizeof(CameraData) * table.cameraDataCount;


	for (auto block : std::vector< TKMovesetHeaderBlocks_>{
		TKMovesetHeaderBlocks_Name,
		TKMovesetHeaderBlocks_Animation,
		TKMovesetHeaderBlocks_Mota,
	})
	{
		Byte* target = blocks_out.GetBlock(block, new_moveset);
		const Byte* source = old_blocks->GetBlock((TTT2::TKMovesetHeaderBlocks_)block, moveset);

		memcpy(target, source, old_blocks->GetBlockSize((TTT2::TKMovesetHeaderBlocks_)block));
	}


	Aliases::BuildAliasDictionary();
	unsigned int orig_character_id = header->characterId;
	unsigned int new_character_id = Aliases::GetCharacterIdAlias(orig_character_id);

	// ** ** //
	for (unsigned int i = 0; i < table.reactionsCount; ++i)
	{
		gAddr::Reactions* target = (gAddr::Reactions*)(blocks_out.GetBlock(TKMovesetHeaderBlocks_Moveset, new_moveset) + table.reactions) + i;
		const TTT2::Reactions* source = (TTT2::Reactions*)(old_blocks->GetBlock(TTT2::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.reactions) + i;

		target->front_pushback = source->front_pushback;
		target->backturned_pushback = source->front_pushback;
		target->left_side_pushback = source->front_pushback;
		target->right_side_pushback = source->front_pushback;
		target->front_counterhit_pushback = source->front_pushback;
		target->downed_pushback = source->front_pushback;
		target->block_pushback = source->front_pushback;

		target->front_direction = source->front_pushback;
		target->back_direction = source->front_pushback;
		target->left_side_direction = source->front_pushback;
		target->right_side_direction = source->front_pushback;
		target->front_counterhit_direction = source->front_pushback;
		target->downed_direction = source->front_pushback;

		target->_0x44_int = source->front_pushback;
		target->_0x48_int = source->front_pushback;

		target->vertical_pushback = source->front_pushback;
		target->standing_moveid = source->front_pushback;
		target->default_moveid = source->front_pushback;
		target->crouch_moveid = source->front_pushback;
		target->counterhit_moveid = source->front_pushback;
		target->crouch_counterhit_moveid = source->front_pushback;
		target->left_side_moveid = source->front_pushback;
		target->crouch_left_side_moveid = source->front_pushback;
		target->right_side_moveid = source->front_pushback;
		target->crouch_right_side_moveid = source->front_pushback;
		target->backturned_moveid = source->front_pushback;
		target->crouch_backturned_moveid = source->front_pushback;
		target->block_moveid = source->front_pushback;
		target->crouch_block_moveid = source->front_pushback;
		target->wallslump_moveid = source->front_pushback;
		target->downed_moveid = source->front_pushback;
	}

	for (unsigned int i = 0; i < table.pushbackExtradataCount; ++i)
	{
		PushbackExtradata* target = (PushbackExtradata*)(blocks_out.GetBlock(TKMovesetHeaderBlocks_Moveset, new_moveset) + table.pushbackExtradata) + i;
		const TTT2::PushbackExtradata* source = (TTT2::PushbackExtradata*)(old_blocks->GetBlock(TTT2::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.pushbackExtradata) + i;

		target->horizontal_offset = source->horizontal_offset;
	}

	for (unsigned int i = 0; i < table.pushbackCount; ++i)
	{
		gAddr::Pushback* target = (gAddr::Pushback*)(blocks_out.GetBlock(TKMovesetHeaderBlocks_Moveset, new_moveset) + table.pushback) + i;
		const TTT2::Pushback* source = (TTT2::Pushback*)(old_blocks->GetBlock(TTT2::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.pushback) + i;

		target->duration = source->duration;
		target->displacement = source->displacement;
		target->num_of_loops = source->num_of_loops;
		target->extradata_addr = source->extradata_addr;
	}

	for (unsigned int i = 0; i < table.requirementCount; ++i)
	{
		Requirement* target = (Requirement*)(blocks_out.GetBlock(TKMovesetHeaderBlocks_Moveset, new_moveset) + table.requirement) + i;
		const TTT2::Requirement* source = (TTT2::Requirement*)(old_blocks->GetBlock(TTT2::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.requirement) + i;

		// todo: alias condition
		target->condition = source->condition;
		target->param_unsigned = source->param_unsigned;

		Aliases::ApplyPropertyAlias(target->condition, target->param_unsigned);
	}

	for (unsigned int i = 0; i < table.cancelExtradataCount; ++i)
	{
		CancelExtradata* target = (CancelExtradata*)(blocks_out.GetBlock(TKMovesetHeaderBlocks_Moveset, new_moveset) + table.cancelExtradata) + i;
		const TTT2::CancelExtradata* source = (TTT2::CancelExtradata*)(old_blocks->GetBlock(TTT2::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.cancelExtradata) + i;

		target->value = source->value;
	}

	for (unsigned int i = 0; i < table.cancelExtradataCount; ++i)
	{
		gAddr::Cancel* target = (gAddr::Cancel*)(blocks_out.GetBlock(TKMovesetHeaderBlocks_Moveset, new_moveset) + table.cancel) + i;
		const TTT2::Cancel* source = (TTT2::Cancel*)(old_blocks->GetBlock(TTT2::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.cancel) + i;

		target->direction = source->direction;
		target->button = source->button;

		target->requirements_addr = source->requirements_addr;
		target->extradata_addr = source->extradata_addr;
		target->detection_start = source->detection_start;
		target->detection_end = source->detection_end;
		target->starting_frame = source->starting_frame;
		target->move_id = source->move_id;
		target->cancel_option = source->cancel_option;
	}

	for (unsigned int i = 0; i < table.groupCancelCount; ++i)
	{
		gAddr::Cancel* target = (gAddr::Cancel*)(blocks_out.GetBlock(TKMovesetHeaderBlocks_Moveset, new_moveset) + table.groupCancel) + i;
		const TTT2::Cancel* source = (TTT2::Cancel*)(old_blocks->GetBlock(TTT2::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.groupCancel) + i;

		target->direction = source->direction;
		target->button = source->button;

		target->requirements_addr = source->requirements_addr;
		target->extradata_addr = source->extradata_addr;
		target->detection_start = source->detection_start;
		target->detection_end = source->detection_end;
		target->starting_frame = source->starting_frame;
		target->move_id = source->move_id;
		target->cancel_option = source->cancel_option;
	}

	for (unsigned int i = 0; i < table.hitConditionCount; ++i)
	{
		gAddr::HitCondition* target = (gAddr::HitCondition*)(blocks_out.GetBlock(TKMovesetHeaderBlocks_Moveset, new_moveset) + table.hitCondition) + i;
		const TTT2::HitCondition* source = (TTT2::HitCondition*)(old_blocks->GetBlock(TTT2::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.hitCondition) + i;

		target->requirements_addr = source->requirements_addr;
		target->damage = source->damage;
		target->reactions_addr = source->reactions_addr;
		target->_0xC_int = 0; // Does not exist
	}

	for (unsigned int i = 0; i < table.voiceclipCount; ++i)
	{
		Voiceclip* target = (Voiceclip*)(blocks_out.GetBlock(TKMovesetHeaderBlocks_Moveset, new_moveset) + table.voiceclip) + i;
		const TTT2::Voiceclip* source = (TTT2::Voiceclip*)(old_blocks->GetBlock(TTT2::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.voiceclip) + i;

		target->id = source->id;
	}

	for (unsigned int i = 0; i < table.extraMovePropertyCount; ++i)
	{
		ExtraMoveProperty* target = (ExtraMoveProperty*)(blocks_out.GetBlock(TKMovesetHeaderBlocks_Moveset, new_moveset) + table.extraMoveProperty) + i;
		const TTT2::ExtraMoveProperty* source = (TTT2::ExtraMoveProperty*)(old_blocks->GetBlock(TTT2::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.extraMoveProperty) + i;

		target->starting_frame = source->starting_frame;
		target->id = source->id;
		target->value_unsigned = source->value_unsigned;

		Aliases::ApplyPropertyAlias(target->id, target->value_unsigned);
	}

	for (unsigned int i = 0; i < table.inputCount; ++i)
	{
		Input* target = (Input*)(blocks_out.GetBlock(TKMovesetHeaderBlocks_Moveset, new_moveset) + table.input) + i;
		const TTT2::Input* source = (TTT2::Input*)(old_blocks->GetBlock(TTT2::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.input) + i;

		target->direction = source->direction;
		target->button = source->button;
	}

	for (unsigned int i = 0; i < table.inputSequenceCount; ++i)
	{
		gAddr::InputSequence* target = (gAddr::InputSequence*)(blocks_out.GetBlock(TKMovesetHeaderBlocks_Moveset, new_moveset) + table.inputSequence) + i;
		const TTT2::InputSequence* source = (TTT2::InputSequence*)(old_blocks->GetBlock(TTT2::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.inputSequence) + i;

		target->input_window_frames = source->input_window_frames;
		target->input_amount = source->input_amount;
		target->_0x4_int = source->_0x0_int8;
		target->input_addr = source->input_addr;
	}

	for (unsigned int i = 0; i < table.projectileCount; ++i)
	{
		gAddr::Projectile* target = (gAddr::Projectile*)(blocks_out.GetBlock(TKMovesetHeaderBlocks_Moveset, new_moveset) + table.projectile) + i;
		const TTT2::Projectile* source = (TTT2::Projectile*)(old_blocks->GetBlock(TTT2::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.projectile) + i;

		// As of this time, don't do any conversion for projectiles
		// Its format is different from the T7 format
		// (The data is still there, in big endian, ready to be parsed by any importer)
		memset(target, 0, sizeof(Projectile));
	}

	for (unsigned int i = 0; i < table.cameraDataCount; ++i)
	{
		CameraData* target = (CameraData*)(blocks_out.GetBlock(TKMovesetHeaderBlocks_Moveset, new_moveset) + table.cameraData) + i;
		const TTT2::CameraData* source = (TTT2::CameraData*)(old_blocks->GetBlock(TTT2::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.cameraData) + i;

		target->_0x0_int = source->_0x0_int;
		target->_0x4_short = source->_0x4_short;
		target->left_side_camera_data = source->left_side_camera_data;
		target->right_side_camera_data = source->right_side_camera_data;
		target->_0xA_short = source->_0xA_short;
	}

	for (unsigned int i = 0; i < table.throwCamerasCount; ++i)
	{
		gAddr::ThrowCamera* target = (gAddr::ThrowCamera*)(blocks_out.GetBlock(TKMovesetHeaderBlocks_Moveset, new_moveset) + table.throwCameras) + i;
		const TTT2::ThrowCamera* source = (TTT2::ThrowCamera*)(old_blocks->GetBlock(TTT2::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.throwCameras) + i;

		target->_0x0_llong = source->_0x0_uint;
		target->cameradata_addr = source->cameradata_addr;
	}


	for (unsigned int i = 0; i < table.unknownParryRelatedCount; ++i)
	{
		UnknownParryRelated* target = (UnknownParryRelated*)(blocks_out.GetBlock(TKMovesetHeaderBlocks_Moveset, new_moveset) + table.unknownParryRelated) + i;
		const TTT2::UnknownParryRelated* source = (TTT2::UnknownParryRelated*)(old_blocks->GetBlock(TTT2::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.unknownParryRelated) + i;

		target->value = source->value;
	}

	for (unsigned int i = 0; i < table.moveBeginningPropCount; ++i)
	{
		gAddr::OtherMoveProperty* target = (gAddr::OtherMoveProperty*)(blocks_out.GetBlock(TKMovesetHeaderBlocks_Moveset, new_moveset) + table.moveBeginningProp) + i;
		const TTT2::OtherMoveProperty* source = (TTT2::OtherMoveProperty*)(old_blocks->GetBlock(TTT2::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.moveBeginningProp) + i;

		target->requirements_addr = source->requirements_addr;
		target->extraprop = source->extraprop;
		target->value = source->value;

		Aliases::ApplyPropertyAlias(target->extraprop, target->value);
	}

	for (unsigned int i = 0; i < table.moveEndingPropCount; ++i)
	{
		gAddr::OtherMoveProperty* target = (gAddr::OtherMoveProperty*)(blocks_out.GetBlock(TKMovesetHeaderBlocks_Moveset, new_moveset) + table.moveEndingProp) + i;
		const TTT2::OtherMoveProperty* source = (TTT2::OtherMoveProperty*)(old_blocks->GetBlock(TTT2::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.moveEndingProp) + i;

		target->requirements_addr = source->requirements_addr;
		target->extraprop = source->extraprop;
		target->value = source->value;

		Aliases::ApplyPropertyAlias(target->extraprop, target->value);
	}

	
	for (unsigned int i = 0; i < table.moveCount; ++i)
	{
		gAddr::Move* target = (gAddr::Move*)(blocks_out.GetBlock(TKMovesetHeaderBlocks_Moveset, new_moveset) + table.move) + i;
		const TTT2::Move* source = (TTT2::Move*)(old_blocks->GetBlock(TTT2::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.move) + i;

		target->name_addr = source->name_addr;
		target->anim_name_addr = source->anim_name_addr;
		target->anim_addr = source->anim_addr;
		target->vuln = source->vuln;
		target->hitlevel = source->hitlevel;
		target->cancel_addr = CONVERT_POSSIBLE_MISSING_ADDR(source->cancel_addr);

		// todo
		target->_0x28_cancel_addr = -1;
		target->_0x30_int__0x28_related = 0;
		target->_0x34_int = 0;
		target->_0x38_cancel_addr = -1;
		target->_0x40_int__0x38_related = 0;
		target->_0x44_int = 0;
		target->_0x48_cancel_addr = -1;
		target->_0x50_int__0x48_related = 0;

		target->transition = source->transition;
		target->_0x56_short = source->_0x56_short;

		// For some reason, these two must be turned into negative to match the T7 format
		target->moveId_val1 = 0 - source->moveId_val1;
		target->moveId_val2 = 0 - source->moveId_val2;

		target->_0x5C_short = 0; // Does not exist
		target->_0x5E_short = 0; // Does not exist
		target->hit_condition_addr = CONVERT_POSSIBLE_MISSING_ADDR(source->hit_condition_addr);
		target->anim_len = source->anim_len;
		target->airborne_start = source->airborne_start;
		target->airborne_end = source->airborne_end;
		target->ground_fall = source->ground_fall;
		target->voicelip_addr = CONVERT_POSSIBLE_MISSING_ADDR(source->voicelip_addr);
		target->extra_move_property_addr = CONVERT_POSSIBLE_MISSING_ADDR(source->extra_move_property_addr);
		target->move_start_extraprop_addr = CONVERT_POSSIBLE_MISSING_ADDR(source->move_start_extraprop_addr);
		target->move_end_extraprop_addr = CONVERT_POSSIBLE_MISSING_ADDR(source->move_end_extraprop_addr);
		target->_0x98_int = Aliases::ConvertMove0x98(source->_0x98_int);
		target->hitbox_location = source->hitbox_location;
		Aliases::ApplyHitboxAlias(target->hitbox_location);
		target->first_active_frame = source->first_active_frame;
		target->last_active_frame = source->last_active_frame;
		target->_0xA8_short = source->_0x6c_short;
		target->distance = source->distance;
		target->_0xAC_int = 0; // Does not exist
	}

	for (unsigned int i = 0; i < _countof(new_movesetInfo->motas.motas); ++i) {
		gameAddr32 motaAddr32 = old_movesetInfo->motas.motas[i];
		if (motaAddr32 == MOVESET_ADDR32_MISSING || true) {
			new_movesetInfo->motas.motas[i] = (MotaHeader*)MOVESET_ADDR_MISSING;
		}
		else {
			uint64_t motaAddr = (uint64_t)0 + motaAddr32;
			new_movesetInfo->motas.motas[i] = (MotaHeader*)motaAddr;// +(new_motaBlock - orig_motaBlock);
		}
	}

	moveset = new_moveset;
	s_moveset = new_moveset_size;
}

// -- Private methods -- //

ImportationErrcode_ ImporterT7::_Import_FromTTT2(const TKMovesetHeader* header, Byte* moveset, uint64_t s_moveset, gameAddr playerAddress, ImportSettings settings, uint8_t& progress)
{
	DEBUG_LOG("_Import_FromTTT2()\n");
	// Basic load is tied to online importing where we might want to import the moveset but not populate empty MOTA and apply character-specific fixes just yet
	const bool BASIC_LOAD = (settings & ImportSettings_BasicLoadOnly) != 0;

	Byte* orig_moveset = moveset;

	TKMovesetHeaderBlocks t7_offsets;
	ConvertToT7Moveset(header, moveset, s_moveset, t7_offsets);

	// List of data blocks within the moveset
	const TKMovesetHeaderBlocks * offsets = &t7_offsets;

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
		// // todo
		//ApplyCharacterIDFixes(moveset, playerAddress, table, header, offsets);
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

	/*
	if (!BASIC_LOAD) {
		// Then write our moveset address to the current player
		m_process->writeInt64(playerAddress + m_game->GetValue("motbin_offset"), gameMoveset);
	}
	*/
	progress = 100;

	/*
	if (!BASIC_LOAD) {
		// Also write camera mota offsts to the player structure if those motas have been exported
		WriteCameraMotasToPlayer(gameMoveset, playerAddress);
	}
	*/
	/*
	if (!BASIC_LOAD) {
		if (settings & ImportSettings_ApplyInstantly) {
			ForcePlayerMove(playerAddress, gameMoveset, 32769);
		}
	}
	*/

	lastLoaded.crc32 = header->crc32;
	lastLoaded.charId = header->characterId;
	lastLoaded.address = gameMoveset;
	lastLoaded.size = s_moveset;

	delete[] moveset;

	return ImportationErrcode_Successful;
}

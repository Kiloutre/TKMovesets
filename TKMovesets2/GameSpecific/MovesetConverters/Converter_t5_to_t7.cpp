#include <string>

#include "helpers.hpp"
#include "Aliases.hpp"
#include "Importer_t7/Importer_t7.hpp"
#include "MovesetConverters.hpp"

#include "Structs_t7.h"
#include "Structs_t5.h"

using namespace StructsT7;
using namespace T7ImportUtils;

// Contains the same structure as StructsT7 but with gameAddr types instead of ptrs types
// Defined here because i don't want any other file to have access to this shortcut
#define gAddr StructsT7_gameAddr
#define T5 StructsT5

// Required macro to properly convert uint32_t -1 to uint64_t -1, if needed
#define CONVERT_POSSIBLE_MISSING_ADDR(x) (x == MOVESET_ADDR32_MISSING ? MOVESET_ADDR_MISSING : x)

namespace T5_T7_Aliases
{
	uint32_t ConvertVoiceclip(uint16_t value);
	unsigned char GetCharacterIdAlias(unsigned char value);
	int ConvertMove0x98(int value);
	uint64_t ConvertCommand(uint32_t command);
};

static Byte* AllocateMovesetArea(const TKMovesetHeader* header, Byte* moveset, uint64_t s_moveset, uint64_t& size_out, TKMovesetHeaderBlocks& blocks_out)
{
	auto offsets = (const T5::TKMovesetHeaderBlocks*)((char*)header + header->block_list);

	auto movesetInfo = (T5::MovesetInfo*)moveset;

	size_t movesetSize = 0;
	blocks_out.movesetInfoBlock = 0;

	movesetSize += Helpers::align8Bytes(offsetof(MovesetInfo, table));
	blocks_out.tableBlock = movesetSize;
	movesetSize += Helpers::align8Bytes(sizeof(MovesetTable));

	blocks_out.motalistsBlock = movesetSize;
	movesetSize += Helpers::align8Bytes(sizeof(MotaList));

	blocks_out.nameBlock = movesetSize;
	movesetSize += offsets->GetBlockSize(T5::TKMovesetHeaderBlocks_Name, s_moveset);

	blocks_out.movesetBlock = movesetSize;
	size_t movesetBlockSize = 0;
	movesetBlockSize += sizeof(Reactions) * movesetInfo->table.reactionsCount;
	movesetBlockSize += sizeof(Requirement) * movesetInfo->table.requirementCount;
	movesetBlockSize += sizeof(HitCondition) * movesetInfo->table.hitConditionCount;
	movesetBlockSize += sizeof(Projectile) * 0;
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
	movesetBlockSize += sizeof(UnknownParryRelated) * 0;
	movesetBlockSize += sizeof(CameraData) * 0;
	movesetBlockSize += sizeof(ThrowCamera) * 0;
	movesetSize += Helpers::align8Bytes(movesetBlockSize);

	blocks_out.animationBlock = movesetSize;
	movesetSize += offsets->GetBlockSize(T5::TKMovesetHeaderBlocks_Animation, s_moveset);
	blocks_out.motaBlock = movesetSize;
	movesetSize += offsets->GetBlockSize(T5::TKMovesetHeaderBlocks_Mota, s_moveset);
	blocks_out.movelistBlock = movesetSize;

	try {
		Byte* new_moveset = new Byte[movesetSize];
		size_out = movesetSize;
		return new_moveset;
	}
	catch (std::bad_alloc& _)
	{
		(void)_;
		DEBUG_ERR("Failed to allocate %llu", movesetSize);
		size_out = 0;
		return nullptr;
	}
}

bool MovesetConverter::T5ToT7::Convert(const TKMovesetHeader* header, Byte*& moveset, uint64_t& s_moveset, TKMovesetHeaderBlocks& blocks_out)
{
	uint64_t new_moveset_size;
	Byte* new_moveset = AllocateMovesetArea(header, moveset, s_moveset, new_moveset_size, blocks_out);

	if (new_moveset == nullptr) {
		return false;
	}

	auto old_blocks = (const T5::TKMovesetHeaderBlocks*)((char*)header + header->block_list);
	auto old_movesetInfo = (T5::MovesetInfo*)moveset;
	auto new_movesetInfo = (MovesetInfo*)new_moveset;

	memcpy(new_movesetInfo, old_movesetInfo, 8);
	new_movesetInfo->character_name_addr = (char*)0 + old_movesetInfo->character_name_addr + sizeof(MovesetInfo) - sizeof(T5::MovesetInfo);
	new_movesetInfo->character_creator_addr = (char*)0 + old_movesetInfo->character_creator_addr + sizeof(MovesetInfo) - sizeof(T5::MovesetInfo);
	new_movesetInfo->date_addr = (char*)0 + old_movesetInfo->date_addr + sizeof(MovesetInfo) - sizeof(T5::MovesetInfo);
	new_movesetInfo->fulldate_addr = (char*)0 + old_movesetInfo->fulldate_addr + sizeof(MovesetInfo) - sizeof(T5::MovesetInfo);

	for (size_t i = 0; i < _countof(new_movesetInfo->orig_aliases); ++i)
	{
		// T5 has less aliases
		if (i < _countof(old_movesetInfo->current_aliases))
		{
			new_movesetInfo->orig_aliases[i] = (uint16_t)old_movesetInfo->orig_aliases[i];
			new_movesetInfo->current_aliases[i] = (uint16_t)old_movesetInfo->current_aliases[i];
		}
		else
		{
			// Fill the ones not provided by T5 with the default 32769 pos
			new_movesetInfo->orig_aliases[i] = (uint16_t)old_movesetInfo->orig_aliases[0];
			new_movesetInfo->current_aliases[i] = (uint16_t)old_movesetInfo->orig_aliases[0];
		}
	}

	memcpy(new_movesetInfo->unknown_values, old_movesetInfo->unknown_values, sizeof(new_movesetInfo->unknown_values));


	gAddr::MovesetTable& table = (gAddr::MovesetTable&)new_movesetInfo->table;

	table.reactionsCount = old_movesetInfo->table.reactionsCount;
	table.requirementCount = old_movesetInfo->table.requirementCount;
	table.hitConditionCount = old_movesetInfo->table.hitConditionCount;
	table.projectileCount = 0;
	table.pushbackCount = old_movesetInfo->table.pushbackCount;
	table.pushbackExtradataCount = old_movesetInfo->table.pushbackExtradataCount;
	table.cancelCount = old_movesetInfo->table.cancelCount;
	table.groupCancelCount = old_movesetInfo->table.groupCancelCount;
	table.cancelExtradataCount = old_movesetInfo->table.cancelExtradataCount;
	table.extraMovePropertyCount = old_movesetInfo->table.extraMovePropertyCount;
	table.moveBeginningPropCount = old_movesetInfo->table.moveBeginningPropCount;
	table.moveEndingPropCount = old_movesetInfo->table.moveEndingPropCount;
	table.moveCount = old_movesetInfo->table.moveCount;
	table.voiceclipCount = old_movesetInfo->table.voiceclipCount;
	table.inputSequenceCount = old_movesetInfo->table.inputSequenceCount;
	table.inputCount = old_movesetInfo->table.inputCount;
	table.unknownParryRelatedCount = 0;
	table.cameraDataCount = 0;
	table.throwCamerasCount = 0;


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


	// Copy name block, animation block & MOTA block
	{
		Byte* target;
		const Byte* source;

		target = blocks_out.GetBlock(TKMovesetHeaderBlocks_Name, new_moveset);
		source = old_blocks->GetBlock(T5::TKMovesetHeaderBlocks_Name, moveset);
		memcpy(target, source, old_blocks->GetBlockSize(T5::TKMovesetHeaderBlocks_Name));

		target = blocks_out.GetBlock(TKMovesetHeaderBlocks_Animation, new_moveset);
		source = old_blocks->GetBlock(T5::TKMovesetHeaderBlocks_Animation, moveset);
		memcpy(target, source, old_blocks->GetBlockSize(T5::TKMovesetHeaderBlocks_Animation));

		target = blocks_out.GetBlock(TKMovesetHeaderBlocks_Mota, new_moveset);
		source = old_blocks->GetBlock(T5::TKMovesetHeaderBlocks_Mota, moveset);
		memcpy(target, source, old_blocks->GetBlockSize(T5::TKMovesetHeaderBlocks_Mota, s_moveset));
	}


	auto propertyAliases = InitAliasDictionary();
	Aliases::BuildAliasDictionary(propertyAliases);

	// ** ** //
	for (unsigned int i = 0; i < table.reactionsCount; ++i)
	{
		gAddr::Reactions* target = (gAddr::Reactions*)(blocks_out.GetBlock(TKMovesetHeaderBlocks_Moveset, new_moveset) + table.reactions) + i;
		const T5::Reactions* source = (T5::Reactions*)(old_blocks->GetBlock(T5::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.reactions) + i;

		target->front_pushback = source->front_pushback;
		target->backturned_pushback = source->backturned_pushback;
		target->left_side_pushback = source->left_side_pushback;
		target->right_side_pushback = source->right_side_pushback;
		target->front_counterhit_pushback = source->front_counterhit_pushback;
		target->downed_pushback = source->downed_pushback;
		target->block_pushback = source->block_pushback;

		target->front_direction = source->front_direction;
		target->back_direction = source->back_direction;
		target->left_side_direction = source->left_side_direction;
		target->right_side_direction = source->right_side_direction;
		target->front_counterhit_direction = source->front_counterhit_direction;
		target->downed_direction = source->downed_direction;

		target->_0x44_int = source->_0x28_int;
		target->_0x48_int = source->_0x2C_int;

		target->vertical_pushback = source->vertical_pushback;
		target->standing_moveid = source->standing_moveid;
		target->default_moveid = source->default_moveid;
		target->crouch_moveid = source->crouch_moveid;
		target->counterhit_moveid = source->counterhit_moveid;
		target->crouch_counterhit_moveid = source->crouch_counterhit_moveid;
		target->left_side_moveid = source->left_side_moveid;
		target->crouch_left_side_moveid = source->crouch_left_side_moveid;
		target->right_side_moveid = source->right_side_moveid;
		target->crouch_right_side_moveid = source->crouch_right_side_moveid;
		target->backturned_moveid = source->backturned_moveid;
		target->crouch_backturned_moveid = source->crouch_backturned_moveid;
		target->block_moveid = source->block_moveid;
		target->crouch_block_moveid = source->crouch_block_moveid;
		target->wallslump_moveid = source->wallslump_moveid;
		target->downed_moveid = source->downed_moveid;
		target->_0x6c = 0;
	}

	for (unsigned int i = 0; i < table.pushbackExtradataCount; ++i)
	{
		PushbackExtradata* target = (PushbackExtradata*)(blocks_out.GetBlock(TKMovesetHeaderBlocks_Moveset, new_moveset) + table.pushbackExtradata) + i;
		const T5::PushbackExtradata* source = (T5::PushbackExtradata*)(old_blocks->GetBlock(T5::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.pushbackExtradata) + i;

		target->horizontal_offset = source->horizontal_offset;
	}

	for (unsigned int i = 0; i < table.pushbackCount; ++i)
	{
		gAddr::Pushback* target = (gAddr::Pushback*)(blocks_out.GetBlock(TKMovesetHeaderBlocks_Moveset, new_moveset) + table.pushback) + i;
		const T5::Pushback* source = (T5::Pushback*)(old_blocks->GetBlock(T5::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.pushback) + i;

		target->duration = source->duration;
		target->displacement = source->displacement;
		target->num_of_loops = source->num_of_loops;
		target->extradata_addr = source->extradata_addr;
	}

	for (unsigned int i = 0; i < table.requirementCount; ++i)
	{
		Requirement* target = (Requirement*)(blocks_out.GetBlock(TKMovesetHeaderBlocks_Moveset, new_moveset) + table.requirement) + i;
		const T5::Requirement* source = (T5::Requirement*)(old_blocks->GetBlock(T5::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.requirement) + i;

		target->condition = source->condition;
		target->param_unsigned = source->param_unsigned;

		Aliases::ApplyPropertyAlias(target->condition, target->param_unsigned, propertyAliases);
	}

	for (unsigned int i = 0; i < table.cancelExtradataCount; ++i)
	{
		CancelExtradata* target = (CancelExtradata*)(blocks_out.GetBlock(TKMovesetHeaderBlocks_Moveset, new_moveset) + table.cancelExtradata) + i;
		const T5::CancelExtradata* source = (T5::CancelExtradata*)(old_blocks->GetBlock(T5::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.cancelExtradata) + i;

		target->value = source->value;
	}

	for (unsigned int i = 0; i < table.moveCount; ++i)
	{
		gAddr::Move* target = (gAddr::Move*)(blocks_out.GetBlock(TKMovesetHeaderBlocks_Moveset, new_moveset) + table.move) + i;
		const T5::Move* source = (T5::Move*)(old_blocks->GetBlock(T5::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.move) + i;

		target->name_addr = source->name_addr;
		target->anim_name_addr = source->anim_name_addr;
		target->anim_addr = source->anim_addr;
		target->vuln = source->vuln;
		target->hitlevel = source->hitlevel;
		target->cancel_addr = CONVERT_POSSIBLE_MISSING_ADDR(source->cancel_addr);

		target->_0x28_cancel_addr = MOVESET_ADDR_MISSING; // Does not exist
		target->_0x30_int__0x28_related = 0; // Does not exist
		target->_0x34_int = 0; // Does not exist
		target->_0x38_cancel_addr = MOVESET_ADDR_MISSING; // Does not exist
		target->_0x40_int__0x38_related = 0;
		target->_0x44_int = 0; // Does not exist
		target->_0x48_cancel_addr = MOVESET_ADDR_MISSING; // Does not exist
		target->_0x50_int__0x48_related = 0;

		target->transition = source->transition;
		target->_0x56_short = 0; // Does not exist

		// For some reason, these two must be turned into negative to match the T7 format
		target->moveId_val1 = 0; // Does not exist
		target->moveId_val2 = 0; // Does not exist

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
		target->_0x98_int = T5_T7_Aliases::ConvertMove0x98(source->_0x98_int);
		target->hitbox_location = source->hitbox_location;
		target->first_active_frame = source->first_active_frame;
		target->last_active_frame = source->last_active_frame;
		target->_0xA8_short = source->_0x6c_short;
		target->distance = source->distance;
		target->_0xAC_int = 0; // Does not exist
	}
	for (unsigned int i = 0; i < table.cancelCount; ++i)
	{
		gAddr::Cancel* target = (gAddr::Cancel*)(blocks_out.GetBlock(TKMovesetHeaderBlocks_Moveset, new_moveset) + table.cancel) + i;
		const T5::Cancel* source = (T5::Cancel*)(old_blocks->GetBlock(T5::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.cancel) + i;

		target->command = T5_T7_Aliases::ConvertCommand(source->command);

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
		const T5::Cancel* source = (T5::Cancel*)(old_blocks->GetBlock(T5::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.groupCancel) + i;

		target->command = T5_T7_Aliases::ConvertCommand(source->command);

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
		const T5::HitCondition* source = (T5::HitCondition*)(old_blocks->GetBlock(T5::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.hitCondition) + i;

		target->requirements_addr = source->requirements_addr;
		target->damage = source->damage;
		target->reactions_addr = source->reactions_addr;
		target->_0xC_int = 0; // Does not exist
	}

	for (unsigned int i = 0; i < table.voiceclipCount; ++i)
	{
		Voiceclip* target = (Voiceclip*)(blocks_out.GetBlock(TKMovesetHeaderBlocks_Moveset, new_moveset) + table.voiceclip) + i;
		const T5::Voiceclip* source = (T5::Voiceclip*)(old_blocks->GetBlock(T5::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.voiceclip) + i;

		target->id = T5_T7_Aliases::ConvertVoiceclip(source->id);
	}

	for (unsigned int i = 0; i < table.extraMovePropertyCount; ++i)
	{
		ExtraMoveProperty* target = (ExtraMoveProperty*)(blocks_out.GetBlock(TKMovesetHeaderBlocks_Moveset, new_moveset) + table.extraMoveProperty) + i;
		const T5::ExtraMoveProperty* source = (T5::ExtraMoveProperty*)(old_blocks->GetBlock(T5::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.extraMoveProperty) + i;

		memset(target, 0, sizeof(*target));
		// Void out extra properties to avoid crashes
		/*
		target->starting_frame = source->starting_frame;
		target->id = source->id;
		target->value_unsigned = source->value_unsigned;

		Aliases::ApplyPropertyAlias(target->id, target->value_unsigned, propertyAliases);
		*/
	}

	for (unsigned int i = 0; i < table.inputCount; ++i)
	{
		Input* target = (Input*)(blocks_out.GetBlock(TKMovesetHeaderBlocks_Moveset, new_moveset) + table.input) + i;
		const T5::Input* source = (T5::Input*)(old_blocks->GetBlock(T5::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.input) + i;
		
		target->command = T5_T7_Aliases::ConvertCommand(source->command);
	}

	for (unsigned int i = 0; i < table.inputSequenceCount; ++i)
	{
		gAddr::InputSequence* target = (gAddr::InputSequence*)(blocks_out.GetBlock(TKMovesetHeaderBlocks_Moveset, new_moveset) + table.inputSequence) + i;
		const T5::InputSequence* source = (T5::InputSequence*)(old_blocks->GetBlock(T5::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.inputSequence) + i;

		target->input_window_frames = source->input_window_frames;
		target->input_amount = source->input_amount;
		target->_0x4_int = source->_0x0_int8;
		target->input_addr = source->input_addr;
	}

	for (unsigned int i = 0; i < table.moveBeginningPropCount; ++i)
	{
		gAddr::OtherMoveProperty* target = (gAddr::OtherMoveProperty*)(blocks_out.GetBlock(TKMovesetHeaderBlocks_Moveset, new_moveset) + table.moveBeginningProp) + i;
		const T5::OtherMoveProperty* source = (T5::OtherMoveProperty*)(old_blocks->GetBlock(T5::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.moveBeginningProp) + i;

		memset(target, 0, sizeof(*target));
		// Void out extra properties to avoid crashes
		/*
		target->requirements_addr = source->requirements_addr;
		target->extraprop = source->extraprop;
		target->value = source->value;

		Aliases::ApplyPropertyAlias(target->extraprop, target->value, propertyAliases);
		*/
	}

	for (unsigned int i = 0; i < table.moveEndingPropCount; ++i)
	{
		gAddr::OtherMoveProperty* target = (gAddr::OtherMoveProperty*)(blocks_out.GetBlock(TKMovesetHeaderBlocks_Moveset, new_moveset) + table.moveEndingProp) + i;
		const T5::OtherMoveProperty* source = (T5::OtherMoveProperty*)(old_blocks->GetBlock(T5::TKMovesetHeaderBlocks_Moveset, moveset) + old_movesetInfo->table.moveEndingProp) + i;

		memset(target, 0, sizeof(*target));
		// Void out extra properties to avoid crashes
		/*
		target->requirements_addr = source->requirements_addr;
		target->extraprop = source->extraprop;
		target->value = source->value;

		Aliases::ApplyPropertyAlias(target->extraprop, target->value, propertyAliases);
		*/
	}


	for (unsigned int i = 0; i < _countof(new_movesetInfo->motas.motas); ++i) {
		gameAddr32 motaAddr32 = old_movesetInfo->motas.motas[i];
		if (motaAddr32 == MOVESET_ADDR32_MISSING) {
			new_movesetInfo->motas.motas[i] = (MotaHeader*)MOVESET_ADDR_MISSING;
		}
		else {
			uint64_t motaAddr = (uint64_t)0 + motaAddr32;
			new_movesetInfo->motas.motas[i] = (MotaHeader*)motaAddr;
		}
	}

	moveset = new_moveset;
	s_moveset = new_moveset_size;

	return true;
}
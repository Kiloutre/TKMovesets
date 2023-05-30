#include "Helpers.hpp"
#include "Importer_t7_Utils.hpp"

// Turns an index into an absolute address, or NULL (0) if index is -1
# define FROM_INDEX(field, listStartAddr, type) (field = (field == -1 ? 0 : listStartAddr + (field * sizeof(type))))


namespace T7ImportUtils
{
	void ConvertDisplayableMovelistOffsets(StructsT7::MvlHead* mvlHead)
	{
		StructsT7::MvlDisplayable* displayable = (StructsT7::MvlDisplayable*)((uint64_t)mvlHead + mvlHead->displayables_offset);
		for (size_t i = 0; i < mvlHead->displayables_count; ++i)
		{
			int32_t absoluteDisplayableOffset = mvlHead->displayables_offset + (int32_t)(i * sizeof(StructsT7::MvlDisplayable));
			for (int j = 0; j < _countof(displayable->all_translation_offsets); ++j) {
				int32_t correctedOffset = displayable->all_translation_offsets[j] - absoluteDisplayableOffset;
				displayable->all_translation_offsets[j] = correctedOffset;
			}
			++displayable;
		}

		StructsT7::MvlPlayable* playable = (StructsT7::MvlPlayable*)((uint64_t)mvlHead + mvlHead->playables_offset);
		for (size_t i = 0; i < mvlHead->playables_count; ++i)
		{
			uint32_t playable_addr = mvlHead->playables_offset + (int32_t)(sizeof(StructsT7::MvlPlayable) * i);
			uint32_t input_sequence_id = playable->input_sequence_offset;
			uint32_t input_sequence_addr = input_sequence_id * sizeof(StructsT7::MvlInput) + mvlHead->inputs_offset;

			playable->input_sequence_offset = input_sequence_addr - playable_addr;
			++playable;
		}
	}


	void ConvertMovesetTableOffsets(const StructsT7::TKMovesetHeaderBlocks* offsets, Byte* moveset, gameAddr gameMoveset)
	{
		StructsT7_gameAddr::MovesetTable* table = (StructsT7_gameAddr::MovesetTable*)(moveset + offsets->tableBlock);
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


	void ConvertMovesetIndexes(Byte* moveset, gameAddr gameMoveset, const StructsT7_gameAddr::MovesetTable* table, const StructsT7::TKMovesetHeaderBlocks* offsets)
	{
		gameAddr blockOffset = gameMoveset + offsets->movesetBlock;

		for (auto& move : StructIterator<StructsT7_gameAddr::Move>(moveset, offsets->movesetBlock + table->move, table->moveCount))
		{
			move.name_addr += gameMoveset + offsets->nameBlock;
			move.anim_name_addr += gameMoveset + offsets->nameBlock;
			move.anim_addr += gameMoveset + offsets->animationBlock;

			FROM_INDEX(move.cancel_addr, blockOffset + table->cancel, StructsT7::Cancel);
			FROM_INDEX(move._0x28_cancel_addr, blockOffset + table->cancel, StructsT7::Cancel);
			FROM_INDEX(move._0x38_cancel_addr, blockOffset + table->cancel, StructsT7::Cancel);
			FROM_INDEX(move._0x48_cancel_addr, blockOffset + table->cancel, StructsT7::Cancel);
			FROM_INDEX(move.hit_condition_addr, blockOffset + table->hitCondition, StructsT7::HitCondition);
			FROM_INDEX(move.voicelip_addr, blockOffset + table->voiceclip, StructsT7::Voiceclip);
			FROM_INDEX(move.extra_move_property_addr, blockOffset + table->extraMoveProperty, StructsT7::ExtraMoveProperty);
			FROM_INDEX(move.move_start_extraprop_addr, blockOffset + table->moveBeginningProp, StructsT7::OtherMoveProperty);
			FROM_INDEX(move.move_end_extraprop_addr, blockOffset + table->moveEndingProp, StructsT7::OtherMoveProperty);
		}

		// Convert projectile ptrs
		for (auto& projectile : StructIterator<StructsT7_gameAddr::Projectile>(moveset, offsets->movesetBlock + table->projectile, table->projectileCount))
		{
			// One projectile actually has both at NULL for some reason ? todo : check
			FROM_INDEX(projectile.cancel_addr, blockOffset + table->cancel, StructsT7::Cancel);
			FROM_INDEX(projectile.hit_condition_addr, blockOffset + table->hitCondition, StructsT7::HitCondition);
		}

		// Convert cancel ptrs
		for (auto& cancel : StructIterator<StructsT7_gameAddr::Cancel>(moveset, offsets->movesetBlock + table->cancel, table->cancelCount))
		{
			FROM_INDEX(cancel.requirements_addr, blockOffset + table->requirement, StructsT7::Requirement);
			FROM_INDEX(cancel.extradata_addr, blockOffset + table->cancelExtradata, StructsT7::CancelExtradata);
		}

		// Convert groupe dcancel ptrs
		for (auto& groupCancel : StructIterator<StructsT7_gameAddr::Cancel>(moveset, offsets->movesetBlock + table->groupCancel, table->groupCancelCount))
		{
			FROM_INDEX(groupCancel.requirements_addr, blockOffset + table->requirement, StructsT7::Requirement);
			FROM_INDEX(groupCancel.extradata_addr, blockOffset + table->cancelExtradata, StructsT7::CancelExtradata);
		}

		// Convert reaction ptrs
		for (auto& reaction : StructIterator<StructsT7_gameAddr::Reactions>(moveset, offsets->movesetBlock + table->reactions, table->reactionsCount))
		{
			FROM_INDEX(reaction.front_pushback, blockOffset + table->pushback, StructsT7::Pushback);
			FROM_INDEX(reaction.backturned_pushback, blockOffset + table->pushback, StructsT7::Pushback);
			FROM_INDEX(reaction.left_side_pushback, blockOffset + table->pushback, StructsT7::Pushback);
			FROM_INDEX(reaction.right_side_pushback, blockOffset + table->pushback, StructsT7::Pushback);
			FROM_INDEX(reaction.front_counterhit_pushback, blockOffset + table->pushback, StructsT7::Pushback);
			FROM_INDEX(reaction.downed_pushback, blockOffset + table->pushback, StructsT7::Pushback);
			FROM_INDEX(reaction.block_pushback, blockOffset + table->pushback, StructsT7::Pushback);
		}

		// Convert input sequence ptrs
		for (auto& inputSequence : StructIterator<StructsT7_gameAddr::InputSequence>(moveset, offsets->movesetBlock + table->inputSequence, table->inputSequenceCount))
		{
			FROM_INDEX(inputSequence.input_addr, blockOffset + table->input, StructsT7::Input);
		}

		// Convert throwCameras ptrs
		for (auto& throwCameras : StructIterator<StructsT7_gameAddr::ThrowCamera>(moveset, offsets->movesetBlock + table->throwCameras, table->throwCamerasCount))
		{
			FROM_INDEX(throwCameras.cameradata_addr, blockOffset + table->cameraData, StructsT7::CameraData);
		}

		// Convert hit conditions ptrs
		for (auto& hitCondition : StructIterator<StructsT7_gameAddr::HitCondition>(moveset, offsets->movesetBlock + table->hitCondition, table->hitConditionCount))
		{
			FROM_INDEX(hitCondition.requirements_addr, blockOffset + table->requirement, StructsT7::Requirement);
			FROM_INDEX(hitCondition.reactions_addr, blockOffset + table->reactions, StructsT7::Reactions);
		}

		// Convert pushback ptrs
		for (auto& pushback : StructIterator<StructsT7_gameAddr::Pushback>(moveset, offsets->movesetBlock + table->pushback, table->pushbackCount))
		{
			FROM_INDEX(pushback.extradata_addr, blockOffset + table->pushbackExtradata, StructsT7::PushbackExtradata);
		}

		// Convert move-start ptrs
		for (auto& moveBeginProp : StructIterator<StructsT7_gameAddr::OtherMoveProperty>(moveset, offsets->movesetBlock + table->moveBeginningProp, table->moveBeginningPropCount))
		{
			FROM_INDEX(moveBeginProp.requirements_addr, blockOffset + table->requirement, StructsT7::Requirement);
		}

		// Convert move-end prop ptrs
		for (auto& moveEndProp : StructIterator<StructsT7_gameAddr::OtherMoveProperty>(moveset, offsets->movesetBlock + table->moveEndingProp, table->moveEndingPropCount))
		{
			FROM_INDEX(moveEndProp.requirements_addr, blockOffset + table->requirement, StructsT7::Requirement);
		}
	}


	void CorrectMovesetInfoValues(StructsT7::MovesetInfo* info, gameAddr gameMoveset)
	{
		info->character_name_addr += gameMoveset;
		info->character_creator_addr += gameMoveset;
		info->date_addr += gameMoveset;
		info->fulldate_addr += gameMoveset;
	}


	void EnforceCurrentAliasesAsDefault(Byte* moveset)
	{
		StructsT7::MovesetInfo* infos = (StructsT7::MovesetInfo*)moveset;

		for (int i = 0; i < _countof(infos->current_aliases); ++i) {
			infos->orig_aliases[i] = infos->current_aliases[i];
		}
	}


	void EnforceDefaultAliasesAsCurrent(Byte* moveset)
	{
		StructsT7::MovesetInfo* infos = (StructsT7::MovesetInfo*)moveset;

		for (int i = 0; i < _countof(infos->current_aliases); ++i) {
			infos->current_aliases[i] = infos->orig_aliases[i];
		}
	}
}

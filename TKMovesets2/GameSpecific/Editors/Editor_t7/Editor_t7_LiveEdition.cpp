#include "Editor_t7.hpp"

#include "helpers.hpp"

// Live edition callbacks that will only be called if live edition is enabled, the moveset is loaded in memory AND if the modified field is valid

void EditorT7::Live_OnMoveEdit(int id, EditorInput* field)
{
	auto& name = field->name;

	if (name == "name") {
		return;
	}

	uint64_t blockStart = live_loadedMoveset + m_offsets->movesetBlock;
	gameAddr move = (uint64_t)m_infos->table.move + blockStart + id * sizeof(Move);
	union {
		uint64_t value_u64;
		int64_t value_s64;
		int32_t value_s32;
		int16_t value_s16;
		float value_float;
	};
	value_u64 = EditorUtils::GetFieldValue(field);

	if (name == "anim_name") {
		uint64_t animBlockStart = live_loadedMoveset + m_offsets->animationBlock;
		gameAddr animAddr = animBlockStart + m_animNameToOffsetMap[field->buffer];
		m_process.writeInt64(move + offsetof(Move, anim_addr), animAddr);
	}
	if (name == "vulnerability") {
		m_process.writeInt32(move + offsetof(Move, vuln), value_s32);
	}
	else if (name == "hitlevel") {
		m_process.writeInt32(move + offsetof(Move, hitlevel), value_s32);
	}
	else if (name == "transition") {
		m_process.writeInt16(move + offsetof(Move, transition), value_s16);
	}
	else if (name == "moveId_val1") {
		m_process.writeInt16(move + offsetof(Move, moveId_val1), value_s16);
	}
	else if (name == "moveId_val2") {
		m_process.writeInt16(move + offsetof(Move, moveId_val2), value_s16);
	}
	else if (name == "anim_len") {
		m_process.writeInt32(move + offsetof(Move, anim_len), value_s32);
	}
	else if (name == "airborne_start") {
		m_process.writeInt32(move + offsetof(Move, airborne_start), value_s32);
	}
	else if (name == "airborne_end") {
		m_process.writeInt32(move + offsetof(Move, airborne_end), value_s32);
	}
	else if (name == "ground_fall") {
		m_process.writeInt32(move + offsetof(Move, ground_fall), value_s32);
	}
	else if (name == "hitbox_location") {
		m_process.writeInt32(move + offsetof(Move, hitbox_location), value_s32);
	}
	else if (name == "first_active_frame") {
		m_process.writeInt32(move + offsetof(Move, first_active_frame), value_s32);
	}
	else if (name == "last_active_frame") {
		m_process.writeInt32(move + offsetof(Move, last_active_frame), value_s32);
	}
	else if (name == "distance") {
		m_process.writeInt16(move + offsetof(Move, distance), value_s16);
	}

	else if (name == "cancel_addr") {
		int id = atoi(field->buffer);
		gameAddr cancelAddr = blockStart + (uint64_t)m_infos->table.cancel + id * sizeof(Cancel);
		m_process.writeInt64(move + offsetof(Move, cancel_addr), cancelAddr);
	}
	else if (name == "hit_condition_addr") {
		int id = atoi(field->buffer);
		gameAddr hitConditionAddr = blockStart + (uint64_t)m_infos->table.hitCondition + id * sizeof(HitCondition);
		m_process.writeInt64(move + offsetof(Move, hit_condition_addr), hitConditionAddr);
	}
	else if (name == "extra_properties_addr") {
		int id = atoi(field->buffer);
		gameAddr propAddr = blockStart + (uint64_t)m_infos->table.extraMoveProperty + id * sizeof(ExtraMoveProperty);
		m_process.writeInt64(move + offsetof(Move, extra_move_property_addr), propAddr);
	}
	else if (name == "move_start_extraprop_addr") {
		int id = atoi(field->buffer);
		gameAddr propAddr = blockStart + (uint64_t)m_infos->table.moveBeginningProp + id * sizeof(OtherMoveProperty);
		m_process.writeInt64(move + offsetof(Move, move_start_extraprop_addr), propAddr);
	}
	else if (name == "move_end_extraprop_addr") {
		int id = atoi(field->buffer);
		gameAddr propAddr = blockStart + (uint64_t)m_infos->table.moveEndingProp + id * sizeof(OtherMoveProperty);
		m_process.writeInt64(move + offsetof(Move, move_end_extraprop_addr), propAddr);
	}
	else if (name == "voiceclip_addr") {
		int id = atoi(field->buffer);
		gameAddr voiceclipAddr = blockStart + (uint64_t)m_infos->table.voiceclip + id * sizeof(Voiceclip);
		m_process.writeInt64(move + offsetof(Move, voicelip_addr), voiceclipAddr);
	}

	else if (name == "cancel_addr_2") {
		int id = atoi(field->buffer);
		gameAddr cancelAddr = blockStart + (uint64_t)m_infos->table.cancel + id * sizeof(Cancel);
		m_process.writeInt64(move + offsetof(Move, _0x28_cancel_addr), cancelAddr);
	}
	else if (name == "cancel_addr_3") {
		int id = atoi(field->buffer);
		gameAddr cancelAddr = blockStart + (uint64_t)m_infos->table.cancel + id * sizeof(Cancel);
		m_process.writeInt64(move + offsetof(Move, _0x38_cancel_addr), cancelAddr);
	}
	else if (name == "cancel_addr_4") {
		int id = atoi(field->buffer);
		gameAddr cancelAddr = blockStart + (uint64_t)m_infos->table.cancel + id * sizeof(Cancel);
		m_process.writeInt64(move + offsetof(Move, _0x48_cancel_addr), cancelAddr);
	}

	else if (name == "_0x34_int") {
		m_process.writeInt32(move + offsetof(Move, _0x34_int), value_s32);
	}
	else if (name == "_0x44_int") {
		m_process.writeInt32(move + offsetof(Move, _0x44_int), value_s32);
	}
	else if (name == "_0x56_short") {
		m_process.writeInt16(move + offsetof(Move, _0x56_short), value_s16);
	}
	else if (name == "_0x5C_short") {
		m_process.writeInt16(move + offsetof(Move, _0x5C_short), value_s16);
	}
	else if (name == "_0x5E_short") {
		m_process.writeInt16(move + offsetof(Move, _0x5E_short), value_s16);
	}
	else if (name == "_0x98_int") {
		m_process.writeInt32(move + offsetof(Move, _0x98_int), value_s32);
	}
	else if (name == "_0xA8_short") {
		m_process.writeInt16(move + offsetof(Move, _0xA8_short), value_s16);
	}
	else if (name == "_0xAC_int") {
		m_process.writeInt32(move + offsetof(Move, _0xAC_int), value_s32);
	}
}

void EditorT7::Live_OnCancelEdit(int id, EditorInput* field)
{
	auto& name = field->name;
	auto& buffer = field->buffer;

	uint64_t blockStart = live_loadedMoveset + m_offsets->movesetBlock;
	gameAddr cancel = blockStart + (uint64_t)m_infos->table.cancel + id * sizeof(Cancel);
	union {
		uint64_t value_u64;
		int64_t value_s64;
		uint32_t value_u32;
		int32_t value_s32;
		uint16_t value_u16;
		int16_t value_s16;
		float value_float;
	};
	value_u64 = EditorUtils::GetFieldValue(field);

	if (name == "command") {
		m_process.writeInt64(cancel + offsetof(Cancel, command), value_s64);
	}
	else if (name == "requirements_addr") {
		gameAddr reqAddr = blockStart + (uint64_t)m_infos->table.requirement + value_s64 * sizeof(Requirement);
		m_process.writeInt64(cancel + offsetof(Cancel, requirements_addr), reqAddr);
	}
	else if (name == "extradata_addr") {
		gameAddr extradataAddr = blockStart + (uint64_t)m_infos->table.cancelExtradata + value_s64 * sizeof(CancelExtradata);
		m_process.writeInt64(cancel + offsetof(Cancel, extradata_addr), extradataAddr);
	}
	else if (name == "detection_start") {
		m_process.writeInt32(cancel + offsetof(Cancel, detection_start), value_s32);
	}
	else if (name == "detection_end") {
		m_process.writeInt32(cancel + offsetof(Cancel, detection_end), value_s32);
	}
	else if (name == "starting_frame") {
		m_process.writeInt32(cancel + offsetof(Cancel, starting_frame), value_s32);
	}
	else if (name == "move_id") {
		m_process.writeInt16(cancel + offsetof(Cancel, move_id), value_s16);
	}
	else if (name == "cancel_option") {
		m_process.writeInt16(cancel + offsetof(Cancel, cancel_option), value_s16);
	}
}

void EditorT7::Live_OnGroupedCancelEdit(int id, EditorInput* field)
{
	auto& name = field->name;
	auto& buffer = field->buffer;

	uint64_t blockStart = live_loadedMoveset + m_offsets->movesetBlock;
	gameAddr cancel = blockStart + (uint64_t)m_infos->table.groupCancel + id * sizeof(Cancel);
	union {
		uint64_t value_u64;
		int64_t value_s64;
		uint32_t value_u32;
		int32_t value_s32;
		uint16_t value_u16;
		int16_t value_s16;
		float value_float;
	};
	value_u64 = EditorUtils::GetFieldValue(field);

	if (name == "command") {
		m_process.writeInt64(cancel + offsetof(Cancel, command), value_s64);
	}
	else if (name == "requirements_addr") {
		gameAddr reqAddr = blockStart + (uint64_t)m_infos->table.requirement + value_s64 * sizeof(Requirement);
		m_process.writeInt64(cancel + offsetof(Cancel, requirements_addr), reqAddr);
	}
	else if (name == "extradata_addr") {
		gameAddr extradataAddr = blockStart + (uint64_t)m_infos->table.cancelExtradata + value_s64 * sizeof(CancelExtradata);
		m_process.writeInt64(cancel + offsetof(Cancel, extradata_addr), extradataAddr);
	}
	else if (name == "detection_start") {
		m_process.writeInt32(cancel + offsetof(Cancel, detection_start), value_s32);
	}
	else if (name == "detection_end") {
		m_process.writeInt32(cancel + offsetof(Cancel, detection_end), value_s32);
	}
	else if (name == "starting_frame") {
		m_process.writeInt32(cancel + offsetof(Cancel, starting_frame), value_s32);
	}
	else if (name == "move_id") {
		m_process.writeInt16(cancel + offsetof(Cancel, move_id), value_s16);
	}
	else if (name == "cancel_option") {
		m_process.writeInt16(cancel + offsetof(Cancel, cancel_option), value_s16);
	}
}

void EditorT7::Live_OnVoiceclipEdit(int id, EditorInput* field)
{
	auto& name = field->name;
	auto& buffer = field->buffer;

	uint64_t blockStart = live_loadedMoveset + m_offsets->movesetBlock;
	gameAddr voiceclip = blockStart + (uint64_t)m_infos->table.voiceclip + id * sizeof(Voiceclip);
	union {
		uint64_t value_u64;
		int64_t value_s64;
		uint32_t value_u32;
		int32_t value_s32;
		uint16_t value_u16;
		int16_t value_s16;
		float value_float;
	};
	value_u64 = EditorUtils::GetFieldValue(field);

	if (name == "id") {
		m_process.writeInt32(voiceclip + offsetof(Voiceclip, id), value_s32);
	}
}

void EditorT7::Live_OnCancelExtraEdit(int id, EditorInput* field)
{
	auto& name = field->name;
	auto& buffer = field->buffer;

	uint64_t blockStart = live_loadedMoveset + m_offsets->movesetBlock;
	gameAddr cancelExtra = blockStart + (uint64_t)m_infos->table.cancelExtradata + id * sizeof(CancelExtradata);
	union {
		uint64_t value_u64;
		int64_t value_s64;
		uint32_t value_u32;
		int32_t value_s32;
		uint16_t value_u16;
		int16_t value_s16;
		float value_float;
	};
	value_u64 = EditorUtils::GetFieldValue(field);

	if (name == "value") {
		m_process.writeInt32(cancelExtra + offsetof(CancelExtradata, value), value_s32);
	}
}

void EditorT7::Live_OnExtrapropertyEdit(int id, EditorInput* field)
{
	auto& name = field->name;
	auto& buffer = field->buffer;

	uint64_t blockStart = live_loadedMoveset + m_offsets->movesetBlock;
	gameAddr prop = blockStart + (uint64_t)m_infos->table.extraMoveProperty + id * sizeof(ExtraMoveProperty);
	union {
		uint64_t value_u64;
		int64_t value_s64;
		uint32_t value_u32;
		int32_t value_s32;
		uint16_t value_u16;
		int16_t value_s16;
		float value_float;
	};
	value_u64 = EditorUtils::GetFieldValue(field);

	if (name == "starting_frame") {
		m_process.writeInt32(prop + offsetof(ExtraMoveProperty, starting_frame), value_s32);
	}
	else if (name == "id") {
		m_process.writeInt32(prop + offsetof(ExtraMoveProperty, id), value_s32);
	}
	else
	{
		if (name == "value_hex") {
			m_process.writeInt32(prop + offsetof(ExtraMoveProperty, value_unsigned), value_s32);
		}
		else if (name == "value_unsigned") {
			m_process.writeInt32(prop + offsetof(ExtraMoveProperty, value_unsigned), value_s32);
		}
		else if (name == "value_signed") {
			m_process.writeInt32(prop + offsetof(ExtraMoveProperty, value_unsigned), value_s32);
		}
		else if (name == "value_float") {
			m_process.writeFloat(prop + offsetof(ExtraMoveProperty, value_unsigned), value_float);
		}
	}
}

void EditorT7::Live_OnMoveBeginPropEdit(int id, EditorInput* field)
{
	auto& name = field->name;
	auto& buffer = field->buffer;

	uint64_t blockStart = live_loadedMoveset + m_offsets->movesetBlock;
	gameAddr prop = blockStart + (uint64_t)m_infos->table.moveBeginningProp + id * sizeof(OtherMoveProperty);
	union {
		uint64_t value_u64;
		int64_t value_s64;
		uint32_t value_u32;
		int32_t value_s32;
		uint16_t value_u16;
		int16_t value_s16;
		float value_float;
	};
	value_u64 = EditorUtils::GetFieldValue(field);

	if (name == "requirements_addr") {
		gameAddr reqAddr = blockStart + (uint64_t)m_infos->table.requirement + value_s64 * sizeof(Requirement);
		m_process.writeInt64(prop + offsetof(OtherMoveProperty, requirements_addr), reqAddr);
	}
	else if (name == "extraprop") {
		m_process.writeInt32(prop + offsetof(OtherMoveProperty, extraprop), value_s32);
	}
	else if (name == "value") {
		m_process.writeInt32(prop + offsetof(OtherMoveProperty, value), value_s32);
	}
}

void EditorT7::Live_OnMoveEndPropEdit(int id, EditorInput* field)
{
	auto& name = field->name;
	auto& buffer = field->buffer;

	uint64_t blockStart = live_loadedMoveset + m_offsets->movesetBlock;
	gameAddr prop = blockStart + (uint64_t)m_infos->table.moveEndingProp + id * sizeof(OtherMoveProperty);
	union {
		uint64_t value_u64;
		int64_t value_s64;
		uint32_t value_u32;
		int32_t value_s32;
		uint16_t value_u16;
		int16_t value_s16;
		float value_float;
	};
	value_u64 = EditorUtils::GetFieldValue(field);

	if (name == "requirements_addr") {
		gameAddr reqAddr = blockStart + (uint64_t)m_infos->table.requirement + value_s64 * sizeof(Requirement);
		m_process.writeInt64(prop + offsetof(OtherMoveProperty, requirements_addr), reqAddr);
	}
	else if (name == "extraprop") {
		m_process.writeInt32(prop + offsetof(OtherMoveProperty, extraprop), value_s32);
	}
	else if (name == "value") {
		m_process.writeInt32(prop + offsetof(OtherMoveProperty, value), value_s32);
	}
}

void EditorT7::Live_OnRequirementEdit(int id, EditorInput* field)
{
	auto& name = field->name;
	auto& buffer = field->buffer;

	uint64_t blockStart = live_loadedMoveset + m_offsets->movesetBlock;
	gameAddr requirement = blockStart + (uint64_t)m_infos->table.requirement + id * sizeof(Requirement);
	union {
		uint64_t value_u64;
		int64_t value_s64;
		uint32_t value_u32;
		int32_t value_s32;
		uint16_t value_u16;
		int16_t value_s16;
		float value_float;
	};
	value_u64 = EditorUtils::GetFieldValue(field);

	if (name == "condition") {
		m_process.writeInt32(requirement + offsetof(Requirement, condition), value_s32);
	}
	else if (name == "param_unsigned") {
		m_process.writeInt32(requirement + offsetof(Requirement, param_unsigned), value_s32);
	}
	else if (name == "param_float") {
		m_process.writeFloat(requirement + offsetof(Requirement, param_float), value_float);
	}
}

void EditorT7::Live_OnHitConditionPropEdit(int id, EditorInput* field)
{
	auto& name = field->name;
	auto& buffer = field->buffer;

	uint64_t blockStart = live_loadedMoveset + m_offsets->movesetBlock;
	gameAddr hitCondition = blockStart + (uint64_t)m_infos->table.hitCondition + id * sizeof(HitCondition);
	union {
		uint64_t value_u64;
		int64_t value_s64;
		uint32_t value_u32;
		int32_t value_s32;
		uint16_t value_u16;
		int16_t value_s16;
		float value_float;
	};
	value_u64 = EditorUtils::GetFieldValue(field);

	if (name == "requirements_addr") {
		gameAddr reqAddr = blockStart + (uint64_t)m_infos->table.requirement + value_s64 * sizeof(Requirement);
		m_process.writeInt64(hitCondition + offsetof(HitCondition, requirements_addr), reqAddr);
	}
	else if (name == "reactions_addr") {
		gameAddr reactAddr = blockStart + (uint64_t)m_infos->table.reactions + value_s64 * sizeof(Reactions);
		m_process.writeInt64(hitCondition + offsetof(HitCondition, reactions_addr), reactAddr);
	}
	else if (name == "damage") {
		m_process.writeInt32(hitCondition + offsetof(HitCondition, damage), value_s32);
	}
	else if (name == "_0xC_int") {
		m_process.writeInt32(hitCondition + offsetof(HitCondition, _0xC_int), value_s32);
	}
}

void EditorT7::Live_OnReactionsEdit(int id, EditorInput* field)
{
	auto& name = field->name;
	auto& buffer = field->buffer;

	uint64_t blockStart = live_loadedMoveset + m_offsets->movesetBlock;
	gameAddr reactions = blockStart + (uint64_t)m_infos->table.reactions + id * sizeof(Reactions);
	union {
		uint64_t value_u64;
		int64_t value_s64;
		uint32_t value_u32;
		int32_t value_s32;
		uint16_t value_u16;
		int16_t value_s16;
		float value_float;
	};
	value_u64 = EditorUtils::GetFieldValue(field);

	//
	if (name == "front_pushback") {
		gameAddr pushbackAddr = blockStart + (uint64_t)m_infos->table.pushback + value_s64 * sizeof(Pushback);
		m_process.writeInt64(reactions + offsetof(Reactions, front_pushback), pushbackAddr);
	}
	else if (name == "backturned_pushback") {
		gameAddr pushbackAddr = blockStart + (uint64_t)m_infos->table.pushback + value_s64 * sizeof(Pushback);
		m_process.writeInt64(reactions + offsetof(Reactions, backturned_pushback), pushbackAddr);
	}
	else if (name == "left_side_pushback") {
		gameAddr pushbackAddr = blockStart + (uint64_t)m_infos->table.pushback + value_s64 * sizeof(Pushback);
		m_process.writeInt64(reactions + offsetof(Reactions, left_side_pushback), pushbackAddr);
	}
	else if (name == "right_side_pushback") {
		gameAddr pushbackAddr = blockStart + (uint64_t)m_infos->table.pushback + value_s64 * sizeof(Pushback);
		m_process.writeInt64(reactions + offsetof(Reactions, right_side_pushback), pushbackAddr);
	}
	else if (name == "front_counterhit_pushback") {
		gameAddr pushbackAddr = blockStart + (uint64_t)m_infos->table.pushback + value_s64 * sizeof(Pushback);
		m_process.writeInt64(reactions + offsetof(Reactions, front_counterhit_pushback), pushbackAddr);
	}
	else if (name == "downed_pushback") {
		gameAddr pushbackAddr = blockStart + (uint64_t)m_infos->table.pushback + value_s64 * sizeof(Pushback);
		m_process.writeInt64(reactions + offsetof(Reactions, downed_pushback), pushbackAddr);
	}
	else if (name == "block_pushback") {
		gameAddr pushbackAddr = blockStart + (uint64_t)m_infos->table.pushback + value_s64 * sizeof(Pushback);
		m_process.writeInt64(reactions + offsetof(Reactions, block_pushback), pushbackAddr);
	}
	//
	else if (name == "default_moveid") {
		m_process.writeInt16(reactions + offsetof(Reactions, default_moveid), value_s16);
	}
	else if (name == "standing_moveid") {
		m_process.writeInt16(reactions + offsetof(Reactions, standing_moveid), value_s16);
	}
	else if (name == "crouch_moveid") {
		m_process.writeInt16(reactions + offsetof(Reactions, crouch_moveid), value_s16);
	}
	else if (name == "counterhit_moveid") {
		m_process.writeInt16(reactions + offsetof(Reactions, counterhit_moveid), value_s16);
	}
	else if (name == "crouch_counterhit_moveid") {
		m_process.writeInt16(reactions + offsetof(Reactions, crouch_counterhit_moveid), value_s16);
	}
	else if (name == "left_side_moveid") {
		m_process.writeInt16(reactions + offsetof(Reactions, left_side_moveid), value_s16);
	}
	else if (name == "crouch_left_side_moveid") {
		m_process.writeInt16(reactions + offsetof(Reactions, crouch_left_side_moveid), value_s16);
	}
	else if (name == "right_side_moveid") {
		m_process.writeInt16(reactions + offsetof(Reactions, right_side_moveid), value_s16);
	}
	else if (name == "crouch_right_side_moveid") {
		m_process.writeInt16(reactions + offsetof(Reactions, crouch_right_side_moveid), value_s16);
	}
	else if (name == "backturned_moveid") {
		m_process.writeInt16(reactions + offsetof(Reactions, backturned_moveid), value_s16);
	}
	else if (name == "crouch_backturned_moveid") {
		m_process.writeInt16(reactions + offsetof(Reactions, crouch_backturned_moveid), value_s16);
	}
	else if (name == "block_moveid") {
		m_process.writeInt16(reactions + offsetof(Reactions, block_moveid), value_s16);
	}
	else if (name == "crouch_block_moveid") {
		m_process.writeInt16(reactions + offsetof(Reactions, crouch_block_moveid), value_s16);
	}
	else if (name == "wallslump_moveid") {
		m_process.writeInt16(reactions + offsetof(Reactions, wallslump_moveid), value_s16);
	}
	else if (name == "downed_moveid") {
		m_process.writeInt16(reactions + offsetof(Reactions, downed_moveid), value_s16);
	}
	//
	else if (name == "front_direction") {
		m_process.writeInt16(reactions + offsetof(Reactions, front_direction), value_s16);
	}
	else if (name == "back_direction") {
		m_process.writeInt16(reactions + offsetof(Reactions, back_direction), value_s16);
	}
	else if (name == "left_side_direction") {
		m_process.writeInt16(reactions + offsetof(Reactions, left_side_direction), value_s16);
	}
	else if (name == "right_side_direction") {
		m_process.writeInt16(reactions + offsetof(Reactions, right_side_direction), value_s16);
	}
	else if (name == "front_counterhit_direction") {
		m_process.writeInt16(reactions + offsetof(Reactions, front_counterhit_direction), value_s16);
	}
	else if (name == "downed_direction") {
		m_process.writeInt16(reactions + offsetof(Reactions, downed_direction), value_s16);
	}

	else if (name == "vertical_pushback") {
		m_process.writeInt16(reactions + offsetof(Reactions, vertical_pushback), value_s16);
	}
	else if (name == "_0x44_int") {
		m_process.writeInt32(reactions + offsetof(Reactions, _0x44_int), value_s32);
	}
	else if (name == "_0x48_int") {
		m_process.writeInt32(reactions + offsetof(Reactions, _0x48_int), value_s32);
	}
}

void EditorT7::Live_OnPushbackEdit(int id, EditorInput* field)
{
	auto& name = field->name;
	auto& buffer = field->buffer;

	uint64_t blockStart = live_loadedMoveset + m_offsets->movesetBlock;
	gameAddr pushback = blockStart + (uint64_t)m_infos->table.hitCondition + id * sizeof(Pushback);
	union {
		uint64_t value_u64;
		int64_t value_s64;
		uint32_t value_u32;
		int32_t value_s32;
		uint16_t value_u16;
		int16_t value_s16;
		float value_float;
	};
	value_u64 = EditorUtils::GetFieldValue(field);

	if (name == "duration") {
		m_process.writeInt16(pushback + offsetof(Pushback, duration), value_s16);
	}
	else if (name == "displacement") {
		m_process.writeInt16(pushback + offsetof(Pushback, displacement), value_s16);
	}
	else if (name == "num_of_loops") {
		m_process.writeInt32(pushback + offsetof(Pushback, num_of_loops), value_s32);
	}
	else if (name == "extradata_addr") {
		gameAddr extraAddr = blockStart + (uint64_t)m_infos->table.pushbackExtradata + value_s64 * sizeof(PushbackExtradata);
		m_process.writeInt64(pushback + offsetof(Pushback, extradata_addr), extraAddr);
	}
}


void EditorT7::Live_OnPushbackExtraEdit(int id, EditorInput* field)
{
	auto& name = field->name;
	auto& buffer = field->buffer;

	uint64_t blockStart = live_loadedMoveset + m_offsets->movesetBlock;
	gameAddr pushbackExtra = blockStart + (uint64_t)m_infos->table.pushbackExtradata + id * sizeof(PushbackExtradata);
	union {
		uint64_t value_u64;
		int64_t value_s64;
		uint32_t value_u32;
		int32_t value_s32;
		uint16_t value_u16;
		int16_t value_s16;
		float value_float;
	};
	value_u64 = EditorUtils::GetFieldValue(field);

	if (name == "horizontal_offset") {
		m_process.writeInt16(pushbackExtra + offsetof(PushbackExtradata, horizontal_offset), value_s16);
	}
}

void EditorT7::Live_OnInputSequenceEdit(int id, EditorInput* field)
{
	auto& name = field->name;
	auto& buffer = field->buffer;

	uint64_t blockStart = live_loadedMoveset + m_offsets->movesetBlock;
	gameAddr inputSequence = blockStart + (uint64_t)m_infos->table.inputSequence + id * sizeof(InputSequence);
	union {
		uint64_t value_u64;
		int64_t value_s64;
		uint32_t value_u32;
		int32_t value_s32;
		uint16_t value_u16;
		int16_t value_s16;
		float value_float;
	};
	value_u64 = EditorUtils::GetFieldValue(field);

	if (name == "reactions_addr") {
		gameAddr inputAddr = blockStart + (uint64_t)m_infos->table.input + value_s64 * sizeof(Input);
		m_process.writeInt64(inputSequence + offsetof(InputSequence, input_addr), inputAddr);
	}
	else if (name == "input_window_frames") {
		m_process.writeInt16(inputSequence + offsetof(InputSequence, input_window_frames), value_s16);
	}
	else if (name == "input_amount") {
		m_process.writeInt16(inputSequence + offsetof(InputSequence, input_amount), value_s16);
	}
	else if (name == "_0x4_int") {
		m_process.writeInt16(inputSequence + offsetof(InputSequence, _0x4_int), value_s16);
	}
}

void EditorT7::Live_OnInputEdit(int id, EditorInput* field)
{
	auto& name = field->name;
	auto& buffer = field->buffer;

	uint64_t blockStart = live_loadedMoveset + m_offsets->movesetBlock;
	gameAddr input = blockStart + (uint64_t)m_infos->table.input + id * sizeof(Input);
	union {
		uint64_t value_u64;
		int64_t value_s64;
		uint32_t value_u32;
		int32_t value_s32;
		uint16_t value_u16;
		int16_t value_s16;
		float value_float;
	};
	value_u64 = EditorUtils::GetFieldValue(field);

	if (name == "button") {
		m_process.writeInt32(input + offsetof(Input, button), value_s32);
	}
	else if (name == "direction")
	{
		m_process.writeInt32(input + offsetof(Input, direction), value_s32);
	}
}

void EditorT7::Live_OnThrowCameraEdit(int id, EditorInput* field)
{
	auto& name = field->name;
	auto& buffer = field->buffer;

	uint64_t blockStart = live_loadedMoveset + m_offsets->movesetBlock;
	gameAddr throwCamera = blockStart + (uint64_t)m_infos->table.throwCameras + id * sizeof(ThrowCamera);
	union {
		uint64_t value_u64;
		int64_t value_s64;
		uint32_t value_u32;
		int32_t value_s32;
		uint16_t value_u16;
		int16_t value_s16;
		float value_float;
	};
	value_u64 = EditorUtils::GetFieldValue(field);

	if (name == "_0x0_llong") {
		m_process.writeInt64(throwCamera + offsetof(ThrowCamera, _0x0_llong), value_s64);
	}
	else if (name == "cameradata_addr")
	{
		gameAddr cameraDataAddr = blockStart + (uint64_t)m_infos->table.cameraData + value_s64 * sizeof(CameraData);
		m_process.writeInt64(throwCamera + offsetof(ThrowCamera, cameradata_addr), cameraDataAddr);
	}
}

void EditorT7::Live_OnCameraDataEdit(int id, EditorInput* field)
{
	auto& name = field->name;
	auto& buffer = field->buffer;

	uint64_t blockStart = live_loadedMoveset + m_offsets->movesetBlock;
	gameAddr cameraData = blockStart + (uint64_t)m_infos->table.cameraData + id * sizeof(CameraData);
	union {
		uint64_t value_u64;
		int64_t value_s64;
		uint32_t value_u32;
		int32_t value_s32;
		uint16_t value_u16;
		int16_t value_s16;
		float value_float;
	};
	value_u64 = EditorUtils::GetFieldValue(field);

	if (name == "_0x0_int") {
		m_process.writeInt32(cameraData + offsetof(CameraData, _0x0_int), value_s32);
	}
	else if (name == "_0x4_short") {
		m_process.writeInt16(cameraData + offsetof(CameraData, _0x4_short), value_s16);
	}
	else if (name == "left_side_camera_data") {
		m_process.writeInt16(cameraData + offsetof(CameraData, left_side_camera_data), value_s16);
	}
	else if (name == "right_side_camera_data") {
		m_process.writeInt16(cameraData + offsetof(CameraData, right_side_camera_data), value_s16);
	}
	else if (name == "_0xA_short") {
		m_process.writeInt16(cameraData + offsetof(CameraData, _0xA_short), value_s16);
	}
}

void EditorT7::Live_OnProjectileEdit(int id, EditorInput* field)
{
	auto& name = field->name;
	auto& buffer = field->buffer;

	uint64_t blockStart = live_loadedMoveset + m_offsets->movesetBlock;
	uint64_t projectile = blockStart + (uint64_t)m_infos->table.projectile + id * sizeof(Projectile);
	union {
		uint64_t value_u64;
		int64_t value_s64;
		uint32_t value_u32;
		int32_t value_s32;
		uint16_t value_u16;
		int16_t value_s16;
		float value_float;
	};
	value_u64 = EditorUtils::GetFieldValue(field);

	if (name == "vfx_id") {
		m_process.writeInt32(projectile + offsetof(Projectile, vfx_id), value_s32);
	}
	else if (name == "vfx_variation_id") {
		m_process.writeInt32(projectile + offsetof(Projectile, vfx_variation_id), value_s32);
	}
	else if (name == "delay") {
		m_process.writeInt32(projectile + offsetof(Projectile, delay), value_s32);
	}
	else if (name == "vertical_velocity") {
		m_process.writeInt32(projectile + offsetof(Projectile, vertical_velocity), value_s32);
	}
	else if (name == "horizontal_velocity") {
		m_process.writeInt32(projectile + offsetof(Projectile, horizontal_velocity), value_s32);
	}
	else if (name == "duration") {
		m_process.writeInt32(projectile + offsetof(Projectile, duration), value_s32);
	}
	else if (name == "no_collision") {
		m_process.writeInt32(projectile + offsetof(Projectile, no_collision), value_s32);
	}
	else if (name == "size") {
		m_process.writeInt32(projectile + offsetof(Projectile, size), value_s32);
	}
	else if (name == "can_hitbox_connect") {
		m_process.writeInt32(projectile + offsetof(Projectile, can_hitbox_connect), value_s32);
	}
	else if (name == "gravity") {
		m_process.writeInt32(projectile + offsetof(Projectile, gravity), value_s32);
	}
	else if (name == "hit_level") {
		m_process.writeInt32(projectile + offsetof(Projectile, hit_level), value_s32);
	}
	else if (name == "voiceclip_on_hit") {
		m_process.writeInt32(projectile + offsetof(Projectile, voiceclip_on_hit), value_s32);
	}

	else if (name == "reactions_addr") {
		uint64_t hitConditionAddr = blockStart + (uint64_t)m_infos->table.hitCondition + value_s64 * sizeof(HitCondition);
		m_process.writeInt64(projectile + offsetof(Projectile, hit_condition_addr), hitConditionAddr);
	}
	else if (name == "reactions_addr") {
		uint64_t cancelAddr = blockStart + (uint64_t)m_infos->table.cancel + value_s64 * sizeof(Cancel);
		m_process.writeInt64(projectile + offsetof(Projectile, cancel_addr), cancelAddr);
	}

	else if (name == "_0x4_int") {
		m_process.writeInt32(projectile + offsetof(Projectile, _0x4_int), value_s32);
	}
	else if (name == "_0xC_int") {
		m_process.writeInt32(projectile + offsetof(Projectile, _0xC_int), value_s32);
	}
	else if (name == "_0x10_int") {
		m_process.writeInt32(projectile + offsetof(Projectile, _0x10_int), value_s32);
	}
	else if (name == "_0x14_int") {
		m_process.writeInt32(projectile + offsetof(Projectile, _0x14_int), value_s32);
	}
	else if (name == "_0x24_int") {
		m_process.writeInt32(projectile + offsetof(Projectile, _0x24_int), value_s32);
	}
	else if (name == "_0x34_int") {
		m_process.writeInt32(projectile + offsetof(Projectile, _0x34_int), value_s32);
	}
	else if (name == "_0x3C_int_1") {
		m_process.writeInt32(projectile + offsetof(Projectile, _0x3C_int[0]), value_s32);
	}
	else if (name == "_0x3C_int_2") {
		m_process.writeInt32(projectile + offsetof(Projectile, _0x3C_int[1]), value_s32);
	}
	else if (name == "_0x3C_int_3") {
		m_process.writeInt32(projectile + offsetof(Projectile, _0x3C_int[2]), value_s32);
	}
	else if (name == "_0x3C_int_4") {
		m_process.writeInt32(projectile + offsetof(Projectile, _0x3C_int[3]), value_s32);
	}
	else if (name == "_0x3C_int_5") {
		m_process.writeInt32(projectile + offsetof(Projectile, _0x3C_int[4]), value_s32);
	}
	else if (name == "_0x3C_int_6") {
		m_process.writeInt32(projectile + offsetof(Projectile, _0x3C_int[5]), value_s32);
	}
	else if (name == "_0x58_int") {
		m_process.writeInt32(projectile + offsetof(Projectile, _0x58_int), value_s32);
	}
	else if (name == "_0x5C_int") {
		m_process.writeInt32(projectile + offsetof(Projectile, _0x5C_int), value_s32);
	}
	else if (name == "_0x70_int") {
		m_process.writeInt32(projectile + offsetof(Projectile, _0x70_int), value_s32);
	}
	else if (name == "_0x74_int") {
		m_process.writeInt32(projectile + offsetof(Projectile, _0x74_int), value_s32);
	}
	else if (name == "_0x7C_int") {
		m_process.writeInt32(projectile + offsetof(Projectile, _0x7C_int), value_s32);
	}
	else if (name == "_0x80_int") {
		m_process.writeInt32(projectile + offsetof(Projectile, _0x80_int), value_s32);
	}
	else if (name == "_0x88_int_1") {
		m_process.writeInt32(projectile + offsetof(Projectile, _0x88_int[0]), value_s32);
	}
	else if (name == "_0x88_int_2") {
		m_process.writeInt32(projectile + offsetof(Projectile, _0x88_int[1]), value_s32);
	}
	else if (name == "_0x88_int_3") {
		m_process.writeInt32(projectile + offsetof(Projectile, _0x88_int[2]), value_s32);
	}
	else if (name == "_0x88_int_4") {
		m_process.writeInt32(projectile + offsetof(Projectile, _0x88_int[3]), value_s32);
	}
	else if (name == "_0x88_int_5") {
		m_process.writeInt32(projectile + offsetof(Projectile, _0x88_int[4]), value_s32);
	}
	else if (name == "_0x88_int_6") {
		m_process.writeInt32(projectile + offsetof(Projectile, _0x88_int[5]), value_s32);
	}
	else if (name == "_0x88_int_7") {
		m_process.writeInt32(projectile + offsetof(Projectile, _0x88_int[6]), value_s32);
	}
	else if (name == "_0x88_int_8") {
		m_process.writeInt32(projectile + offsetof(Projectile, _0x88_int[7]), value_s32);
	}
}

void EditorT7::Live_OnFieldEdit(EditorWindowType type, int id, EditorInput* field)
{
	if (live_loadedMoveset == 0) {
		return;
	}
	DEBUG_LOG("Applying live edit: window type %d, item id %d, field name '%s', buffer [%s]\n", type, id, field->name.c_str(), field->buffer);

	switch (type)
	{
	case TEditorWindowType_Move:
		Live_OnMoveEdit(id, field);
		break;
	case TEditorWindowType_Voiceclip:
		Live_OnVoiceclipEdit(id, field);
		break;

	case TEditorWindowType_Cancel:
		Live_OnCancelEdit(id, field);
		break;
	case TEditorWindowType_CancelExtradata:
		Live_OnCancelExtraEdit(id, field);
		break;
	case TEditorWindowType_GroupedCancel:
		Live_OnGroupedCancelEdit(id, field);
		break;
	case TEditorWindowType_Extraproperty:
		Live_OnExtrapropertyEdit(id, field);
		break;
	case TEditorWindowType_MoveBeginProperty:
		Live_OnMoveBeginPropEdit(id, field);
		break;
	case TEditorWindowType_MoveEndProperty:
		Live_OnMoveEndPropEdit(id, field);
		break;

	case TEditorWindowType_Requirement:
		Live_OnRequirementEdit(id, field);
		break;

	case TEditorWindowType_HitCondition:
		Live_OnHitConditionPropEdit(id, field);
		break;
	case TEditorWindowType_Reactions:
		Live_OnReactionsEdit(id, field);
		break;
	case TEditorWindowType_Pushback:
		Live_OnPushbackEdit(id, field);
		break;
	case TEditorWindowType_PushbackExtradata:
		Live_OnPushbackExtraEdit(id, field);
		break;

	case TEditorWindowType_InputSequence:
		Live_OnInputSequenceEdit(id, field);
		break;
	case TEditorWindowType_Input:
		Live_OnInputEdit(id, field);
		break;

	case TEditorWindowType_Projectile:
		Live_OnProjectileEdit(id, field);
		break;

	case TEditorWindowType_CameraData:
		Live_OnCameraDataEdit(id, field);
		break;
	case TEditorWindowType_ThrowCamera:
		Live_OnThrowCameraEdit(id, field);
		break;


	case TEditorWindowType_MovelistDisplayable:
		Live_OnMovelistDisplayableEdit(id, field);
		break;
	case TEditorWindowType_MovelistPlayable:
		Live_OnMovelistPlayableEdit(id, field);
		break;
	case TEditorWindowType_MovelistInput:
		Live_OnMovelistInputEdit(id, field);
		break;
	}
	return;
}
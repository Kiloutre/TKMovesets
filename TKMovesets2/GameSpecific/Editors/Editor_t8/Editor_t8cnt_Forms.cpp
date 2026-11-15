#include "Editor_t8cnt.hpp"

#include <type_traits>
#include <stdexcept>
using namespace EditorUtils;

#define CREATE_FIELD(k, c, f, v) CreateField<decltype(v)>(k, drawOrder, inputMap, c, f, v)

// The list walkers below never read past their table, even when a list misses its terminator or starts out of range
template<class Table> static void RequireListRange(const Table& table, uint64_t id, uint64_t count = 1)
{
	if (id >= table.size() || count > table.size() - id) {
		throw std::out_of_range("Editor list exceeds its table.");
	}
}

// ===== Pushback Extra ===== //

std::vector<InputMap> EditorT8CNT::GetPushbackExtraListInputs(uint16_t id, int listSize, VectorSet<std::string>& drawOrder)
{
	std::vector<InputMap> inputListMap;

	RequireListRange(m_iterators.pushback_extras, id, listSize > 0 ? listSize : 1);
	auto pushbackExtra = m_iterators.pushback_extras.begin() + id;

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	do
	{
		InputMap inputMap;

		CREATE_FIELD("horizontal_offset", 0, EditorInput_U16_Changeable, pushbackExtra->horizontal_offset);

		WriteFieldFullname(inputMap, "pushback_extradata");
		inputListMap.push_back(std::move(inputMap));
		pushbackExtra++;
	} while (--listSize > 0);

	return inputListMap;
}

void EditorT8CNT::SavePushbackExtra(uint16_t id, InputMap& inputs)
{
	auto pushbackExtra = m_iterators.pushback_extras[id];

	SetMemberValue(&pushbackExtra->horizontal_offset, inputs.at("horizontal_offset"));
}

// ===== Pushback ===== //

InputMap EditorT8CNT::GetPushbackInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	InputMap inputMap;

	auto pushback = m_iterators.pushbacks[id];

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	CREATE_FIELD("duration", 0, EditorInput_U16, pushback->duration);
	CREATE_FIELD("displacement", 0, EditorInput_U16_Changeable, pushback->displacement);
	CREATE_FIELD("num_of_loops", 0, EditorInput_U32, pushback->offsets_count);
	CREATE_FIELD("extradata_addr", 0, EditorInput_PTR, pushback->extradata_addr);

	WriteFieldFullname(inputMap, "pushback");
	return inputMap;
}

bool EditorT8CNT::ValidatePushbackField(EditorInput* field)
{
	auto& name = field->name;

	if (name == "extradata_addr")
	{
		int listIdx = atoi(field->buffer);
		// No negative allowed here
		return 0 <= listIdx && listIdx < (int)m_infos->table.pushbackExtradataCount;
	}

	return true;
}

void EditorT8CNT::SavePushback(uint16_t id, InputMap& inputs)
{
	auto pushback = m_iterators.pushbacks[id];

	SetMemberValue(&pushback->duration, inputs.at("duration"));
	SetMemberValue(&pushback->displacement, inputs.at("displacement"));
	SetMemberValue(&pushback->offsets_count, inputs.at("num_of_loops"));
	SetMemberValue(&pushback->extradata_addr, inputs.at("extradata_addr"));
}

// ===== Projectile ===== //

InputMap EditorT8CNT::GetProjectileInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	InputMap inputMap;

	auto projectile = m_iterators.projectiles[id];

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	CREATE_FIELD("vfx_id", 0, EditorInput_U32, projectile->vfx_id);
	CREATE_FIELD("vfx_variation_id", 0, EditorInput_U32, projectile->vfx_variation_id);
	CREATE_FIELD("delay", 0, EditorInput_U32, projectile->delay);
	CREATE_FIELD("vertical_velocity", 0, EditorInput_U32, projectile->vertical_velocity);
	CREATE_FIELD("horizontal_velocity", 0, EditorInput_U32, projectile->horizontal_velocity);
	CREATE_FIELD("duration", 0, EditorInput_U32, projectile->duration);
	CREATE_FIELD("no_collision", 0, EditorInput_U32, projectile->no_collision);
	CREATE_FIELD("size", 0, EditorInput_U32, projectile->size);
	CREATE_FIELD("can_hitbox_connect", 0, EditorInput_U32, projectile->can_hitbox_connect);
	CREATE_FIELD("gravity", 0, EditorInput_U32, projectile->gravity);
	CREATE_FIELD("hit_level", 0, EditorInput_H32, projectile->hit_level);
	CREATE_FIELD("voiceclip_on_hit", 0, EditorInput_H32, projectile->voiceclip_on_hit);

	CREATE_FIELD("hit_condition_addr", 2, EditorInput_PTR, projectile->hit_condition_addr);
	CREATE_FIELD("cancel_addr", 2, EditorInput_PTR, projectile->cancel_addr);

	CREATE_FIELD("_0x4_int", 3, EditorInput_U32_Changeable, projectile->_0x4_int);
	CREATE_FIELD("_0xC_int", 3, EditorInput_U32_Changeable, projectile->_0xC_int);
	CREATE_FIELD("_0x10_int", 3, EditorInput_U32_Changeable, projectile->_0x10_int);
	CREATE_FIELD("_0x14_int", 3, EditorInput_U32_Changeable, projectile->_0x14_int);
	CREATE_FIELD("_0x24_int", 3, EditorInput_U32_Changeable, projectile->_0x24_int);
	CREATE_FIELD("_0x34_int", 3, EditorInput_U32_Changeable, projectile->_0x34_int);
	CREATE_FIELD("_0x3C_int_1", 3, EditorInput_U32_Changeable, projectile->_0x3C_int[0]);
	CREATE_FIELD("_0x3C_int_2", 3, EditorInput_U32_Changeable, projectile->_0x3C_int[1]);
	CREATE_FIELD("_0x3C_int_3", 3, EditorInput_U32_Changeable, projectile->_0x3C_int[2]);
	CREATE_FIELD("_0x3C_int_4", 3, EditorInput_U32_Changeable, projectile->_0x3C_int[3]);
	CREATE_FIELD("_0x3C_int_5", 3, EditorInput_U32_Changeable, projectile->_0x3C_int[4]);
	CREATE_FIELD("_0x3C_int_6", 3, EditorInput_U32_Changeable, projectile->_0x3C_int[5]);
	CREATE_FIELD("_0x58_int", 3, EditorInput_U32_Changeable, projectile->_0x58_int);
	CREATE_FIELD("_0x5C_int", 3, EditorInput_U32_Changeable, projectile->_0x5C_int);
	CREATE_FIELD("_0x70_int", 3, EditorInput_U32_Changeable, projectile->_0x70_int);
	CREATE_FIELD("_0x74_int", 3, EditorInput_U32_Changeable, projectile->_0x74_int);
	CREATE_FIELD("_0x7C_int", 3, EditorInput_U32_Changeable, projectile->_0x7C_int);
	CREATE_FIELD("_0x80_int", 3, EditorInput_U32_Changeable, projectile->_0x80_int);
	CREATE_FIELD("_0x88_int_1", 3, EditorInput_U32_Changeable, projectile->_0x88_int[0]);
	CREATE_FIELD("_0x88_int_2", 3, EditorInput_U32_Changeable, projectile->_0x88_int[1]);
	CREATE_FIELD("_0x88_int_3", 3, EditorInput_U32_Changeable, projectile->_0x88_int[2]);
	CREATE_FIELD("_0x88_int_4", 3, EditorInput_U32_Changeable, projectile->_0x88_int[3]);
	CREATE_FIELD("_0x88_int_5", 3, EditorInput_U32_Changeable, projectile->_0x88_int[4]);
	CREATE_FIELD("_0x88_int_6", 3, EditorInput_U32_Changeable, projectile->_0x88_int[5]);
	CREATE_FIELD("_0x88_int_7", 3, EditorInput_U32_Changeable, projectile->_0x88_int[6]);
	CREATE_FIELD("_0x88_int_8", 3, EditorInput_U32_Changeable, projectile->_0x88_int[7]);

	WriteFieldFullname(inputMap, "projectile");
	return inputMap;
}

bool EditorT8CNT::ValidateProjectileField(EditorInput* field)
{
	auto& name = field->name;

	if (name == "hit_condition_addr")
	{
		int listIdx = atoi(field->buffer);
		// No negative allowed here
		return 0 <= listIdx && listIdx < (int)m_infos->table.hitConditionCount;
	}
	else if (name == "cancel_addr")
	{
		int listIdx = atoi(field->buffer);
		// No negative allowed here
		return 0 <= listIdx && listIdx < (int)m_infos->table.cancelCount;
	}

	return true;
}

void EditorT8CNT::SaveProjectile(uint16_t id, InputMap& inputs)
{
	auto projectile = m_iterators.projectiles[id];

	SetMemberValue(&projectile->vfx_id, inputs.at("vfx_id"));
	SetMemberValue(&projectile->vfx_variation_id, inputs.at("vfx_variation_id"));
	SetMemberValue(&projectile->delay, inputs.at("delay"));
	SetMemberValue(&projectile->vertical_velocity, inputs.at("vertical_velocity"));
	SetMemberValue(&projectile->horizontal_velocity, inputs.at("horizontal_velocity"));
	SetMemberValue(&projectile->duration, inputs.at("duration"));
	SetMemberValue(&projectile->no_collision, inputs.at("no_collision"));
	SetMemberValue(&projectile->size, inputs.at("size"));
	SetMemberValue(&projectile->hit_level, inputs.at("hit_level"));
	SetMemberValue(&projectile->voiceclip_on_hit, inputs.at("voiceclip_on_hit"));
	SetMemberValue(&projectile->can_hitbox_connect, inputs.at("can_hitbox_connect"));
	SetMemberValue(&projectile->gravity, inputs.at("gravity"));

	SetMemberValue(&projectile->hit_condition_addr, inputs.at("hit_condition_addr"));
	SetMemberValue(&projectile->cancel_addr, inputs.at("cancel_addr"));

	SetMemberValue(&projectile->_0x34_int, inputs.at("_0x34_int"));
	SetMemberValue(&projectile->_0x4_int, inputs.at("_0x4_int"));
	SetMemberValue(&projectile->_0xC_int, inputs.at("_0xC_int"));
	SetMemberValue(&projectile->_0x10_int, inputs.at("_0x10_int"));
	SetMemberValue(&projectile->_0x14_int, inputs.at("_0x14_int"));
	SetMemberValue(&projectile->_0x24_int, inputs.at("_0x24_int"));
	SetMemberValue(&projectile->_0x3C_int[0], inputs.at("_0x3C_int_1"));
	SetMemberValue(&projectile->_0x3C_int[1], inputs.at("_0x3C_int_2"));
	SetMemberValue(&projectile->_0x3C_int[2], inputs.at("_0x3C_int_3"));
	SetMemberValue(&projectile->_0x3C_int[3], inputs.at("_0x3C_int_4"));
	SetMemberValue(&projectile->_0x3C_int[4], inputs.at("_0x3C_int_5"));
	SetMemberValue(&projectile->_0x3C_int[5], inputs.at("_0x3C_int_6"));
	SetMemberValue(&projectile->_0x58_int, inputs.at("_0x58_int"));
	SetMemberValue(&projectile->_0x5C_int, inputs.at("_0x5C_int"));
	SetMemberValue(&projectile->_0x70_int, inputs.at("_0x70_int"));
	SetMemberValue(&projectile->_0x74_int, inputs.at("_0x74_int"));
	SetMemberValue(&projectile->_0x7C_int, inputs.at("_0x7C_int"));
	SetMemberValue(&projectile->_0x80_int, inputs.at("_0x80_int"));
	SetMemberValue(&projectile->_0x88_int[0], inputs.at("_0x88_int_1"));
	SetMemberValue(&projectile->_0x88_int[1], inputs.at("_0x88_int_2"));
	SetMemberValue(&projectile->_0x88_int[2], inputs.at("_0x88_int_3"));
	SetMemberValue(&projectile->_0x88_int[3], inputs.at("_0x88_int_4"));
	SetMemberValue(&projectile->_0x88_int[4], inputs.at("_0x88_int_5"));
	SetMemberValue(&projectile->_0x88_int[5], inputs.at("_0x88_int_6"));
	SetMemberValue(&projectile->_0x88_int[6], inputs.at("_0x88_int_7"));
	SetMemberValue(&projectile->_0x88_int[7], inputs.at("_0x88_int_8"));
}

// ===== Input Sequence  ===== //

InputMap EditorT8CNT::GetInputSequenceInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	InputMap inputMap;

	auto sequence = m_iterators.input_sequences[id];

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	CREATE_FIELD("input_addr", 0, EditorInput_PTR, sequence->input_addr);
	CREATE_FIELD("_0x4_int", 0, EditorInput_U32, sequence->_0x4_int);
	CREATE_FIELD("input_amount", 0, EditorInput_U16, sequence->input_amount);
	CREATE_FIELD("input_window_frames", 0, EditorInput_U16, sequence->input_window_frames);

	WriteFieldFullname(inputMap, "input_sequence");
	return inputMap;
}

bool EditorT8CNT::ValidateInputSequenceField(EditorInput* field)
{
	auto& name = field->name;

	if (name == "input_addr")
	{
		int listIdx = atoi(field->buffer);
		// No negative allowed here
		return 0 <= listIdx && listIdx < (int)m_infos->table.inputCount;
	}

	return true;
}

void EditorT8CNT::SaveInputSequence(uint16_t id, InputMap& inputs)
{
	auto sequence = m_iterators.input_sequences[id];

	SetMemberValue(&sequence->input_window_frames, inputs.at("input_window_frames"));
	SetMemberValue(&sequence->input_amount, inputs.at("input_amount"));
	SetMemberValue(&sequence->_0x4_int, inputs.at("_0x4_int"));
	SetMemberValue(&sequence->input_addr, inputs.at("input_addr"));
}

// ===== Inputs ===== //

std::vector<InputMap> EditorT8CNT::GetInputListInputs(uint16_t id, int listSize, VectorSet<std::string>& drawOrder)
{
	std::vector<InputMap> inputListMap;

	RequireListRange(m_iterators.inputs, id, listSize > 0 ? listSize : 1);
	auto input = m_iterators.inputs.begin() + id;

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	do
	{
		InputMap inputMap;

		CREATE_FIELD("direction", 0, EditorInput_H32, input->direction);
		CREATE_FIELD("button", 0, EditorInput_H32, input->button);

		WriteFieldFullname(inputMap, "input");
		inputListMap.push_back(std::move(inputMap));
		input++;
	} while (--listSize > 0);

	return inputListMap;
}

void EditorT8CNT::SaveInput(uint16_t id, InputMap& inputs)
{
	auto input = m_iterators.inputs.begin() + id;

	SetMemberValue(&input->direction, inputs.at("direction"));
	SetMemberValue(&input->button, inputs.at("button"));
}

// ===== Throw cameras ===== //

InputMap EditorT8CNT::GetThrowCameraInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	InputMap inputMap;

	auto throwCamera = m_iterators.throw_datas[id];

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	CREATE_FIELD("side", 0, EditorInput_U64_Changeable, throwCamera->side);
	CREATE_FIELD("cameradata_addr", 0, EditorInput_PTR, throwCamera->cameradata_addr);

	WriteFieldFullname(inputMap, "throw_camera");
	return inputMap;
}

bool EditorT8CNT::ValidateThrowCameraField(EditorInput* field)
{
	auto& name = field->name;

	if (name == "cameradata_addr")
	{
		int listIdx = atoi(field->buffer);
		// No negative allowed here
		return 0 <= listIdx && listIdx < (int)m_infos->table.cameraDataCount;
	}

	return true;
}

void EditorT8CNT::SaveThrowCamera(uint16_t id, InputMap& inputs)
{
	auto throwCamera = m_iterators.throw_datas[id];

	SetMemberValue(&throwCamera->side, inputs.at("side"));
	SetMemberValue(&throwCamera->cameradata_addr, inputs.at("cameradata_addr"));
}

// ===== Camera data ===== //

InputMap EditorT8CNT::GetCameraDataInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	InputMap inputMap;

	auto cameraData = m_iterators.camera_datas[id];

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	CREATE_FIELD("pick_probability", 0, EditorInput_U32_Changeable, cameraData->pick_probability);
	CREATE_FIELD("camera_type", 0, EditorInput_U16_Changeable, cameraData->camera_type);
	CREATE_FIELD("left_side_camera_data", 0, EditorInput_U16_Changeable, cameraData->left_side_camera_data);
	CREATE_FIELD("right_side_camera_data", 0, EditorInput_U16_Changeable, cameraData->right_side_camera_data);
	CREATE_FIELD("additional_rotation", 0, EditorInput_U16_Changeable, cameraData->additional_rotation);

	WriteFieldFullname(inputMap, "camera_data");
	return inputMap;
}


void EditorT8CNT::SaveCameraData(uint16_t id, InputMap& inputs)
{
	auto cameraData = m_iterators.camera_datas[id];

	SetMemberValue(&cameraData->pick_probability, inputs.at("pick_probability"));
	SetMemberValue(&cameraData->camera_type, inputs.at("camera_type"));
	SetMemberValue(&cameraData->left_side_camera_data, inputs.at("left_side_camera_data"));
	SetMemberValue(&cameraData->right_side_camera_data, inputs.at("right_side_camera_data"));
	SetMemberValue(&cameraData->additional_rotation, inputs.at("additional_rotation"));
}


// ===== Reactions ===== //

InputMap EditorT8CNT::GetReactionsInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	InputMap inputMap;

	auto reaction = m_iterators.reactions[id];

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	CREATE_FIELD("front_pushback", 1, EditorInput_PTR, reaction->front_pushback);
	CREATE_FIELD("backturned_pushback", 1, EditorInput_PTR, reaction->backturned_pushback);
	CREATE_FIELD("left_side_pushback", 1, EditorInput_PTR, reaction->left_side_pushback);
	CREATE_FIELD("right_side_pushback", 1, EditorInput_PTR, reaction->right_side_pushback);
	CREATE_FIELD("front_counterhit_pushback", 1, EditorInput_PTR, reaction->front_counterhit_pushback);
	CREATE_FIELD("downed_pushback", 1, EditorInput_PTR, reaction->downed_pushback);
	CREATE_FIELD("block_pushback", 1, EditorInput_PTR, reaction->block_pushback);

	CREATE_FIELD("standing_moveid", 3, EditorInput_U16 | EditorInput_Clickable, reaction->standing_moveid);
	CREATE_FIELD("crouch_moveid", 3, EditorInput_U16 | EditorInput_Clickable, reaction->crouch_moveid);
	CREATE_FIELD("counterhit_moveid", 3, EditorInput_U16 | EditorInput_Clickable, reaction->counterhit_moveid);
	CREATE_FIELD("crouch_counterhit_moveid", 3, EditorInput_U16 | EditorInput_Clickable, reaction->crouch_counterhit_moveid);
	CREATE_FIELD("left_side_moveid", 3, EditorInput_U16 | EditorInput_Clickable, reaction->left_side_moveid);
	CREATE_FIELD("crouch_left_side_moveid", 3, EditorInput_U16 | EditorInput_Clickable, reaction->crouch_left_side_moveid);
	CREATE_FIELD("right_side_moveid", 3, EditorInput_U16 | EditorInput_Clickable, reaction->right_side_moveid);
	CREATE_FIELD("crouch_right_side_moveid", 3, EditorInput_U16 | EditorInput_Clickable, reaction->crouch_right_side_moveid);
	CREATE_FIELD("backturned_moveid", 3, EditorInput_U16 | EditorInput_Clickable, reaction->backturned_moveid);
	CREATE_FIELD("crouch_backturned_moveid", 3, EditorInput_U16 | EditorInput_Clickable, reaction->crouch_backturned_moveid);
	CREATE_FIELD("block_moveid", 3, EditorInput_U16 | EditorInput_Clickable, reaction->block_moveid);
	CREATE_FIELD("crouch_block_moveid", 3, EditorInput_U16 | EditorInput_Clickable, reaction->crouch_block_moveid);
	CREATE_FIELD("wallslump_moveid", 3, EditorInput_U16 | EditorInput_Clickable, reaction->wallslump_moveid);
	CREATE_FIELD("downed_moveid", 3, EditorInput_U16 | EditorInput_Clickable, reaction->downed_moveid);

	CREATE_FIELD("front_direction", 5, EditorInput_U16_Changeable, reaction->front_direction);
	CREATE_FIELD("back_direction", 5, EditorInput_U16_Changeable, reaction->back_direction);
	CREATE_FIELD("left_side_direction", 5, EditorInput_U16_Changeable, reaction->left_side_direction);
	CREATE_FIELD("right_side_direction", 5, EditorInput_U16_Changeable, reaction->right_side_direction);
	CREATE_FIELD("front_counterhit_direction", 5, EditorInput_U16_Changeable, reaction->front_counterhit_direction);
	CREATE_FIELD("downed_direction", 5, EditorInput_U16_Changeable, reaction->downed_direction);

	CREATE_FIELD("vertical_pushback", 7, EditorInput_U16_Changeable, reaction->vertical_pushback);

	WriteFieldFullname(inputMap, "reactions");
	return inputMap;
}

bool EditorT8CNT::ValidateReactionsField(EditorInput* field)
{
	auto& name = field->name;

	if (name.endsWith("_moveid"))
	{
		int moveId = atoi(field->buffer);
		if (moveId >= m_infos->table.moveCount) {
			if (moveId < 0x8000) {
				return false;
			}
			if (moveId >= (0x8000 + aliases.size())) {
				return false;
			}
		}
		else if (moveId < 0) {
			return false;
		}
	}
	else if (name.endsWith("_pushback"))
	{
		int listIdx = atoi(field->buffer);
		// No negative allowed here
		return 0 <= listIdx && listIdx < (int)m_infos->table.pushbackCount;
	}

	return true;
}

void EditorT8CNT::SaveReactions(uint16_t id, InputMap& inputs)
{
	auto reaction = m_iterators.reactions[id];

	SetMemberValue(&reaction->front_pushback, inputs.at("front_pushback"));
	SetMemberValue(&reaction->backturned_pushback, inputs.at("backturned_pushback"));
	SetMemberValue(&reaction->left_side_pushback, inputs.at("left_side_pushback"));
	SetMemberValue(&reaction->right_side_pushback, inputs.at("right_side_pushback"));
	SetMemberValue(&reaction->front_counterhit_pushback, inputs.at("front_counterhit_pushback"));
	SetMemberValue(&reaction->downed_pushback, inputs.at("downed_pushback"));
	SetMemberValue(&reaction->block_pushback, inputs.at("block_pushback"));

	SetMemberValue(&reaction->front_direction, inputs.at("front_direction"));
	SetMemberValue(&reaction->back_direction, inputs.at("back_direction"));
	SetMemberValue(&reaction->left_side_direction, inputs.at("left_side_direction"));
	SetMemberValue(&reaction->right_side_direction, inputs.at("right_side_direction"));
	SetMemberValue(&reaction->front_counterhit_direction, inputs.at("front_counterhit_direction"));
	SetMemberValue(&reaction->downed_direction, inputs.at("downed_direction"));

	SetMemberValue(&reaction->standing_moveid, inputs.at("standing_moveid"));
	SetMemberValue(&reaction->crouch_moveid, inputs.at("crouch_moveid"));
	SetMemberValue(&reaction->counterhit_moveid, inputs.at("counterhit_moveid"));
	SetMemberValue(&reaction->crouch_counterhit_moveid, inputs.at("crouch_counterhit_moveid"));
	SetMemberValue(&reaction->left_side_moveid, inputs.at("left_side_moveid"));
	SetMemberValue(&reaction->crouch_left_side_moveid, inputs.at("crouch_left_side_moveid"));
	SetMemberValue(&reaction->right_side_moveid, inputs.at("right_side_moveid"));
	SetMemberValue(&reaction->crouch_right_side_moveid, inputs.at("crouch_right_side_moveid"));
	SetMemberValue(&reaction->backturned_moveid, inputs.at("backturned_moveid"));
	SetMemberValue(&reaction->crouch_backturned_moveid, inputs.at("crouch_backturned_moveid"));
	SetMemberValue(&reaction->block_moveid, inputs.at("block_moveid"));
	SetMemberValue(&reaction->crouch_block_moveid, inputs.at("crouch_block_moveid"));
	SetMemberValue(&reaction->wallslump_moveid, inputs.at("wallslump_moveid"));
	SetMemberValue(&reaction->downed_moveid, inputs.at("downed_moveid"));

	SetMemberValue(&reaction->vertical_pushback, inputs.at("vertical_pushback"));
}

// ===== Hit conditions ===== //

std::vector<InputMap> EditorT8CNT::GetHitConditionListInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::vector<InputMap> inputListMap;

	RequireListRange(m_iterators.hit_conditions, id);
	uint64_t remaining = m_iterators.hit_conditions.size() - id;
	auto hitCondition = m_iterators.hit_conditions.begin() + id;
	auto req = m_iterators.requirements.begin();

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	do
	{
		InputMap inputMap;

		CREATE_FIELD("requirements_addr", 0, EditorInput_PTR, hitCondition->requirements_addr);
		CREATE_FIELD("damage", 0, EditorInput_U32, hitCondition->damage);
		CREATE_FIELD("_0xC_int", 0, EditorInput_U32_Changeable, hitCondition->_0xC_int);
		CREATE_FIELD("reactions_addr", 0, EditorInput_PTR, hitCondition->reactions_addr);

		WriteFieldFullname(inputMap, "hit_condition");
		inputListMap.push_back(std::move(inputMap));

		// An out-of-range requirement list or the end of the table also ends the list
		const uint64_t requirementId = (uint64_t)hitCondition->requirements_addr;
		if (requirementId >= m_iterators.requirements.size() ||
			req[(unsigned int)requirementId].condition == constants[EditorConstants_RequirementEnd]) {
			break;
		}
		if (--remaining == 0) {
			break;
		}

		hitCondition++;
	} while (true);

	return inputListMap;
}

void EditorT8CNT::SaveHitCondition(uint16_t id, InputMap& inputs)
{
	auto hitCondition = m_iterators.hit_conditions[id];

	SetMemberValue(&hitCondition->requirements_addr, inputs.at("requirements_addr"));
	SetMemberValue(&hitCondition->damage, inputs.at("damage"));
	SetMemberValue(&hitCondition->_0xC_int, inputs.at("_0xC_int"));
	SetMemberValue(&hitCondition->reactions_addr, inputs.at("reactions_addr"));
}

bool EditorT8CNT::ValidateHitConditionField(EditorInput* field)
{
	auto& name = field->name;

	if (name == "requirements_addr") {
		int listIdx = atoi(field->buffer);
		// No negative allowed here
		return 0 <= listIdx && listIdx < (int)m_infos->table.requirementCount;
	}
	else if (name == "reactions_addr") {
		int listIdx = atoi(field->buffer);
		// No negative allowed here
		return 0 <= listIdx && listIdx < (int)m_infos->table.reactionsCount;
	}

	return true;
}

// ===== Requirements  ===== //

std::vector<InputMap> EditorT8CNT::GetRequirementListInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::vector<InputMap> inputListMap;

	RequireListRange(m_iterators.requirements, id);
	uint64_t remaining = m_iterators.requirements.size() - id;
	auto req = m_iterators.requirements.begin() + id;

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	do
	{
		InputMap inputMap;

		unsigned short conditionType =
			(req->condition >= 0x8000 ? EditorInput_H32_Changeable : EditorInput_U32_Changeable);

		CREATE_FIELD("condition", 0, conditionType, req->condition);
		for (unsigned int i = 0; i < _countof(req->param); ++i)
		{
			std::string identifier = std::format("param_unsigned{}", i);
			std::string identifier2 = std::format("param_float{}", i);
			CREATE_FIELD(identifier.c_str(), 0, EditorInput_U32_Changeable, req->param[i].param_unsigned);
			CREATE_FIELD(identifier2.c_str(), 0, EditorInput_Float, req->param[i].param_float);
		}

		WriteFieldFullname(inputMap, "requirement");
		inputListMap.push_back(std::move(inputMap));
	} while ((req++)->condition != constants[EditorConstants_RequirementEnd] && --remaining > 0);

	return inputListMap;
}


void EditorT8CNT::SaveRequirement(uint16_t id, InputMap& inputs)
{
	auto req = m_iterators.requirements[id];

	SetMemberValue(&req->condition, inputs.at("condition"));
	//SetMemberValue(&req->param_unsigned, inputs.at("param_unsigned"));
}

// ===== Cancel Extradata ===== //

InputMap EditorT8CNT::GetCancelExtraInput(uint16_t id, VectorSet<std::string>& drawOrder)
{
	InputMap inputMap;

	auto cancelExtra = m_iterators.cancel_extras[id];

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	CREATE_FIELD("value", 0, EditorInput_H32_Changeable, cancelExtra->value);

	WriteFieldFullname(inputMap, "cancel_extra");
	return inputMap;
}

void EditorT8CNT::SaveCancelExtra(uint16_t id, InputMap& inputs)
{
	auto cancelExtra = m_iterators.cancel_extras[id];

	SetMemberValue(&cancelExtra->value, inputs.at("value"));
}

// ===== Cancel ===== //

std::vector<InputMap> EditorT8CNT::GetCancelListInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::vector<InputMap> inputListMap;

	RequireListRange(m_iterators.cancels, id);
	uint64_t remaining = m_iterators.cancels.size() - id;
	auto cancel = m_iterators.cancels.begin() + id;

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	do
	{
		InputMap inputMap;

		CREATE_FIELD("command", 0, EditorInput_H64, cancel->command);
		CREATE_FIELD("requirements_addr", 0, EditorInput_PTR, cancel->requirements_addr);
		CREATE_FIELD("extradata_addr", 0, EditorInput_PTR, cancel->extradata_addr);
		CREATE_FIELD("detection_start", 0, EditorInput_U32, cancel->detection_start);
		CREATE_FIELD("detection_end", 0, EditorInput_U32, cancel->detection_end);
		CREATE_FIELD("starting_frame", 0, EditorInput_U32, cancel->starting_frame);
		CREATE_FIELD("move_id", 0, EditorInput_U16 | EditorInput_Clickable, cancel->move_id);
		CREATE_FIELD("cancel_option", 0, EditorInput_U16_Changeable, cancel->cancel_option);

		WriteFieldFullname(inputMap, "cancel");
		inputListMap.push_back(std::move(inputMap));
	} while ((cancel++)->command != constants[EditorConstants_CancelCommandEnd] && --remaining > 0);

	return inputListMap;
}

void EditorT8CNT::SaveCancel(uint16_t id, InputMap& inputs)
{
	auto cancel = m_iterators.cancels[id];

	SetMemberValue(&cancel->command, inputs.at("command"));
	SetMemberValue(&cancel->requirements_addr, inputs.at("requirements_addr"));
	SetMemberValue(&cancel->extradata_addr, inputs.at("extradata_addr"));
	SetMemberValue(&cancel->detection_start, inputs.at("detection_start"));
	SetMemberValue(&cancel->detection_end, inputs.at("detection_end"));
	SetMemberValue(&cancel->starting_frame, inputs.at("starting_frame"));
	SetMemberValue(&cancel->move_id, inputs.at("move_id"));
	SetMemberValue(&cancel->cancel_option, inputs.at("cancel_option"));
}

bool EditorT8CNT::ValidateCancelField(EditorInput* field)
{
	auto& name = field->name;

	// move_id is not validated here but in the cancel class since it can serve as both a group cancel & move id
	if (name == "requirements_addr") {
		int listIdx = atoi(field->buffer);
		// No negative allowed here
		return 0 <= listIdx && listIdx < (int)m_infos->table.requirementCount;
	}
	else if (name == "extradata_addr") {
		int listIdx = atoi(field->buffer);
		// No negative allowed here
		return 0 <= listIdx && listIdx < (int)m_infos->table.cancelExtradataCount;
	}
	else if (name == "command") {
		uint64_t command = (uint64_t)strtoll(field->buffer, nullptr, 16) & 0xFFFFFFFF;
		if (command >= constants[EditorConstants_InputSequenceCommandStart]) {
			int listIdx = (unsigned int)(command - constants[EditorConstants_InputSequenceCommandStart]);
			return listIdx < (int)m_infos->table.inputCount;
		}
	}

	return true;
}

// ===== Grouped Cancel ===== //

std::vector<InputMap> EditorT8CNT::GetGroupedCancelListInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::vector<InputMap> inputListMap;

	RequireListRange(m_iterators.grouped_cancels, id);
	uint64_t remaining = m_iterators.grouped_cancels.size() - id;
	auto cancel = m_iterators.grouped_cancels.begin() + id;

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	do
	{
		InputMap inputMap;

		CREATE_FIELD("command", 0, EditorInput_H64, cancel->command);
		CREATE_FIELD("requirements_addr", 0, EditorInput_PTR, cancel->requirements_addr);
		CREATE_FIELD("extradata_addr", 0, EditorInput_PTR, cancel->extradata_addr);
		CREATE_FIELD("detection_start", 0, EditorInput_U32, cancel->detection_start);
		CREATE_FIELD("detection_end", 0, EditorInput_U32, cancel->detection_end);
		CREATE_FIELD("starting_frame", 0, EditorInput_U32, cancel->starting_frame);
		CREATE_FIELD("move_id", 0, EditorInput_U16 | EditorInput_Clickable, cancel->move_id);
		CREATE_FIELD("cancel_option", 0, EditorInput_U16_Changeable, cancel->cancel_option);

		WriteFieldFullname(inputMap, "grouped_cancel");
		inputListMap.push_back(std::move(inputMap));
	} while ((cancel++)->command != constants[EditorConstants_GroupedCancelCommandEnd] && --remaining > 0);

	return inputListMap;
}

void EditorT8CNT::SaveGroupedCancel(uint16_t id, InputMap& inputs)
{
	auto cancel = m_iterators.grouped_cancels[id];

	SetMemberValue(&cancel->command, inputs.at("command"));
	SetMemberValue(&cancel->requirements_addr, inputs.at("requirements_addr"));
	SetMemberValue(&cancel->extradata_addr, inputs.at("extradata_addr"));
	SetMemberValue(&cancel->detection_start, inputs.at("detection_start"));
	SetMemberValue(&cancel->detection_end, inputs.at("detection_end"));
	SetMemberValue(&cancel->starting_frame, inputs.at("starting_frame"));
	SetMemberValue(&cancel->move_id, inputs.at("move_id"));
	SetMemberValue(&cancel->cancel_option, inputs.at("cancel_option"));
}

bool EditorT8CNT::ValidateGroupedCancelField(EditorInput* field)
{
	auto& name = field->name;

	if (name == "move_id") {
		int moveId = atoi(field->buffer);
		if (moveId >= m_infos->table.moveCount) {
			if (moveId < 0x8000) {
				return false;
			}
			if (moveId >= (0x8000 + aliases.size())) {
				return false;
			}
		}
		else if (moveId < 0) {
			return false;
		}
	}
	else if (name == "requirements_addr") {
		int listIdx = atoi(field->buffer);
		// No negative allowed here
		return 0 <= listIdx && listIdx < (int)m_infos->table.requirementCount;
	}
	else if (name == "extradata_addr") {
		int listIdx = atoi(field->buffer);
		// No negative allowed here
		return 0 <= listIdx && listIdx < (int)m_infos->table.cancelExtradataCount;
	}
	else if (name == "command") {
		if (EditorT8CNT::IsCommandInputSequence(field->buffer))
		{
			int listIdx = EditorT8CNT::GetCommandInputSequenceID(field->buffer);
			return listIdx < (int)m_infos->table.inputCount;
		}
	}

	return true;
}

// ===== Other move properties (start) ===== //

std::vector<InputMap> EditorT8CNT::GetMoveStartPropertyListInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::vector<InputMap> inputListMap;

	RequireListRange(m_iterators.move_start_properties, id);
	uint64_t remaining = m_iterators.move_start_properties.size() - id;
	auto prop = m_iterators.move_start_properties.begin() + id;

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	do
	{
		InputMap inputMap;

		CREATE_FIELD("requirements_addr", 0, EditorInput_PTR, prop->requirements_addr);
		CREATE_FIELD("extraprop", 0, EditorInput_H32_Changeable, prop->extraprop);
		//CREATE_FIELD("value", 0, EditorInput_U32_Changeable, prop->value);

		WriteFieldFullname(inputMap, "move_start_extraprop");
		inputListMap.push_back(std::move(inputMap));
	} while ((prop++)->extraprop != constants[EditorConstants_RequirementEnd] && --remaining > 0);

	return inputListMap;
}

void EditorT8CNT::SaveMoveStartProperty(uint16_t id, InputMap& inputs)
{
	auto prop = m_iterators.move_start_properties[id];

	SetMemberValue(&prop->requirements_addr, inputs.at("requirements_addr"));
	SetMemberValue(&prop->extraprop, inputs.at("extraprop"));
	//SetMemberValue(&prop->value, inputs.at("value"));
}

// ===== Other move properties (end) ===== //

std::vector<InputMap> EditorT8CNT::GetMoveEndPropertyListInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::vector<InputMap> inputListMap;

	RequireListRange(m_iterators.move_end_properties, id);
	uint64_t remaining = m_iterators.move_end_properties.size() - id;
	auto prop = m_iterators.move_end_properties.begin() + id;

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	do
	{
		InputMap inputMap;

		CREATE_FIELD("requirements_addr", 0, EditorInput_PTR, prop->requirements_addr);
		CREATE_FIELD("extraprop", 0, EditorInput_H32_Changeable, prop->extraprop);
		//CREATE_FIELD("value", 0, EditorInput_U32_Changeable, prop->value);

		WriteFieldFullname(inputMap, "move_end_extraprop");
		inputListMap.push_back(std::move(inputMap));
	} while ((prop++)->extraprop != constants[EditorConstants_RequirementEnd] && --remaining > 0);

	return inputListMap;
}

void EditorT8CNT::SaveMoveEndProperty(uint16_t id, InputMap& inputs)
{
	auto prop = m_iterators.move_end_properties[id];

	SetMemberValue(&prop->requirements_addr, inputs.at("requirements_addr"));
	SetMemberValue(&prop->extraprop, inputs.at("extraprop"));
	//SetMemberValue(&prop->value, inputs.at("value"));
}

bool EditorT8CNT::ValidateOtherMoveProperty(EditorInput* field)
{
	auto& name = field->name;

	if (name == "requirements_addr") {
		int listIdx = atoi(field->buffer);
		return 0 <= listIdx && listIdx < (int)m_infos->table.requirementCount;
	}
	return true;
}

// ===== ExtraProperties ===== //

std::vector<InputMap> EditorT8CNT::GetExtrapropListInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::vector<InputMap> inputListMap;

	RequireListRange(m_iterators.extra_move_properties, id);
	uint64_t remaining = m_iterators.extra_move_properties.size() - id;
	auto prop = m_iterators.extra_move_properties.begin() + id;

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	do
	{
		InputMap inputMap;

		CREATE_FIELD("starting_frame", 0, EditorInput_U32_Changeable, prop->starting_frame);
		CREATE_FIELD("id", 0, EditorInput_H32, prop->id);
		/*
		CREATE_FIELD("value_signed", 0, EditorInput_S32, prop->value_signed);
		CREATE_FIELD("value_unsigned", 0, EditorInput_U32, prop->value_unsigned);
		CREATE_FIELD("value_hex", 0, EditorInput_H32, prop->value_unsigned);
		CREATE_FIELD("value_float", 0, EditorInput_Float, prop->value_float);
		*/

		WriteFieldFullname(inputMap, "extraproperty");
		inputListMap.push_back(std::move(inputMap));
	} while ((prop++)->id != constants[EditorConstants_ExtraPropertyEnd] && --remaining > 0); // Property id 0 ends the list

	return inputListMap;
}

void EditorT8CNT::SaveExtraproperty(uint16_t id, InputMap& inputs)
{
	auto prop = m_iterators.extra_move_properties[id];

	SetMemberValue(&prop->starting_frame, inputs.at("starting_frame"));
	SetMemberValue(&prop->id, inputs.at("id"));
	//SetMemberValue(&prop->value_unsigned, inputs.at("value_unsigned"));
}

// ===== Voiceclips ===== //

std::vector<InputMap> EditorT8CNT::GetVoiceclipListInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::vector<InputMap> inputListMap;

	RequireListRange(m_iterators.voiceclips, id);
	uint64_t remaining = m_iterators.voiceclips.size() - id;
	auto voiceclip = m_iterators.voiceclips.begin() + id;

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	do
	{
		InputMap inputMap;

		CREATE_FIELD("id", 0, EditorInput_H32, voiceclip->id);

		WriteFieldFullname(inputMap, "voiceclip");
		inputListMap.push_back(std::move(inputMap));
	} while ((voiceclip++)->id != (uint32_t)-1 && --remaining > 0);

	return inputListMap;
}


void EditorT8CNT::SaveVoiceclip(uint16_t id, InputMap& inputs)
{
	auto voiceclip = m_iterators.voiceclips[id];

	SetMemberValue(&voiceclip->id, inputs.at("id"));
}

// ===== MOVES ===== //

InputMap EditorT8CNT::GetMoveInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	InputMap inputMap;

	auto move = m_iterators.moves[id];

	char* nameBlock = (char*)(m_movesetData + m_offsets->nameBlock);

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	CREATE_FIELD("move_name", 0, 0, nameBlock + move->name_addr);
	CREATE_FIELD("anim_name", 0, EditorInput_ClickableAlways, nameBlock + move->anim_name_addr);
	CREATE_FIELD("vulnerability", 0, EditorInput_U32_Changeable, move->vuln);
	CREATE_FIELD("hitlevel", 0, EditorInput_H32, move->hitlevel);
	CREATE_FIELD("transition", 0, EditorInput_U16 | EditorInput_Clickable, move->transition);
	CREATE_FIELD("moveId_val1", 0, EditorInput_U16, move->moveId_val1);
	CREATE_FIELD("moveId_val2", 0, EditorInput_U16, move->moveId_val2);
	CREATE_FIELD("anim_len", 0, EditorInput_U32, move->anim_len);
	CREATE_FIELD("airborne_start", 0, EditorInput_U32, move->airborne_start);
	CREATE_FIELD("airborne_end", 0, EditorInput_U32, move->airborne_end);
	CREATE_FIELD("ground_fall", 0, EditorInput_U32, move->ground_fall);
	// first/last_active_frame: the move's active window. hitbox_location / _2 and first/last_active_frame_2:
	// hitbox slots 0 and 1 (StructsT8CNT::Move::hitboxes).
	CREATE_FIELD("hitbox_location", 0, EditorInput_H32, move->hitboxes[0].location);
	CREATE_FIELD("hitbox_location_2", 0, EditorInput_H32, move->hitboxes[1].location);
	CREATE_FIELD("first_active_frame", 0, EditorInput_U32, move->first_active_frame);
	CREATE_FIELD("last_active_frame", 0, EditorInput_U32, move->last_active_frame);
	CREATE_FIELD("first_active_frame_2", 0, EditorInput_U32, move->hitboxes[0].first_active_frame);
	CREATE_FIELD("last_active_frame_2", 0, EditorInput_U32, move->hitboxes[0].last_active_frame);
	CREATE_FIELD("collision", 0, EditorInput_U16_Changeable, move->collision);
	CREATE_FIELD("distance", 0, EditorInput_U16_Changeable, move->distance);

	CREATE_FIELD("cancel_addr", 2, EditorInput_PTR, move->cancel_addr);
	CREATE_FIELD("hit_condition_addr", 2, EditorInput_PTR, move->hit_condition_addr);
	CREATE_FIELD("extra_properties_addr", 2, EditorInput_PTR, move->extra_move_property_addr);
	CREATE_FIELD("move_start_extraprop_addr", 2, EditorInput_PTR, move->move_start_extraprop_addr);
	CREATE_FIELD("move_end_extraprop_addr", 2, EditorInput_PTR, move->move_end_extraprop_addr);
	CREATE_FIELD("voiceclip_addr", 2, EditorInput_PTR, move->voicelip_addr);

	CREATE_FIELD("cancel_addr_2", 3, EditorInput_PTR, move->_0x30_cancel_addr);
	CREATE_FIELD("cancel_related_id_2", 3, EditorInput_U32, move->_0x38_int__0x30_related);
	CREATE_FIELD("cancel_addr_3", 3, EditorInput_PTR, move->_0x40_cancel_addr);
	CREATE_FIELD("cancel_related_id_3", 3, EditorInput_U32, move->_0x48_int__0x40_related);
	CREATE_FIELD("cancel_addr_4", 3, EditorInput_PTR, move->_0x50_cancel_addr);
	CREATE_FIELD("cancel_related_id_4", 3, EditorInput_U32, move->_0x58_int__0x50_related);

	CREATE_FIELD("_0x3C_int", 5, EditorInput_U32_Changeable, move->_0x3C_int);
	CREATE_FIELD("_0x4C_int", 5, EditorInput_U32_Changeable, move->_0x4C_int);
	CREATE_FIELD("_0x5E_short", 5, EditorInput_U16_Changeable, move->_0x5E_short);
	CREATE_FIELD("_0x64_short", 5, EditorInput_U16_Changeable, move->_0x64_short);
	CREATE_FIELD("_0x66_short", 5, EditorInput_U16_Changeable, move->_0x66_short);
	CREATE_FIELD("_0x70", 5, EditorInput_H32_Changeable, move->_0x70);
	CREATE_FIELD("_0x74", 5, EditorInput_H32_Changeable, move->_0x74);
	CREATE_FIELD("_0xA8_int", 5, EditorInput_H32_Changeable, move->_0xA8_int);
	CREATE_FIELD("_0xAC_int", 5, EditorInput_H32_Changeable, move->_0xAC_int);

	WriteFieldFullname(inputMap, "move");
	return inputMap;
}


uint64_t EditorT8CNT::SaveMoveName(const char* name, gameAddr offset)
{
    if (BlockString(StructsT8CNT::TKMovesetHeaderBlocks_Name, offset) == name) return offset;
    // Names may share storage with other moves, animation names or metadata.
    return CreateMoveName(name);
}

void EditorT8CNT::SaveMove(uint16_t id, InputMap& inputs)
{
	auto move = m_iterators.moves[id];

	SetMemberValue(&move->vuln, inputs.at("vulnerability"));
	SetMemberValue(&move->hitlevel, inputs.at("hitlevel"));
	SetMemberValue(&move->transition, inputs.at("transition"));
	SetMemberValue(&move->moveId_val1, inputs.at("moveId_val1"));
	SetMemberValue(&move->moveId_val2, inputs.at("moveId_val2"));
	SetMemberValue(&move->anim_len, inputs.at("anim_len"));
	SetMemberValue(&move->airborne_start, inputs.at("airborne_start"));
	SetMemberValue(&move->airborne_end, inputs.at("airborne_end"));
	SetMemberValue(&move->ground_fall, inputs.at("ground_fall"));
	SetMemberValue(&move->hitboxes[0].location, inputs.at("hitbox_location"));
	SetMemberValue(&move->hitboxes[1].location, inputs.at("hitbox_location_2"));
	SetMemberValue(&move->hitboxes[0].first_active_frame, inputs.at("first_active_frame_2"));
	SetMemberValue(&move->hitboxes[0].last_active_frame, inputs.at("last_active_frame_2"));
	SetMemberValue(&move->first_active_frame, inputs.at("first_active_frame"));
	SetMemberValue(&move->last_active_frame, inputs.at("last_active_frame"));
	SetMemberValue(&move->distance, inputs.at("distance"));

	SetMemberValue(&move->cancel_addr, inputs.at("cancel_addr"));
	SetMemberValue(&move->hit_condition_addr, inputs.at("hit_condition_addr"));
	SetMemberValue(&move->extra_move_property_addr, inputs.at("extra_properties_addr"));
	SetMemberValue(&move->move_start_extraprop_addr, inputs.at("move_start_extraprop_addr"));
	SetMemberValue(&move->move_end_extraprop_addr, inputs.at("move_end_extraprop_addr"));
	SetMemberValue(&move->voicelip_addr, inputs.at("voiceclip_addr"));

	
	SetMemberValue(&move->_0x30_cancel_addr, inputs.at("cancel_addr_2"));
	SetMemberValue(&move->_0x38_int__0x30_related, inputs.at("cancel_related_id_2"));
	SetMemberValue(&move->_0x40_cancel_addr, inputs.at("cancel_addr_3"));
	SetMemberValue(&move->_0x48_int__0x40_related, inputs.at("cancel_related_id_3"));
	SetMemberValue(&move->_0x50_cancel_addr, inputs.at("cancel_addr_4"));
	SetMemberValue(&move->_0x58_int__0x50_related, inputs.at("cancel_related_id_4"));

	SetMemberValue(&move->_0x3C_int, inputs.at("_0x3C_int"));
	SetMemberValue(&move->_0x4C_int, inputs.at("_0x4C_int"));
	SetMemberValue(&move->_0x5E_short, inputs.at("_0x5E_short"));
	SetMemberValue(&move->_0x64_short, inputs.at("_0x64_short"));
	SetMemberValue(&move->_0x5E_short, inputs.at("_0x5E_short"));
	SetMemberValue(&move->_0x64_short, inputs.at("_0x64_short"));
	SetMemberValue(&move->_0x66_short, inputs.at("_0x66_short"));
	SetMemberValue(&move->_0x70, inputs.at("_0x70"));
	SetMemberValue(&move->_0x70, inputs.at("_0x74"));

	
	char* newAnimName = inputs.at("anim_name")->buffer;

    if (BlockString(StructsT8CNT::TKMovesetHeaderBlocks_Name, move->anim_name_addr) != newAnimName)
        throw std::logic_error("T8 encrypted animation references cannot be edited yet");

	// Save move name at the end because it may imply reallocation and invalidation of existing pointers
	char* newName = inputs.at("move_name")->buffer;

    const auto nameOffset = SaveMoveName(newName, move->name_addr);
    m_iterators.moves[id]->name_addr = nameOffset;

	// In case we updated the name, reload the movelist display
	ReloadDisplayableMoveList();
}

bool EditorT8CNT::ValidateMoveField(EditorInput* field)
{
	auto& name = field->name;

	if (name == "anim_name") {
		return m_animNameToOffsetMap.find(field->buffer) != m_animNameToOffsetMap.end();
	}

	else if (name.startsWith("cancel_addr")) {
		int listIdx = atoi(field->buffer);
		return -1 <= listIdx && listIdx < (int)m_infos->table.cancelCount;
	}

	else if (name == "hit_condition_addr") {
		int listIdx = atoi(field->buffer);
		return 0 <= listIdx && listIdx < (int)m_infos->table.hitConditionCount;
	}

	else if (name == "extra_properties_addr") {
		int listIdx = atoi(field->buffer);
		return -1 <= listIdx && listIdx < (int)m_infos->table.extraMovePropertyCount;
	}

	else if (name == "move_start_extraprop_addr") {
		int listIdx = atoi(field->buffer);
		return -1 <= listIdx && listIdx < (int)m_infos->table.moveBeginningPropCount;
	}

	else if (name == "move_end_extraprop_addr") {
		int listIdx = atoi(field->buffer);
		return -1 <= listIdx && listIdx < (int)m_infos->table.moveEndingPropCount;
	}

	else if (name == "voiceclip_addr") {
		int listIdx = atoi(field->buffer);
		return -1 <= listIdx && listIdx < (int)m_infos->table.voiceclipCount;
	}

	else if (name == "transition") {
		int moveId = atoi(field->buffer);
		if (moveId >= m_infos->table.moveCount) {
			if (moveId < 0x8000) {
				return false;
			}
			if (moveId >= (0x8000 + aliases.size())) {
				return false;
			}
		}
		else if (moveId < 0) {
			return false;
		}
	}

	return true;
}

// ===== Generic =====

void EditorT8CNT::SaveItem(EditorWindowType type, uint16_t id, InputMap& inputs)
{
    MutateBatch([&] {
        if (id >= GetStructureCount(type)) throw std::out_of_range("Invalid record ID");
        for (const auto& [key, field] : inputs) InputString(field);

	// Saving is one on an individual item basis, even for lists
	switch (type)
	{
	case TEditorWindowType_Move:
		SaveMove(id, inputs);
		break;
	case TEditorWindowType_Voiceclip:
		SaveVoiceclip(id, inputs);
		break;
	case TEditorWindowType_Extraproperty:
		SaveExtraproperty(id, inputs);
		break;
	case TEditorWindowType_Cancel:
		SaveCancel(id, inputs);
		break;
	case TEditorWindowType_CancelExtradata:
		SaveCancelExtra(id, inputs);
		break;
	case TEditorWindowType_GroupedCancel:
		SaveGroupedCancel(id, inputs);
		break;
	case TEditorWindowType_Requirement:
		SaveRequirement(id, inputs);
		break;
	case TEditorWindowType_HitCondition:
		SaveHitCondition(id, inputs);
		break;
	case TEditorWindowType_Reactions:
		SaveReactions(id, inputs);
		break;
	case TEditorWindowType_Pushback:
		SavePushback(id, inputs);
		break;
	case TEditorWindowType_PushbackExtradata:
		SavePushbackExtra(id, inputs);
		break;
	case TEditorWindowType_MoveBeginProperty:
		SaveMoveStartProperty(id, inputs);
		break;
	case TEditorWindowType_MoveEndProperty:
		SaveMoveEndProperty(id, inputs);
		break;
	case TEditorWindowType_InputSequence:
		SaveInputSequence(id, inputs);
		break;
	case TEditorWindowType_Input:
		SaveInput(id, inputs);
		break;
	case TEditorWindowType_Projectile:
		SaveProjectile(id, inputs);
		break;
	case TEditorWindowType_ThrowCamera:
		SaveThrowCamera(id, inputs);
		break;
	case TEditorWindowType_CameraData:
		SaveCameraData(id, inputs);
		break;
	case TEditorWindowType_MovelistDisplayable:
		SaveMovelistDisplayable(id, inputs);
		break;
	case TEditorWindowType_MovelistPlayable:
		SaveMovelistPlayable(id, inputs);
		break;
	case TEditorWindowType_MovelistInput:
		SaveMovelistInput(id, inputs);
		break;
	}
    });
}

InputMap EditorT8CNT::GetFormFields(EditorWindowType type, uint16_t id, VectorSet<std::string>& drawOrder)
{
	// Builds an input list for a specific window type (single struct) and fill default values according to the given ID
	switch (type)
	{
	case TEditorWindowType_Move:
		return GetMoveInputs(id, drawOrder);
		break;
	case TEditorWindowType_CancelExtradata:
		return GetCancelExtraInput(id, drawOrder);
		break;
	case TEditorWindowType_Reactions:
		return GetReactionsInputs(id, drawOrder);
		break;
	case TEditorWindowType_Pushback:
		return GetPushbackInputs(id, drawOrder);
		break;
	case TEditorWindowType_InputSequence:
		return GetInputSequenceInputs(id, drawOrder);
		break;
	case TEditorWindowType_Projectile:
		return GetProjectileInputs(id, drawOrder);
		break;
	case TEditorWindowType_ThrowCamera:
		return GetThrowCameraInputs(id, drawOrder);
		break;
	case TEditorWindowType_CameraData:
		return GetCameraDataInputs(id, drawOrder);
		break;
	case TEditorWindowType_MovelistPlayable:
		return GetMovelistPlayableInputs(id, drawOrder);
		break;
	}
	return InputMap();
}

std::vector<InputMap> EditorT8CNT::GetFormFieldsList(EditorWindowType type, uint16_t id, VectorSet<std::string>& drawOrder)
{
	// Builds an input list for a specific window type (list of structs) and fill default values according to the given ID
	switch (type)
	{
	case TEditorWindowType_Extraproperty:
		return GetExtrapropListInputs(id, drawOrder);
		break;
	case TEditorWindowType_MoveBeginProperty:
		return GetMoveStartPropertyListInputs(id, drawOrder);
		break;
	case TEditorWindowType_MoveEndProperty:
		return GetMoveEndPropertyListInputs(id, drawOrder);
		break;
	case TEditorWindowType_Cancel:
		return GetCancelListInputs(id, drawOrder);
		break;
	case TEditorWindowType_GroupedCancel:
		return GetGroupedCancelListInputs(id, drawOrder);
		break;
	case TEditorWindowType_Requirement:
		return GetRequirementListInputs(id, drawOrder);
		break;
	case TEditorWindowType_HitCondition:
		return GetHitConditionListInputs(id, drawOrder);
		break;
	case TEditorWindowType_Voiceclip:
		return GetVoiceclipListInputs(id, drawOrder);
		break;

	case TEditorWindowType_MovelistDisplayable:
		return GetMovelistDisplayablesInputs(0, drawOrder);
		break;
	}
	return std::vector<InputMap>();
}

std::vector<InputMap> EditorT8CNT::GetFormFieldsList(EditorWindowType type, uint16_t id, VectorSet<std::string>& drawOrder, int listSize)
{
	// Specific to lists with unguessable sizes : we provide the size as an argument
	switch (type)
	{
	case TEditorWindowType_Input:
		return GetInputListInputs(id, listSize, drawOrder);
		break;
	case TEditorWindowType_PushbackExtradata:
		return GetPushbackExtraListInputs(id, listSize, drawOrder);
		break;

	case TEditorWindowType_MovelistInput:
		return GetMovelistInputListInputs(id, listSize, drawOrder);
		break;
	}
	return GetFormFieldsList(type, id, drawOrder);
}


InputMap EditorT8CNT::GetListSingleForm(EditorWindowType type, uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::vector<InputMap> list;

	switch (type)
	{
	case TEditorWindowType_MovelistDisplayable:
		list = GetMovelistDisplayablesInputs(id, drawOrder, true);
		break;
	default:
		list = GetFormFieldsList(type, id, drawOrder, 1);
		break;
	}

    if (list.empty()) throw std::invalid_argument("Editor field factory returned no rows.");
    return std::move(list.front());
}

bool EditorT8CNT::ValidateField(EditorWindowType fieldType, EditorInput* field)
{
	if (!ValidateFieldType(field)) {
		return false;
	}

	switch (fieldType)
	{
	case TEditorWindowType_Move:
		return ValidateMoveField(field);
		break;
	case TEditorWindowType_Cancel:
		return ValidateCancelField(field);
		break;
	case TEditorWindowType_GroupedCancel:
		return ValidateGroupedCancelField(field);
		break;
	case TEditorWindowType_HitCondition:
		return ValidateHitConditionField(field);
		break;
	case TEditorWindowType_Reactions:
		return ValidateReactionsField(field);
		break;
	case TEditorWindowType_Pushback:
		return ValidatePushbackField(field);
		break;
	case TEditorWindowType_MoveBeginProperty:
	case TEditorWindowType_MoveEndProperty:
		return ValidateOtherMoveProperty(field);
		break;
	case TEditorWindowType_InputSequence:
		return ValidateInputSequenceField(field);
		break;
	case TEditorWindowType_Projectile:
		return ValidateProjectileField(field);
		break;
	case TEditorWindowType_ThrowCamera:
		return ValidateThrowCameraField(field);
		break;

	case TEditorWindowType_MovelistDisplayable:
		return ValidateMovelistDisplayableField(field);
		break;
	case TEditorWindowType_MovelistPlayable:
		return ValidateMovelistPlayableField(field);
		break;

	}

	return true;
}

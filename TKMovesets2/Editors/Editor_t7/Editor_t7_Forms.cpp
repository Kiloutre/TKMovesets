#include "Editor_t7.hpp"

#include <type_traits>
using namespace EditorUtils;

#define CREATE_FIELD(k, c, f, v) CreateField<decltype(v)>(k, drawOrder, inputMap, c, f, v)

// ===== Pushback Extra ===== //

std::vector<std::map<std::string, EditorInput*>> EditorT7::GetPushbackExtraListInputs(uint16_t id, int listSize, VectorSet<std::string>& drawOrder)
{
	std::vector<std::map<std::string, EditorInput*>> inputListMap;

	auto pushbackExtra = m_iterators.pushback_extras.begin() + id;

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	do
	{
		std::map<std::string, EditorInput*> inputMap;

		CREATE_FIELD("horizontal_offset", 0, EditorInput_U16_Changeable, pushbackExtra->horizontal_offset);

		WriteFieldFullname(inputMap, "pushback_extradata");
		inputListMap.push_back(inputMap);
		pushbackExtra++;
	} while (--listSize > 0);

	return inputListMap;
}

void EditorT7::SavePushbackExtra(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto pushbackExtra = m_iterators.pushback_extras[id];

	SetMemberValue(&pushbackExtra->horizontal_offset, inputs["horizontal_offset"]);
}

// ===== Pushback ===== //

std::map<std::string, EditorInput*> EditorT7::GetPushbackInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::map<std::string, EditorInput*> inputMap;

	auto pushback = m_iterators.pushbacks[id];

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	CREATE_FIELD("duration", 0, EditorInput_U16, pushback->duration);
	CREATE_FIELD("displacement", 0, EditorInput_U16_Changeable, pushback->displacement);
	CREATE_FIELD("num_of_loops", 0, EditorInput_U32, pushback->num_of_loops);
	CREATE_FIELD("extradata_addr", 0, EditorInput_PTR, pushback->extradata_addr);

	WriteFieldFullname(inputMap, "pushback");
	return inputMap;
}

bool EditorT7::ValidatePushbackField(EditorInput* field)
{
	std::string& name = field->name;

	if (name == "extradata_addr")
	{
		int listIdx = atoi(field->buffer);
		// No negative allowed here
		return 0 <= listIdx && listIdx < (int)m_infos->table.pushbackExtradataCount;
	}

	return true;
}

void EditorT7::SavePushback(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto pushback = m_iterators.pushbacks[id];

	SetMemberValue(&pushback->duration, inputs["duration"]);
	SetMemberValue(&pushback->displacement, inputs["displacement"]);
	SetMemberValue(&pushback->num_of_loops, inputs["num_of_loops"]);
	SetMemberValue(&pushback->extradata_addr, inputs["extradata_addr"]);
}

// ===== Projectile ===== //

std::map<std::string, EditorInput*> EditorT7::GetProjectileInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::map<std::string, EditorInput*> inputMap;

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

bool EditorT7::ValidateProjectileField(EditorInput* field)
{
	std::string& name = field->name;

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

void EditorT7::SaveProjectile(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto projectile = m_iterators.projectiles[id];

	SetMemberValue(&projectile->vfx_id, inputs["vfx_id"]);
	SetMemberValue(&projectile->vfx_variation_id, inputs["vfx_variation_id"]);
	SetMemberValue(&projectile->delay, inputs["delay"]);
	SetMemberValue(&projectile->vertical_velocity, inputs["vertical_velocity"]);
	SetMemberValue(&projectile->horizontal_velocity, inputs["horizontal_velocity"]);
	SetMemberValue(&projectile->duration, inputs["duration"]);
	SetMemberValue(&projectile->no_collision, inputs["no_collision"]);
	SetMemberValue(&projectile->size, inputs["size"]);
	SetMemberValue(&projectile->hit_level, inputs["hit_level"]);
	SetMemberValue(&projectile->voiceclip_on_hit, inputs["voiceclip_on_hit"]);
	SetMemberValue(&projectile->can_hitbox_connect, inputs["can_hitbox_connect"]);
	SetMemberValue(&projectile->gravity, inputs["gravity"]);

	SetMemberValue(&projectile->hit_condition_addr, inputs["hit_condition_addr"]);
	SetMemberValue(&projectile->cancel_addr, inputs["cancel_addr"]);

	SetMemberValue(&projectile->_0x34_int, inputs["_0x34_int"]);
	SetMemberValue(&projectile->_0x4_int, inputs["_0x4_int"]);
	SetMemberValue(&projectile->_0xC_int, inputs["_0xC_int"]);
	SetMemberValue(&projectile->_0x10_int, inputs["_0x10_int"]);
	SetMemberValue(&projectile->_0x14_int, inputs["_0x14_int"]);
	SetMemberValue(&projectile->_0x24_int, inputs["_0x24_int"]);
	SetMemberValue(&projectile->_0x3C_int[0], inputs["_0x3C_int_1"]);
	SetMemberValue(&projectile->_0x3C_int[1], inputs["_0x3C_int_2"]);
	SetMemberValue(&projectile->_0x3C_int[2], inputs["_0x3C_int_3"]);
	SetMemberValue(&projectile->_0x3C_int[3], inputs["_0x3C_int_4"]);
	SetMemberValue(&projectile->_0x3C_int[4], inputs["_0x3C_int_5"]);
	SetMemberValue(&projectile->_0x3C_int[5], inputs["_0x3C_int_6"]);
	SetMemberValue(&projectile->_0x58_int, inputs["_0x58_int"]);
	SetMemberValue(&projectile->_0x5C_int, inputs["_0x5C_int"]);
	SetMemberValue(&projectile->_0x70_int, inputs["_0x70_int"]);
	SetMemberValue(&projectile->_0x74_int, inputs["_0x74_int"]);
	SetMemberValue(&projectile->_0x7C_int, inputs["_0x7C_int"]);
	SetMemberValue(&projectile->_0x80_int, inputs["_0x80_int"]);
	SetMemberValue(&projectile->_0x88_int[0], inputs["_0x88_int_1"]);
	SetMemberValue(&projectile->_0x88_int[1], inputs["_0x88_int_2"]);
	SetMemberValue(&projectile->_0x88_int[2], inputs["_0x88_int_3"]);
	SetMemberValue(&projectile->_0x88_int[3], inputs["_0x88_int_4"]);
	SetMemberValue(&projectile->_0x88_int[4], inputs["_0x88_int_5"]);
	SetMemberValue(&projectile->_0x88_int[5], inputs["_0x88_int_6"]);
	SetMemberValue(&projectile->_0x88_int[6], inputs["_0x88_int_7"]);
	SetMemberValue(&projectile->_0x88_int[7], inputs["_0x88_int_8"]);
}

// ===== Input Sequence  ===== //

std::map<std::string, EditorInput*> EditorT7::GetInputSequenceInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::map<std::string, EditorInput*> inputMap;

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

bool EditorT7::ValidateInputSequenceField(EditorInput* field)
{
	std::string& name = field->name;

	if (name == "input_addr")
	{
		int listIdx = atoi(field->buffer);
		// No negative allowed here
		return 0 <= listIdx && listIdx < (int)m_infos->table.inputCount;
	}

	return true;
}

void EditorT7::SaveInputSequence(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto sequence = m_iterators.input_sequences[id];

	SetMemberValue(&sequence->input_window_frames, inputs["input_window_frames"]);
	SetMemberValue(&sequence->input_amount, inputs["input_amount"]);
	SetMemberValue(&sequence->_0x4_int, inputs["_0x4_int"]);
	SetMemberValue(&sequence->input_addr, inputs["input_addr"]);
}

// ===== Inputs ===== //

std::vector<std::map<std::string, EditorInput*>> EditorT7::GetInputListInputs(uint16_t id, int listSize, VectorSet<std::string>& drawOrder)
{
	std::vector<std::map<std::string, EditorInput*>> inputListMap;

	auto input = m_iterators.inputs.begin() + id;

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	do
	{
		std::map<std::string, EditorInput*> inputMap;

		CREATE_FIELD("direction", 0, EditorInput_H32, input->direction);
		CREATE_FIELD("button", 0, EditorInput_H32, input->button);

		WriteFieldFullname(inputMap, "input");
		inputListMap.push_back(inputMap);
		input++;
	} while (--listSize > 0);

	return inputListMap;
}

void EditorT7::SaveInput(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto input = m_iterators.inputs.begin() + id;

	SetMemberValue(&input->direction, inputs["direction"]);
	SetMemberValue(&input->button, inputs["button"]);
}

// ===== Throw cameras ===== //

std::map<std::string, EditorInput*> EditorT7::GetThrowCameraInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::map<std::string, EditorInput*> inputMap;

	auto throwCamera = m_iterators.throw_datas[id];

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	CREATE_FIELD("cameradata_addr", 0, EditorInput_PTR, throwCamera->cameradata_addr);
	CREATE_FIELD("_0x0_llong", 0, EditorInput_U64_Changeable, throwCamera->_0x0_llong);

	WriteFieldFullname(inputMap, "throw_camera");
	return inputMap;
}

bool EditorT7::ValidateThrowCameraField(EditorInput* field)
{
	std::string& name = field->name;

	if (name == "cameradata_addr")
	{
		int listIdx = atoi(field->buffer);
		// No negative allowed here
		return 0 <= listIdx && listIdx < (int)m_infos->table.cameraDataCount;
	}

	return true;
}

void EditorT7::SaveThrowCamera(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto throwCamera = m_iterators.throw_datas[id];

	SetMemberValue(&throwCamera->_0x0_llong, inputs["_0x0_int"]);
	SetMemberValue(&throwCamera->cameradata_addr, inputs["cameradata_addr"]);
}

// ===== Camera data ===== //

std::map<std::string, EditorInput*> EditorT7::GetCameraDataInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::map<std::string, EditorInput*> inputMap;

	auto cameraData = m_iterators.camera_datas[id];

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	CREATE_FIELD("_0x0_int", 0, EditorInput_U32_Changeable, cameraData->_0x0_int);
	CREATE_FIELD("_0x4_short", 0, EditorInput_U16_Changeable, cameraData->_0x4_short);
	CREATE_FIELD("left_side_camera_data", 0, EditorInput_U16_Changeable, cameraData->left_side_camera_data);
	CREATE_FIELD("right_side_camera_data", 0, EditorInput_U16_Changeable, cameraData->right_side_camera_data);
	CREATE_FIELD("_0xA_short", 0, EditorInput_U16_Changeable, cameraData->_0xA_short);

	WriteFieldFullname(inputMap, "camera_data");
	return inputMap;
}


void EditorT7::SaveCameraData(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto cameraData = m_iterators.camera_datas[id];

	SetMemberValue(&cameraData->_0x0_int, inputs["_0x0_int"]);
	SetMemberValue(&cameraData->_0x4_short, inputs["_0x4_short"]);
	SetMemberValue(&cameraData->left_side_camera_data, inputs["left_side_camera_data"]);
	SetMemberValue(&cameraData->right_side_camera_data, inputs["right_side_camera_data"]);
	SetMemberValue(&cameraData->_0xA_short, inputs["_0xA_short"]);
}


// ===== Reactions ===== //

std::map<std::string, EditorInput*> EditorT7::GetReactionsInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::map<std::string, EditorInput*> inputMap;

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

	CREATE_FIELD("default_moveid", 3, EditorInput_U16 | EditorInput_Clickable, reaction->default_moveid);
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
	CREATE_FIELD("_0x44_int", 7, EditorInput_U32_Changeable, reaction->_0x44_int);
	CREATE_FIELD("_0x48_int", 7, EditorInput_U32_Changeable, reaction->_0x48_int);

	WriteFieldFullname(inputMap, "reactions");
	return inputMap;
}

bool EditorT7::ValidateReactionsField(EditorInput* field)
{
	std::string& name = field->name;

	if (Helpers::endsWith(name, "_moveid"))
	{
		int moveId = atoi(field->buffer);
		if (moveId >= m_infos->table.moveCount) {
			if (moveId < 0x8000) {
				return false;
			}
			if (moveId >= (0x8000 + m_aliases->size())) {
				return false;
			}
		}
		else if (moveId < 0) {
			return false;
		}
	}
	else if (Helpers::endsWith(name, "_pushback"))
	{
		int listIdx = atoi(field->buffer);
		// No negative allowed here
		return 0 <= listIdx && listIdx < (int)m_infos->table.pushbackCount;
	}

	return true;
}

void EditorT7::SaveReactions(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto reaction = m_iterators.reactions[id];

	SetMemberValue(&reaction->front_pushback, inputs["front_pushback"]);
	SetMemberValue(&reaction->backturned_pushback, inputs["backturned_pushback"]);
	SetMemberValue(&reaction->left_side_pushback, inputs["left_side_pushback"]);
	SetMemberValue(&reaction->right_side_pushback, inputs["right_side_pushback"]);
	SetMemberValue(&reaction->front_counterhit_pushback, inputs["front_counterhit_pushback"]);
	SetMemberValue(&reaction->downed_pushback, inputs["downed_pushback"]);
	SetMemberValue(&reaction->block_pushback, inputs["block_pushback"]);

	SetMemberValue(&reaction->front_direction, inputs["front_direction"]);
	SetMemberValue(&reaction->back_direction, inputs["back_direction"]);
	SetMemberValue(&reaction->left_side_direction, inputs["left_side_direction"]);
	SetMemberValue(&reaction->right_side_direction, inputs["right_side_direction"]);
	SetMemberValue(&reaction->front_counterhit_direction, inputs["front_counterhit_direction"]);
	SetMemberValue(&reaction->downed_direction, inputs["downed_direction"]);

	SetMemberValue(&reaction->default_moveid, inputs["default_moveid"]);
	SetMemberValue(&reaction->standing_moveid, inputs["standing_moveid"]);
	SetMemberValue(&reaction->crouch_moveid, inputs["crouch_moveid"]);
	SetMemberValue(&reaction->counterhit_moveid, inputs["counterhit_moveid"]);
	SetMemberValue(&reaction->crouch_counterhit_moveid, inputs["crouch_counterhit_moveid"]);
	SetMemberValue(&reaction->left_side_moveid, inputs["left_side_moveid"]);
	SetMemberValue(&reaction->crouch_left_side_moveid, inputs["crouch_left_side_moveid"]);
	SetMemberValue(&reaction->right_side_moveid, inputs["right_side_moveid"]);
	SetMemberValue(&reaction->crouch_right_side_moveid, inputs["crouch_right_side_moveid"]);
	SetMemberValue(&reaction->backturned_moveid, inputs["backturned_moveid"]);
	SetMemberValue(&reaction->crouch_backturned_moveid, inputs["crouch_backturned_moveid"]);
	SetMemberValue(&reaction->block_moveid, inputs["block_moveid"]);
	SetMemberValue(&reaction->crouch_block_moveid, inputs["crouch_block_moveid"]);
	SetMemberValue(&reaction->wallslump_moveid, inputs["wallslump_moveid"]);
	SetMemberValue(&reaction->downed_moveid, inputs["downed_moveid"]);

	SetMemberValue(&reaction->vertical_pushback, inputs["vertical_pushback"]);
	SetMemberValue(&reaction->_0x44_int, inputs["_0x44_int"]);
	SetMemberValue(&reaction->_0x48_int, inputs["_0x48_int"]);
}

// ===== Hit conditions ===== //

std::vector<std::map<std::string, EditorInput*>> EditorT7::GetHitConditionListInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::vector<std::map<std::string, EditorInput*>> inputListMap;

	auto hitCondition = m_iterators.hit_conditions.begin() + id;
	auto req = m_iterators.requirements.begin();

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	do
	{
		std::map<std::string, EditorInput*> inputMap;

		CREATE_FIELD("requirements_addr", 0, EditorInput_PTR, hitCondition->requirements_addr);
		CREATE_FIELD("damage", 0, EditorInput_U32, hitCondition->damage);
		CREATE_FIELD("_0xC_int", 0, EditorInput_U32_Changeable, hitCondition->_0xC_int);
		CREATE_FIELD("reactions_addr", 0, EditorInput_PTR, hitCondition->reactions_addr);

		WriteFieldFullname(inputMap, "hit_condition");
		inputListMap.push_back(inputMap);

		if (req[(unsigned int)hitCondition->requirements_addr].condition == constants->at(EditorConstants_RequirementEnd)) {
			break;
		}

		hitCondition++;
	} while (true);

	return inputListMap;
}

void EditorT7::SaveHitCondition(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto hitCondition = m_iterators.hit_conditions[id];

	SetMemberValue(&hitCondition->requirements_addr, inputs["requirements_addr"]);
	SetMemberValue(&hitCondition->damage, inputs["damage"]);
	SetMemberValue(&hitCondition->_0xC_int, inputs["_0xC_int"]);
	SetMemberValue(&hitCondition->reactions_addr, inputs["reactions_addr"]);
}

bool EditorT7::ValidateHitConditionField(EditorInput* field)
{
	std::string& name = field->name;

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

std::vector<std::map<std::string, EditorInput*>> EditorT7::GetRequirementListInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::vector<std::map<std::string, EditorInput*>> inputListMap;

	auto req = m_iterators.requirements.begin() + id;

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	do
	{
		std::map<std::string, EditorInput*> inputMap;

		unsigned short conditionType =
			(req->condition >= 0x8000 ? EditorInput_H32_Changeable : EditorInput_U32_Changeable);

		CREATE_FIELD("condition", 0, conditionType, req->condition);
		CREATE_FIELD("param_unsigned", 0, EditorInput_U32_Changeable, req->param_unsigned);
		CREATE_FIELD("param_float", 0, EditorInput_Float, req->param_float);

		WriteFieldFullname(inputMap, "requirement");
		inputListMap.push_back(inputMap);
	} while ((req++)->condition != constants->at(EditorConstants_RequirementEnd));

	return inputListMap;
}


void EditorT7::SaveRequirement(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto req = m_iterators.requirements[id];

	SetMemberValue(&req->condition, inputs["condition"]);
	SetMemberValue(&req->param_unsigned, inputs["param_unsigned"]);
}

// ===== Cancel Extradata ===== //

std::map<std::string, EditorInput*> EditorT7::GetCancelExtraInput(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::map<std::string, EditorInput*> inputMap;

	auto cancelExtra = m_iterators.cancel_extras[id];

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	CREATE_FIELD("value", 0, EditorInput_H32_Changeable, cancelExtra->value);

	WriteFieldFullname(inputMap, "cancel_extra");
	return inputMap;
}

void EditorT7::SaveCancelExtra(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto cancelExtra = m_iterators.cancel_extras[id];

	SetMemberValue(&cancelExtra->value, inputs["value"]);
}

// ===== Cancel ===== //

std::vector<std::map<std::string, EditorInput*>> EditorT7::GetCancelListInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::vector<std::map<std::string, EditorInput*>> inputListMap;

	auto cancel = m_iterators.cancels.begin() + id;

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	do
	{
		std::map<std::string, EditorInput*> inputMap;

		CREATE_FIELD("command", 0, EditorInput_H64, cancel->command);
		CREATE_FIELD("requirements_addr", 0, EditorInput_PTR, cancel->requirements_addr);
		CREATE_FIELD("extradata_addr", 0, EditorInput_PTR, cancel->extradata_addr);
		CREATE_FIELD("detection_start", 0, EditorInput_U32, cancel->detection_start);
		CREATE_FIELD("detection_end", 0, EditorInput_U32, cancel->detection_end);
		CREATE_FIELD("starting_frame", 0, EditorInput_U32, cancel->starting_frame);
		CREATE_FIELD("move_id", 0, EditorInput_U16 | EditorInput_Clickable, cancel->move_id);
		CREATE_FIELD("cancel_option", 0, EditorInput_U16_Changeable, cancel->cancel_option);

		WriteFieldFullname(inputMap, "cancel");
		inputListMap.push_back(inputMap);
	} while ((cancel++)->command != constants->at(EditorConstants_CancelCommandEnd));

	return inputListMap;
}

void EditorT7::SaveCancel(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto cancel = m_iterators.cancels[id];

	SetMemberValue(&cancel->command, inputs["command"]);
	SetMemberValue(&cancel->requirements_addr, inputs["requirements_addr"]);
	SetMemberValue(&cancel->extradata_addr, inputs["extradata_addr"]);
	SetMemberValue(&cancel->detection_start, inputs["detection_start"]);
	SetMemberValue(&cancel->detection_end, inputs["detection_end"]);
	SetMemberValue(&cancel->starting_frame, inputs["starting_frame"]);
	SetMemberValue(&cancel->move_id, inputs["move_id"]);
	SetMemberValue(&cancel->cancel_option, inputs["cancel_option"]);
}

bool EditorT7::ValidateCancelField(EditorInput* field)
{
	std::string& name = field->name;

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
		if (command >= constants->at(EditorConstants_InputSequenceCommandStart)) {
			int listIdx = (unsigned int)(command - constants->at(EditorConstants_InputSequenceCommandStart));
			return listIdx < (int)m_infos->table.inputCount;
		}
	}

	return true;
}

// ===== Grouped Cancel ===== //

std::vector<std::map<std::string, EditorInput*>> EditorT7::GetGroupedCancelListInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::vector<std::map<std::string, EditorInput*>> inputListMap;

	auto cancel = m_iterators.grouped_cancels.begin() + id;

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	do
	{
		std::map<std::string, EditorInput*> inputMap;

		CREATE_FIELD("command", 0, EditorInput_H64, cancel->command);
		CREATE_FIELD("requirements_addr", 0, EditorInput_PTR, cancel->requirements_addr);
		CREATE_FIELD("extradata_addr", 0, EditorInput_PTR, cancel->extradata_addr);
		CREATE_FIELD("detection_start", 0, EditorInput_U32, cancel->detection_start);
		CREATE_FIELD("detection_end", 0, EditorInput_U32, cancel->detection_end);
		CREATE_FIELD("starting_frame", 0, EditorInput_U32, cancel->starting_frame);
		CREATE_FIELD("move_id", 0, EditorInput_U16 | EditorInput_Clickable, cancel->move_id);
		CREATE_FIELD("cancel_option", 0, EditorInput_U16_Changeable, cancel->cancel_option);

		WriteFieldFullname(inputMap, "grouped_cancel");
		inputListMap.push_back(inputMap);
	} while ((cancel++)->command != constants->at(EditorConstants_GroupedCancelCommandEnd));

	return inputListMap;
}

void EditorT7::SaveGroupedCancel(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto cancel = m_iterators.grouped_cancels[id];

	SetMemberValue(&cancel->command, inputs["command"]);
	SetMemberValue(&cancel->requirements_addr, inputs["requirements_addr"]);
	SetMemberValue(&cancel->extradata_addr, inputs["extradata_addr"]);
	SetMemberValue(&cancel->detection_start, inputs["detection_start"]);
	SetMemberValue(&cancel->detection_end, inputs["detection_end"]);
	SetMemberValue(&cancel->starting_frame, inputs["starting_frame"]);
	SetMemberValue(&cancel->move_id, inputs["move_id"]);
	SetMemberValue(&cancel->cancel_option, inputs["cancel_option"]);
}

bool EditorT7::ValidateGroupedCancelField(EditorInput* field)
{
	std::string& name = field->name;

	if (name == "move_id") {
		int moveId = atoi(field->buffer);
		if (moveId >= m_infos->table.moveCount) {
			if (moveId < 0x8000) {
				return false;
			}
			if (moveId >= (0x8000 + m_aliases->size())) {
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
		if (EditorT7::IsCommandInputSequence(field->buffer))
		{
			int listIdx = EditorT7::GetCommandInputSequenceID(field->buffer);
			return listIdx < (int)m_infos->table.inputCount;
		}
	}

	return true;
}

// ===== Other move properties (start) ===== //

std::vector<std::map<std::string, EditorInput*>> EditorT7::GetMoveStartPropertyListInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::vector<std::map<std::string, EditorInput*>> inputListMap;

	auto prop = m_iterators.move_start_properties.begin() + id;

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	do
	{
		std::map<std::string, EditorInput*> inputMap;

		CREATE_FIELD("requirements_addr", 0, EditorInput_PTR, prop->requirements_addr);
		CREATE_FIELD("extraprop", 0, EditorInput_H32_Changeable, prop->extraprop);
		CREATE_FIELD("value", 0, EditorInput_U32_Changeable, prop->value);

		WriteFieldFullname(inputMap, "move_start_extraprop");
		inputListMap.push_back(inputMap);
	} while ((prop++)->extraprop != constants->at(EditorConstants_RequirementEnd));

	return inputListMap;
}

void EditorT7::SaveMoveStartProperty(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto prop = m_iterators.move_start_properties[id];

	SetMemberValue(&prop->requirements_addr, inputs["requirements_addr"]);
	SetMemberValue(&prop->extraprop, inputs["extraprop"]);
	SetMemberValue(&prop->value, inputs["value"]);
}

// ===== Other move properties (end) ===== //

std::vector<std::map<std::string, EditorInput*>> EditorT7::GetMoveEndPropertyListInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::vector<std::map<std::string, EditorInput*>> inputListMap;

	auto prop = m_iterators.move_end_properties.begin() + id;

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	do
	{
		std::map<std::string, EditorInput*> inputMap;

		CREATE_FIELD("requirements_addr", 0, EditorInput_PTR, prop->requirements_addr);
		CREATE_FIELD("extraprop", 0, EditorInput_H32_Changeable, prop->extraprop);
		CREATE_FIELD("value", 0, EditorInput_U32_Changeable, prop->value);

		WriteFieldFullname(inputMap, "move_end_extraprop");
		inputListMap.push_back(inputMap);
	} while ((prop++)->extraprop != constants->at(EditorConstants_RequirementEnd));

	return inputListMap;
}

void EditorT7::SaveMoveEndProperty(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto prop = m_iterators.move_end_properties[id];

	SetMemberValue(&prop->requirements_addr, inputs["requirements_addr"]);
	SetMemberValue(&prop->extraprop, inputs["extraprop"]);
	SetMemberValue(&prop->value, inputs["value"]);
}

bool EditorT7::ValidateOtherMoveProperty(EditorInput* field)
{
	std::string& name = field->name;

	if (name == "requirements_addr") {
		int listIdx = atoi(field->buffer);
		return 0 <= listIdx && listIdx < (int)m_infos->table.requirementCount;
	}
	return true;
}

// ===== ExtraProperties ===== //

std::vector<std::map<std::string, EditorInput*>> EditorT7::GetExtrapropListInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::vector<std::map<std::string, EditorInput*>> inputListMap;

	auto prop = m_iterators.extra_move_properties.begin() + id;

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	do
	{
		std::map<std::string, EditorInput*> inputMap;

		CREATE_FIELD("starting_frame", 0, EditorInput_U32_Changeable, prop->starting_frame);
		CREATE_FIELD("id", 0, EditorInput_H32, prop->id);
		CREATE_FIELD("value_signed", 0, EditorInput_S32, prop->value_signed);
		CREATE_FIELD("value_unsigned", 0, EditorInput_U32, prop->value_unsigned);
		CREATE_FIELD("value_hex", 0, EditorInput_H32, prop->value_unsigned);
		CREATE_FIELD("value_float", 0, EditorInput_Float, prop->value_float);

		WriteFieldFullname(inputMap, "extraproperty");
		inputListMap.push_back(inputMap);
	} while ((prop++)->id != 0);

	return inputListMap;
}

void EditorT7::SaveExtraproperty(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto prop = m_iterators.extra_move_properties[id];

	SetMemberValue(&prop->starting_frame, inputs["starting_frame"]);
	SetMemberValue(&prop->id, inputs["id"]);
	SetMemberValue(&prop->value_unsigned, inputs["value_unsigned"]);
}

// ===== Voiceclips ===== //

std::vector<std::map<std::string, EditorInput*>> EditorT7::GetVoiceclipListInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::vector<std::map<std::string, EditorInput*>> inputListMap;

	auto voiceclip = m_iterators.voiceclips.begin() + id;

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	do
	{
		std::map<std::string, EditorInput*> inputMap;

		CREATE_FIELD("id", 0, EditorInput_H32, voiceclip->id);

		WriteFieldFullname(inputMap, "voiceclip");
		inputListMap.push_back(inputMap);
	} while ((voiceclip++)->id != (uint32_t)-1);

	return inputListMap;
}


void EditorT7::SaveVoiceclip(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto voiceclip = m_iterators.voiceclips[id];

	SetMemberValue(&voiceclip->id, inputs["id"]);
}

// ===== MOVES ===== //

std::map<std::string, EditorInput*> EditorT7::GetMoveInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::map<std::string, EditorInput*> inputMap;

	auto move = m_iterators.moves[id];

	char* nameBlock = (char*)(m_movesetData + m_header->offsets.nameBlock);

	// TODO to do TOFIX to fix : crash on move->anim_name_addr

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	CREATE_FIELD("move_name", 0, 0, nameBlock + move->name_addr);
	CREATE_FIELD("anim_name", 0, EditorInput_ClickableAlways, nameBlock + move->anim_name_addr);
	CREATE_FIELD("vulnerability", 0, EditorInput_U32, move->vuln);
	CREATE_FIELD("hitlevel", 0, EditorInput_H32, move->hitlevel);
	CREATE_FIELD("transition", 0, EditorInput_U16 | EditorInput_Clickable, move->transition);
	CREATE_FIELD("moveId_val1", 0, EditorInput_U16, move->moveId_val1);
	CREATE_FIELD("moveId_val2", 0, EditorInput_U16, move->moveId_val2);
	CREATE_FIELD("anim_len", 0, EditorInput_U32, move->anim_len);
	CREATE_FIELD("airborne_start", 0, EditorInput_U32, move->airborne_start);
	CREATE_FIELD("airborne_end", 0, EditorInput_U32, move->airborne_end);
	CREATE_FIELD("ground_fall", 0, EditorInput_U32, move->ground_fall);
	CREATE_FIELD("hitbox_location", 0, EditorInput_H32, move->hitbox_location);
	CREATE_FIELD("first_active_frame", 0, EditorInput_U32, move->first_active_frame);
	CREATE_FIELD("last_active_frame", 0, EditorInput_U32, move->last_active_frame);
	CREATE_FIELD("distance", 0, EditorInput_U16_Changeable, move->distance);

	CREATE_FIELD("cancel_addr", 2, EditorInput_PTR, move->cancel_addr);
	CREATE_FIELD("hit_condition_addr", 2, EditorInput_PTR, move->hit_condition_addr);
	CREATE_FIELD("extra_properties_addr", 2, EditorInput_PTR, move->extra_move_property_addr);
	CREATE_FIELD("move_start_extraprop_addr", 2, EditorInput_PTR, move->move_start_extraprop_addr);
	CREATE_FIELD("move_end_extraprop_addr", 2, EditorInput_PTR, move->move_end_extraprop_addr);
	CREATE_FIELD("voiceclip_addr", 2, EditorInput_PTR, move->voicelip_addr);

	CREATE_FIELD("cancel_addr_2", 3, EditorInput_PTR, move->_0x28_cancel_addr);
	CREATE_FIELD("cancel_related_id_2", 3, EditorInput_U32, move->_0x30_int__0x28_related);
	CREATE_FIELD("cancel_addr_3", 3, EditorInput_PTR, move->_0x38_cancel_addr);
	CREATE_FIELD("cancel_related_id_3", 3, EditorInput_U32, move->_0x40_int__0x38_related);
	CREATE_FIELD("cancel_addr_4", 3, EditorInput_PTR, move->_0x48_cancel_addr);
	CREATE_FIELD("cancel_related_id_4", 3, EditorInput_U32, move->_0x50_int__0x48_related);

	CREATE_FIELD("_0x34_int", 5, EditorInput_U32_Changeable, move->_0x34_int);
	CREATE_FIELD("_0x44_int", 5, EditorInput_U32_Changeable, move->_0x44_int);
	CREATE_FIELD("_0x56_short", 5, EditorInput_U16_Changeable, move->_0x56_short);
	CREATE_FIELD("_0x5C_short", 5, EditorInput_U16_Changeable, move->_0x5C_short);
	CREATE_FIELD("_0x5E_short", 5, EditorInput_U16_Changeable, move->_0x5E_short);
	CREATE_FIELD("_0x98_int", 5, EditorInput_H32_Changeable, move->_0x98_int);
	CREATE_FIELD("_0xA8_short", 5, EditorInput_U16_Changeable, move->_0xA8_short);
	CREATE_FIELD("_0xAC_short", 5, EditorInput_U16_Changeable, move->_0xAC_short);

	WriteFieldFullname(inputMap, "move");
	return inputMap;
}


void EditorT7::SaveMoveName(const char* moveName, gameAddr move_name_addr)
{
	const size_t moveNameSize = strlen(moveName) + 1;

	uint64_t newMovesetSize = 0;
	Byte* newMoveset = nullptr;

	// Find position where to insert new name
	const uint64_t moveNameOffset = m_header->infos.header_size + m_header->offsets.nameBlock + move_name_addr;
	const uint64_t moveNameEndOffset = moveNameOffset + moveNameSize;
	const uint64_t orig_moveNameEndOffset = moveNameOffset + strlen((char*)m_moveset + moveNameOffset) + 1;

	// Find the very last move name ending offset
	uint64_t lastNameEndOffset = m_header->infos.header_size + m_header->offsets.movesetBlock;
	while (*(m_moveset + (lastNameEndOffset - 2)) == 0)
	{
		// Because the block after the nameBlock needs to be aligned, we also have to find the last offset and see
		// - the impact of our insertion, see if it misaligns the ending position
		lastNameEndOffset--;
	}
	// Shift by the new size difference
	const uint64_t orig_moveNameSize = strlen((char*)m_moveset + moveNameOffset) + 1;
	const uint64_t orig_movelistStartOffset = m_header->infos.header_size + m_header->offsets.movesetBlock;
	lastNameEndOffset += moveNameSize - orig_moveNameSize;

	// Align the end of the name block to 8 bytes to get the start of the movelist block
	const uint64_t movelistStartOffset = Helpers::align8Bytes(lastNameEndOffset);

	// Now that we know the aligned size of the name block, we can finally allocate
	newMovesetSize = movelistStartOffset + (m_movesetSize - orig_movelistStartOffset);
	newMoveset = (Byte*)calloc(1, newMovesetSize);
	if (newMoveset == nullptr) {
		return;
	}

	// Shift offsets in the moveset table & in our header
	const uint64_t extraBlockSize = movelistStartOffset - orig_movelistStartOffset;

	for (int i = 0; i < _countof(m_header->offsets.blocks); ++i)
	{
		if ((m_header->infos.header_size + m_header->offsets.blocks[i]) >= orig_movelistStartOffset) {
			m_header->offsets.blocks[i] += extraBlockSize;
		}
	}

	memset(newMoveset, 0, newMovesetSize);

	// Copy start //

	memcpy(newMoveset, m_moveset, moveNameOffset);

	// Write our new name
	strcpy_s((char*)newMoveset + moveNameOffset, strlen(moveName) + 1, moveName);

	// Copy all the data after new name until the end of the name block
	memcpy(newMoveset + moveNameEndOffset, m_moveset + orig_moveNameEndOffset, orig_movelistStartOffset - orig_moveNameEndOffset);

	// Copy all the data from the start of the moveset block to the end
	memcpy(newMoveset + movelistStartOffset, m_moveset + orig_movelistStartOffset, m_movesetSize - orig_movelistStartOffset);

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);

	// Shift moveset name addr offsets
	const uint64_t extraNameSize = moveNameEndOffset - orig_moveNameEndOffset;
	for (auto& move : m_iterators.moves)
	{
		if (move.name_addr > move_name_addr) {
			move.name_addr += extraNameSize;
		}
		if (move.anim_name_addr > move_name_addr) {
			move.anim_name_addr += extraNameSize;
		}
	}
}

void EditorT7::SaveMove(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto move = m_iterators.moves[id];

	SetMemberValue(&move->vuln, inputs["vulnerability"]);
	SetMemberValue(&move->hitlevel, inputs["hitlevel"]);
	SetMemberValue(&move->transition, inputs["transition"]);
	SetMemberValue(&move->moveId_val1, inputs["moveId_val1"]);
	SetMemberValue(&move->moveId_val2, inputs["moveId_val2"]);
	SetMemberValue(&move->anim_len, inputs["anim_len"]);
	SetMemberValue(&move->airborne_start, inputs["airborne_start"]);
	SetMemberValue(&move->airborne_end, inputs["airborne_end"]);
	SetMemberValue(&move->ground_fall, inputs["ground_fall"]);
	SetMemberValue(&move->hitbox_location, inputs["hitbox_location"]);
	SetMemberValue(&move->last_active_frame, inputs["last_active_frame"]);
	SetMemberValue(&move->last_active_frame, inputs["last_active_frame"]);
	SetMemberValue(&move->distance, inputs["distance"]);

	SetMemberValue(&move->cancel_addr, inputs["cancel_addr"]);
	SetMemberValue(&move->hit_condition_addr, inputs["hit_condition_addr"]);
	SetMemberValue(&move->extra_move_property_addr, inputs["extra_properties_addr"]);
	SetMemberValue(&move->move_start_extraprop_addr, inputs["move_start_extraprop_addr"]);
	SetMemberValue(&move->move_end_extraprop_addr, inputs["move_end_extraprop_addr"]);
	SetMemberValue(&move->voicelip_addr, inputs["voiceclip_addr"]);

	SetMemberValue(&move->_0x28_cancel_addr, inputs["cancel_addr_2"]);
	SetMemberValue(&move->_0x30_int__0x28_related, inputs["cancel_related_id_2"]);
	SetMemberValue(&move->_0x38_cancel_addr, inputs["cancel_addr_3"]);
	SetMemberValue(&move->_0x40_int__0x38_related, inputs["cancel_related_id_3"]);
	SetMemberValue(&move->_0x48_cancel_addr, inputs["cancel_addr_4"]);
	SetMemberValue(&move->_0x50_int__0x48_related, inputs["cancel_related_id_4"]);

	SetMemberValue(&move->_0x34_int, inputs["_0x34_int"]);
	SetMemberValue(&move->_0x44_int, inputs["_0x44_int"]);
	SetMemberValue(&move->_0x56_short, inputs["_0x56_short"]);
	SetMemberValue(&move->_0x5C_short, inputs["_0x5C_short"]);
	SetMemberValue(&move->_0x5E_short, inputs["_0x5E_short"]);
	SetMemberValue(&move->_0x98_int, inputs["_0x98_int"]);
	SetMemberValue(&move->_0xA8_short, inputs["_0xA8_short"]);
	SetMemberValue(&move->_0xAC_short, inputs["_0xAC_short"]);

	if (m_animNameToOffsetMap->find(inputs["anim_name"]->buffer) != m_animNameToOffsetMap->end()) {
		// Todo: if old animation is unused, delete it
		move->anim_addr = m_animNameToOffsetMap->at(inputs["anim_name"]->buffer);
	}

	// Save move name at the end because it may imply reallocation and invalidation of existing pointers
	char* newName = inputs["move_name"]->buffer;
	char* namePtr = (char*)(m_movesetData + m_header->offsets.nameBlock);

	if (strlen(newName) != strlen(namePtr + move->name_addr)) {
		// Only re-allocate moveset & shift offsets if the length doesn't match
		// todo: detect name collision
		SaveMoveName(newName, move->name_addr);
	}
	else {
		strcpy_s(namePtr + move->name_addr, strlen(newName) + 1, newName);
	}

	// In case we updated the name, reload the movelist display
	ReloadDisplayableMoveList();
}

bool EditorT7::ValidateMoveField(EditorInput* field)
{
	std::string& name = field->name;

	if (name == "anim_name") {
		return m_animNameToOffsetMap->find(field->buffer) != m_animNameToOffsetMap->end();
	}

	else if (Helpers::startsWith(name, "cancel_addr")) {
		int listIdx = atoi(field->buffer);
		return -1 <= listIdx && listIdx < (int)m_infos->table.cancelCount;
	}

	else if (name == "hit_condition_addr") {
		int listIdx = atoi(field->buffer);
		return -1 <= listIdx && listIdx < (int)m_infos->table.hitConditionCount;
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
			if (moveId >= (0x8000 + m_aliases->size())) {
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

void EditorT7::SaveItem(EditorWindowType_ type, uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	// Saving is one on an individual item basis, even for lists
	switch (type)
	{
	case EditorWindowType_Move:
		SaveMove(id, inputs);
		break;
	case EditorWindowType_Voiceclip:
		SaveVoiceclip(id, inputs);
		break;
	case EditorWindowType_Extraproperty:
		SaveExtraproperty(id, inputs);
		break;
	case EditorWindowType_Cancel:
		SaveCancel(id, inputs);
		break;
	case EditorWindowType_CancelExtradata:
		SaveCancelExtra(id, inputs);
		break;
	case EditorWindowType_GroupedCancel:
		SaveGroupedCancel(id, inputs);
		break;
	case EditorWindowType_Requirement:
		SaveRequirement(id, inputs);
		break;
	case EditorWindowType_HitCondition:
		SaveHitCondition(id, inputs);
		break;
	case EditorWindowType_Reactions:
		SaveReactions(id, inputs);
		break;
	case EditorWindowType_Pushback:
		SavePushback(id, inputs);
		break;
	case EditorWindowType_PushbackExtradata:
		SavePushbackExtra(id, inputs);
		break;
	case EditorWindowType_MoveBeginProperty:
		SaveMoveStartProperty(id, inputs);
		break;
	case EditorWindowType_MoveEndProperty:
		SaveMoveEndProperty(id, inputs);
		break;
	case EditorWindowType_InputSequence:
		SaveInputSequence(id, inputs);
		break;
	case EditorWindowType_Input:
		SaveInput(id, inputs);
		break;
	case EditorWindowType_Projectile:
		SaveProjectile(id, inputs);
		break;
	case EditorWindowType_ThrowCamera:
		SaveThrowCamera(id, inputs);
		break;
	case EditorWindowType_CameraData:
		SaveCameraData(id, inputs);
		break;
	case EditorWindowType_MovelistDisplayable:
		SaveMovelistDisplayable(id, inputs);
		break;
	case EditorWindowType_MovelistPlayable:
		SaveMovelistPlayable(id, inputs);
		break;
	case EditorWindowType_MovelistInput:
		SaveMovelistInput(id, inputs);
		break;
	}
}

std::map<std::string, EditorInput*> EditorT7::GetFormFields(EditorWindowType_ type, uint16_t id, VectorSet<std::string>& drawOrder)
{
	// Builds an input list for a specific window type (single struct) and fill default values according to the given ID
	switch (type)
	{
	case EditorWindowType_Move:
		return GetMoveInputs(id, drawOrder);
		break;
	case EditorWindowType_CancelExtradata:
		return GetCancelExtraInput(id, drawOrder);
		break;
	case EditorWindowType_Reactions:
		return GetReactionsInputs(id, drawOrder);
		break;
	case EditorWindowType_Pushback:
		return GetPushbackInputs(id, drawOrder);
		break;
	case EditorWindowType_InputSequence:
		return GetInputSequenceInputs(id, drawOrder);
		break;
	case EditorWindowType_Projectile:
		return GetProjectileInputs(id, drawOrder);
		break;
	case EditorWindowType_ThrowCamera:
		return GetThrowCameraInputs(id, drawOrder);
		break;
	case EditorWindowType_CameraData:
		return GetCameraDataInputs(id, drawOrder);
		break;
	case EditorWindowType_MovelistPlayable:
		return GetMovelistPlayableInputs(id, drawOrder);
		break;
	}
	return std::map<std::string, EditorInput*>();
}

std::vector<std::map<std::string, EditorInput*>> EditorT7::GetFormFieldsList(EditorWindowType_ type, uint16_t id, VectorSet<std::string>& drawOrder)
{
	// Builds an input list for a specific window type (list of structs) and fill default values according to the given ID
	switch (type)
	{
	case EditorWindowType_Extraproperty:
		return GetExtrapropListInputs(id, drawOrder);
		break;
	case EditorWindowType_MoveBeginProperty:
		return GetMoveStartPropertyListInputs(id, drawOrder);
		break;
	case EditorWindowType_MoveEndProperty:
		return GetMoveEndPropertyListInputs(id, drawOrder);
		break;
	case EditorWindowType_Cancel:
		return GetCancelListInputs(id, drawOrder);
		break;
	case EditorWindowType_GroupedCancel:
		return GetGroupedCancelListInputs(id, drawOrder);
		break;
	case EditorWindowType_Requirement:
		return GetRequirementListInputs(id, drawOrder);
		break;
	case EditorWindowType_HitCondition:
		return GetHitConditionListInputs(id, drawOrder);
		break;
	case EditorWindowType_Voiceclip:
		return GetVoiceclipListInputs(id, drawOrder);
		break;

	case EditorWindowType_MovelistDisplayable:
		return GetMovelistDisplayablesInputs(0, drawOrder);
		break;
	}
	return std::vector<std::map<std::string, EditorInput*>>();
}

std::vector<std::map<std::string, EditorInput*>> EditorT7::GetFormFieldsList(EditorWindowType_ type, uint16_t id, VectorSet<std::string>& drawOrder, int listSize)
{
	// Specific to lists with unguessable sizes : we provide the size as an argument
	switch (type)
	{
	case EditorWindowType_Input:
		return GetInputListInputs(id, listSize, drawOrder);
		break;
	case EditorWindowType_PushbackExtradata:
		return GetPushbackExtraListInputs(id, listSize, drawOrder);
		break;

	case EditorWindowType_MovelistInput:
		return GetMovelistInputListInputs(id, listSize, drawOrder);
		break;
	}
	return GetFormFieldsList(type, id, drawOrder);
}

bool EditorT7::ValidateField(EditorWindowType_ fieldType, EditorInput* field)
{
	if (!ValidateFieldType(field)) {
		return false;
	}

	switch (fieldType)
	{
	case EditorWindowType_Move:
		return ValidateMoveField(field);
		break;
	case EditorWindowType_Cancel:
		return ValidateCancelField(field);
		break;
	case EditorWindowType_GroupedCancel:
		return ValidateGroupedCancelField(field);
		break;
	case EditorWindowType_HitCondition:
		return ValidateHitConditionField(field);
		break;
	case EditorWindowType_Reactions:
		return ValidateReactionsField(field);
		break;
	case EditorWindowType_Pushback:
		return ValidatePushbackField(field);
		break;
	case EditorWindowType_MoveBeginProperty:
	case EditorWindowType_MoveEndProperty:
		return ValidateOtherMoveProperty(field);
		break;
	case EditorWindowType_InputSequence:
		return ValidateInputSequenceField(field);
		break;
	case EditorWindowType_Projectile:
		return ValidateProjectileField(field);
		break;
	case EditorWindowType_ThrowCamera:
		return ValidateThrowCameraField(field);
		break;
	case EditorWindowType_MovelistDisplayable:
		return ValidateMovelistDisplayableField(field);
		break;
	}

	return true;
}
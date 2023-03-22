#include <map>
#include <format>
#include <fstream>

# include "Editor_t7.hpp"
# include "Helpers.hpp"

using namespace EditorUtils;

#define gAddr StructsT7_gameAddr
#define SET_DEFAULT_VAL(fieldName, format, value) sprintf_s(inputMap[fieldName]->buffer, FORM_INPUT_BUFSIZE, format, value)
#define CREATE_STRING_FIELD(a, c, d, g) drawOrder.push_back(a), inputMap[a] = new EditorInput { .category = c, .imguiInputFlags = 0, .flags = EditorInput_String | d }, SET_DEFAULT_VAL(a, "%s", g)
#define CREATE_FIELD(a, c, e, g) drawOrder.push_back(a), inputMap[a] = new EditorInput { .category = c, .imguiInputFlags = GetFieldCharset(e), .flags = e }, SET_DEFAULT_VAL(a, GetFieldFormat(e), g), SetInputfieldColor(inputMap[a])

static void WriteFieldFullname(std::map<std::string, EditorInput*>& inputMap, std::string baseIdentifier)
{
	// Finishing touch
	for (auto& [name, input] : inputMap) {
		// Duplicate the name inside the structure, this is more convenient for me in some places, helps writing a lot shorter code
		input->name = name;
		input->fullName = "edition." + baseIdentifier + "." + name;
	}
}

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

		CREATE_FIELD("horizontal_offset", 0, EditorInput_U16, pushbackExtra->horizontal_offset);

		WriteFieldFullname(inputMap, "pushback_extradata");
		inputListMap.push_back(inputMap);
		pushbackExtra++;
	} while (--listSize > 0);

	return inputListMap;
}

void EditorT7::SavePushbackExtra(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto pushbackExtra = m_iterators.pushback_extras[id];

	pushbackExtra->horizontal_offset = GetFieldValue(inputs["horizontal_offset"]);
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
	CREATE_FIELD("displacement", 0, EditorInput_U16, pushback->displacement);
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

	pushback->duration = GetFieldValue(inputs["duration"]);
	pushback->displacement = GetFieldValue(inputs["displacement"]);
	pushback->num_of_loops = GetFieldValue(inputs["num_of_loops"]);
	pushback->extradata_addr = GetFieldValue(inputs["extradata_addr"]);
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

	CREATE_FIELD("_0x4_int", 3, EditorInput_U32, projectile->_0x4_int);
	CREATE_FIELD("_0xC_int", 3, EditorInput_U32, projectile->_0xC_int);
	CREATE_FIELD("_0x10_int", 3, EditorInput_U32, projectile->_0x10_int);
	CREATE_FIELD("_0x14_int", 3, EditorInput_U32, projectile->_0x14_int);
	CREATE_FIELD("_0x24_int", 3, EditorInput_U32, projectile->_0x24_int);
	CREATE_FIELD("_0x34_int", 3, EditorInput_U32, projectile->_0x34_int);
	CREATE_FIELD("_0x3C_int_1", 3, EditorInput_U32, projectile->_0x3C_int[0]);
	CREATE_FIELD("_0x3C_int_2", 3, EditorInput_U32, projectile->_0x3C_int[1]);
	CREATE_FIELD("_0x3C_int_3", 3, EditorInput_U32, projectile->_0x3C_int[2]);
	CREATE_FIELD("_0x3C_int_4", 3, EditorInput_U32, projectile->_0x3C_int[3]);
	CREATE_FIELD("_0x3C_int_5", 3, EditorInput_U32, projectile->_0x3C_int[4]);
	CREATE_FIELD("_0x3C_int_6", 3, EditorInput_U32, projectile->_0x3C_int[5]);
	CREATE_FIELD("_0x58_int", 3, EditorInput_U32, projectile->_0x58_int);
	CREATE_FIELD("_0x5C_int", 3, EditorInput_U32, projectile->_0x5C_int);
	CREATE_FIELD("_0x70_int", 3, EditorInput_U32, projectile->_0x58_int);
	CREATE_FIELD("_0x74_int", 3, EditorInput_U32, projectile->_0x74_int);
	CREATE_FIELD("_0x7C_int", 3, EditorInput_U32, projectile->_0x7C_int);
	CREATE_FIELD("_0x80_int", 3, EditorInput_U32, projectile->_0x80_int);
	CREATE_FIELD("_0x88_int_1", 3, EditorInput_U32, projectile->_0x88_int[0]);
	CREATE_FIELD("_0x88_int_2", 3, EditorInput_U32, projectile->_0x88_int[1]);
	CREATE_FIELD("_0x88_int_3", 3, EditorInput_U32, projectile->_0x88_int[2]);
	CREATE_FIELD("_0x88_int_4", 3, EditorInput_U32, projectile->_0x88_int[3]);
	CREATE_FIELD("_0x88_int_5", 3, EditorInput_U32, projectile->_0x88_int[4]);
	CREATE_FIELD("_0x88_int_6", 3, EditorInput_U32, projectile->_0x88_int[5]);
	CREATE_FIELD("_0x88_int_7", 3, EditorInput_U32, projectile->_0x88_int[6]);
	CREATE_FIELD("_0x88_int_8", 3, EditorInput_U32, projectile->_0x88_int[7]);

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

	projectile->vfx_id = GetFieldValue(inputs["vfx_id"]);
	projectile->vfx_variation_id = GetFieldValue(inputs["vfx_variation_id"]);
	projectile->delay = GetFieldValue(inputs["delay"]);
	projectile->vertical_velocity = GetFieldValue(inputs["vertical_velocity"]);
	projectile->horizontal_velocity = GetFieldValue(inputs["horizontal_velocity"]);
	projectile->duration = GetFieldValue(inputs["duration"]);
	projectile->no_collision = GetFieldValue(inputs["no_collision"]);
	projectile->size = GetFieldValue(inputs["size"]);
	projectile->hit_level = GetFieldValue(inputs["hit_level"]);
	projectile->voiceclip_on_hit = GetFieldValue(inputs["voiceclip_on_hit"]);
	projectile->can_hitbox_connect = GetFieldValue(inputs["can_hitbox_connect"]);
	projectile->gravity = GetFieldValue(inputs["gravity"]);

	projectile->hit_condition_addr = GetFieldValue(inputs["hit_condition_addr"]);
	projectile->cancel_addr = GetFieldValue(inputs["cancel_addr"]);

	projectile->_0x34_int = GetFieldValue(inputs["_0x34_int"]);
	projectile->_0x4_int = GetFieldValue(inputs["_0x4_int"]);
	projectile->_0xC_int = GetFieldValue(inputs["_0xC_int"]);
	projectile->_0x10_int = GetFieldValue(inputs["_0x10_int"]);
	projectile->_0x14_int = GetFieldValue(inputs["_0x14_int"]);
	projectile->_0x24_int = GetFieldValue(inputs["_0x24_int"]);
	projectile->_0x3C_int[0] = GetFieldValue(inputs["_0x3C_int_1"]);
	projectile->_0x3C_int[1] = GetFieldValue(inputs["_0x3C_int_2"]);
	projectile->_0x3C_int[2] = GetFieldValue(inputs["_0x3C_int_3"]);
	projectile->_0x3C_int[3] = GetFieldValue(inputs["_0x3C_int_4"]);
	projectile->_0x3C_int[4] = GetFieldValue(inputs["_0x3C_int_5"]);
	projectile->_0x3C_int[5] = GetFieldValue(inputs["_0x3C_int_6"]);
	projectile->_0x58_int = GetFieldValue(inputs["_0x58_int"]);
	projectile->_0x5C_int = GetFieldValue(inputs["_0x5C_int"]);
	projectile->_0x70_int = GetFieldValue(inputs["_0x70_int"]);
	projectile->_0x74_int = GetFieldValue(inputs["_0x74_int"]);
	projectile->_0x7C_int = GetFieldValue(inputs["_0x7C_int"]);
	projectile->_0x80_int = GetFieldValue(inputs["_0x80_int"]);
	projectile->_0x88_int[0] = GetFieldValue(inputs["_0x88_int_1"]);
	projectile->_0x88_int[1] = GetFieldValue(inputs["_0x88_int_2"]);
	projectile->_0x88_int[2] = GetFieldValue(inputs["_0x88_int_3"]);
	projectile->_0x88_int[3] = GetFieldValue(inputs["_0x88_int_4"]);
	projectile->_0x88_int[4] = GetFieldValue(inputs["_0x88_int_5"]);
	projectile->_0x88_int[5] = GetFieldValue(inputs["_0x88_int_6"]);
	projectile->_0x88_int[6] = GetFieldValue(inputs["_0x88_int_7"]);
	projectile->_0x88_int[7] = GetFieldValue(inputs["_0x88_int_8"]);
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

	sequence->input_window_frames = GetFieldValue(inputs["input_window_frames"]);
	sequence->input_amount = GetFieldValue(inputs["input_amount"]);
	sequence->_0x4_int = GetFieldValue(inputs["_0x4_int"]);
	sequence->input_addr = GetFieldValue(inputs["input_addr"]);
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

	input->direction = GetFieldValue(inputs["direction"]);
	input->button = GetFieldValue(inputs["button"]);
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

	CREATE_FIELD("front_direction", 5, EditorInput_U16, reaction->front_direction);
	CREATE_FIELD("back_direction", 5, EditorInput_U16, reaction->back_direction);
	CREATE_FIELD("left_side_direction", 5, EditorInput_U16, reaction->left_side_direction);
	CREATE_FIELD("right_side_direction", 5, EditorInput_U16, reaction->right_side_direction);
	CREATE_FIELD("front_counterhit_direction", 5, EditorInput_U16, reaction->front_counterhit_direction);
	CREATE_FIELD("downed_direction", 5, EditorInput_U16, reaction->downed_direction);

	CREATE_FIELD("vertical_pushback", 7, EditorInput_U16, reaction->vertical_pushback);
	CREATE_FIELD("_0x44_int", 7, EditorInput_U32, reaction->_0x44_int);
	CREATE_FIELD("_0x48_int", 7, EditorInput_U32, reaction->_0x48_int);

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
			if (moveId >= (0x8000 + m_aliases.size())) {
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

	reaction->front_pushback = GetFieldValue(inputs["front_pushback"]);
	reaction->backturned_pushback = GetFieldValue(inputs["backturned_pushback"]);
	reaction->left_side_pushback = GetFieldValue(inputs["left_side_pushback"]);
	reaction->right_side_pushback = GetFieldValue(inputs["right_side_pushback"]);
	reaction->front_counterhit_pushback = GetFieldValue(inputs["front_counterhit_pushback"]);
	reaction->downed_pushback = GetFieldValue(inputs["downed_pushback"]);
	reaction->block_pushback = GetFieldValue(inputs["block_pushback"]);

	reaction->front_direction = GetFieldValue(inputs["front_direction"]);
	reaction->back_direction = GetFieldValue(inputs["back_direction"]);
	reaction->left_side_direction = GetFieldValue(inputs["left_side_direction"]);
	reaction->right_side_direction = GetFieldValue(inputs["right_side_direction"]);
	reaction->front_counterhit_direction = GetFieldValue(inputs["front_counterhit_direction"]);
	reaction->downed_direction = GetFieldValue(inputs["downed_direction"]);

	reaction->default_moveid = GetFieldValue(inputs["default_moveid"]);
	reaction->standing_moveid = GetFieldValue(inputs["standing_moveid"]);
	reaction->crouch_moveid = GetFieldValue(inputs["crouch_moveid"]);
	reaction->counterhit_moveid = GetFieldValue(inputs["counterhit_moveid"]);
	reaction->crouch_counterhit_moveid = GetFieldValue(inputs["crouch_counterhit_moveid"]);
	reaction->left_side_moveid = GetFieldValue(inputs["left_side_moveid"]);
	reaction->crouch_left_side_moveid = GetFieldValue(inputs["crouch_left_side_moveid"]);
	reaction->right_side_moveid = GetFieldValue(inputs["right_side_moveid"]);
	reaction->crouch_right_side_moveid = GetFieldValue(inputs["crouch_right_side_moveid"]);
	reaction->backturned_moveid = GetFieldValue(inputs["backturned_moveid"]);
	reaction->crouch_backturned_moveid = GetFieldValue(inputs["crouch_backturned_moveid"]);
	reaction->block_moveid = GetFieldValue(inputs["block_moveid"]);
	reaction->crouch_block_moveid = GetFieldValue(inputs["crouch_block_moveid"]);
	reaction->wallslump_moveid = GetFieldValue(inputs["wallslump_moveid"]);
	reaction->downed_moveid = GetFieldValue(inputs["downed_moveid"]);

	reaction->vertical_pushback = GetFieldValue(inputs["vertical_pushback"]);
	reaction->_0x44_int = GetFieldValue(inputs["_0x44_int"]);
	reaction->_0x48_int = GetFieldValue(inputs["_0x48_int"]);
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
		CREATE_FIELD("_0xC_int", 0, EditorInput_U32, hitCondition->_0xC_int);
		CREATE_FIELD("reactions_addr", 0, EditorInput_PTR, hitCondition->reactions_addr);

		WriteFieldFullname(inputMap, "hit_condition");
		inputListMap.push_back(inputMap);

		if (req[hitCondition->requirements_addr].condition == constants[EditorConstants_RequirementEnd]) {
			break;
		}

		hitCondition++;
	} while (true);

	return inputListMap;
}

void EditorT7::SaveHitCondition(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto hitCondition = m_iterators.hit_conditions[id];

	hitCondition->requirements_addr = GetFieldValue(inputs["requirements_addr"]);
	hitCondition->damage = GetFieldValue(inputs["damage"]);
	hitCondition->_0xC_int = GetFieldValue(inputs["_0xC_int"]);
	hitCondition->reactions_addr = GetFieldValue(inputs["reactions_addr"]);
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

		CREATE_FIELD("condition", 0, EditorInput_U32, req->condition);
		CREATE_FIELD("param", 0, EditorInput_U32, req->param);

		WriteFieldFullname(inputMap, "requirement");
		inputListMap.push_back(inputMap);
	} while ((req++)->condition != constants[EditorConstants_RequirementEnd]);

	return inputListMap;
}


void EditorT7::SaveRequirement(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto req = m_iterators.requirements[id];

	req->condition = GetFieldValue(inputs["condition"]);
	req->param = GetFieldValue(inputs["param"]);
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
	CREATE_FIELD("value", 0, EditorInput_H32, cancelExtra->value);

	WriteFieldFullname(inputMap, "cancel_extra");
	return inputMap;
}

void EditorT7::SaveCancelExtra(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto cancelExtra = m_iterators.cancel_extras[id];

	cancelExtra->value = GetFieldValue(inputs["value"]);
}

// ===== Cancel ===== //

std::vector<std::map<std::string, EditorInput*>> EditorT7::GetCancelListInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::vector<std::map<std::string, EditorInput*>> inputListMap;

	auto cancel= m_iterators.cancels.begin() + id;

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
		CREATE_FIELD("cancel_option", 0, EditorInput_U16, cancel->cancel_option);

		WriteFieldFullname(inputMap, "cancel");
		inputListMap.push_back(inputMap);
	} while ((cancel++)->command != constants[EditorConstants_CancelCommandEnd]);
	
	return inputListMap;
}

void EditorT7::SaveCancel(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto cancel = m_iterators.cancels[id];

	cancel->command = GetFieldValue(inputs["command"]);
	cancel->requirements_addr = GetFieldValue(inputs["requirements_addr"]);
	cancel->extradata_addr = GetFieldValue(inputs["extradata_addr"]);
	cancel->detection_start = GetFieldValue(inputs["detection_start"]);
	cancel->detection_end = GetFieldValue(inputs["detection_end"]);
	cancel->starting_frame = GetFieldValue(inputs["starting_frame"]);
	cancel->move_id = GetFieldValue(inputs["move_id"]);
	cancel->cancel_option = GetFieldValue(inputs["cancel_option"]);
}

bool EditorT7::ValidateCancelField(EditorInput* field)
{
	std::string& name = field->name;

	// move_id is not validated here but in the cancel class since it can serve as both a group cancel & move id
	if (name == "requirements_addr") {
		int listIdx = atoi(field->buffer);
		// No negative allowed here
		return 0 <= listIdx && listIdx < (int)m_infos->table.requirementCount;
	} else if (name == "extradata_addr") {
		int listIdx = atoi(field->buffer);
		// No negative allowed here
		return 0 <= listIdx && listIdx < (int)m_infos->table.cancelExtradataCount;
	}
	else if (name == "command") {
		uint64_t command = (uint64_t)strtoll(field->buffer, nullptr, 16) & 0xFFFFFFFF;
		if (command >= constants[EditorConstants_InputSequenceCommandStart]) {
			int listIdx = command - constants[EditorConstants_InputSequenceCommandStart];
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
		CREATE_FIELD("cancel_option", 0, EditorInput_U16, cancel->cancel_option);

		WriteFieldFullname(inputMap, "grouped_cancel");
		inputListMap.push_back(inputMap);
	} while ((cancel++)->command != constants[EditorConstants_GroupedCancelCommandEnd]);

	return inputListMap;
}

void EditorT7::SaveGroupedCancel(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto cancel = m_iterators.grouped_cancels[id];

	cancel->command = GetFieldValue(inputs["command"]);
	cancel->requirements_addr = GetFieldValue(inputs["requirements_addr"]);
	cancel->extradata_addr = GetFieldValue(inputs["extradata_addr"]);
	cancel->detection_start = GetFieldValue(inputs["detection_start"]);
	cancel->detection_end = GetFieldValue(inputs["detection_end"]);
	cancel->starting_frame = GetFieldValue(inputs["starting_frame"]);
	cancel->move_id = GetFieldValue(inputs["move_id"]);
	cancel->cancel_option = GetFieldValue(inputs["cancel_option"]);
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
			if (moveId >= (0x8000 + m_aliases.size())) {
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
		uint64_t command = (uint64_t)strtoll(field->buffer, nullptr, 16) & 0xFFFFFFFF;
		if (command >= constants[EditorConstants_InputSequenceCommandStart]) {
			int listIdx = command - constants[EditorConstants_InputSequenceCommandStart];
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
		CREATE_FIELD("extraprop", 0, EditorInput_H32, prop->extraprop);
		CREATE_FIELD("value", 0, EditorInput_U32, prop->value);

		WriteFieldFullname(inputMap, "move_start_property");
		inputListMap.push_back(inputMap);
	} while ((prop++)->extraprop != constants[EditorConstants_RequirementEnd]);

	return inputListMap;
}

void EditorT7::SaveMoveStartProperty(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto prop = m_iterators.move_start_properties[id];

	prop->requirements_addr = GetFieldValue(inputs["requirements_addr"]);
	prop->extraprop = GetFieldValue(inputs["extraprop"]);
	prop->value = GetFieldValue(inputs["value"]);
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
		CREATE_FIELD("extraprop", 0, EditorInput_H32, prop->extraprop);
		CREATE_FIELD("value", 0, EditorInput_U32, prop->value);

		WriteFieldFullname(inputMap, "move_end_property");
		inputListMap.push_back(inputMap);
	} while ((prop++)->extraprop != constants[EditorConstants_RequirementEnd]);

	return inputListMap;
}

void EditorT7::SaveMoveEndProperty(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto prop = m_iterators.move_end_properties[id];

	prop->requirements_addr = GetFieldValue(inputs["requirements_addr"]);
	prop->extraprop = GetFieldValue(inputs["extraprop"]);
	prop->value = GetFieldValue(inputs["value"]);
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

		CREATE_FIELD("starting_frame", 0, EditorInput_U32, prop->starting_frame);
		CREATE_FIELD("id", 0, EditorInput_H32, prop->id);
		CREATE_FIELD("value", 0, EditorInput_U32, prop->value);

		WriteFieldFullname(inputMap, "extraproperty");
		inputListMap.push_back(inputMap);
	} while ((prop++)->starting_frame != 0);

	return inputListMap;
}

void EditorT7::SaveExtraproperty(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto prop = m_iterators.extra_move_properties[id];

	prop->starting_frame = GetFieldValue(inputs["starting_frame"]);
	prop->id = GetFieldValue(inputs["id"]);
	prop->value = GetFieldValue(inputs["value"]);
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
	} while ((voiceclip++)->id != (uint32_t)- 1);

	return inputListMap;
}


void EditorT7::SaveVoiceclip(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto voiceclip = m_iterators.voiceclips[id];

	voiceclip->id = GetFieldValue(inputs["id"]);
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
	CREATE_STRING_FIELD("move_name", 0, 0, nameBlock + move->name_addr);
	CREATE_STRING_FIELD("anim_name", 0, EditorInput_ClickableAlways, nameBlock + move->anim_name_addr);
	CREATE_FIELD("vulnerability", 0, EditorInput_U32, move->vuln);
	CREATE_FIELD("hitlevel", 0, EditorInput_H32, move->hitlevel);
	CREATE_FIELD("transition", 0, EditorInput_U16 | EditorInput_Clickable, move->transition);
	CREATE_FIELD("moveId_val1", 0, EditorInput_U16, move->moveId_val1);
	CREATE_FIELD("moveId_val2", 0, EditorInput_U16, move->moveId_val2);
	CREATE_FIELD("anim_len", 0, EditorInput_U32, move->anim_len);
	CREATE_FIELD("airborne_start", 0, EditorInput_U32, move->airborne_start);
	CREATE_FIELD("airborne_end", 0, EditorInput_U32, move->airborne_end);
	CREATE_FIELD("ground_fall", 0, EditorInput_U32, move->ground_fall);
	CREATE_FIELD("hitbox_location", 0 , EditorInput_H32, move->hitbox_location);
	CREATE_FIELD("first_active_frame", 0, EditorInput_U32, move->first_active_frame);
	CREATE_FIELD("last_active_frame", 0, EditorInput_U32, move->last_active_frame);
	CREATE_FIELD("distance", 0, EditorInput_U16, move->distance);

	CREATE_FIELD("cancel_addr", 2, EditorInput_PTR,  move->cancel_addr);
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

	CREATE_FIELD("_0x34_int", 5, EditorInput_U32, move->_0x34_int);
	CREATE_FIELD("_0x44_int", 5, EditorInput_U32, move->_0x44_int);
	CREATE_FIELD("_0x56_short", 5, EditorInput_U16, move->_0x56_short);
	CREATE_FIELD("_0x5C_short", 5, EditorInput_U16, move->_0x5C_short);
	CREATE_FIELD("_0x5E_short", 5, EditorInput_U16, move->_0x5E_short);
	CREATE_FIELD("_0x98_int", 5, EditorInput_U32, move->_0x98_int);
	CREATE_FIELD("_0xA8_short", 5, EditorInput_U16, move->_0xA8_short);
	CREATE_FIELD("_0xAC_short", 5, EditorInput_U16, move->_0xAC_short);

	WriteFieldFullname(inputMap, "move");
	return inputMap;
}

uint64_t EditorT7::CreateMoveName(const char* moveName)
{
	const size_t moveNameSize = strlen(moveName) + 1;

	uint64_t newMovesetSize = 0;
	Byte* newMoveset = nullptr;

	// Find position where to insert new name
	uint64_t moveNameOffset = sizeof(TKMovesetHeader) + m_header->offsets.movesetBlock;
	const uint64_t orig_moveNameEndOffset = moveNameOffset;
	while (*(m_moveset + (moveNameOffset - 2)) == 0)
	{
		// Have to find the insert offset backward because the name block is always aligned to 8 bytes
		// We want to erase as many empty bytes because of past alignment and then re-align to 8 bytes
		moveNameOffset--;
	}

	const uint64_t relativeMoveNameOffset = moveNameOffset - m_header->offsets.nameBlock - sizeof(TKMovesetHeader);
	const uint64_t moveNameEndOffset = Helpers::align8Bytes(moveNameOffset + moveNameSize);

	// Because of 8 bytes alignment, we can only calcualte the new size after knowing where to write everything
	newMovesetSize = moveNameEndOffset + (m_movesetSize - orig_moveNameEndOffset);
	newMoveset = (Byte*)calloc(1, newMovesetSize);
	if (newMoveset == nullptr) {
		return 0;
	}


	// Shift offsets now that we know the extra allocated size. Need to do it before LoadMovesetPtr.
	const uint64_t extraNameSize = moveNameEndOffset - orig_moveNameEndOffset;
	m_header->offsets.movesetBlock += extraNameSize;
	m_header->offsets.animationBlock += extraNameSize;
	m_header->offsets.motaBlock += extraNameSize;

	// Copy start //

	memcpy(newMoveset, m_moveset, moveNameOffset);

	// Write our new name
	memcpy(newMoveset + moveNameOffset, moveName, moveNameSize);

	// Copy all the data after our name
	memcpy(newMoveset + moveNameEndOffset, m_moveset + orig_moveNameEndOffset, m_movesetSize - orig_moveNameEndOffset);

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);

	return relativeMoveNameOffset;
}

void EditorT7::SaveMoveName(const char* moveName, gameAddr move_name_addr)
{
	const size_t moveNameSize = strlen(moveName) + 1;

	uint64_t newMovesetSize = 0;
	Byte* newMoveset = nullptr;

	// Find position where to insert new name
	const uint64_t moveNameOffset = sizeof(TKMovesetHeader) + m_header->offsets.nameBlock + move_name_addr;
	const uint64_t moveNameEndOffset = moveNameOffset + moveNameSize;
	const uint64_t orig_moveNameEndOffset = moveNameOffset + strlen((char*)m_moveset + moveNameOffset) + 1;

	// Find the very last move name ending offset
	uint64_t lastNameEndOffset = sizeof(TKMovesetHeader) + m_header->offsets.movesetBlock;
	while (*(m_moveset + (lastNameEndOffset - 2)) == 0)
	{
		// Because the block after the nameBlock needs to be aligned, we also have to find the last offset and see
		// - the impact of our insertion, see if it misaligns the ending position
		lastNameEndOffset--;
	}
	// Shift by the new size difference
	const uint64_t orig_moveNameSize = strlen((char*)m_moveset + moveNameOffset) + 1;
	const uint64_t orig_movelistStartOffset = sizeof(TKMovesetHeader) + m_header->offsets.movesetBlock;
	lastNameEndOffset += moveNameSize - orig_moveNameSize;

	// Align the end of the name block to 8 bytes to get the start of the movelist block
	const uint64_t movelistStartOffset = Helpers::align8Bytes(lastNameEndOffset);

	// Now that we know the aligned size of the name block, we can finally allocate
	newMovesetSize = movelistStartOffset + (m_movesetSize - orig_movelistStartOffset);
	newMoveset = (Byte*)calloc(1, newMovesetSize);
	if (newMoveset == nullptr) {
		return ;
	}
	memset(newMoveset, 0, newMovesetSize);

	// Copy start //

	memcpy(newMoveset, m_moveset, moveNameOffset);

	// Write our new name
	strcpy((char*)newMoveset + moveNameOffset, moveName);

	// Copy all the data after new name until the end of the name block
	memcpy(newMoveset + moveNameEndOffset, m_moveset + orig_moveNameEndOffset, orig_movelistStartOffset - orig_moveNameEndOffset);

	// Copy all the data from the start of the moveset block to the end
	memcpy(newMoveset + movelistStartOffset, m_moveset + orig_movelistStartOffset, m_movesetSize - orig_movelistStartOffset);

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);

	// Shift offsets in the moveset table & in our header
	const uint64_t extraBlockSize = movelistStartOffset - orig_movelistStartOffset;
	m_header->offsets.movesetBlock += extraBlockSize;
	m_header->offsets.animationBlock += extraBlockSize;
	m_header->offsets.motaBlock += extraBlockSize;

	// Shift moveset name addr offsets
	const uint64_t extraNameSize = moveNameEndOffset - orig_moveNameEndOffset;
	uint64_t movesetListOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.move;
	gAddr::Move* move = (gAddr::Move*)(m_movesetData + movesetListOffset);
	for (size_t i = 0; i < m_infos->table.moveCount; ++i)
	{
		if (move[i].name_addr > move_name_addr) {
			move[i].name_addr += extraNameSize;
		}
		if (move[i].anim_name_addr > move_name_addr) {
			move[i].anim_name_addr += extraNameSize;
		}
	}
}

void EditorT7::SaveMove(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto move = m_iterators.moves[id];

	move->vuln = GetFieldValue(inputs["vulnerability"]);
	move->hitlevel = GetFieldValue(inputs["hitlevel"]);
	move->transition = GetFieldValue(inputs["transition"]);
	move->moveId_val1 = GetFieldValue(inputs["moveId_val1"]);
	move->moveId_val2 = GetFieldValue(inputs["moveId_val2"]);
	move->anim_len = GetFieldValue(inputs["anim_len"]);
	move->airborne_start = GetFieldValue(inputs["airborne_start"]);
	move->airborne_end = GetFieldValue(inputs["airborne_end"]);
	move->ground_fall = GetFieldValue(inputs["ground_fall"]);
	move->hitbox_location = GetFieldValue(inputs["hitbox_location"]);
	move->last_active_frame = GetFieldValue(inputs["last_active_frame"]);
	move->last_active_frame = GetFieldValue(inputs["last_active_frame"]);
	move->distance = GetFieldValue(inputs["distance"]);

	move->cancel_addr = GetFieldValue(inputs["cancel_addr"]);
	move->hit_condition_addr = GetFieldValue(inputs["hit_condition_addr"]);
	move->extra_move_property_addr = GetFieldValue(inputs["extra_properties_addr"]);
	move->move_start_extraprop_addr = GetFieldValue(inputs["move_start_extraprop_addr"]);
	move->move_end_extraprop_addr = GetFieldValue(inputs["move_end_extraprop_addr"]);
	move->voicelip_addr = GetFieldValue(inputs["voiceclip_addr"]);

	move->_0x28_cancel_addr = GetFieldValue(inputs["cancel_addr_2"]);
	move->_0x30_int__0x28_related = GetFieldValue(inputs["cancel_related_id_2"]);
	move->_0x38_cancel_addr = GetFieldValue(inputs["cancel_addr_3"]);
	move->_0x40_int__0x38_related = GetFieldValue(inputs["cancel_related_id_3"]);
	move->_0x48_cancel_addr = GetFieldValue(inputs["cancel_addr_4"]);
	move->_0x50_int__0x48_related = GetFieldValue(inputs["cancel_related_id_4"]);

	move->_0x34_int = GetFieldValue(inputs["_0x34_int"]);
	move->_0x44_int = GetFieldValue(inputs["_0x44_int"]);
	move->_0x56_short = GetFieldValue(inputs["_0x56_short"]);
	move->_0x5C_short = GetFieldValue(inputs["_0x5C_short"]);
	move->_0x5E_short = GetFieldValue(inputs["_0x5E_short"]);
	move->_0x98_int = GetFieldValue(inputs["_0x98_int"]);
	move->_0xA8_short = GetFieldValue(inputs["_0xA8_short"]);
	move->_0xAC_short = GetFieldValue(inputs["_0xAC_short"]);


	if (m_animNameToOffsetMap.find(inputs["anim_name"]->buffer) != m_animNameToOffsetMap.end()) {
		// Todo: if old animation is unused, delete it
		move->anim_addr = m_animNameToOffsetMap[inputs["anim_name"]->buffer];
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
		strcpy(namePtr + move->name_addr, newName);
	}

	// In case we updated the name, reload the movelist display
	ReloadDisplayableMoveList();
}

bool EditorT7::ValidateMoveField( EditorInput* field)
{
	std::string& name = field->name;

	if (name == "anim_name") {
		return m_animNameToOffsetMap.find(field->buffer) != m_animNameToOffsetMap.end();
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
			if (moveId >= (0x8000 + m_aliases.size())) {
				return false;
			}
		}else if (moveId < 0) {
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
	}

	return true;
}

// ===== Other ===== //

void EditorT7::LoadMovesetPtr(Byte* t_moveset, uint64_t t_movesetSize)
{
	m_moveset = t_moveset;
	m_movesetSize = t_movesetSize;

	// Start getting pointers toward useful data structures
	// Also get the actual game-moveset (past our header) pointer
	m_header = (TKMovesetHeader*)t_moveset;
	m_movesetData = t_moveset + m_header->infos.header_size + m_header->offsets.movesetInfoBlock;
	m_movesetDataSize = m_movesetSize - m_header->infos.header_size + m_header->offsets.movesetInfoBlock;
	m_infos = (MovesetInfo*)m_movesetData;

	// Update our useful iterators
	uint64_t movesetBlock = (uint64_t)m_movesetData + m_header->offsets.movesetBlock;

	m_iterators.moves.Set(movesetBlock, m_infos->table.move, m_infos->table.moveCount);
	m_iterators.requirements.Set(movesetBlock, m_infos->table.requirement, m_infos->table.requirementCount);
	m_iterators.hit_conditions.Set(movesetBlock, m_infos->table.hitCondition, m_infos->table.hitConditionCount);
	m_iterators.cancels.Set(movesetBlock, m_infos->table.cancel, m_infos->table.cancelCount);
	m_iterators.grouped_cancels.Set(movesetBlock, m_infos->table.groupCancel, m_infos->table.groupCancelCount);
	m_iterators.reactions.Set(movesetBlock, m_infos->table.reactions, m_infos->table.reactionsCount);
	m_iterators.pushbacks.Set(movesetBlock, m_infos->table.pushback, m_infos->table.pushbackCount);
	m_iterators.pushback_extras.Set(movesetBlock, m_infos->table.pushbackExtradata, m_infos->table.pushbackExtradataCount);
	m_iterators.cancel_extras.Set(movesetBlock, m_infos->table.cancelExtradata, m_infos->table.cancelExtradataCount);
	m_iterators.extra_move_properties.Set(movesetBlock, m_infos->table.extraMoveProperty, m_infos->table.extraMovePropertyCount);
	m_iterators.move_start_properties.Set(movesetBlock, m_infos->table.moveBeginningProp, m_infos->table.moveBeginningPropCount);
	m_iterators.move_end_properties.Set(movesetBlock, m_infos->table.moveEndingProp, m_infos->table.moveEndingPropCount);
	m_iterators.projectiles.Set(movesetBlock, m_infos->table.projectile, m_infos->table.projectileCount);
	m_iterators.input_sequences.Set(movesetBlock, m_infos->table.inputSequence, m_infos->table.inputSequenceCount);
	m_iterators.inputs.Set(movesetBlock, m_infos->table.input, m_infos->table.inputCount);
	m_iterators.voiceclips.Set(movesetBlock, m_infos->table.voiceclip, m_infos->table.voiceclipCount);
	m_iterators.throw_datas.Set(movesetBlock, m_infos->table.throws, m_infos->table.throwsCount);
	m_iterators.camera_datas.Set(movesetBlock, m_infos->table.cameraData, m_infos->table.cameraDataCount);
}

void EditorT7::LoadMoveset(Byte* t_moveset, uint64_t t_movesetSize)
{
	constants = {
		   {EditorConstants_RequirementEnd, 881},
		   {EditorConstants_CancelCommandEnd, 0x8000},
		   {EditorConstants_ExtraPropertyEnd, 32769}, // 0x8001
		   {EditorConstants_GroupedCancelCommand, 0x800B},
		   {EditorConstants_GroupedCancelCommandEnd, 0x800C},
		   {EditorConstants_InputSequenceCommandStart, 0x800D},
	};

	LoadMovesetPtr(t_moveset, t_movesetSize);

	// Get aliases as a vector
	uint16_t* aliasesPtr = m_infos->aliases1;
	for (uint16_t i = 0; i < 112 + 36; ++i) {
		m_aliases.push_back(aliasesPtr[i]);
	}

	// Build anim name : offset list
	uint64_t movesetListOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.move;
	gAddr::Move* movePtr = (gAddr::Move*)(m_movesetData + movesetListOffset);
	char const* namePtr = (char const*)(m_movesetData + m_header->offsets.nameBlock);
	
	for (size_t i = 0; i < m_infos->table.moveCount; ++i)
	{
		const char* animName = namePtr + movePtr[i].anim_name_addr;
		std::string animName_str(animName);
		gameAddr animOffset = movePtr[i].anim_addr;

		if (m_animNameToOffsetMap.find(animName_str) != m_animNameToOffsetMap.end() && m_animNameToOffsetMap[animName_str] != animOffset) {
			// Same animation name refers to a different offset. Create a unique animation name for it.
			// Move names being similar is irrelevant, but i build an anim name <-> anim offset map, so i need uniqueness here.

			animName_str += " (2)";
			unsigned int num = 2;
			while (m_animNameToOffsetMap.find(animName_str) != m_animNameToOffsetMap.end()) {
				animName_str = std::format("{} {}", animName, ++num);
			}

			uint64_t newNameOffset = CreateMoveName(animName_str.c_str());
			if (newNameOffset != 0) {
				// Reallocation was done, update the pointers we are using
				movesetListOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.move;
				movePtr = (gAddr::Move*)(m_movesetData + movesetListOffset);
				namePtr = (char const*)(m_movesetData + m_header->offsets.nameBlock);
				movePtr[i].anim_name_addr = newNameOffset;
				animName = namePtr + movePtr[i].anim_name_addr;
			} else {
				// Could not generate new name
				throw std::exception();
			}
		}

		m_animNameToOffsetMap[animName_str] = animOffset;
		m_animOffsetToNameOffset[animOffset] = movePtr[i].anim_name_addr;
	}

	movesetTable.aliases = m_aliases;
	movesetTable.groupCancelCount = m_infos->table.groupCancelCount;
}

void EditorT7::ReloadDisplayableMoveList(std::vector<DisplayableMove*>* ref)
{
	if (ref != nullptr) {
		displayableMovelist = ref;
	}

	displayableMovelist->clear();

	uint64_t movesetListOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.move;
	gAddr::Move* movePtr = (gAddr::Move*)(m_movesetData + movesetListOffset);
	char const* namePtr = (char const*)(m_movesetData + m_header->offsets.nameBlock);

	uint16_t* aliases = m_infos->aliases1;
	uint8_t aliasesCount = (uint8_t)m_aliases.size();

	uint16_t moveId = 0;
	for (gAddr::Move* move = movePtr; moveId < m_infos->table.moveCount; ++moveId, ++move)
	{
		std::string moveName = std::string(namePtr + move->name_addr);

		uint16_t aliasId = 0;
		EditorMoveFlags flags = 0;

		for (uint8_t j = 0; j < aliasesCount; ++j) {
			if (aliases[j] == moveId) {
				aliasId = 0x8000 + j;
				flags |= EditorMoveFlags_Generic;
				break;
			}
		}

		if (move->hitlevel || move->hitbox_location || move->first_active_frame || move->last_active_frame) {
			if (move->hitlevel && move->hitbox_location && move->first_active_frame && move->last_active_frame) {
				flags |= EditorMoveFlags_Attack;
			} else {
				flags |= EditorMoveFlags_OtherAttack;
			}
		}

		if (Helpers::endsWith(moveName, "_y")) {
			flags |= EditorMoveFlags_Throw;
		} else if (Helpers::endsWith(moveName, "_n")) {
			flags |= EditorMoveFlags_ThrowReaction;
		}

		if (Helpers::startsWith(moveName, MOVESET_CUSTOM_MOVE_NAME_PREFIX)) {
			flags |= EditorMoveFlags_Custom;
		}

		displayableMovelist->push_back(new DisplayableMove{
			.moveId_str = std::to_string(moveId),
			.name = moveName,
			.alias_str = aliasId == 0 ? std::string() : std::to_string(aliasId),
			.color = EditorUtils::GetMoveColorFromFlag(flags),
			.moveId = moveId,
			.aliasId = aliasId,
			.flags = flags,
		});
	}
}

// ===== Utils ===== //

uint16_t EditorT7::GetCurrentMoveID(uint8_t playerId)
{
	gameAddr playerAddress = m_game->ReadPtr("t7_p1_addr");
	if (playerId > 0) {
		playerAddress += playerId * m_game->addrFile->GetSingleValue("val:t7_playerstruct_size");
	}

	uint16_t moveId = m_process->readInt16(playerAddress + m_game->addrFile->GetSingleValue("val:t7_currmove_id"));
	if (moveId >= 0x8000) {
		moveId = m_aliases[moveId - (uint16_t)0x8000];
	}

	return moveId;
}

void EditorT7::SetCurrentMove(uint8_t playerId, gameAddr playerMoveset, size_t moveId)
{
	gameAddr playerAddress = m_game->ReadPtr("t7_p1_addr");
	if (playerId > 0) {
		playerAddress += playerId * m_game->addrFile->GetSingleValue("val:t7_playerstruct_size");
	}

	{
		gameAddr movesetOffset = playerAddress + m_game->addrFile->GetSingleValue("val:t7_motbin_offset");
		// movesetOffset is the one offset we can't touch because it is the moveset the characte reverts to when transitioning to a generic anim
		m_process->writeInt64(movesetOffset + 0x8, playerMoveset);
		m_process->writeInt64(movesetOffset + 0x10, playerMoveset);
		m_process->writeInt64(movesetOffset + 0x18, playerMoveset);
		m_process->writeInt64(movesetOffset + 0x20, playerMoveset);
	}

	if (moveId >= 0x8000) {
		// If is alias, convert it to its regular move id thanks to the alias list (uint16_t each) starting at 0x28
		moveId = m_process->readInt16(playerMoveset + 0x28 + (0x2 * (moveId - 0x8000)));
	}

	gameAddr moveAddr = m_process->readInt64(playerMoveset + 0x210) + moveId * sizeof(Move);

	// Write a big number to the frame timer to force the current move end
	m_process->writeInt32(playerAddress + m_game->addrFile->GetSingleValue("val:t7_currmove_timer"), 99999);
	// Tell the game which move to play NEXT
	m_process->writeInt64(playerAddress + m_game->addrFile->GetSingleValue("val:t7_nextmove_addr"), moveAddr);
	// Also tell the ID of the current move. This isn't required per se, but not doing that would make the current move ID 0, which i don't like.
	m_process->writeInt64(playerAddress + m_game->addrFile->GetSingleValue("val:t7_currmove_id"), moveId);
}

// -- Anim extraction -- //

void EditorT7::OrderAnimationsExtraction(const std::string& characterFilename)
{
	if (animationExtractionStatus & ExtractionStatus_Started) {
		return;
	}

	animationExtractionStatus = ExtractionStatus_Started;

	// Create moveset copy because the extraction run in another thread and i don't want the moveset being modified / reallocated while i access it
	Byte* moveset = (Byte*)malloc(m_movesetSize);
	if (moveset == nullptr) {
		animationExtractionStatus = ExtractionStatus_Failed;
		return;
	}
	memcpy((void*)moveset, m_moveset, m_movesetSize);

	// Start in another thread to avoid the display thread hanging
	const Byte* baseAnimPtr = m_movesetData + m_header->offsets.animationBlock;
	char const* namePtr = (char const*)(m_movesetData + m_header->offsets.nameBlock);
	animExtractionThread = std::thread(&EditorT7::ExtractAnimations, this, moveset, baseAnimPtr, namePtr, std::string(characterFilename));
}

void EditorT7::ExtractAnimations(Byte* moveset, const Byte* baseAnimPtr, const char* namePtr, const std::string& characterFilename)
{
	std::string outputFolder;

	outputFolder = std::format(EDITOR_LIB_DIRECTORY "/{}", characterFilename.substr(0, characterFilename.find_last_of('.')).c_str());
	CreateDirectory(EDITOR_LIB_DIRECTORY, nullptr);
	CreateDirectory(outputFolder.c_str(), nullptr);

	const int animCount = m_animOffsetToNameOffset.size();
	auto it = m_animOffsetToNameOffset.begin();
	auto end = m_animOffsetToNameOffset.end();

	for (int idx = 0; idx < animCount; ++idx)
	{
		const char* name = namePtr + it->second;
		auto& offset = it->first;
		uint64_t size;

		std::advance(it, 1);
		if (it == end) {
			// For the very last animation, we get the size by looking at the start of the next block
			// This is a bit flawed because of 8 bytes alignement, but what's a little 7 bytes at most, for one anim?
			size = (m_header->offsets.motaBlock - m_header->offsets.animationBlock) - offset;
		}
		else {
			size = it->first - offset;
			if (size == 0) {
				// Two animation names referring to the same anim offset. Loop until we find a different offset in order to get the proper size
				auto it_copy = it;
				while (it_copy != end && it_copy->first == offset) {
					std::advance(it_copy, 1);
				}
				size = it_copy->first - offset;
			}
		}

		std::string filename = std::format("{}/{}.bin", outputFolder, name);
		// todo: check if anim exists already
		std::ofstream file(filename.c_str(), std::ios::binary);

		if (file.fail()) {
			continue;
		}

		const char* anim = (char*)baseAnimPtr + offset;
		file.write(anim, size);
	}

	free(moveset);
	animationExtractionStatus = ExtractionStatus_Finished;
	// Not calling detach on the thread apparently doesn't exit it cleanly here and crashes on debug mode
	animExtractionThread.detach();
}


void EditorT7::ImportAnimation(const std::string& filename, int moveid)
{
	//todo
}
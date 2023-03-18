#include <map>
#include <format>

# include "Editor_t7.hpp"
# include "Helpers.hpp"

using namespace EditorUtils;

#define gAddr StructsT7_gameAddr
#define SET_DEFAULT_VAL(fieldName, format, value) sprintf_s(inputMap[fieldName]->buffer, FORM_INPUT_BUFSIZE, format, value)
#define CREATE_STRING_FIELD(a, c, g) drawOrder.push_back(a), inputMap[a] = new EditorInput { .category = c, .imguiInputFlags = 0, .flags = EditorInput_String }, SET_DEFAULT_VAL(a, "%s", g)
#define CREATE_FIELD(a, c, e, g) drawOrder.push_back(a), inputMap[a] = new EditorInput { .category = c, .imguiInputFlags = GetFieldCharset(e), .flags = e }, SET_DEFAULT_VAL(a, GetFieldFormat(e), g)

static void WriteFieldFullname(std::map<std::string, EditorInput*>& inputMap, std::string baseIdentifier)
{
	// Finishing touch
	for (auto& [name, input] : inputMap) {
		// Duplicate the name inside the structure, this is more convenient for me in some places, helps writing a lot shorter code
		input->name = name;
		input->field_fullname = "edition." + baseIdentifier + "." + name;
	}
}

// ===== Pushback Extra ===== //

std::map<std::string, EditorInput*> EditorT7::GetPushbackExtraInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::map<std::string, EditorInput*> inputMap;

	uint64_t pushbackExtraOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.pushbackExtradata;
	PushbackExtradata* pushbackExtra = (PushbackExtradata*)(m_movesetData + pushbackExtraOffset) + id;

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	CREATE_FIELD("horizontal_offset", 1, EditorInput_U16, pushbackExtra->horizontal_offset);

	WriteFieldFullname(inputMap, "pushback_extradata");
	return inputMap;
}

void EditorT7::SavePushbackExtra(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	uint64_t pushbackExtraOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.pushbackExtradata;
	PushbackExtradata* pushbackExtra= (PushbackExtradata*)(m_movesetData + pushbackExtraOffset) + id;

	pushbackExtra->horizontal_offset = atoi(inputs["horizontal_offset"]->buffer);
}

// ===== Pushback ===== //

std::map<std::string, EditorInput*> EditorT7::GetPushbackInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::map<std::string, EditorInput*> inputMap;

	uint64_t pushbackOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.pushback;
	gAddr::Pushback* pushback = (gAddr::Pushback*)(m_movesetData + pushbackOffset) + id;

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	CREATE_FIELD("duration", 0, EditorInput_U16, pushback->displacement);
	CREATE_FIELD("displacement", 0, EditorInput_S16, pushback->duration);
	CREATE_FIELD("num_of_loops", 0, EditorInput_U32, pushback->num_of_loops);
	CREATE_FIELD("extradata_addr", 0, EditorInput_PTR, pushback->duration);

	WriteFieldFullname(inputMap, "pushback");
	return inputMap;
}

bool EditorT7::ValidatePushbackField(std::string name, EditorInput* field)
{
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
	uint64_t pushbackOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.pushback;
	gAddr::Pushback* pushback = (gAddr::Pushback*)(m_movesetData + pushbackOffset) + id;

	pushback->duration = atoi(inputs["duration"]->buffer);
	pushback->displacement = atoi(inputs["displacement"]->buffer);
	pushback->num_of_loops = atoi(inputs["num_of_loops"]->buffer);
	pushback->extradata_addr = atoi(inputs["extradata_addr"]->buffer);

}

// ===== Reactions ===== //

std::map<std::string, EditorInput*> EditorT7::GetReactionsInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::map<std::string, EditorInput*> inputMap;

	uint64_t reactionsOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.reactions;
	gAddr::Reactions* reaction = (gAddr::Reactions*)(m_movesetData + reactionsOffset) + id;

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

bool EditorT7::ValidateReactionsField(std::string name, EditorInput* field)
{
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
	uint64_t reactionsOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.reactions;
	gAddr::Reactions* reaction = (gAddr::Reactions*)(m_movesetData + reactionsOffset) + id;

	reaction->front_pushback = atoi(inputs["front_pushback"]->buffer);
	reaction->backturned_pushback = atoi(inputs["backturned_pushback"]->buffer);
	reaction->left_side_pushback = atoi(inputs["left_side_pushback"]->buffer);
	reaction->right_side_pushback = atoi(inputs["right_side_pushback"]->buffer);
	reaction->front_counterhit_pushback = atoi(inputs["front_counterhit_pushback"]->buffer);
	reaction->downed_pushback = atoi(inputs["downed_pushback"]->buffer);
	reaction->block_pushback = atoi(inputs["block_pushback"]->buffer);

	reaction->front_direction = atoi(inputs["front_direction"]->buffer);
	reaction->back_direction = atoi(inputs["back_direction"]->buffer);
	reaction->left_side_direction = atoi(inputs["left_side_direction"]->buffer);
	reaction->right_side_direction = atoi(inputs["right_side_direction"]->buffer);
	reaction->front_counterhit_direction = atoi(inputs["front_counterhit_direction"]->buffer);
	reaction->downed_direction = atoi(inputs["downed_direction"]->buffer);

	reaction->default_moveid = atoi(inputs["default_moveid"]->buffer);
	reaction->standing_moveid = atoi(inputs["standing_moveid"]->buffer);
	reaction->crouch_moveid = atoi(inputs["crouch_moveid"]->buffer);
	reaction->counterhit_moveid = atoi(inputs["counterhit_moveid"]->buffer);
	reaction->crouch_counterhit_moveid = atoi(inputs["crouch_counterhit_moveid"]->buffer);
	reaction->left_side_moveid = atoi(inputs["left_side_moveid"]->buffer);
	reaction->crouch_left_side_moveid = atoi(inputs["crouch_left_side_moveid"]->buffer);
	reaction->right_side_moveid = atoi(inputs["right_side_moveid"]->buffer);
	reaction->crouch_right_side_moveid = atoi(inputs["crouch_right_side_moveid"]->buffer);
	reaction->backturned_moveid = atoi(inputs["backturned_moveid"]->buffer);
	reaction->crouch_backturned_moveid = atoi(inputs["crouch_backturned_moveid"]->buffer);
	reaction->block_moveid = atoi(inputs["block_moveid"]->buffer);
	reaction->crouch_block_moveid = atoi(inputs["crouch_block_moveid"]->buffer);
	reaction->wallslump_moveid = atoi(inputs["wallslump_moveid"]->buffer);
	reaction->downed_moveid = atoi(inputs["downed_moveid"]->buffer);

	reaction->vertical_pushback = atoi(inputs["vertical_pushback"]->buffer);
	reaction->_0x44_int = atoi(inputs["_0x44_int"]->buffer);
	reaction->_0x48_int = atoi(inputs["_0x48_int"]->buffer);
}

// ===== Hit conditions ===== //

std::vector<std::map<std::string, EditorInput*>> EditorT7::GetHitConditionListInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::vector<std::map<std::string, EditorInput*>> inputListMap;

	uint64_t hitConditionOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.hitCondition;
	gAddr::HitCondition* hitCondition = (gAddr::HitCondition*)(m_movesetData + hitConditionOffset) + id;

	uint64_t requirementOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.requirement;
	Requirement* req = (Requirement*)(m_movesetData + requirementOffset);

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
	uint64_t hitConditionOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.hitCondition;
	gAddr::HitCondition* hitCondition = (gAddr::HitCondition*)(m_movesetData + hitConditionOffset) + id;

	hitCondition->requirements_addr = (uint64_t)atoi(inputs["requirements_addr"]->buffer);
	hitCondition->damage = (uint32_t)atoi(inputs["damage"]->buffer);
	hitCondition->_0xC_int = (uint32_t)atoi(inputs["_0xC_int"]->buffer);
	hitCondition->reactions_addr = (uint64_t)atoi(inputs["reactions_addr"]->buffer);
}

bool EditorT7::ValidateHitConditionField(std::string name, EditorInput* field)
{
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

	uint64_t requirementOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.requirement;
	Requirement* req = (Requirement*)(m_movesetData + requirementOffset) + id;

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
	uint64_t requirementOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.requirement;
	Requirement* req = (Requirement*)(m_movesetData + requirementOffset) + id;

	req->condition = (uint32_t)atoi(inputs["condition"]->buffer);
	req->param = (uint32_t)atoi(inputs["param"]->buffer);
}

// ===== Cancel Extradata ===== //

std::map<std::string, EditorInput*> EditorT7::GetCancelExtraInput(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::map<std::string, EditorInput*> inputMap;

	uint64_t cancelExtraOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.cancelExtradata;
	CancelExtradata* cancelExtra = (CancelExtradata*)(m_movesetData + cancelExtraOffset) + id;

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
	uint64_t cancelExtraOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.cancelExtradata;
	CancelExtradata* cancelExtra = (CancelExtradata*)(m_movesetData + cancelExtraOffset) + id;

	cancelExtra->value = (uint32_t)strtol(inputs["value"]->buffer, nullptr, 16);
}

// ===== Cancel ===== //

std::vector<std::map<std::string, EditorInput*>> EditorT7::GetCancelListInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::vector<std::map<std::string, EditorInput*>> inputListMap;

	uint64_t cancelOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.cancel;
	gAddr::Cancel* cancel = (gAddr::Cancel*)(m_movesetData + cancelOffset) + id;

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
	uint64_t cancelOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.cancel;
	gAddr::Cancel* cancel = (gAddr::Cancel*)(m_movesetData + cancelOffset) + id;

	cancel->command = (uint64_t)strtoll(inputs["command"]->buffer, nullptr, 16);
	cancel->requirements_addr = (uint64_t)atoi(inputs["requirements_addr"]->buffer);
	cancel->extradata_addr = (uint64_t)atoi(inputs["extradata_addr"]->buffer);
	cancel->detection_start = (uint32_t)atoi(inputs["detection_start"]->buffer);
	cancel->detection_end = (uint32_t)atoi(inputs["detection_end"]->buffer);
	cancel->starting_frame = (uint32_t)atoi(inputs["starting_frame"]->buffer);
	cancel->move_id = (uint16_t)atoi(inputs["move_id"]->buffer);
	cancel->cancel_option = (uint16_t)atoi(inputs["cancel_option"]->buffer);
}

bool EditorT7::ValidateCancelField(std::string name, EditorInput* field)
{
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

	return true;
}

// ===== Grouped Cancel ===== //

std::vector<std::map<std::string, EditorInput*>> EditorT7::GetGroupedCancelListInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::vector<std::map<std::string, EditorInput*>> inputListMap;

	uint64_t cancelOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.groupCancel;
	gAddr::Cancel* cancel = (gAddr::Cancel*)(m_movesetData + cancelOffset) + id;

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
	uint64_t cancelOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.groupCancel;
	gAddr::Cancel* cancel = (gAddr::Cancel*)(m_movesetData + cancelOffset) + id;

	cancel->command = (uint64_t)strtoll(inputs["command"]->buffer, nullptr, 16);
	cancel->requirements_addr = (uint64_t)atoi(inputs["requirements_addr"]->buffer);
	cancel->extradata_addr = (uint64_t)atoi(inputs["extradata_addr"]->buffer);
	cancel->detection_start = (uint32_t)atoi(inputs["detection_start"]->buffer);
	cancel->detection_end = (uint32_t)atoi(inputs["detection_end"]->buffer);
	cancel->starting_frame = (uint32_t)atoi(inputs["starting_frame"]->buffer);
	cancel->move_id = (uint16_t)atoi(inputs["move_id"]->buffer);
	cancel->cancel_option = (uint16_t)atoi(inputs["cancel_option"]->buffer);
}

bool EditorT7::ValidateGroupedCancelField(std::string name, EditorInput* field)
{
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

	return true;
}

// ===== Other move properties (start) ===== //

std::vector<std::map<std::string, EditorInput*>> EditorT7::GetMoveStartPropertyListInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::vector<std::map<std::string, EditorInput*>> inputListMap;

	uint64_t otherPropOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.moveBeginningProp;
	gAddr::OtherMoveProperty* prop = (gAddr::OtherMoveProperty*)(m_movesetData + otherPropOffset) + id;

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

	uint64_t otherPropOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.moveBeginningProp;
	gAddr::OtherMoveProperty* prop = (gAddr::OtherMoveProperty*)(m_movesetData + otherPropOffset) + id;

	prop->requirements_addr = atoi(inputs["requirements_addr"]->buffer);
	prop->extraprop = (uint32_t)strtoll(inputs["extraprop"]->buffer, nullptr, 16);
	prop->value = (uint32_t)atoi(inputs["value"]->buffer);
}

// ===== Other move properties (end) ===== //

std::vector<std::map<std::string, EditorInput*>> EditorT7::GetMoveEndPropertyListInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::vector<std::map<std::string, EditorInput*>> inputListMap;

	uint64_t otherPropOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.moveBeginningProp;
	gAddr::OtherMoveProperty* prop = (gAddr::OtherMoveProperty*)(m_movesetData + otherPropOffset) + id;

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

	uint64_t otherPropOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.moveEndingProp;
	gAddr::OtherMoveProperty* prop = (gAddr::OtherMoveProperty*)(m_movesetData + otherPropOffset) + id;

	prop->requirements_addr = atoi(inputs["requirements_addr"]->buffer);
	prop->extraprop = (uint32_t)strtoll(inputs["extraprop"]->buffer, nullptr, 16);
	prop->value = (uint32_t)atoi(inputs["value"]->buffer);
}

bool EditorT7::ValidateOtherMoveProperty(std::string name, EditorInput* field)
{
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

	uint64_t extraPropOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.extraMoveProperty;
	ExtraMoveProperty* prop = (ExtraMoveProperty*)(m_movesetData + extraPropOffset) + id;

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
	uint64_t extraPropOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.extraMoveProperty;
	ExtraMoveProperty* prop = (ExtraMoveProperty*)(m_movesetData + extraPropOffset) + id;

	prop->starting_frame = (uint32_t)atoi(inputs["starting_frame"]->buffer);
	prop->id = (uint32_t)strtoll(inputs["id"]->buffer, nullptr, 16);
	prop->value = (uint32_t)atoi(inputs["value"]->buffer);
}

// ===== Voiceclips ===== //

std::map<std::string, EditorInput*> EditorT7::GetVoiceclipInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::map<std::string, EditorInput*> inputMap;

	uint64_t voiceclipOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.voiceclip;
	Voiceclip* voiceclip = (Voiceclip*)(m_movesetData + voiceclipOffset) + id;

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	CREATE_FIELD("id", 0, EditorInput_H32, voiceclip->id);

	WriteFieldFullname(inputMap, "voiceclip");
	return inputMap;
}


void EditorT7::SaveVoiceclip(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	uint64_t voiceclipOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.voiceclip;
	Voiceclip* voiceclip = (Voiceclip*)(m_movesetData + voiceclipOffset) + id;

	voiceclip->id = (uint32_t)strtol(inputs["id"]->buffer, nullptr, 16);
}

// ===== MOVES ===== //

std::map<std::string, EditorInput*> EditorT7::GetMoveInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::map<std::string, EditorInput*> inputMap;

	uint64_t movesetListOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.move;
	gAddr::Move* move = (gAddr::Move*)(m_movesetData + movesetListOffset) + id;

	char* nameBlock = (char*)(m_movesetData + m_header->offsets.nameBlock);

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	CREATE_STRING_FIELD("move_name", 0, nameBlock + move->name_addr);
	CREATE_STRING_FIELD("anim_name", 0, nameBlock + move->anim_name_addr);
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

	CREATE_FIELD("cancel_id", 2, EditorInput_PTR,  move->cancel_addr);
	CREATE_FIELD("hit_condition_id", 2, EditorInput_PTR, move->hit_condition_addr);
	CREATE_FIELD("extra_properties_id", 2, EditorInput_PTR, move->extra_move_property_addr);
	CREATE_FIELD("beginning_extra_properties_id", 2, EditorInput_PTR, move->move_start_extraprop_addr);
	CREATE_FIELD("ending_extra_properties_id", 2, EditorInput_PTR, move->move_end_extraprop_addr);
	CREATE_FIELD("voiceclip_id", 2, EditorInput_PTR, move->voicelip_addr);

	CREATE_FIELD("cancel_id_2", 3, EditorInput_PTR, move->_0x28_cancel_addr);
	CREATE_FIELD("cancel_id_2_related", 3, EditorInput_U32, move->_0x30_int__0x28_related);
	CREATE_FIELD("cancel_id_3", 3, EditorInput_PTR, move->_0x38_cancel_addr);
	CREATE_FIELD("cancel_id_3_related", 3, EditorInput_U32, move->_0x40_int__0x38_related);
	CREATE_FIELD("cancel_id_4", 3, EditorInput_PTR, move->_0x48_cancel_addr);
	CREATE_FIELD("cancel_id_4_related", 3, EditorInput_U32, move->_0x50_int__0x48_related);

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

	// Copy start //

	memcpy(newMoveset, m_moveset, moveNameOffset);

	// Write our new name
	memcpy(newMoveset + moveNameOffset, moveName, moveNameSize);

	// Copy all the data after our name
	memcpy(newMoveset + moveNameEndOffset, m_moveset + orig_moveNameEndOffset, m_movesetSize - orig_moveNameEndOffset);

	// Assign new moveset
	free(m_moveset);
	LoadMovesetPtr(newMoveset, newMovesetSize);

	// Shift offsets in the moveset table & in our header
	const uint64_t extraNameSize = moveNameEndOffset - orig_moveNameEndOffset;
	m_header->offsets.movesetBlock += extraNameSize;
	m_header->offsets.animationBlock += extraNameSize;
	m_header->offsets.motaBlock += extraNameSize;

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
	uint64_t movesetListOffset = m_header->offsets.movesetBlock + (uint64_t)m_infos->table.move;
	gAddr::Move* move = (gAddr::Move*)(m_movesetData + movesetListOffset) + id;

	if (m_animNameToOffsetMap.find(inputs["anim_name"]->buffer) != m_animNameToOffsetMap.end()) {
		move->anim_addr = m_animNameToOffsetMap[inputs["anim_name"]->buffer];
	}
	move->vuln = (uint32_t)atoi(inputs["vulnerability"]->buffer);
	move->hitlevel = (uint32_t)strtol(inputs["hitlevel"]->buffer, nullptr, 16);
	move->transition = (uint16_t)atoi(inputs["transition"]->buffer);
	move->moveId_val1 = (uint16_t)atoi(inputs["moveId_val1"]->buffer);
	move->moveId_val2 = (uint16_t)atoi(inputs["moveId_val2"]->buffer);
	move->anim_len = (uint32_t)atoi(inputs["anim_len"]->buffer);
	move->airborne_start = (uint32_t)atoi(inputs["airborne_start"]->buffer);
	move->airborne_end = (uint32_t)atoi(inputs["airborne_end"]->buffer);
	move->ground_fall = (uint32_t)atoi(inputs["ground_fall"]->buffer);
	move->hitbox_location = (uint32_t)strtol(inputs["hitbox_location"]->buffer, nullptr, 16);
	move->last_active_frame = (uint32_t)atoi(inputs["last_active_frame"]->buffer);
	move->last_active_frame = (uint32_t)atoi(inputs["last_active_frame"]->buffer);
	move->distance = (uint16_t)atoi(inputs["distance"]->buffer);

	move->cancel_addr = atoll(inputs["cancel_id"]->buffer);
	move->hit_condition_addr = atoll(inputs["hit_condition_id"]->buffer);
	move->extra_move_property_addr = atoll(inputs["extra_properties_id"]->buffer);
	move->move_start_extraprop_addr = atoll(inputs["beginning_extra_properties_id"]->buffer);
	move->move_end_extraprop_addr = atoll(inputs["ending_extra_properties_id"]->buffer);
	move->voicelip_addr = atoll(inputs["voiceclip_id"]->buffer);

	move->_0x28_cancel_addr = atoll(inputs["cancel_id_2"]->buffer);
	move->_0x30_int__0x28_related = atoi(inputs["cancel_id_2_related"]->buffer);
	move->_0x38_cancel_addr = atoll(inputs["cancel_id_3"]->buffer);
	move->_0x40_int__0x38_related = atoi(inputs["cancel_id_3_related"]->buffer);
	move->_0x48_cancel_addr = atoll(inputs["cancel_id_4"]->buffer);
	move->_0x50_int__0x48_related = atoi(inputs["cancel_id_4_related"]->buffer);

	move->_0x34_int = atoi(inputs["_0x34_int"]->buffer);
	move->_0x44_int = atoi(inputs["_0x44_int"]->buffer);
	move->_0x56_short = atoi(inputs["_0x56_short"]->buffer);
	move->_0x5C_short = atoi(inputs["_0x5C_short"]->buffer);
	move->_0x5E_short = atoi(inputs["_0x5E_short"]->buffer);
	move->_0x98_int = atoi(inputs["_0x98_int"]->buffer);
	move->_0xA8_short = atoi(inputs["_0xA8_short"]->buffer);
	move->_0xAC_short = atoi(inputs["_0xAC_short"]->buffer);

	// Save move name at the end because it may implay reallocation and invalidation of existing pointers
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

bool EditorT7::ValidateMoveField(std::string name, EditorInput* field)
{
	if (name == "anim_name") {
		return m_animNameToOffsetMap.find(field->buffer) != m_animNameToOffsetMap.end();
	}

	else if (name == "cancel_id" || name == "cancel_id_2" ||
		name == "cancel_id_3" || name == "cancel_id_4") {
		int listIdx = atoi(field->buffer);
		return -1 <= listIdx && listIdx < (int)m_infos->table.cancelCount;
	}

	else if (name == "hit_condition_id") {
		int listIdx = atoi(field->buffer);
		return -1 <= listIdx && listIdx < (int)m_infos->table.hitConditionCount;
	}

	else if (name == "extra_properties_id") {
		int listIdx = atoi(field->buffer);
		return -1 <= listIdx && listIdx < (int)m_infos->table.extraMovePropertyCount;
	}

	else if (name == "beginning_extra_properties_id") {
		int listIdx = atoi(field->buffer);
		return -1 <= listIdx && listIdx < (int)m_infos->table.moveBeginningPropCount;
	}

	else if (name == "ending_extra_properties_id") {
		int listIdx = atoi(field->buffer);
		return -1 <= listIdx && listIdx < (int)m_infos->table.moveEndingPropCount;
	}

	else if (name == "voiceclip_id") {
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
	case EditorWindowType_Voiceclip:
		return GetVoiceclipInputs(id, drawOrder);
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
	case EditorWindowType_PushbackExtradata:
		return GetPushbackExtraInputs(id, drawOrder);
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
	}
	return std::vector<std::map<std::string, EditorInput*>>();
}

bool EditorT7::ValidateField(EditorWindowType_ fieldType, std::string fieldShortName, EditorInput* field)
{
	if (field->buffer[0] == '\0') {
		// There are no fields that should be allowed to be empty
		return false;
	}

	auto flags = field->flags;

	if (flags & EditorInput_H64) {
		if (strlen(field->buffer) > 16) {
			return false;
		}
	}
	else if (flags & EditorInput_H32) {
		if (strlen(field->buffer) > 8) {
			return false;
		}
	}
	else if (flags & EditorInput_H16) {
		if (strlen(field->buffer) > 4) {
			return false;
		}
	}
	else if (flags & (EditorInput_U64 | EditorInput_S64)) {
		if (strlen(field->buffer) > 19) {
			return false;
		}
		if ((flags & EditorInput_U64) && atoll(field->buffer) < 0) {
			return false;
		}
	}
	else if (flags & EditorInput_U32) {
		long long num = atoll(field->buffer);
		if (num > UINT_MAX || num < 0) {
			return false;
		}
	}
	else if (flags & EditorInput_S32) {
		long long num = atoll(field->buffer);
		if (num > INT_MAX || num < INT_MIN) {
			return false;
		}
	}
	else if (flags & EditorInput_U16) {
		int num = atoi(field->buffer);
		if (num > USHRT_MAX || num < 0) {
			return false;
		}
	}
	else if (flags & EditorInput_S16) {
		int num = atoi(field->buffer);
		if (num > SHRT_MAX || num < SHRT_MIN) {
			return false;
		}
	}

	switch (fieldType)
	{
	case EditorWindowType_Move:
		return ValidateMoveField(fieldShortName, field);
		break;
	case EditorWindowType_Cancel:
		return ValidateCancelField(fieldShortName, field);
		break;
	case EditorWindowType_GroupedCancel:
		return ValidateGroupedCancelField(fieldShortName, field);
		break;
	case EditorWindowType_HitCondition:
		return ValidateHitConditionField(fieldShortName, field);
		break;
	case EditorWindowType_Reactions:
		return ValidateReactionsField(fieldShortName, field);
		break;
	case EditorWindowType_Pushback:
		return ValidatePushbackField(fieldShortName, field);
		break;
	case EditorWindowType_MoveBeginProperty:
	case EditorWindowType_MoveEndProperty:
		return ValidateOtherMoveProperty(fieldShortName, field);
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
}

void EditorT7::LoadMoveset(Byte* t_moveset, uint64_t t_movesetSize)
{
	constants = {
		   {EditorConstants_RequirementEnd, 881},
		   {EditorConstants_CancelCommandEnd, 0x8000},
		   {EditorConstants_GroupedCancelCommand, 0x800B},
		   {EditorConstants_GroupedCancelCommandEnd, 0x800C},
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
		//printf("%d/%d - %llx\n", i, m_infos->table.moveCount, movePtr[i].anim_name_addr);
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
}

EditorTable EditorT7::GetMovesetTable()
{
	return EditorTable{
		.aliases = m_aliases,
		.groupCancelCount = m_infos->table.groupCancelCount
	};
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

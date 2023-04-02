#include "Editor_t7.hpp"

using namespace EditorUtils;

#define SET_DEFAULT_VAL(fieldName, format, value) sprintf_s(inputMap[fieldName]->buffer, FORM_INPUT_BUFSIZE, format, value)
#define CREATE_STRING_FIELD(a, c, d, g) drawOrder.push_back(a), inputMap[a] = new EditorInput { .category = c, .imguiInputFlags = 0, .flags = EditorInput_String | d }, SET_DEFAULT_VAL(a, "%s", g)
#define CREATE_FIELD(a, c, e, g) drawOrder.push_back(a), inputMap[a] = new EditorInput { .category = c, .imguiInputFlags = GetFieldCharset(e), .flags = e }, SET_DEFAULT_VAL(a, GetFieldFormat(e), g), SetInputfieldColor(inputMap[a])

// -- Inputs -- //

std::vector<std::map<std::string, EditorInput*>> EditorT7::GetMovelistInputListInputs(uint16_t id, int listSize, VectorSet<std::string>& drawOrder)
{
	std::vector<std::map<std::string, EditorInput*>> inputListMap;

	auto input = m_iterators.mvl_inputs.begin() + id;

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.
	do
	{
		std::map<std::string, EditorInput*> inputMap;

		CREATE_FIELD("directions", 0, EditorInput_H8, input->directions);
		CREATE_FIELD("buttons", 0, EditorInput_H8, input->buttons);
		CREATE_FIELD("frame_duration", 0, EditorInput_U16, input->frame_duration);

		WriteFieldFullname(inputMap, "mvl_input");
		inputListMap.push_back(inputMap);
		input++;
	} while (--listSize > 0);

	return inputListMap;
}

void EditorT7::SaveMovelistInput(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto input = m_iterators.mvl_inputs.begin() + id;

	SetMemberValue(&input->directions, inputs["direction"]);
	SetMemberValue(&input->buttons, inputs["button"]);
	SetMemberValue(&input->frame_duration, inputs["frame_duration"]);
}

// -- Displayables -- //

std::vector<std::map<std::string, EditorInput*>> EditorT7::GetMovelistDisplayablesInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::vector<std::map<std::string, EditorInput*>> inputListMap;

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.

	for (auto& displayable : m_iterators.mvl_displayables)
	{
		std::map<std::string, EditorInput*> inputMap;

		CREATE_FIELD("type", 0, EditorInput_H32, displayable.type);
		CREATE_FIELD("playable_id", 0, EditorInput_S16 | EditorInput_Interactable, displayable.playable_id);

		WriteFieldFullname(inputMap, "mvl_displayable");
		inputListMap.push_back(inputMap);
	}

	return inputListMap;
}

void EditorT7::SaveMovelistDisplayable(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto displayable = m_iterators.mvl_displayables[id];

	SetMemberValue(&displayable->type, inputs["type"]);
	SetMemberValue(&displayable->playable_id, inputs["playable_id"]);
}

bool EditorT7::ValidateMovelistDisplayableField(EditorInput* field)
{
	std::string& name = field->name;

	if (name == "playable_id")
	{
		int listIdx = atoi(field->buffer);
		return -1 <= listIdx && listIdx < (int)m_iterators.mvl_playables.size();
	}

	return true;
}

// -- Playables -- //

std::map<std::string, EditorInput*> EditorT7::GetMovelistPlayableInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::map<std::string, EditorInput*> inputMap;

	auto playable = m_iterators.mvl_playables[id];

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.

	MvlHead* mvlHead = (MvlHead*)(m_movesetData + m_header->offsets.movelistBlock);

	uint32_t playable_addr = mvlHead->playables_offset + sizeof(MvlPlayable) * id;
	uint32_t input_sequence_start = mvlHead->inputs_offset;
	uint32_t input_sequence_addr = playable_addr + playable->input_sequence_offset;
	uint32_t input_sequence_id = (input_sequence_addr - input_sequence_start) / sizeof(MvlInput);

	CREATE_FIELD("_unk0x0", 0, EditorInput_U16_Changeable, playable->_unk0x0);
	CREATE_FIELD("distance", 0, EditorInput_U16, playable->distance);
	CREATE_FIELD("p2_rotation", 0, EditorInput_U16_Changeable, playable->p2_rotation);
	CREATE_FIELD("_unk0x6", 0, EditorInput_U16_Changeable, playable->_unk0x6);
	CREATE_FIELD("_unk0x8", 0, EditorInput_U16_Changeable, playable->_unk0x8);
	CREATE_FIELD("p1_facing_related", 0, EditorInput_U16_Changeable, playable->p1_facing_related);
	CREATE_FIELD("_unk0xc", 0, EditorInput_U16_Changeable, playable->_unk0xc);
	CREATE_FIELD("input_sequence_id", 0, EditorInput_U16 | EditorInput_Interactable, input_sequence_id);
	CREATE_FIELD("input_count", 0, EditorInput_U16, playable->input_count);
	CREATE_FIELD("has_rage", 0, EditorInput_U16_Changeable, playable->has_rage);
	CREATE_FIELD("_unk0x16", 0, EditorInput_U16_Changeable, playable->_unk0x16);

	WriteFieldFullname(inputMap, "mvl_playablei");
	return inputMap;
}

void EditorT7::SaveMovelistPlayable(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto playable = m_iterators.mvl_playables[id];

	MvlHead* mvlHead = (MvlHead*)(m_movesetData + m_header->offsets.movelistBlock);

	uint32_t playable_addr = mvlHead->playables_offset + sizeof(MvlPlayable) * id;
	uint32_t input_sequence_id = (uint32_t)GetFieldValue(inputs["input_sequence_id"]);
	uint32_t input_sequence_addr = input_sequence_id * sizeof(MvlInput) + mvlHead->inputs_offset;
	uint32_t input_sequence_offset = input_sequence_addr - playable_addr;

	SetMemberValue(&playable->_unk0x0, inputs["_unk0x0"]);
	SetMemberValue(&playable->distance, inputs["distance"]);
	SetMemberValue(&playable->p2_rotation, inputs["p2_rotation"]);
	SetMemberValue(&playable->_unk0x6, inputs["_unk0x6"]);
	SetMemberValue(&playable->_unk0x8, inputs["_unk0x8"]);
	SetMemberValue(&playable->p1_facing_related, inputs["p1_facing_related"]);
	SetMemberValue(&playable->_unk0xc, inputs["_unk0xc"]);
	SetMemberValue(&playable->input_count, inputs["input_count"]);
	playable->input_sequence_offset = input_sequence_offset;
	SetMemberValue(&playable->has_rage, inputs["has_rage"]);
	SetMemberValue(&playable->_unk0x16, inputs["_unk0x16"]);
}

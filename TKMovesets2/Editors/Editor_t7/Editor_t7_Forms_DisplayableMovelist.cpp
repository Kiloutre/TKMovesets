#include "Editor_t7.hpp"

#include <format>

using namespace EditorUtils;

#define CREATE_STRING_FIELD(k, c, f, v, s) CreateField<decltype(v)>(k, drawOrder, inputMap, c, EditorInput_String | f, v, s)
#define CREATE_FIELD(k, c, f, v) CreateField<decltype(v)>(k, drawOrder, inputMap, c, f, v)

// Utils

std::string EditorT7::GetMovelistDisplayableText(uint32_t offset)
{
	const char* entryString = (char*)m_mvlHead + offset;

	std::string convertedString;

	int maxLen = (int)strlen(entryString);
	for (int i = 0; i < maxLen;)
	{
		if ((unsigned char)entryString[i] == 0xE3 && (i + 2 < maxLen) && (unsigned char)entryString[i + 1] == 0x80)
		{
			switch ((unsigned char)entryString[i + 2])
			{
			case 0x90:
				convertedString += "{[}";
				i += 3;
				continue;
				break;
			case 0x91:
				convertedString += "{]}";
				i += 3;
				continue;
				break;
			}
			// Might be korean/japanese characters
		}
		else if ((unsigned char)entryString[i] == 0xEE && (i + 2 < maxLen) && (unsigned char)entryString[i + 1] == 0x80)
		{
			auto entry = EditorT7Utils::mvlDisplay_characterConversion.find((unsigned char)entryString[i + 2]);

			if (entry != EditorT7Utils::mvlDisplay_characterConversion.end()) {
				convertedString += entry->second;
				i += 3;
				continue;
			}

		}
		else if ((unsigned char)entryString[i] == 0xEE && (i + 2 < maxLen) && (unsigned char)entryString[i + 1] == 0x81)
		{
			switch ((unsigned char)entryString[i + 2])
			{
			case 0xA3:
				convertedString += "{>}";
				i += 3;
				continue;
				break;
			case 0xA2:
				convertedString += "{<}";
				i += 3;
				continue;
				break;
			}
		}
		else if ((unsigned char)entryString[i] == 0xE2 && (i + 2 < maxLen) && (unsigned char)entryString[i + 1] == 0x96)
		{
			switch ((unsigned char)entryString[i + 2])
			{
			case 0xA0:
				convertedString += "{X}";
				i += 3;
				continue;
				break;
			}
		}

		convertedString += entryString[i];
		++i;
	}

	if (convertedString.size() >= FORM_INPUT_MAX_BUFSIZE) {
		DEBUG_LOG("Movelist: cropping string. Size: %u, offset: %x, string: [%s]\n", maxLen, offset, entryString);
		convertedString.erase(FORM_INPUT_MAX_BUFSIZE - 1);
	}
	return convertedString;
}

std::string EditorT7::GetMovelistDisplayableLabel(std::map<std::string, EditorInput*>& fieldMap)
{
	std::string retVal;
	uint32_t icons_1 = (uint32_t)GetFieldValue(fieldMap["icons"]);
	uint8_t icons_2 = (uint8_t)GetFieldValue(fieldMap["icons_2"]);
	uint8_t combo_damage = (uint8_t)GetFieldValue(fieldMap["combo_damage"]);
	uint8_t combo_difficulty = (uint8_t)GetFieldValue(fieldMap["combo_difficulty"]);

	// Note that these 4 icons can't show at once, it's only 3 max. I still display 4 in the GUI for clarity.
	if (icons_1 & 0xFF) {
		retVal += " A";
	}

	if (icons_1 & 0xFF00) {
		retVal += " !S";
	}

	if (icons_1 & 0xFF000000) {
		retVal += " HA";
	}

	if (icons_2 & 0xFF) {
		retVal += " !WB";
	}

	if (combo_difficulty > 0)
	{
		retVal += " " + std::to_string(combo_damage) + " DMG ";
		
		const char difficultyBuffer[] = "***";
		int index = (sizeof(difficultyBuffer) - 1) - combo_difficulty;
		retVal += &difficultyBuffer[max(0, index)];
	}

	if (retVal.size() != 0) {
		retVal = " -" + retVal;
	}
	return retVal;
}

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
		CREATE_FIELD("frame_duration", 0, EditorInput_U8, input->frame_duration);
		CREATE_FIELD("_unkn_0x2", 0, EditorInput_U8, input->_unkn_0x2);

		WriteFieldFullname(inputMap, "mvl_input");
		inputListMap.push_back(inputMap);
		input++;
	} while (--listSize > 0);

	return inputListMap;
}

void EditorT7::SaveMovelistInput(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto input = m_iterators.mvl_inputs.begin() + id;

	SetMemberValue(&input->directions, inputs["directions"]);
	SetMemberValue(&input->buttons, inputs["buttons"]);
	SetMemberValue(&input->frame_duration, inputs["frame_duration"]);
	SetMemberValue(&input->_unkn_0x2, inputs["_unkn_0x2"]);
}

// -- Displayables -- //

std::vector<std::map<std::string, EditorInput*>> EditorT7::GetMovelistDisplayablesInputs(uint16_t id, VectorSet<std::string>& drawOrder)
{
	std::vector<std::map<std::string, EditorInput*>> inputListMap;

	// Set up fields. Draw order is same as declaration order because of macro.
	// Default value is written from the last two arguments, also thanks to the macro
	// (fieldName, category, EditorInputFlag, value)
	// 0 has no category name. Even categories are open by default, odd categories are hidden by default.

	int listIndex = 0;
	for (auto& displayable : m_iterators.mvl_displayables)
	{
		std::map<std::string, EditorInput*> inputMap;
		CREATE_FIELD("__id", 0, EditorInput_S32, id + listIndex);

		CREATE_FIELD("type", 0, EditorInput_H32, displayable.type);
		CREATE_FIELD("playable_id", 0, EditorInput_S16 | EditorInput_Interactable, displayable.playable_id);

		
		if (id == (uint16_t)-1) {
			// Used for list creation
			for (int i = 0; i < _countof(displayable.all_translation_offsets); ++i) {
				// Set translation offsets to 0, will allocate new space for each when saving
				displayable.all_translation_offsets[i] = 0;
			}
		}

		for (int i = 0; i < _countof(displayable.title_translation_offsets); ++i) {
			std::string key = "title_translation_" + std::to_string(i);
			std::string value = (id == (uint16_t)-1) ? " " : GetMovelistDisplayableText(displayable.title_translation_offsets[i]);

			CREATE_STRING_FIELD(key, 0, EditorInput_String, value.c_str(), FORM_INPUT_MAX_BUFSIZE);
			// This hidden field will be used in order to know whether entry should be a new allocation or not
			CREATE_FIELD(key + "_offset", 0, EditorInput_U32, (id == (uint16_t)-1) ? 0 : displayable.title_translation_offsets[i])->visible = false;
		}
		for (int i = 0; i < _countof(displayable.translation_offsets); ++i) {
			std::string key = "translation_" + std::to_string(i);
			std::string value = (id == (uint16_t)-1) ? " " : GetMovelistDisplayableText(displayable.translation_offsets[i]);

			CREATE_STRING_FIELD(key, 0, EditorInput_String, value.c_str(), FORM_INPUT_MAX_BUFSIZE);
			// This hidden field will be used in order to know whether entry should be a new allocation or not
			CREATE_FIELD(key + "_offset", 0, EditorInput_U32, (id == (uint16_t)-1) ? 0 : displayable.translation_offsets[i])->visible = false;
		}


		CREATE_FIELD("icons", 0, EditorInput_H32, displayable.icons);
		CREATE_FIELD("icons_2", 0, EditorInput_H32, displayable.icons_2);
		CREATE_FIELD("combo_difficulty", 0, EditorInput_U8, displayable.combo_difficulty);
		CREATE_FIELD("combo_damage", 0, EditorInput_U8, displayable.combo_damage);


		CREATE_FIELD("_unk0x40", 1, EditorInput_H32_Changeable, displayable._unk0x40);
		CREATE_FIELD("_unk0x46", 1, EditorInput_H16_Changeable, displayable._unk0x46);
		CREATE_FIELD("_unk0x153", 1, EditorInput_H8_Changeable, displayable._unk0x153);

		for (int ofst = 0x4C; ofst <= 0x170; ofst += 4)
		{
			switch (ofst)
			{
				case 0x14C: //icons
				case 0x150: //icons_2
					break;
				default:
					int value = *(int*)((char*)&displayable + ofst);
					std::string key = std::format("unk_{:x}", ofst);
					CREATE_FIELD(key, 1, EditorInput_H32_Changeable, value);
					break;
			}
		}

		WriteFieldFullname(inputMap, "mvl_displayable");
		inputListMap.push_back(inputMap);

		if (id == -1) {
			// When generating a single list item, id is -1
			break;
		}
		++listIndex;
	}

	return inputListMap;
}

void EditorT7::SaveMovelistDisplayable(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto displayable = m_iterators.mvl_displayables[id];

	SetMemberValue(&displayable->type, inputs["type"]);
	SetMemberValue(&displayable->playable_id, inputs["playable_id"]);
	SetMemberValue(&displayable->icons, inputs["icons"]);
	SetMemberValue(&displayable->icons_2, inputs["icons_2"]);
	SetMemberValue(&displayable->combo_difficulty, inputs["combo_difficulty"]);
	SetMemberValue(&displayable->combo_damage, inputs["combo_damage"]);

	SetMemberValue(&displayable->_unk0x40, inputs["_unk0x40"]);
	SetMemberValue(&displayable->_unk0x46, inputs["_unk0x46"]);
	SetMemberValue(&displayable->_unk0x153, inputs["_unk0x153"]);

	for (int ofst = 0x4C; ofst <= 0x170; ofst += 4)
	{
		switch (ofst)
		{
			case 0x14C: //icons
			case 0x150: //icons_2? combo difficulty, damage
				break;
			default:
				int* valuePtr = (int*)((char*)displayable + ofst);
				std::string key = std::format("unk_{:x}", ofst);
				SetMemberValue(valuePtr, inputs[key]);
				break;
		}
	}

	/*
	for (int i = 0; i < _countof(displayable->title_translation_offsets); ++i) {
		std::string key = "title_translation_" + std::to_string(i);

		std::string convertedBuffer = EditorT7Utils::ConvertMovelistDisplayableTextToGameText(inputs[key]->buffer);

		bool allocateNewSpace = false;
		int newLen = (int)convertedBuffer.size();
		int currentLen = 0;
		uint32_t offset = displayable->title_translation_offsets[i];
		char* currentString = (char*)m_mvlHead + offset;
		uint32_t copySize = (newLen + 1);

		printf("\n -- Displayable %d - translation %d --\n ", id, i);

		if (offset == 0) {
			printf("EMPTY\n");
			allocateNewSpace = true;
			currentLen = -1;
			displayable->title_translation_offsets[i] = m_mvlHead->displayables_offset;
		}
		else {
			printf("[%s] to [%s]\n", currentString, convertedBuffer.c_str());
			currentLen = (int)strlen(currentString);
		}


		if (newLen != currentLen || allocateNewSpace) {
			// Reallocation
			printf("-- ModifyMovelistDisplayableTextSize -- ofset %d, currLen %d, newLen %d\n", displayable->title_translation_offsets[i], currentLen + 1, newLen + 1);
			printf("old addr: %llx\n", (uint64_t)displayable - (uint64_t)m_mvlHead);
			printf("old offset: %x\n", displayable->title_translation_offsets[i]);
			if (i + 1 < (int)_countof(displayable->title_translation_offsets)) printf("old offset + 1: %x\n", displayable->title_translation_offsets[i + 1]);
			ModifyMovelistDisplayableTextSize(displayable->title_translation_offsets[i], currentLen + 1, newLen + 1);

			displayable = m_iterators.mvl_displayables[id];
			printf("new addr: %llx\n", (uint64_t)displayable - (uint64_t)m_mvlHead);
			if (i + 1 < (int)_countof(displayable->title_translation_offsets)) printf("new offset + 1: %x\n", displayable->title_translation_offsets[i + 1]);
			currentString = (char*)m_mvlHead + displayable->title_translation_offsets[i];
			printf("new offset: %x - [%s]\n", displayable->title_translation_offsets[i], (char*)m_mvlHead + displayable->title_translation_offsets[i]);
		}

		strcpy_s(currentString, copySize, convertedBuffer.c_str());
	}
	*/

	for (int i = 0; i < _countof(displayable->translation_offsets); ++i) {
		std::string key = "translation_" + std::to_string(i);

		std::string convertedBuffer = EditorT7Utils::ConvertMovelistDisplayableTextToGameText(inputs[key]->buffer);

		bool allocateNewSpace = false;
		int newLen = (int)convertedBuffer.size();
		int currentLen = 0;
		uint32_t offset = GetFieldValue(inputs[key + "_offset"]);
		char* currentString = (char*)m_mvlHead + offset;
		uint32_t copySize = (newLen + 1);

		//printf("\n -- Displayable %d - translation %d --\n ", id, i);

		if (offset == 0) {
			//printf("EMPTY\n");
			allocateNewSpace = true;
			currentLen = -1;
			displayable->translation_offsets[i] = m_mvlHead->displayables_offset;
		}
		else {
			//printf("[%s] to [%s]\n", currentString, convertedBuffer.c_str());
			currentLen = (int)strlen(currentString);
		}


		if (newLen != currentLen || allocateNewSpace) {
			// Reallocation
			//printf("-- ModifyMovelistDisplayableTextSize -- ofset %d, currLen %d, newLen %d\n", displayable->translation_offsets[i], currentLen + 1, newLen + 1);
			//printf("old addr: %llx\n", (uint64_t)displayable - (uint64_t)m_mvlHead);
			//printf("old offset: %x\n", displayable->translation_offsets[i]);
			//if (i + 1 < (int)_countof(displayable->translation_offsets)) printf("old offset + 1: %x\n", displayable->translation_offsets[i + 1]);
			ModifyMovelistDisplayableTextSize(displayable->translation_offsets[i], currentLen + 1, newLen + 1);

			displayable = m_iterators.mvl_displayables[id];
			//printf("new addr: %llx\n", (uint64_t)displayable - (uint64_t)m_mvlHead);
			//if (i + 1 < (int)_countof(displayable->translation_offsets)) printf("new offset + 1: %x\n", displayable->translation_offsets[i + 1]);
			currentString = (char*)m_mvlHead + displayable->translation_offsets[i];
			//printf("new offset: %x - [%s]\n", displayable->translation_offsets[i], (char*)m_mvlHead + displayable->translation_offsets[i]);
		}

		//printf("Copy string at %llx - %d bytes\n",  (uint64_t)currentString - (uint64_t)m_moveset, copySize);
		strcpy_s(currentString, copySize, convertedBuffer.c_str());
	}
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

	CREATE_FIELD("distance", 0, EditorInput_U16, playable->distance);
	CREATE_FIELD("p2_action", 0, EditorInput_U16_Changeable, playable->p2_action);
	CREATE_FIELD("p2_rotation", 0, EditorInput_U16_Changeable, playable->p2_rotation);
	CREATE_FIELD("_unk0x6", 0, EditorInput_U16_Changeable, playable->_unk0x6);
	CREATE_FIELD("_unk0x8", 0, EditorInput_U16_Changeable, playable->_unk0x8);
	CREATE_FIELD("p1_facing_related", 0, EditorInput_U16_Changeable, playable->p1_facing_related);
	CREATE_FIELD("_unk0xc", 0, EditorInput_U16_Changeable, playable->_unk0xc);
	CREATE_FIELD("input_sequence_id", 0, EditorInput_U16 | EditorInput_Interactable, playable->input_sequence_offset);
	CREATE_FIELD("input_count", 0, EditorInput_U16, playable->input_count);
	CREATE_FIELD("has_rage", 0, EditorInput_U16_Changeable, playable->has_rage);
	CREATE_FIELD("_unk0x16", 0, EditorInput_U16_Changeable, playable->_unk0x16);

	WriteFieldFullname(inputMap, "mvl_playable");
	return inputMap;
}

void EditorT7::SaveMovelistPlayable(uint16_t id, std::map<std::string, EditorInput*>& inputs)
{
	auto playable = m_iterators.mvl_playables[id];

	uint32_t playable_addr = m_mvlHead->playables_offset + sizeof(MvlPlayable) * id;
	uint32_t input_sequence_id = (uint32_t)GetFieldValue(inputs["input_sequence_id"]);
	uint32_t input_sequence_addr = input_sequence_id * sizeof(MvlInput) + m_mvlHead->inputs_offset;
	uint32_t input_sequence_offset = input_sequence_addr - playable_addr;

	SetMemberValue(&playable->p2_action, inputs["p2_action"]);
	SetMemberValue(&playable->distance, inputs["distance"]);
	SetMemberValue(&playable->p2_rotation, inputs["p2_rotation"]);
	SetMemberValue(&playable->_unk0x6, inputs["_unk0x6"]);
	SetMemberValue(&playable->_unk0x8, inputs["_unk0x8"]);
	SetMemberValue(&playable->p1_facing_related, inputs["p1_facing_related"]);
	SetMemberValue(&playable->_unk0xc, inputs["_unk0xc"]);
	SetMemberValue(&playable->input_count, inputs["input_count"]);
	SetMemberValue(&playable->input_sequence_offset, inputs["input_sequence_id"]);
	SetMemberValue(&playable->has_rage, inputs["has_rage"]);
	SetMemberValue(&playable->_unk0x16, inputs["_unk0x16"]);
}

// Utils

std::string EditorT7::GetDisplayableMovelistInputStr(const char* directions, const char* buttons)
{
	std::string retVal;

	unsigned int d = strtol(directions, nullptr, 16);
	unsigned int b = strtol(buttons, nullptr, 16);

	const char* directionStrings[4] = { "/U", "/F", "/D", "/B" };
	for (int i = 0; i < 4; ++i)
	{
		if (d & (0x10 << i)) {
			retVal += directionStrings[i];
		}
	}

	std::string buttons_str;
	const char* buttonStrings[4] = { "+2", "+4", "+3", "+1" };
	for (int i = 0; i < 4; ++i)
	{
		if (b & (0x10 << i)) {
			buttons_str += buttonStrings[i];
		}
	}

	if (retVal.size() != 0) {
		retVal.erase(0, 1);
	}

	if (buttons_str.size() != 0) {
		if (retVal.size() == 0) {
			buttons_str.erase(0, 1);
		}
		retVal += buttons_str;
	}

	if (retVal.size() == 0 && (b + d) != 0) {
		retVal = "???";
	}

	return retVal;
}

int EditorT7::GetDisplayableMovelistEntryColor(EditorInput* field)
{
	MvlDisplayableType type = (uint32_t)GetFieldValue(field);

	switch (type & 0xFFFF)
	{
	case 8:
		return MVL_CATEGORY;
		break;
	case 2:
	case 3:
	case 4:
	case 6:
	case 7:
	case 9:
		return MVL_DISABLED;
		break;
	case 10:
		return MVL_COMBO;
		break;
	default:
		return 0;
		break;
	}
}

bool EditorT7::IsMovelistDisplayableEntryCategory(EditorInput* field)
{
	MvlDisplayableType type = (uint32_t)GetFieldValue(field);
	return type == 8;
}

bool EditorT7::IsMovelistDisplayableEntryCombo(EditorInput* field)
{
	MvlDisplayableType type = (uint32_t)GetFieldValue(field);
	return type == 10;
}
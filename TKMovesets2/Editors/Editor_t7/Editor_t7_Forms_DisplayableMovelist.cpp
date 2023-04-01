#include "Editor_t7.hpp"

using namespace EditorUtils;

#define SET_DEFAULT_VAL(fieldName, format, value) sprintf_s(inputMap[fieldName]->buffer, FORM_INPUT_BUFSIZE, format, value)
#define CREATE_STRING_FIELD(a, c, d, g) drawOrder.push_back(a), inputMap[a] = new EditorInput { .category = c, .imguiInputFlags = 0, .flags = EditorInput_String | d }, SET_DEFAULT_VAL(a, "%s", g)
#define CREATE_FIELD(a, c, e, g) drawOrder.push_back(a), inputMap[a] = new EditorInput { .category = c, .imguiInputFlags = GetFieldCharset(e), .flags = e }, SET_DEFAULT_VAL(a, GetFieldFormat(e), g), SetInputfieldColor(inputMap[a])

std::vector<std::map<std::string, EditorInput*>> EditorT7::GetMovelistDisplayablesInputs(uint16_t id, VectorSet<std::string>& drawOrder)
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
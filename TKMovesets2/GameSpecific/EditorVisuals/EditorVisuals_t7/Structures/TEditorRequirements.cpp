#include <format>

#include "TEditorStructures.hpp"
#include "EditorVisuals.hpp"

void TEditorRequirements::OnUpdate(int listIdx, EditorInput* field)
{
	auto& name = field->name;
	auto& fields = m_items[listIdx]->identifierMap;
	const int bufsize = field->bufsize;

	if (name == "param_unsigned")
	{
		uint64_t value = EditorUtils::GetFieldValue(field);
		sprintf_s(fields["param_float"]->buffer, bufsize, "%.7g", *(float*)&value);
	}
	else if (name == "param_float")
	{
		uint64_t value = EditorUtils::GetFieldValue(field);
		sprintf_s(fields["param_unsigned"]->buffer, bufsize, "%u", *(uint32_t*)&value);
	}

	BuildItemDetails(listIdx);
	m_editor->Live_OnFieldEdit(windowType, structureId + listIdx, field);
}

void TEditorRequirements::BuildItemDetails(int listIdx)
{
	std::string label;

	auto& map = m_items[listIdx]->identifierMap;

	int id = (int)EditorUtils::GetFieldValue(map["condition"]);
	int64_t value = (int64_t)EditorUtils::GetFieldValue(map["param_unsigned"]);

	const char* idLabel = m_baseWindow->labels->GetText(id);
	std::string valueText;

	if (value > 15 || value < 0) {
		valueText = std::format("{} / {} / 0x{:x} / {:f}f", *(int32_t*)&value, value, value, *(float*)&value);
	}
	else {
		valueText = std::to_string(value);
	}

	if (idLabel == nullptr) {
		label = std::format("{} / 0x{:x} = {}", id, id, valueText);
	}
	else {
		label = std::format("{} = {}", idLabel, valueText);
	}

	m_items[listIdx]->itemLabel = label;
}
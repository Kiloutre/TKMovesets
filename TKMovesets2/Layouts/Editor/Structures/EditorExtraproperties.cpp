#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorExtraproperties.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorExtraproperties::EditorExtraproperties(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow)
{
	windowType = EditorWindowType_Extraproperty;
	m_baseWindow = baseWindow;
	InitForm(windowTitleBase, t_id, editor);
}

void EditorExtraproperties::OnUpdate(int listIdx, EditorInput* field)
{
	auto& name = field->name;
	auto& fields = m_items[listIdx]->identifierMaps;
	const int bufsize = field->bufsize;

	if (name == "value_unsigned")
	{
		uint64_t value = EditorUtils::GetFieldValue(field);

		sprintf_s(fields["value_signed"]->buffer, bufsize, "%d", *(int32_t*)&value);
		sprintf_s(fields["value_hex"]->buffer, bufsize, "0x%X", (uint32_t)value);
		sprintf_s(fields["value_float"]->buffer, bufsize, "%f", *(float*)&value);
	}
	else if (name == "value_signed")
	{
		uint64_t value = EditorUtils::GetFieldValue(field);

		sprintf_s(fields["value_unsigned"]->buffer, bufsize, "%u", *(uint32_t*)&value);
		sprintf_s(fields["value_hex"]->buffer, bufsize, "0x%X", (uint32_t)value);
		sprintf_s(fields["value_signed"]->buffer, bufsize, "%f", *(float*)&value);
	}
	else if (name == "value_hex")
	{
		uint64_t value = EditorUtils::GetFieldValue(field);

		sprintf_s(fields["value_unsigned"]->buffer, bufsize, "%u", *(uint32_t*)&value);
		sprintf_s(fields["value_signed"]->buffer, bufsize, "%d", *(int32_t*)&value);
		sprintf_s(fields["value_float"]->buffer, bufsize, "%f", *(float*)&value);
	}
	else if (name == "value_float")
	{
		uint64_t value = EditorUtils::GetFieldValue(field);

		sprintf_s(fields["value_unsigned"]->buffer, bufsize, "%u", *(uint32_t*)&value);
		sprintf_s(fields["value_signed"]->buffer, bufsize, "%d", *(int32_t*)&value);
		sprintf_s(fields["value_hex"]->buffer, bufsize, "0x%X", *(uint32_t*)&value);
	}

	BuildItemDetails(listIdx);
	m_editor->Live_OnFieldEdit(windowType, id + listIdx, field);
}

void EditorExtraproperties::BuildItemDetails(int listIdx)
{
	std::string label;

	auto& map = m_items[listIdx]->identifierMaps;

	int startingFrame = atoi(map["starting_frame"]->buffer);
	int id = (uint64_t)strtoll(map["id"]->buffer, nullptr, 16);
	uint64_t value = (uint64_t)strtoll(map["value_unsigned"]->buffer, nullptr, 10);

	const char* idLabel = m_baseWindow->labels->GetText(id);

	std::string startingFrameText;
	std::string valueText;

	if (startingFrame >= 32769) {
		// Starting frame can sometimes be at 0x4000 and above. todo : look into what this does
		switch (startingFrame)
		{
		case 9999:
			startingFrameText = _("edition.extraproperty.every_frame");
			break;
		case 32769:
			startingFrameText = _("edition.extraproperty.instant");
			break;
		default:
			startingFrameText = std::format("{} (+{})", _("edition.extraproperty.instant"), startingFrame - 32769);
			break;
		}
	}
	else {
		startingFrameText = std::to_string(startingFrame);
	}

	if (value > 15) {
		valueText = std::format("{} / {} / 0x{:x} / {:f}f", *(int32_t*)&value, value, value, *(float*)&value);
	}
	else {
		valueText = std::to_string(value);
	}

	if (idLabel == nullptr) {
		label = std::format("{}: 0x{:x} =  {}", startingFrameText, id, valueText);
	} else {
		label = std::format("{}: {} = {}", startingFrameText, idLabel, valueText);
	}

	m_items[listIdx]->itemLabel = label;


	if (m_editor->IsPropertyThrowCameraRef(map["id"]->buffer))
	{
		m_items[listIdx]->color = PROPID_THROW_CAM;
		map["value_signed"]->visible = false;
		map["value_hex"]->visible = false;
		map["value_float"]->visible = false;
		EditorFormUtils::SetFieldDisplayText(map["value_unsigned"], _("edition.extraproperty.throw_camera_id"));
	}
	else {
		m_items[listIdx]->color = 0;
		map["value_signed"]->visible = true;
		map["value_hex"]->visible = true;
		map["value_float"]->visible = true;
		EditorFormUtils::SetFieldDisplayText(map["value_unsigned"], _(map["value_unsigned"]->fullName.c_str()));
	}
}
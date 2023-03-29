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
	int64_t value = (int64_t)strtoll(map["value_unsigned"]->buffer, nullptr, 10);

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
	else if (startingFrame >= 0x4000)
	{
		const char* format_str = _("edition.extraproperty.every_nth_frame");
		const int bufsize = strlen(format_str) + 8;
		char* buf = new char[bufsize];
		sprintf_s(buf, bufsize, format_str, startingFrame - 0x4000);
		startingFrameText = std::string(buf);
		delete[] buf;
	}
	else {
		startingFrameText = std::to_string(startingFrame);
	}

	if (value > 15 || value < 0) {
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

	bool isCameraRef = m_editor->IsPropertyThrowCameraRef(map["id"]->buffer);
	bool isProjectileRef = m_editor->IsPropertyProjectileRef(map["id"]->buffer);
	if (isCameraRef || isProjectileRef)
	{
		map["value_unsigned"]->flags |= EditorInput_Clickable;
		if (isCameraRef) {
			m_items[listIdx]->color = PROPID_THROW_CAM;
			EditorFormUtils::SetFieldDisplayText(map["value_unsigned"], _("edition.extraproperty.throw_camera_id"));
			int id = atoi(map["value_unsigned"]->buffer);
			map["value_unsigned"]->errored = id < 0 || id >= m_editor->movesetTable.throwCamerasCount;
		}
		else if (isProjectileRef) {
			m_items[listIdx]->color = PROPID_PROJECTILE;
			EditorFormUtils::SetFieldDisplayText(map["value_unsigned"], _("edition.extraproperty.projectile_id"));
			map["value_unsigned"]->errored = id < 0 || id >= m_editor->movesetTable.projectileCount;
		}

		map["value_signed"]->visible = false;
		map["value_hex"]->visible = false;
		map["value_float"]->visible = false;
	}
	else {
		if (map["value_unsigned"]->flags & EditorInput_Clickable) {
			map["value_unsigned"]->flags -= EditorInput_Clickable;
		}
		m_items[listIdx]->color = 0;
		map["value_signed"]->visible = true;
		map["value_hex"]->visible = true;
		map["value_float"]->visible = true;
		EditorFormUtils::SetFieldDisplayText(map["value_unsigned"], _(map["value_unsigned"]->fullName.c_str()));
	}
}

void EditorExtraproperties::OnFieldLabelClick(int listIdx, EditorInput* field)
{
	auto& map = m_items[listIdx]->identifierMaps;
	int id = atoi(field->buffer);

	bool isProjectileRef = m_editor->IsPropertyProjectileRef(map["id"]->buffer);
	if (m_editor->IsPropertyThrowCameraRef(map["id"]->buffer)) {
		m_baseWindow->OpenFormWindow(EditorWindowType_ThrowCamera, id);
	}
	else {
		m_baseWindow->OpenFormWindow(EditorWindowType_Projectile, id);
	}
}
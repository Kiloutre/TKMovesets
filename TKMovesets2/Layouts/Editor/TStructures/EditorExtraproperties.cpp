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
	auto& fields = m_items[listIdx]->identifierMap;
	const int bufsize = field->bufsize;

	if (name == "value_unsigned")
	{
		union {
			uint64_t value_uint64;
			uint32_t value_uint32;
			int32_t value_int32;
			float value_float;
		};
		value_uint64 = EditorUtils::GetFieldValue(field);

		sprintf_s(fields["value_signed"]->buffer, bufsize, "%d", value_int32);
		sprintf_s(fields["value_hex"]->buffer, bufsize, "0x%X", value_uint32);
		sprintf_s(fields["value_float"]->buffer, bufsize, "%f", value_float);
	}
	else if (name == "value_signed")
	{
		union {
			uint64_t value_uint64;
			uint32_t value_uint32;
			int32_t value_int32;
			float value_float;
		};
		value_uint64 = EditorUtils::GetFieldValue(field);

		sprintf_s(fields["value_unsigned"]->buffer, bufsize, "%d", value_uint32);
		sprintf_s(fields["value_hex"]->buffer, bufsize, "0x%X", value_uint32);
		sprintf_s(fields["value_float"]->buffer, bufsize, "%f", value_float);
	}
	else if (name == "value_hex")
	{
		union {
			uint64_t value_uint64;
			uint32_t value_uint32;
			int32_t value_int32;
			float value_float;
		};
		value_uint64 = EditorUtils::GetFieldValue(field);

		sprintf_s(fields["value_signed"]->buffer, bufsize, "%d", value_int32);
		sprintf_s(fields["value_unsigned"]->buffer, bufsize, "0x%X", value_uint32);
		sprintf_s(fields["value_float"]->buffer, bufsize, "%f", value_float);
	}
	else if (name == "value_float")
	{
		union {
			uint64_t value_uint64;
			uint32_t value_uint32;
			int32_t value_int32;
			float value_float;
		};
		value_uint64 = EditorUtils::GetFieldValue(field);

		sprintf_s(fields["value_signed"]->buffer, bufsize, "0x%X", value_int32);
		sprintf_s(fields["value_unsigned"]->buffer, bufsize, "%d", value_uint32);
		sprintf_s(fields["value_hex"]->buffer, bufsize, "0x%X", value_uint32);
	}

	BuildItemDetails(listIdx);
	m_editor->Live_OnFieldEdit(windowType, structureId + listIdx, field);
}

void EditorExtraproperties::BuildItemDetails(int listIdx)
{
	std::string label;

	auto& map = m_items[listIdx]->identifierMap;

	unsigned int startingFrame = (unsigned int)EditorUtils::GetFieldValue(map["starting_frame"]);
	unsigned int propId = (unsigned int)EditorUtils::GetFieldValue(map["id"]);
	union {
		uint64_t value_uint64;
		uint32_t value_uint32;
		int32_t value_int32;
		float value_float;
	};
	value_uint64 = EditorUtils::GetFieldValue(map["value_signed"]);

	const char* idLabel = m_baseWindow->labels->GetText(propId);

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
		const int bufsize = (int)strlen(format_str) + 8;
		char* buf = new char[bufsize];
		sprintf_s(buf, bufsize, format_str, startingFrame - 0x4000);
		startingFrameText = std::string(buf);
		delete[] buf;
	}
	else {
		startingFrameText = std::to_string(startingFrame);
	}

	if (value_int32 > 15 || value_int32 < 0) {
		valueText = std::format("{} / {} / 0x{:x} / {:f}f", value_int32, value_uint32, value_uint32, value_float);
	}
	else {
		valueText = std::to_string(value_int32);
	}

	if (idLabel == nullptr) {
		label = std::format("{}: 0x{:x} =  {}", startingFrameText, propId, valueText);
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
			int throwId = atoi(map["value_unsigned"]->buffer);
			map["value_unsigned"]->errored = throwId < 0 || throwId >= (int)m_editor->GetStructureCount(EditorWindowType_ThrowCamera);
		}
		else if (isProjectileRef) {
			m_items[listIdx]->color = PROPID_PROJECTILE;
			EditorFormUtils::SetFieldDisplayText(map["value_unsigned"], _("edition.extraproperty.projectile_id"));
			int projectileId = atoi(map["value_unsigned"]->buffer);
			map["value_unsigned"]->errored = projectileId < 0 || projectileId >= (int)m_editor->GetStructureCount(EditorWindowType_Projectile);
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
	auto& map = m_items[listIdx]->identifierMap;
	int id = atoi(field->buffer);

	bool isProjectileRef = m_editor->IsPropertyProjectileRef(map["id"]->buffer);
	if (m_editor->IsPropertyThrowCameraRef(map["id"]->buffer)) {
		m_baseWindow->OpenFormWindow(EditorWindowType_ThrowCamera, id);
	}
	else {
		m_baseWindow->OpenFormWindow(EditorWindowType_Projectile, id);
	}
}


void EditorExtraproperties::PreItemRender(int listIdx)
{
	if (listIdx + 1 == m_listSize) {
		return;
	}

	auto cursor = ImGui::GetCursorPos();
	float pos_x = ImGui::GetContentRegionAvail().x - 150;
	
	ImGui::SetCursorPosX(pos_x);
	ImGui::PushStyleColor(ImGuiCol_Button, FORM_SPECIAL_BTN);
	if (ImGui::Button(">", ImVec2(20, 18)))
	{
		//play
	}
	ImGui::PopStyleColor();

	ImGui::SetCursorPos(cursor);
}
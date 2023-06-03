#include <format>

#include "TEditorStructures.hpp"
#include "Localization.hpp"
#include "EditorVisuals_t7.hpp"

void TEditorExtraproperties::OnUpdate(int listIdx, EditorInput* field)
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
		sprintf_s(fields["value_float"]->buffer, bufsize, "%.7g", value_float);
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
		sprintf_s(fields["value_float"]->buffer, bufsize, "%.7g", value_float);
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
		sprintf_s(fields["value_float"]->buffer, bufsize, "%.7g", value_float);
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

void TEditorExtraproperties::BuildItemDetails(int listIdx)
{
	auto editor = Editor<EditorT7>();

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
        try {
            char* buf = new char[bufsize];
            sprintf_s(buf, bufsize, format_str, startingFrame - 0x4000);
            startingFrameText = std::string(buf);
            delete[] buf;
        } catch (std::bad_alloc&) {
            DEBUG_ERR("Extraprop startingFrameText: Failed to allocate buffer");
        }
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

	bool isCameraRef = editor->IsPropertyThrowCameraRef(map["id"]->buffer);
	bool isProjectileRef = editor->IsPropertyProjectileRef(map["id"]->buffer);
	if (isCameraRef || isProjectileRef)
	{
		map["value_unsigned"]->flags |= EditorInput_Clickable;
		if (isCameraRef) {
			m_items[listIdx]->color = PROPID_THROW_CAM;
			EditorFormUtils::SetFieldDisplayText(map["value_unsigned"], _("edition.extraproperty.throw_camera_id"));
			int throwId = atoi(map["value_unsigned"]->buffer);
			map["value_unsigned"]->errored = throwId < 0 || throwId >= (int)editor->GetStructureCount(TEditorWindowType_ThrowCamera);
		}
		else if (isProjectileRef) {
			m_items[listIdx]->color = PROPID_PROJECTILE;
			EditorFormUtils::SetFieldDisplayText(map["value_unsigned"], _("edition.extraproperty.projectile_id"));
			int projectileId = atoi(map["value_unsigned"]->buffer);
			map["value_unsigned"]->errored = projectileId < 0 || projectileId >= (int)editor->GetStructureCount(TEditorWindowType_Projectile);
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

void TEditorExtraproperties::OnFieldLabelClick(int listIdx, EditorInput* field)
{
	auto editor = Editor<EditorT7>();

	auto& map = m_items[listIdx]->identifierMap;
	int id = atoi(field->buffer);

	bool isProjectileRef = editor->IsPropertyProjectileRef(map["id"]->buffer);
	if (editor->IsPropertyThrowCameraRef(map["id"]->buffer)) {
		m_baseWindow->OpenFormWindow(TEditorWindowType_ThrowCamera, id);
	}
	else {
		m_baseWindow->OpenFormWindow(TEditorWindowType_Projectile, id);
	}
}


void TEditorExtraproperties::PreItemRender(int listIdx)
{
	// play extraprop button

	/*
	if (listIdx + 1 == m_listSize) {
		return;
	}

	auto cursor = ImGui::GetCursorPos();
	float pos_x = ImGui::GetContentRegionAvail().x - 140;
	
	ImGui::SetCursorPosX(pos_x);
	ImGui::PushStyleColor(ImGuiCol_Button, FORM_SPECIAL_BTN);
	ImGui::PushID(listIdx);
	if (ImGui::Button(">", ImVec2(20, 18)))
	{
		auto identifierMap = m_items[listIdx]->identifierMap;

		// Only execute if none of the related fields are errored
		bool errored = false;
		for (auto& [key, v] : identifierMap) {
			if (v->errored) {
				errored = true;
				break;
			}
		}

		if (!errored) {
			m_editor->ExecuteExtraprop(identifierMap["id"], identifierMap["value_unsigned"]);
		}

	}
	ImGui::PopID();
	ImGui::PopStyleColor();

	ImGui::SetCursorPos(cursor);
	*/
}
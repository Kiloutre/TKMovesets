#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorMovelistDisplayable.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorMovelistDisplayable::EditorMovelistDisplayable(std::string windowTitleBase, Editor* editor, EditorWindowBase* baseWindow)
{
	windowType = EditorWindowType_MovelistDisplayable;
	m_baseWindow = baseWindow;
	uniqueType = true;
	InitForm(windowTitleBase, 0, editor);
	// Trigger update to build all the item details
	OnUpdate(0, nullptr);
}

// -- Private methods-- //

void EditorMovelistDisplayable::OnUpdate(int listIdx, EditorInput* field)
{
	m_editor->Live_OnFieldEdit(windowType, structureId + listIdx, field);

	int visibleIndex = 1;
	bool comboStarted = false;
	for (int i = 0; i < m_listSize; ++i)
	{
		auto& item = m_items[i];
		auto& typeField = item->identifierMaps["type"];

		item->color = m_editor->GetDisplayableMovelistEntryColor(typeField);
		bool isPlayable = ((int64_t)EditorUtils::GetFieldValue(item->identifierMaps["playable_id"])) >= 0;

		if (m_editor->IsMovelistDisplayableEntryCategory(typeField)) {
			item->itemLabel = std::format("- {} -", item->identifierMaps["title_translation_1"]->buffer);
		}
		else {
			if (!isPlayable) {
				item->color = MVL_DISABLED;
			}

			item->itemLabel = std::format("{} - {}", visibleIndex, item->identifierMaps["title_translation_1"]->buffer);
			if (!comboStarted && m_editor->IsMovelistDisplayableEntryCombo(typeField)) {
				visibleIndex = 1;
				comboStarted = true;
			}
			else {
				++visibleIndex;
			}
		}
	}
}

void EditorMovelistDisplayable::OnFieldLabelClick(int listIdx, EditorInput* field)
{
	int referenceId = atoi(field->buffer);
	std::string& name = field->name;

	if (name == "playable_id") {
		if (referenceId != -1) {
			m_baseWindow->OpenFormWindow(EditorWindowType_MovelistPlayable, referenceId);
		}
	}
}

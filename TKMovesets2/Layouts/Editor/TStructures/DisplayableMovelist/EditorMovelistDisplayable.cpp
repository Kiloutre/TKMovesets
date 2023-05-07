#include <ImGui.h>
#include <format>

#include "imgui_extras.hpp"
#include "EditorMovelistDisplayable.hpp"
#include "Localization.hpp"


// -- Public methods -- //

void EditorMovelistDisplayable::OnInitEnd()
{
	BuidAllLabels();
}

// -- Private methods-- //

void EditorMovelistDisplayable::BuidAllLabels()
{
	int visibleIndex = 1;
	bool comboStarted = false;
	for (int i = 0; i < m_listSize; ++i)
	{
		auto& item = m_items[i];
		auto& identifierMap = item->identifierMap;
		auto& typeField = identifierMap["type"];
		auto& playableField = identifierMap["playable_id"];

		item->color = m_editor->GetDisplayableMovelistEntryColor(typeField);
		bool isPlayable = ((int64_t)EditorUtils::GetFieldValue(playableField)) >= 0;

		if (m_editor->IsMovelistDisplayableEntryCategory(typeField)) {
			item->itemLabel = std::format("- {} -", identifierMap["translation_1"]->buffer);

			if (playableField->flags & EditorInput_Interactable) {
				playableField->flags ^= EditorInput_Interactable;
			}
		}
		else {
			if (!isPlayable) {
				item->color = MVL_DISABLED;
				if (playableField->flags & EditorInput_Interactable) {
					playableField->flags ^= EditorInput_Interactable;
				}
			}
			else if ((playableField->flags & EditorInput_Interactable) == 0) {
				playableField->flags |= EditorInput_Interactable;
			}

			if (!comboStarted && m_editor->IsMovelistDisplayableEntryCombo(typeField)) {
				visibleIndex = 1;
				comboStarted = true;
			}

			std::string extraLabel = m_editor->GetMovelistDisplayableLabel(identifierMap);
			item->itemLabel = std::format("{} - {}{}", visibleIndex, identifierMap["title_translation_1"]->buffer, extraLabel.c_str());
			++visibleIndex;
		}
	}
}

void EditorMovelistDisplayable::OnUpdate(int listIdx, EditorInput* field)
{
	m_editor->Live_OnFieldEdit(windowType, structureId + listIdx, field);
	BuidAllLabels();
}

void EditorMovelistDisplayable::OnFieldLabelClick(int listIdx, EditorInput* field)
{
	int referenceId = atoi(field->buffer);
	auto& name = field->name;

	if (name == "playable_id") {
		if (referenceId != -1) {
			m_baseWindow->OpenFormWindow(EditorWindowType_MovelistPlayable, referenceId);
		}
	}
}

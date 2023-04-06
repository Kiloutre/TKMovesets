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
	BuidAllLabels();
}

// -- Private methods-- //

void EditorMovelistDisplayable::Apply()
{
	if (!IsFormValid()) {
		return;
	}

	if (m_listSizeChange != 0)
	{
		// If items were added/removed, reallocate entire moveset
		int newSize = (int)m_listSize;
		int oldSize = newSize - m_listSizeChange;
		std::vector<int> itemIndexes;
		for (auto& item : m_items) {
			itemIndexes.push_back(item->id);
		}
		m_editor->ModifyListSize2(windowType, structureId, itemIndexes, m_deletedItemIds);
		if (m_listSize != 0) {
				OnApplyResize(m_listSizeChange, oldSize);
		}
		m_listSizeChange = 0;
	}

	unsavedChanges = false;
	justAppliedChanges = true;
	m_requestedClosure = false;

	if (m_listSize == 0) {
		// List is now empty, no nede to kepe this open
		popen = false;
		return;
	}

	// Write into every individual item
	/*
	for (uint32_t listIndex = 0; listIndex < m_listSize; ++listIndex) {
		m_editor->SaveItem(windowType, structureId + listIndex, m_items[listIndex]->identifierMaps);
	}
	*/

	// After everything was saved, re-set the IDs of the list items in case reordering/deletion/creation happened
	for (uint32_t listIndex = 0; listIndex < m_listSize; ++listIndex) {
		m_items[listIndex]->id = structureId + listIndex;
	}
	m_deletedItemIds.clear();

	OnApply();
}

void EditorMovelistDisplayable::BuidAllLabels()
{
	int visibleIndex = 1;
	bool comboStarted = false;
	for (int i = 0; i < m_listSize; ++i)
	{
		auto& item = m_items[i];
		auto& identifierMap = item->identifierMaps;
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
	//m_editor->Live_OnFieldEdit(windowType, structureId + listIdx, field);
	BuidAllLabels();
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

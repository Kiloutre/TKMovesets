#pragma once

#include "Editor.hpp"
#include "EditorFormList.hpp"

// Windowed class managing a list of structs
class EditorMoveEndProperty : public EditorFormList
{
private:
	void OnApplyResize(int sizeChange, int oldSize) override;
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
public:
	EditorMoveEndProperty(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow);
	void RequestFieldUpdate(std::string fieldName, int valueChange, int listStart, int listEnd) override;
};
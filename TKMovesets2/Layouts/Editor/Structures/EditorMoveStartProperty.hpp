#pragma once

#include "Editor.hpp"
#include "EditorFormList.hpp"

// Windowed class managing a list of structs
class EditorMoveStartProperty : public EditorFormList
{
private:
	void OnResize(int sizeChange, int oldSize) override;
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
public:
	EditorMoveStartProperty(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow);
};
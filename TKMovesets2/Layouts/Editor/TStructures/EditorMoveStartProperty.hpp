#pragma once

#include "Editor.hpp"
#include "EditorFormList.hpp"

// Windowed class managing a list of structs
class EditorMoveStartProperty : public EditorFormList
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
public:
	using EditorFormList::EditorFormList;
	void RequestFieldUpdate(EditorWindowType_ winType, int valueChange, int listStart, int listEnd) override;
};
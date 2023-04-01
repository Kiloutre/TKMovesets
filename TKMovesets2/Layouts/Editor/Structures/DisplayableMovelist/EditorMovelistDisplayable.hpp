#pragma once

#include "Editor.hpp"
#include "EditorFormList.hpp"

class EditorMovelistDisplayable : public EditorFormList
{
	void BuildItemDetails(int listIdx) override;
	void OnUpdate(int listIdx, EditorInput* field) override;
public:
	EditorMovelistDisplayable(std::string windowTitleBase, Editor* editor, EditorWindowBase* baseWindow);
};
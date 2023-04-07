#pragma once

#include "Editor.hpp"
#include "EditorFormList.hpp"

class EditorMovelistInput : public EditorFormList
{
private:
	void BuildItemDetails(int listIdx) override;
	void OnUpdate(int listIdx, EditorInput* field) override;
public:
	EditorMovelistInput(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow, int listSize);
};
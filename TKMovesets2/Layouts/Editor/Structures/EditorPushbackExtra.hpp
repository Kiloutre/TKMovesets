#pragma once

#include "Editor.hpp"
#include "EditorFormList.hpp"

class EditorPushbackExtra : public EditorFormList
{
private:
	void OnUpdate(int listIdx, EditorInput* field) override;
	void BuildItemDetails(int listIdx) override;
public:
	EditorPushbackExtra(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow, int listSize);
};
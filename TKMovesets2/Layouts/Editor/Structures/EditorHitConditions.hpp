#pragma once

#include "Editor.hpp"
#include "EditorFormList.hpp"

class EditorHitConditions : public EditorFormList
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
public:
	EditorHitConditions(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow);
};
#pragma once

#include "Editor.hpp"
#include "EditorFormList.hpp"
#include "EditorWindowBase.hpp"

class EditorCancels : public EditorFormList
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
	void OnUpdate(int listIdx, EditorInput* field) override;
	void BuildItemLabel(int listIdx) override;
public:
	EditorCancels(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow);
};
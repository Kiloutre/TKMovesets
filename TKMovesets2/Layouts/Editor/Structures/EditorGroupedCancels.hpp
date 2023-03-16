#pragma once

#include "Editor.hpp"
#include "EditorFormList.hpp"

class EditorGroupedCancels : public EditorFormList
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
	void OnUpdate(int listIdx, EditorInput* field) override;
public:
	EditorGroupedCancels(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow);
};
#pragma once

#include "Editor.hpp"
#include "EditorFormList.hpp"

class EditorExtraproperties : public EditorFormList
{
private:
	void OnUpdate(int listIdx, EditorInput* field) override;
	void BuildItemDetails(int listIdx) override;
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
public:
	EditorExtraproperties(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow);
};
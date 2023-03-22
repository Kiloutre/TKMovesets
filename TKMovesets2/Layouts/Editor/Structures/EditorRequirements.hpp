#pragma once

#include "Editor.hpp"
#include "EditorFormList.hpp"

class EditorRequirements : public EditorFormList
{
private:
	void BuildItemDetails(int listIdx) override;
	void OnUpdate(int listIdx, EditorInput* field) override;
public:
	EditorRequirements(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow);
};
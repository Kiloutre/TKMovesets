#pragma once

#include "Editor.hpp"
#include "EditorFormList.hpp"

class EditorGroupedCancels : public EditorFormList
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
	void BuildItemDetails(int listIdx) override;
	void OnResize(int sizeChange, int oldSize) override;
	void RequestFieldUpdate(std::string fieldName, int valueChange, int listStart, int listEnd) override;
public:
	EditorGroupedCancels(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow);
};
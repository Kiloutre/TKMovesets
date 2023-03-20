#pragma once

#include "Editor.hpp"
#include "EditorFormList.hpp"
#include "EditorWindowBase.hpp"

class EditorCancels : public EditorFormList
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
	void OnUpdate(int listIdx, EditorInput* field) override;
	void BuildItemDetails(int listIdx) override;
	void OnResize(int sizeChange, int oldSize) override;
public:
	EditorCancels(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow);
	void RequestFieldUpdate(std::string fieldName, int valueChange, int listStart, int listEnd) override;
};
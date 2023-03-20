#pragma once

#include "Editor.hpp"
#include "EditorForm.hpp"

class EditorMove : public EditorForm
{
private:
	void OnFieldLabelClick(EditorInput* field) override;
	void OnApply() override;
	void ApplyWindowName(bool reapplyWindowProperties = true) override;
	void OnUpdate(int listIdx, EditorInput* field) override;
public:
	EditorMove(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow);
	void RequestFieldUpdate(std::string fieldName, int valueChange, int listStart, int listEnd) override;
};
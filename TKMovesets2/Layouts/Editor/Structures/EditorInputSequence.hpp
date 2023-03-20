#pragma once

#include "Editor.hpp"
#include "EditorForm.hpp"
#include "EditorWindowBase.hpp"

class EditorInputSequence : public EditorForm
{
private:
	void OnFieldLabelClick(EditorInput* field) override;
public:
	EditorInputSequence(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow);
	void RequestFieldUpdate(std::string fieldName, int valueChange, int listStart, int listEnd) override;
};
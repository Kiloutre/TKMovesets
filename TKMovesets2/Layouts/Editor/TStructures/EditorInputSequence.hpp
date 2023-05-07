#pragma once

#include "Editor.hpp"
#include "EditorForm.hpp"
#include "EditorWindowBase.hpp"

class EditorInputSequence : public EditorForm
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
public:
	using EditorForm::EditorForm;
	void RequestFieldUpdate(EditorWindowType_ winType, int valueChange, int listStart, int listEnd) override;
};
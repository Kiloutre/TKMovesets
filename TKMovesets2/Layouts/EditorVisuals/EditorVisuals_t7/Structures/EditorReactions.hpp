#pragma once

#include "Editor.hpp"
#include "EditorForm.hpp"

class EditorReactions: public EditorForm
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
public:
	using EditorForm::EditorForm;
};
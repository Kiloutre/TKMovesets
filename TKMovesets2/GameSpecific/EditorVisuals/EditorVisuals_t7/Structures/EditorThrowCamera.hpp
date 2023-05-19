#pragma once

#include "EditorForm.hpp"

class EditorThrowCamera : public EditorForm
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
public:
	using EditorForm::EditorForm;
};
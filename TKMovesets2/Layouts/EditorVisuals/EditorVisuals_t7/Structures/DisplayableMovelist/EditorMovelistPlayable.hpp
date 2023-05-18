#pragma once

#include "EditorForm.hpp"

class EditorMovelistPlayable : public EditorForm
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
	void RequestFieldUpdate(EditorWindowType winType, int valueChange, int listStart, int listEnd) override;
	void OnUpdate(int listIdx, EditorInput* field) override;
public:
	using EditorForm::EditorForm;
};
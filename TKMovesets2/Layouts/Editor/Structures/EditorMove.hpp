#pragma once

#include "Editor.hpp"
#include "EditorForm.hpp"

class EditorMove : public EditorForm
{
private:
	void OnFieldLabelClick(EditorInput* field) override;
	void OnApply() override;
public:
	EditorMove(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow);
	// All of our EditorForm derived classes are created with the new keyword, so we do have to commit suicide here
	//~EditorMove() { printf("destructed move\n");  delete this; }
};
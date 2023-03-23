#pragma once

#include "Editor.hpp"
#include "EditorForm.hpp"

class EditorThrowCamera : public EditorForm
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
public:
	EditorThrowCamera(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow);
};
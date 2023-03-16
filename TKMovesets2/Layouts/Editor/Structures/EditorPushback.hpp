#pragma once

#include "Editor.hpp"
#include "EditorForm.hpp"
#include "EditorWindowBase.hpp"

class EditorPushback : public EditorForm
{
private:
	void OnFieldLabelClick(EditorInput* field) override;
public:
	EditorPushback(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow);
};
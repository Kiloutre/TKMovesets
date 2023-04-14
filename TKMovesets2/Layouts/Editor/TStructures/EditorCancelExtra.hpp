#pragma once

#include "Editor.hpp"
#include "EditorForm.hpp"

class EditorCancelExtra : public EditorForm
{
public:
	EditorCancelExtra(std::string windowTitleBase, uint32_t t_id, Editor* editor);
};
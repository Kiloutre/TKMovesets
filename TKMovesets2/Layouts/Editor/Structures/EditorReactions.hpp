#pragma once

#include "Editor.hpp"
#include "EditorForm.hpp"

class EditorReactions: public EditorForm
{
public:
	EditorReactions(std::string windowTitleBase, uint32_t t_id, Editor* editor);
};
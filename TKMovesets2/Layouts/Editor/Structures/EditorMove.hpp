#pragma once

#include "Editor.hpp"
#include "EditorForm.hpp"

class EditorMove : public EditorForm
{
public:
	EditorMove(std::string windowTitleBase, uint32_t t_id, Editor* editor);
};
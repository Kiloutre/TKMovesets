#pragma once

#include "Editor.hpp"
#include "EditorForm.hpp"

class EditorPushbackExtra : public EditorForm
{
public:
	EditorPushbackExtra(std::string windowTitleBase, uint32_t t_id, Editor* editor);
};
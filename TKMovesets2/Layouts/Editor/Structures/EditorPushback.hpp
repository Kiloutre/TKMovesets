#pragma once

#include "Editor.hpp"
#include "EditorForm.hpp"

class EditorPushback : public EditorForm
{
public:
	EditorPushback(std::string windowTitleBase, uint32_t t_id, Editor* editor);
};
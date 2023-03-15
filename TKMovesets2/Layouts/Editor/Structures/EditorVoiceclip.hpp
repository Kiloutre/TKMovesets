#pragma once

#include "Editor.hpp"
#include "EditorForm.hpp"

// Windowed class managing a single move
class EditorVoiceclip : public EditorForm
{
public:
	EditorVoiceclip(std::string windowTitleBase, uint32_t t_id, Editor* editor);
};
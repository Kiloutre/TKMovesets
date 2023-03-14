#pragma once

#include "Editor.hpp"
#include "EditorForm.hpp"

// Windowed class managing a single move
class EditorVoiceclip : public EditorForm
{
private:
	// Apply the change to the moveset
	void Apply() override;
public:

	EditorVoiceclip(std::string windowTitleBase, uint32_t t_id, Editor* editor);
};
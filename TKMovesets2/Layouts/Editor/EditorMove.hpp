#pragma once

#include <map>
#include <set>
#include <string>

#include "Editor.hpp"
#include "EditorForm.hpp"


// Windowed class managing a single move
class EditorMove : public EditorForm
{
private:
	// Apply the change to the moveset
	void Apply() override;
public:

	EditorMove(std::string windowTitleBase, uint32_t t_id, Editor* editor);
};
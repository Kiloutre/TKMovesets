#pragma once

#include <string>

#include "Editor.hpp"

class EditorMove
{
private:
	// Contains the window title to display
	std::string m_windowTitle;
	// The form's fields
	std::vector<EditorInput*> m_inputs;
	// List of bool that state which field is invalid
	bool* m_erroredFields;
	// Used when saving
	Editor* m_editor = nullptr;
	// Stores the amount of categories within the form
	uint8_t m_categoryAmount = 0;

public:
	// Stores the state of the window. If close, this class will be destroyed soon
	bool popen = true;
	// If set to true, will display an unsaved icon on the window title bar
	bool unsavedChanges = false;
	// If true, we must call SetNextWndowFocus() before creating our window
	bool setFocus = false;
	// Contains our move ID
	uint16_t moveId;

	EditorMove(std::string windowTitleBase, uint16_t t_moveId, Editor* editor);
	~EditorMove();
	void Render();
	// Apply the change to the moveset
	void Apply();
};
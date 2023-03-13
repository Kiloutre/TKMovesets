#pragma once

#include <string>
#include <map>

#include "Editor.hpp"

class EditorMove
{
private:
	// Contains the window title to display
	std::string m_windowTitle;
	// The form's fields, contains the field buffer along with its validity as a boolean and more useful data 
	std::map<std::string, EditorInput*> m_inputMap;
	// Containst the form fields grouped by categories, contained within their draw order. Same data as m_inputMap but stored differently, used for display.
	std::map<int, std::vector<EditorInput*>> m_inputs;
	// Used when saving
	Editor* m_editor = nullptr;
	// Stores the amount of categories within the form
	uint8_t m_categoryAmount = 0;
	// Contain the field type to pass to generic Editor methods
	std::string m_windowType = "move";

	// Render a single input field in the form
	void RenderInput(EditorInput* field);
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
#pragma once

#include <string>

class EditorMove
{
private:
	std::string m_windowTitle;

public:
	// Stores the state of the window. If close, this class will be destroyed soon
	bool popen = true;
	// If set to true, will display an unsaved icon on the window title bar
	bool unsavedChanges = false;
	// If true, we must call SetNextWndowFocus() before creating our window
	bool setFocus = false;
	// Contains our move ID
	uint16_t moveId;

	EditorMove(std::string windowTitleBase, uint16_t t_moveId);
	void Render();
};
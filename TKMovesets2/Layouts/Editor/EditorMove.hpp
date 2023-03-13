#pragma once

#include <map>
#include <set>
#include <string>

#include "Editor.hpp"

struct ClickableFieldEvent
{
	std::string eventName;
	char* buffer;
};

// Windowed class managing a single move
class EditorMove
{
private:
	// Contains the window title to display
	std::string m_windowTitle;
	// The form's fields, contains the field buffer along with its validity as a boolean and more useful data 
	std::map<std::string, EditorInput*> m_inputMap;
	// Contains the list of category IDs. Does not need to be contiguous. Even categories are open by default, odd ones aren't.
	std::set<uint8_t> m_categories;
	// Containst the form fields grouped by categories, contained within their draw order. Same data as m_inputMap but stored differently, used for display.
	std::map<int, std::vector<EditorInput*>> m_inputs;
	// Used when saving
	Editor* m_editor = nullptr;
	// Contain the field type to pass to generic Editor methods
	std::string m_windowType = "move";
	// If a field is clicked, this will contain its full name and buffer 
	ClickableFieldEvent m_event{ "", nullptr };
	// Contains false if we need to send the event to the next GetFormClickEvent() call
	bool m_consumedEvent = true;

	// Render a single input field in the form
	void RenderInput(EditorInput* field);
	// Render a single input label in the form
	void RenderLabel(EditorInput* field);
	// Apply the change to the moveset
	void Apply();
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
	// Returns a ptr to a clickable event info (field full name, buffer) or nullptr if no new event to fetch
	ClickableFieldEvent* GetFormClickEvent();
};
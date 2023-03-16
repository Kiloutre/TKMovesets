#pragma once

#include <map>
#include <set>
#include <string>

#include "Editor.hpp"
#include "EditorWindowBase.hpp"


namespace EditorFormUtils
{
	// Helps calculate the amount of drawable columns for forms
	int GetColumnCount();
	
	// Returns a string used to build translation strings depending on the window type (move, cancel, etc...)
	std::string GetWindowTypeName(EditorWindowType_ type);
}

// Windowed class managing a single struct
class EditorForm
{
protected:
	// Contains the window title to display
	std::string m_windowTitle;
	// The form's fields, contains the field buffer along with its validity as a boolean and more useful data 
	std::map<std::string, EditorInput*> m_fieldIdentifierMap;
	// Te form fields grouped by categories, contained within their draw order. Same data as m_inputMap but stored differently, used for display.
	std::map<int, std::vector<EditorInput*>> m_fieldsCategoryMap;
	// Contains the list of category IDs. Does not need to be contiguous. Even categories are open by default, odd ones aren't.
	std::set<uint8_t> m_categories;
	// Contains false if we need to send the event to the next GetFormClickEvent() call
	bool m_consumedEvent = true;
	// Used when saving
	Editor* m_editor = nullptr;
	// Contains the identifier with which to prefix translation strings
	std::string m_identifierPrefix;
	// Contains a reference to the main editor window, used for event handling
	EditorWindowBase* m_baseWindow = nullptr;

	// Called whenever a field changes (and is valid). listIdx is always 0 if not a list of structs.
	virtual void OnUpdate(int listIdx, EditorInput* field) {};
	// Called when clicking a field
	virtual void OnFieldLabelClick(EditorInput* field) {};
	// Render inputs from the <category : input> map
	void RenderInputs(std::vector<EditorInput*>& inputs, int category, int columnCount);
	// Render a single input field in the form
	void RenderInput(EditorInput* field);
	// Render a single input label in the form
	void RenderLabel(EditorInput* field);
	// Called when clicking the apply button, will check for field errors, set variables and then save the data
	virtual void Apply();
	// Returns false if any field has an error state
	virtual bool IsFormValid();
	// Inits the form
	void InitForm(std::string windowTitleBase, uint32_t t_id, Editor* editor);
public:
	// Stores the state of the window. If close, this class will be destroyed soon
	bool popen = true;
	// If set to true, will display an unsaved icon on the window title bar
	bool unsavedChanges = false;
	// If is true, changes were just applied. Read by EditorWindows to signal when to highlight the Save/Import button after modifications.
	bool justAppliedChanges = false;
	// If true, we must call SetNextWndowFocus() before creating our window
	bool setFocus = false;
	// Contains our ID or list starting ID for lists (they are contiguous)
	uint32_t id = 0;
	// Type of the widnow, passed to editor functions
	EditorWindowType_ windowType;

	virtual void Render();
};
#pragma once

#include <map>
#include <set>
#include <string>

#include "Editor.hpp"
#include "EditorWindowBase.hpp"


namespace EditorFormUtils
{
	// Sets the field display text and calculate the text size for alignment
	void SetFieldDisplayText(EditorInput* field, const std::string& newName);

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
	InputMap m_fieldIdentifierMap;
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
	// Become true if there were unsaved changes but the user attempted to close the window anyway. Confirmation will be required.
	bool m_requestedClosure = false;
	// Contains the string identifiers to avoid having to compute them at every individual Render() call
	std::map<int, std::string> m_categoryStringIdentifiers;
	// Contains the base title of the main window that we will preprend our title to
	std::string m_windowTitleBase;
	// Contains window info to avoid re-fetching it multiple times per render
	struct WindowInfo
	{
		ImVec2 pos;
		ImVec2 size;
	} m_winInfo;
	// Used for alignment calculations
	float m_labelWidthHalf = 0.0f;

	virtual void PostRender() {};
	// Actual rendering function
	virtual void RenderInternal();
	// Sets the window name and reapplies its position and size because imgui ties window pos / size to their name
	virtual void ApplyWindowName(bool reapplyWindowProperties = true);
	// Called whenever the changes are successfully applied
	virtual void OnApply() {};
	// Called whenever a field changes (and is valid). listIdx is always 0 if not a list of structs.
	virtual void OnUpdate(int listIdx, EditorInput* field);
	// Called when clicking a field
	virtual void OnFieldLabelClick(int listIdx, EditorInput* field) {};
	// Renders buttons to ask if the changes should be discarded or not, if trying to leave without applying
	void RenderDiscardButtons();
	// Render inputs from the <category : input> map
	void RenderInputs(int listIdx, std::vector<EditorInput*>& inputs, int category, int columnCount);
	// Render a single input field in the form
	void RenderInput(int listIdx, EditorInput* field);
	// Render a single input label in the form
	void RenderLabel(int listIdx, EditorInput* field);
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
	uint32_t structureId = 0;
	// Type of the widnow, passed to editor functions
	EditorWindowType_ windowType = EditorWindowType_Invalid;
	// The dock ID to set during the next render 
	int nextDockId = -1;
	// Stores the last viewport the windowwas rendered in. If isn't the main window's, this window won't be closed automatically.
	ImGuiViewport* currentViewport = nullptr;
	// Set to true in a derived constructor to prevent the creation of multiple windows of the derived class
	bool uniqueType = false;
	virtual ~EditorForm();

	// Updates a field (if not errored) containing an ID or an integer value by adding the given value
	virtual void RequestFieldUpdate(EditorWindowType_ winType, int valueChange, int listStart, int listEnd) {};
	void Render();
};
#pragma once

#include <map>
#include <set>
#include <functional>

#include "EditorLogic.hpp"

class EditorVisuals;

namespace EditorFormUtils
{
	InputMap CloneFields(const InputMap& fields, size_t& budget);
    std::map<int, std::vector<EditorInput*>> CloneCategories(const InputMap& source, const InputMap& copy,
        const std::map<int, std::vector<EditorInput*>>& categories);
    void ValidateFields(const InputMap& fields, const VectorSet<std::string>& order);
	// Sets the field display text and calculate the text size for alignment
	void SetFieldDisplayText(EditorInput* field, const std::string& newName);

	// Helps calculate the amount of drawable columns for forms
	int GetColumnCount();
}

// Windowed class managing a single struct
class EditorForm
{
public:
    // Headless captures only (anim-preview-shot open=SECTION): category headers with these localized labels open.
    static inline std::set<std::string> forceOpenCategories;
    struct TransactionState {
        virtual ~TransactionState() = default;
        virtual void Commit() noexcept = 0;
    };
protected:
    struct SavedState;
    void EditForm(const std::function<bool()>& action);
    uint32_t DuplicateStructure();
    bool TryDuplicateStructure();
    bool TryDeleteStructure();
    virtual void OpenDuplicatedForm(uint32_t id, int listSize);
    virtual void PrepareDuplication(unsigned int newId, unsigned int listSize) {};
    virtual void OnDeletion();
    bool m_formEditActive = false;
    virtual std::unique_ptr<TransactionState> BeginEditTransaction();
	std::string m_operationError;
	void TryApply();
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
	EditorLogic* m_editor = nullptr;
	// Contains the identifier with which to prefix translation strings
	std::string m_windowTypeName;
	// Contains a reference to the main editor window, used for event handling
	EditorVisuals* m_baseWindow = nullptr;
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
	// Set by RenderInternal when the pointer is over an area with its own right-click use (the animation preview's
	// view pans with right drag): no context menu then.
	bool m_ignoreRightClick = false;
	// Draws the fields in a child of the given size (the animation preview beside them), with the form's column layout
	// computed for that width.
	void RenderFieldsInChild(const char* id, ImVec2 size);
	// Used with changing data types to prevent .unsavedChanges from being set to true
	bool m_ignoreNextChange = false;
	// True if the deletion popup is open
	bool m_deletionPopupOpen = false;
	// Marks if deletion is possible on this structure
	bool m_isDeletable = true;
    bool m_canDuplicate = true;
	// Will be set to true when opening the context menu if the clipboard can be pasted
	bool m_canPaste = false;

	// Copy the entire form, evrey field of every tiem, to the clipboard
	virtual void CopyFormToClipboard() const;
	// Pastes the content of the clipboard to the current form.
	// Content of the form must be returned by "CopyFormToClipboard" and match the current window type.
	virtual void PasteFormFromClipboard();
	virtual void PasteText(std::string_view text);
	virtual std::vector<size_t> ClipboardWidths() const;
	virtual size_t ClipboardRowLimit() const { return 1; }
	virtual void FlushPendingFields();
	// Returns true if the current clipboard content can be pasted into this form
	bool CanPasteFormFromClipboard() const;
	// Returns the casted base window for when wanting to call non-generic methods
	template <class T> T* BaseWindow() {
		return (T*)m_baseWindow;
	};
	// Returns the casted editor for when wanting to call non-generic methods
	template <class T> T* Editor() {
		return (T*)m_editor;
	};
	// Called when the structure is duplicated
	virtual void OnDuplication(unsigned int newId, unsigned int listSize) {};
	// Delete the current structure from the moveset and close the structure window
	virtual void DeleteStructure();
	// Returns the size of the current structure list or 0 for non-list structures
	virtual int GetOriginalStructureListSize() { return 0; };
	// Called after rendering the form, useful to implement extra-logic code such as subwindows in derived classes
	virtual void PostRender() {};
	// Actual rendering function
	virtual void RenderInternal();
	// False while the form shows content with nothing to apply (a move form's cancel graph): no Apply footer
	virtual bool ShowsApplyButton() const { return true; }
	// Sets the window name and reapplies its position and size because imgui ties window pos / size to their name
	virtual void ApplyWindowName();
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
	// Render a single input field in the form. Game-specific forms may draw a decoded widget (combo) first.
	virtual void RenderInput(int listIdx, EditorInput* field);
	// Render a single input label in the form
	virtual void RenderLabel(int listIdx, EditorInput* field);
	// Called when clicking the apply button, will check for field errors, set variables and then save the data
	virtual void Apply();
	// Returns false if any field has an error state
	virtual bool IsFormValid() const;
	// Inits the form
	virtual void InitForm(std::string windowTitleBase, uint32_t t_id, EditorLogic* editor);
	// Returns true if the current form window has been right clicked
	bool IsWindowRightClicked() const;
	// Overrideable to render extra items in the form's context menu
	virtual void RenderExtraContextMenuItems() {};
	// Items about the row under the cursor (list forms), drawn before the structure duplication item
	virtual void RenderItemContextMenuItems() {};
public:
    // Uncommitted checkpoints restore without allocation, including original pointer identity.
    virtual std::unique_ptr<TransactionState> BeginTransaction(size_t& budget);
	// Stores the state of the window. If close, this class will be destroyed soon
	bool popen = true;
	// If set to true, will display an unsaved icon on the window title bar
	bool unsavedChanges = false;
	// If is true, changes were just applied. Read by EditorWindows to signal when to highlight the Save/Import button after modifications.
	bool justAppliedChanges = false;
	// If true, we must call SetNextWndowFocus() before creating our window
	bool setFocus = true;
	// Contains our ID or list starting ID for lists (they are contiguous)
	uint32_t structureId = 0;
	// Type of the window, passed to editor functions
	EditorWindowType windowType = EditorWindowType_INVALID;
	// The dock ID to set during the next render 
	int nextDockId = -1;
	// Stores the last viewport the window was rendered in. If isn't the main window's, this window won't be closed automatically.
	ImGuiViewport* currentViewport = nullptr;
	// Set to true in a derived constructor to prevent the creation of multiple windows of the derived class
	bool uniqueType = false;
	// Set to true before pasting in order for the field's visual to be updated
	bool moveFocusAway = false;
	// True if this current form's window is focused. Used to know if an editor window or its children have focus / right click behaviour.
	bool windowFocused = false;

	EditorForm() {};
	EditorForm(const std::string& parentWindowName, EditorWindowType windowType, uint16_t t_structureId, EditorLogic* editor, EditorVisuals* baseWindow, int listSize, const char* typeName);
	virtual ~EditorForm();

	// Called after the constructor
	virtual void OnInitEnd() {};
	// Last operation error shown in the form (tests).
	const std::string& OperationError() const { return m_operationError; }
	// Display text of a field (tests). listIdx is ignored by single forms.
	virtual std::string FieldDisplayName(int listIdx, const char* name) const { (void)listIdx; auto* field = m_fieldIdentifierMap.at(name); return field ? field->displayName : std::string(); }
	// Updates a field (if not errored) containing an ID or an integer value by adding the given value
	virtual void RequestFieldUpdate(EditorWindowType winType, int valueChange, int listStart, int listEnd) {};
	void Render(bool parent_window_focused);
};

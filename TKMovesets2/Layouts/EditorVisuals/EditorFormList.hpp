#pragma once

#include <map>
#include <set>
#include <string>

#include "EditorForm.hpp"

enum EditorFormTreeview_
{
	EditorFormTreeview_Opened,
	EditorFormTreeview_Closed,
	EditorFormTreeview_ForceOpen,
	EditorFormTreeview_ForceClose,
};

struct FieldItem
{
	// The form's fields, contains the field buffer along with its validity as a boolean and more useful data 
	InputMap identifierMap;
	// Containst the form fields grouped by categories, contained within their draw order. Same data as m_inputMap but stored differently, used for display.
	std::map<int, std::vector<EditorInput*>> categoryMap;
	// Determines what tree node should be open
	std::string itemLabel;
	// Determines what tree node should be open
	EditorFormTreeview_ openStatus = EditorFormTreeview_Closed;
	// Background color of the item
	int color = 0;
	// Absolute id of the item
	int id = -1;
};

class EditorFormList : public EditorForm
{
protected:
	std::vector<FieldItem*> m_items;
	// Contains the size of the current list. Value can change.
	size_t m_listSize = 0;
	// If positive or negative, applying will result in either the list growing (with reallocation) or shrinking
	int m_listSizeChange = 0;
	// Identifier that will be sent when we are being resized
	std::string m_resizeEventKey;
	// Absolute structure IDs of the deleted items sicne the last Apply()
	std::set<int> m_deletedItemIds;

	// Create a new item to the list, set position to -1 for appending to the end.
	// Will copy the data at the inserted index. Returns the final insertion position.
	unsigned int CreateNewItem(int insertionPosition=-1);
	// Copy the entire form, evrey field of every tiem, to the clipboard
	void CopyFormToClipboard() const override;
	// Pastes the content of the clipboard to the current form.
	// Content of the form must be returned by "CopyFormToClipboard" and match the current window type.
	void PasteFormFromClipboard() override;
	// Delete this entire structure from the moveset and close the window
	void DeleteStructure() override;
	// Get the size of the structure before we made unsaved changes to it
	virtual int GetOriginalStructureListSize() { return (int)m_listSize - (int)m_listSizeChange; };
	// Called before an item's treenode is rendered, use to display stuff on the item treenode. Called before RenderListControlButtons();
	virtual void PreItemRender(int listIdx) {};
	// Called whenver the list is reordered, used to  update field labels when index is important
	virtual void OnReorder();
	// Called whenever the list is resized, used to update field labels when index is important
	virtual void OnResize();
	// Called whenever applying with a resized list, used  to issue field updates to other windows
	virtual void OnApplyResize(int sizeChange, int oldSize);
	// Builds a string label that will be shown as the title of the item tree view
	virtual void BuildItemDetails(int listIdx);
	// Save every list item individually
	virtual void Apply() override;
	// Returns false if any field has an error state
	bool IsFormValid() const override;
	// Displays buttons to move, create or delete individual list items
	void RenderListControlButtons(int listIdx);
	// Notify fields in other windows to be updated
	virtual void RequestFieldUpdate(EditorWindowType winType, int valueChange, int listStart, int listEnd) override;

	void InitForm(std::string windowTitleBase, uint32_t t_id, EditorLogic* editor) override;
	// Called whenever a field changes (and is valid).
	virtual void OnUpdate(int listIdx, EditorInput* field) override;
public:
	EditorFormList(const std::string& parentWindowName, EditorWindowType t_windowType, uint16_t t_structureId, EditorLogic* editor, EditorVisuals* baseWindow, int listSize, const char* typeName);
	virtual ~EditorFormList() override;

	// Called after the constructor
	virtual void OnInitEnd() override;
	void RenderInternal() override;
};
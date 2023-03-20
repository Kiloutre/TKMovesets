#pragma once

#include <map>
#include <set>
#include <string>

#include "Editor.hpp"
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
	std::map<std::string, EditorInput*> identifierMaps;
	// Containst the form fields grouped by categories, contained within their draw order. Same data as m_inputMap but stored differently, used for display.
	std::map<int, std::vector<EditorInput*>> categoryMaps;
	// Determines what tree node should be open
	std::string itemLabel;
	// Determines what tree node should be open
	EditorFormTreeview_ openStatus = EditorFormTreeview_Closed;
	int color = 0;
};

class EditorFormList : public EditorForm
{
protected:
	std::vector<FieldItem*> m_items;
	// Contains the size of the current list. Value can change.
	size_t m_listSize = 0;
	// If positive or negative, applying will result in either the list growing (with reallocation) or shrinking
	int m_listSizeChange = 0;

	// Called whenever the list is resized, used  to issue field updates to other windows
	virtual void OnResize(int sizeChange, int oldSize) {};
	// Called when clicking a field
	virtual void OnFieldLabelClick(int listIdx, EditorInput* field) {};
	// Builds a string label that will be shown as the title of the item tree view
	virtual void BuildItemDetails(int listIdx);
	// Save every list item individually
	void Apply() override;
	// Returns false if any field has an error state
	bool IsFormValid() override;
	// Displays buttons to move, create or delete individual list items
	void RenderListControlButtons(int listIdx);
	

	void RenderLabel(int listIdx, EditorInput* field);
	void RenderInput(int listIdx, EditorInput* field);
	void RenderInputs(int listIdx, std::vector<EditorInput*>& inputs, int category, int columnCount);
public:
	//
	void InitForm(std::string windowTitleBase, uint32_t t_id, Editor* editor);
	void Render() override;
};
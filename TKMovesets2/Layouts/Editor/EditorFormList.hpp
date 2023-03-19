#pragma once

#include <map>
#include <set>
#include <string>

#include "Editor.hpp"
#include "EditorForm.hpp"

enum EditorFormTreeview_
{
	EditorFormTreeview_Default,
	EditorFormTreeview_ForceOpen,
	EditorFormTreeview_ForceClose,
};

class EditorFormList : public EditorForm
{
protected:
	// The form's fields, contains the field buffer along with its validity as a boolean and more useful data 
	std::vector<std::map<std::string, EditorInput*>> m_fieldIdentifierMaps;
	// Containst the form fields grouped by categories, contained within their draw order. Same data as m_inputMap but stored differently, used for display.
	std::vector<std::map<int, std::vector<EditorInput*>>> m_fieldsCategoryMaps;
	// The label of each list item
	std::vector<std::string> m_itemLabels;
	// Contains the size of the current list. Might change.
	int m_listSize = 0;
	// If positive or negative, applying will result in either the list growing (with reallocation) or shrinking
	int m_listSizeChange = 0;
	// Determines what tree node should be open
	std::vector<EditorFormTreeview_> m_itemOpenStatus;

	// Called when clicking a field
	virtual void OnFieldLabelClick(int listIdx, EditorInput* field) {};
	// Builds a string label that will be shown as the title of the item tree view
	virtual void BuildItemLabel(int listIdx);
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
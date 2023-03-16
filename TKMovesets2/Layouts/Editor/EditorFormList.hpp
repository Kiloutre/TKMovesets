#pragma once

#include <map>
#include <set>
#include <string>

#include "Editor.hpp"
#include "EditorForm.hpp"

class EditorFormList : public EditorForm
{
protected:
	// The form's fields, contains the field buffer along with its validity as a boolean and more useful data 
	std::vector<std::map<std::string, EditorInput*>> m_fieldIdentifierMaps;
	// Containst the form fields grouped by categories, contained within their draw order. Same data as m_inputMap but stored differently, used for display.
	std::vector<std::map<int, std::vector<EditorInput*>>> m_fieldsCategoryMaps;

	// Called when clicking a field
	virtual void OnFieldLabelClick(int listIdx, EditorInput* field) {};
	// Save every list item individually
	void Apply() override;
	// Returns false if any field has an error state
	bool IsFormValid() override;

	void RenderLabel(int listIdx, EditorInput* field);
	void RenderInput(int listIdx, EditorInput* field);
	void RenderInputs(int listIdx, std::vector<EditorInput*>& inputs, int category, int columnCount);
public:
	//
	void InitForm(std::string windowTitleBase, uint32_t t_id, Editor* editor);
	void Render() override;
};
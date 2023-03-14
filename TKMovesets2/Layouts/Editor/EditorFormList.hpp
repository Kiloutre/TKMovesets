#pragma once

#include <map>
#include <set>
#include <string>

#include "Editor.hpp"
#include "EditorForm.hpp"

// Windowed class managing a single move
class EditorFormList : public EditorForm
{
protected:
	// The form's fields, contains the field buffer along with its validity as a boolean and more useful data 
	std::vector<std::map<std::string, EditorInput*>> m_fieldIdentifierMaps;
	// Containst the form fields grouped by categories, contained within their draw order. Same data as m_inputMap but stored differently, used for display.
	std::vector<std::map<int, std::vector<EditorInput*>>> m_fieldsCategoryMaps;

	// Returns false if any field has an error state
	bool IsFormValid() override;
public:
	//
	void InitForm(std::string windowTitleBase, uint32_t t_id, Editor* editor, std::vector<std::string>& drawOrder);
	void Render() override;
};
#pragma once

#include "Editor.hpp"
#include "EditorFormList.hpp"

class EditorExtraproperties : public EditorFormList
{
private:
	void OnUpdate(int listIdx, EditorInput* field) override;
	void BuildItemDetails(int listIdx) override;
	void PreItemRender(int listIdx) override;
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
public:
	using EditorFormList::EditorFormList;
};
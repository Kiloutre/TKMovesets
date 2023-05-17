#pragma once

#include "Editor.hpp"
#include "EditorFormList.hpp"

class EditorMovelistDisplayable : public EditorFormList
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
	void BuildItemDetails(int listIdx) override {};
	void OnUpdate(int listIdx, EditorInput* field) override;
	void BuidAllLabels();
public:
	using EditorFormList::EditorFormList;

	void OnInitEnd() override;
};
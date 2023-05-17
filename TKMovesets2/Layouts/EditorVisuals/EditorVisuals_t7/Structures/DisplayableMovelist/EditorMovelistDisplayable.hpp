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
	EditorMovelistDisplayable(std::string windowTitleBase, Editor* editor, EditorWindowBase* baseWindow);

	void OnInitEnd() override;
};
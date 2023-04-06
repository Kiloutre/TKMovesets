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
	void Apply() override; // todo: remove
public:
	EditorMovelistDisplayable(std::string windowTitleBase, Editor* editor, EditorWindowBase* baseWindow);
};
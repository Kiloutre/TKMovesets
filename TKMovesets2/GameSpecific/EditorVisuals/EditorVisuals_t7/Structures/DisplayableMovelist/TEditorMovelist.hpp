#pragma once

#include "EditorFormList.hpp"

class TEditorMovelistDisplayable : public EditorFormList
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

class TEditorMovelistPlayable : public EditorForm
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
	void RequestFieldUpdate(EditorWindowType winType, int valueChange, int listStart, int listEnd) override;
	void OnUpdate(int listIdx, EditorInput* field) override;
public:
	using EditorForm::EditorForm;
};

class TEditorMovelistInput : public EditorFormList
{
private:
	void BuildItemDetails(int listIdx) override;
	void OnUpdate(int listIdx, EditorInput* field) override;
public:
	using EditorFormList::EditorFormList;
};
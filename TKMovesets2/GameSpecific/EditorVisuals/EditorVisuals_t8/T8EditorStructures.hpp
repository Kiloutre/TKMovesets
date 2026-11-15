#pragma once

#include "EditorFormList.hpp"
#include "EditorVisuals_t7/Structures/Subwindows/TEditorMove_Animations.hpp"


class T8EditorExtraproperties : public EditorFormList
{
private:
	void OnUpdate(int listIdx, EditorInput* field) override;
	void BuildItemDetails(int listIdx) override;
	void PreItemRender(int listIdx) override;
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
public:
	using EditorFormList::EditorFormList;
};


class T8EditorRequirements : public EditorFormList
{
private:
	void BuildItemDetails(int listIdx) override;
	void OnUpdate(int listIdx, EditorInput* field) override;
public:
	using EditorFormList::EditorFormList;
};


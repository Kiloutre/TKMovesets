#pragma once

#include "EditorFormList.hpp"
#include "Structures/Subwindows/TEditorMove_Animations.hpp"

class TEditorCancels : public EditorFormList
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
	void OnUpdate(int listIdx, EditorInput* field) override;
	void BuildItemDetails(int listIdx) override;
	void RenderExtraContextMenuItems() override;
public:
	using EditorFormList::EditorFormList;
	void RequestFieldUpdate(EditorWindowType winType, int valueChange, int listStart, int listEnd) override;
};


class TEditorGroupedCancels : public EditorFormList
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
	void BuildItemDetails(int listIdx) override;
	void RenderExtraContextMenuItems() override;
public:
	using EditorFormList::EditorFormList;
	void RequestFieldUpdate(EditorWindowType winType, int valueChange, int listStart, int listEnd) override;
};


class TEditorExtraproperties : public EditorFormList
{
private:
	void OnUpdate(int listIdx, EditorInput* field) override;
	void BuildItemDetails(int listIdx) override;
	void PreItemRender(int listIdx) override;
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
public:
	using EditorFormList::EditorFormList;
};


class TEditorHitConditions : public EditorFormList
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
public:
	using EditorFormList::EditorFormList;
	void RequestFieldUpdate(EditorWindowType winType, int valueChange, int listStart, int listEnd) override;
};


class TEditorInputSequence : public EditorForm
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
public:
	using EditorForm::EditorForm;
	void RequestFieldUpdate(EditorWindowType winType, int valueChange, int listStart, int listEnd) override;
};


class TEditorInputStruct : public EditorFormList
{
private:
	void BuildItemDetails(int listIdx) override;
	void OnUpdate(int listIdx, EditorInput* field) override;
public:
	using EditorFormList::EditorFormList;
};


class TEditorMove : public EditorForm
{
private:
	// Stores whether to display the animation list 
	bool m_animationListOpen = false;
	// If the move name was changed. Used to re-apply window name.
	bool m_renamed = false;
	// Window containing the animation list to choose from
	TEditorMove_Animations* m_animationList = nullptr;

	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
	void OnApply() override;
	void ApplyWindowName(bool reapplyWindowProperties = true) override;
	void OnUpdate(int listIdx, EditorInput* field) override;
	void OnInitEnd() override;
	void PostRender() override;
	void RenderExtraContextMenuItems() override;
	void OnDuplication(unsigned int moveId, unsigned int listSize) override;

	// Loads the list of animation available in our library and allows to import one of them
	void OpenAnimationList();
public:
	using EditorForm::EditorForm;
	void RequestFieldUpdate(EditorWindowType winType, int valueChange, int listStart, int listEnd) override;
};


class TEditorMoveEndProperty : public EditorFormList
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
public:
	using EditorFormList::EditorFormList;
	void RequestFieldUpdate(EditorWindowType winType, int valueChange, int listStart, int listEnd) override;
};


class TEditorMoveStartProperty : public EditorFormList
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
public:
	using EditorFormList::EditorFormList;
	void RequestFieldUpdate(EditorWindowType winType, int valueChange, int listStart, int listEnd) override;
};


class TEditorProjectile : public EditorForm
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
public:
	using EditorForm::EditorForm;
	void RequestFieldUpdate(EditorWindowType winType, int valueChange, int listStart, int listEnd) override;
};


class TEditorPushback : public EditorForm
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
public:
	using EditorForm::EditorForm;
	void RequestFieldUpdate(EditorWindowType winType, int valueChange, int listStart, int listEnd) override;
};


class TEditorPushbackExtra : public EditorFormList
{
private:
	void OnUpdate(int listIdx, EditorInput* field) override;
	void BuildItemDetails(int listIdx) override;
public:
	using EditorFormList::EditorFormList;
};


class TEditorReactions : public EditorForm
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
public:
	using EditorForm::EditorForm;
};


class TEditorRequirements : public EditorFormList
{
private:
	void BuildItemDetails(int listIdx) override;
	void OnUpdate(int listIdx, EditorInput* field) override;
public:
	using EditorFormList::EditorFormList;
};


class TEditorThrowCamera : public EditorForm
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
public:
	using EditorForm::EditorForm;
};


class TEditorVoiceclip : public EditorFormList
{
private:
	void BuildItemDetails(int listIdx) override;
	void OnUpdate(int listIdx, EditorInput* field) override;
	void OnResize() override;
	void OnReorder() override;
public:
	using EditorFormList::EditorFormList;
};
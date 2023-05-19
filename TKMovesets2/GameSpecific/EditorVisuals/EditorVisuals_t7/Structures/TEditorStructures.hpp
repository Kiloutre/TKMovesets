#pragma once

#include "EditorFormList.hpp"
#include "Structures/Subwindows/EditorMove_Animations.hpp"

class EditorCancels : public EditorFormList
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


class EditorGroupedCancels : public EditorFormList
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
	void BuildItemDetails(int listIdx) override;
	void RenderExtraContextMenuItems() override;
public:
	using EditorFormList::EditorFormList;
	void RequestFieldUpdate(EditorWindowType winType, int valueChange, int listStart, int listEnd) override;
};


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


class EditorHitConditions : public EditorFormList
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
public:
	using EditorFormList::EditorFormList;
	void RequestFieldUpdate(EditorWindowType winType, int valueChange, int listStart, int listEnd) override;
};


class EditorInputSequence : public EditorForm
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
public:
	using EditorForm::EditorForm;
	void RequestFieldUpdate(EditorWindowType winType, int valueChange, int listStart, int listEnd) override;
};


class EditorInputStruct : public EditorFormList
{
private:
	void BuildItemDetails(int listIdx) override;
	void OnUpdate(int listIdx, EditorInput* field) override;
public:
	using EditorFormList::EditorFormList;
};


class EditorMove : public EditorForm
{
private:
	// Stores whether to display the animation list 
	bool m_animationListOpen = false;
	// If the move name was changed. Used to re-apply window name.
	bool m_renamed = false;
	// Window containing the animation list to choose from
	EditorMove_Animations* m_animationList = nullptr;

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


class EditorMoveEndProperty : public EditorFormList
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
public:
	using EditorFormList::EditorFormList;
	void RequestFieldUpdate(EditorWindowType winType, int valueChange, int listStart, int listEnd) override;
};


class EditorMoveStartProperty : public EditorFormList
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
public:
	using EditorFormList::EditorFormList;
	void RequestFieldUpdate(EditorWindowType winType, int valueChange, int listStart, int listEnd) override;
};


class EditorProjectile : public EditorForm
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
public:
	using EditorForm::EditorForm;
	void RequestFieldUpdate(EditorWindowType winType, int valueChange, int listStart, int listEnd) override;
};


class EditorPushback : public EditorForm
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
public:
	using EditorForm::EditorForm;
	void RequestFieldUpdate(EditorWindowType winType, int valueChange, int listStart, int listEnd) override;
};


class EditorPushbackExtra : public EditorFormList
{
private:
	void OnUpdate(int listIdx, EditorInput* field) override;
	void BuildItemDetails(int listIdx) override;
public:
	using EditorFormList::EditorFormList;
};


class EditorReactions : public EditorForm
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
public:
	using EditorForm::EditorForm;
};


class EditorRequirements : public EditorFormList
{
private:
	void BuildItemDetails(int listIdx) override;
	void OnUpdate(int listIdx, EditorInput* field) override;
public:
	using EditorFormList::EditorFormList;
};


class EditorThrowCamera : public EditorForm
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
public:
	using EditorForm::EditorForm;
};


class EditorVoiceclip : public EditorFormList
{
private:
	void BuildItemDetails(int listIdx) override;
	void OnUpdate(int listIdx, EditorInput* field) override;
	void OnResize() override;
	void OnReorder() override;
public:
	using EditorFormList::EditorFormList;
};
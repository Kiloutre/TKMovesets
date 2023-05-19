#pragma once

#include "EditorForm.hpp"
#include "Structures/Subwindows/EditorMove_Animations.hpp"

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
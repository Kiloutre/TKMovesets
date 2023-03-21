#pragma once

#include "Editor.hpp"
#include "EditorForm.hpp"

class EditorMove : public EditorForm
{
private:
	// Stores whether to display the animation list 
	bool m_animationListOpen = false;
	// If the move name was changed. Used to re-apply window name.
	bool m_renamed = false;

	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
	void OnApply() override;
	void ApplyWindowName(bool reapplyWindowProperties = true) override;
	void OnUpdate(int listIdx, EditorInput* field) override;
	void PostRender() override;

	// Loads the list of animation available in our library and allows to import one of them
	void OpenAnimationList();
public:
	EditorMove(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow);
	void RequestFieldUpdate(std::string fieldName, int valueChange, int listStart, int listEnd) override;
};
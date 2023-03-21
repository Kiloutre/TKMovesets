#pragma once

#include "Editor.hpp"
#include "EditorFormList.hpp"

class EditorPushbackExtra : public EditorFormList
{
private:
	void OnApplyResize(int sizeChange, int oldSize) override;
public:
	EditorPushbackExtra(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow, int listSize);
	void RequestFieldUpdate(std::string fieldName, int valueChange, int listStart, int listEnd) override;
};
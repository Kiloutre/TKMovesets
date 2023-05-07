#pragma once

#include "Editor.hpp"
#include "EditorFormList.hpp"
#include "EditorWindowBase.hpp"

class EditorCancels : public EditorFormList
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
	void OnUpdate(int listIdx, EditorInput* field) override;
	void BuildItemDetails(int listIdx) override;
	void RenderExtraContextMenuItems() override;
public:
	using EditorFormList::EditorFormList;
	void RequestFieldUpdate(EditorWindowType_ winType, int valueChange, int listStart, int listEnd) override;
};
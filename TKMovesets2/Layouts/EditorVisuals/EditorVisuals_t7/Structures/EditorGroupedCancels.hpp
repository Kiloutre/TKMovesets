#pragma once

#include "EditorFormList.hpp"

class EditorGroupedCancels : public EditorFormList
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
	void BuildItemDetails(int listIdx) override;
	void RenderExtraContextMenuItems() override;
public:
	using EditorFormList::EditorFormList;
	void RequestFieldUpdate(EditorWindowType_ winType, int valueChange, int listStart, int listEnd) override;
};
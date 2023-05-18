#pragma once

#include "EditorFormList.hpp"

class EditorHitConditions : public EditorFormList
{
private:
	void OnFieldLabelClick(int listIdx, EditorInput* field) override;
public:
	using EditorFormList::EditorFormList;
	void RequestFieldUpdate(EditorWindowType_ winType, int valueChange, int listStart, int listEnd) override;
};
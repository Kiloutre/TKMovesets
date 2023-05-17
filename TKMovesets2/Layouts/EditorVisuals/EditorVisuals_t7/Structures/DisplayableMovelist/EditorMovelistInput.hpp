#pragma once

#include "Editor.hpp"
#include "EditorFormList.hpp"

class EditorMovelistInput : public EditorFormList
{
private:
	void BuildItemDetails(int listIdx) override;
	void OnUpdate(int listIdx, EditorInput* field) override;
public:
	using EditorFormList::EditorFormList;
};
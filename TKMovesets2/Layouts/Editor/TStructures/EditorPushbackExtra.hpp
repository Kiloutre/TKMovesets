#pragma once

#include "Editor.hpp"
#include "EditorFormList.hpp"

class EditorPushbackExtra : public EditorFormList
{
private:
	void OnUpdate(int listIdx, EditorInput* field) override;
	void BuildItemDetails(int listIdx) override;
public:
	using EditorFormList::EditorFormList;
};
#pragma once

#include "EditorFormList.hpp"

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
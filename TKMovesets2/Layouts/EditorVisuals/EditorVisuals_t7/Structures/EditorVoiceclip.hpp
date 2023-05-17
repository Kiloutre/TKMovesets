#pragma once

#include "Editor.hpp"
#include "EditorFormList.hpp"

// Windowed class managing a single struct
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
#pragma once

#include "Editor.hpp"
#include "EditorFormList.hpp"

// Windowed class managing a single struct
class EditorVoiceclip : public EditorFormList
{
private:
	void OnResize(int sizeChange, int oldSize) override;
	void BuildItemDetails(int listIdx) override;
	void OnUpdate(int listIdx, EditorInput* field) override;
public:
	EditorVoiceclip(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow);
};
#pragma once

#include "Editor.hpp"
#include "EditorFormList.hpp"

class EditorRequirements : public EditorFormList
{
private:
	void OnResize(int sizeChange, int oldSize) override;
public:
	EditorRequirements(std::string windowTitleBase, uint32_t t_id, Editor* editor);
	void RequestFieldUpdate(std::string fieldName, int valueChange, int listStart, int listEnd) override;
};
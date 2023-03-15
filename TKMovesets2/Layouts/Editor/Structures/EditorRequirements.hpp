#pragma once

#include "Editor.hpp"
#include "EditorFormList.hpp"

class EditorRequirements : public EditorFormList
{
public:
	EditorRequirements(std::string windowTitleBase, uint32_t t_id, Editor* editor);
};
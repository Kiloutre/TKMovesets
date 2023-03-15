#pragma once

#include "Editor.hpp"
#include "EditorFormList.hpp"

class EditorGroupedCancels : public EditorFormList
{
public:
	EditorGroupedCancels(std::string windowTitleBase, uint32_t t_id, Editor* editor);
};
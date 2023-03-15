#pragma once

#include "Editor.hpp"
#include "EditorFormList.hpp"

class EditorCancels : public EditorFormList
{
public:
	EditorCancels(std::string windowTitleBase, uint32_t t_id, Editor* editor);
};
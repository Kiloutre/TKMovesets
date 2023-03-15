#pragma once

#include "Editor.hpp"
#include "EditorFormList.hpp"

// Windowed class managing a list of structs
class EditorEndOtherProp : public EditorFormList
{
public:
	EditorEndOtherProp(std::string windowTitleBase, uint32_t t_id, Editor* editor);
};
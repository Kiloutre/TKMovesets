#pragma once

#include "Editor.hpp"
#include "EditorFormList.hpp"

// Windowed class managing a list of structs
class EditorMoveStartProperty : public EditorFormList
{
public:
	EditorMoveStartProperty(std::string windowTitleBase, uint32_t t_id, Editor* editor);
};
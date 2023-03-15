#pragma once

#include "Editor.hpp"
#include "EditorFormList.hpp"

// Windowed class managing a list of structs
class EditorMoveEndProperty : public EditorFormList
{
public:
	EditorMoveEndProperty(std::string windowTitleBase, uint32_t t_id, Editor* editor);
};
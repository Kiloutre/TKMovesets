#pragma once

#include "Editor.hpp"
#include "EditorFormList.hpp"

// Windowed class managing a single move
class EditorExtraproperties : public EditorFormList
{
public:
	EditorExtraproperties(std::string windowTitleBase, uint32_t t_id, Editor* editor);
};
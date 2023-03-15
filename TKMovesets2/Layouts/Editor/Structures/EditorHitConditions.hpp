#pragma once

#include "Editor.hpp"
#include "EditorFormList.hpp"

// Windowed class managing a single move
class EditorHitConditions : public EditorFormList
{
public:
	EditorHitConditions(std::string windowTitleBase, uint32_t t_id, Editor* editor);
};
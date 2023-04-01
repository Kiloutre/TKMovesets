#pragma once

#include "Editor.hpp"
#include "EditorForm.hpp"

class EditorMovelistPlayable : public EditorForm
{
public:
	EditorMovelistPlayable(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow);
};
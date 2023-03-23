#pragma once

#include "Editor.hpp"
#include "EditorForm.hpp"

class EditorCameraData : public EditorForm
{
public:
	EditorCameraData(std::string windowTitleBase, uint32_t t_id, Editor* editor);
};
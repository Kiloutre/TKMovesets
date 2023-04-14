#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorThrowCamera.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorThrowCamera::EditorThrowCamera(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow)
{
	windowType = EditorWindowType_ThrowCamera;
	m_baseWindow = baseWindow;
	InitForm(windowTitleBase, t_id, editor);
}

void EditorThrowCamera::OnFieldLabelClick(int listIdx, EditorInput* field)
{
	if (field->name == "cameradata_addr")
	{
		int id = atoi(field->buffer);
		m_baseWindow->OpenFormWindow(EditorWindowType_CameraData, id);
	}
}
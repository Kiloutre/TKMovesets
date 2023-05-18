#include "imgui_extras.hpp"
#include "EditorThrowCamera.hpp"
#include "Localization.hpp"
#include "EditorVisuals.hpp"

void EditorThrowCamera::OnFieldLabelClick(int listIdx, EditorInput* field)
{
	if (field->name == "cameradata_addr")
	{
		int id = atoi(field->buffer);
		m_baseWindow->OpenFormWindow(EditorWindowType_CameraData, id);
	}
}
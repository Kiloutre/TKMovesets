#include "TEditorStructures.hpp"
#include "EditorVisuals_t7.hpp"

void EditorThrowCamera::OnFieldLabelClick(int listIdx, EditorInput* field)
{
	if (field->name == "cameradata_addr")
	{
		int id = atoi(field->buffer);
		m_baseWindow->OpenFormWindow(TEditorWindowType_CameraData, id);
	}
}
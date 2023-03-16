#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorPushback.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorPushback::EditorPushback(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow)
{
	windowType = EditorWindowType_Pushback;
	m_baseWindow = baseWindow;
	InitForm(windowTitleBase, t_id, editor);
}

void EditorPushback::OnFieldLabelClick(EditorInput* field)
{
	int id = atoi(field->buffer);
	std::string& name = field->name;

	if (name == "extradata_addr") {
		m_baseWindow->OpenFormWindow(EditorWindowType_PushbackExtradata, id);
	}
}
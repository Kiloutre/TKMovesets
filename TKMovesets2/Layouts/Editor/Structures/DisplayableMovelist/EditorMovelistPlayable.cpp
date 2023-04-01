#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorMovelistPlayable.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorMovelistPlayable::EditorMovelistPlayable(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow)
{
	windowType = EditorWindowType_MovelistPlayable;
	m_baseWindow = baseWindow;
	InitForm(windowTitleBase, t_id, editor);
}

#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorMoveEndProperty.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorMoveEndProperty::EditorMoveEndProperty(std::string windowTitleBase, uint32_t t_id, Editor* editor)
{
	windowType = EditorWindowType_MoveEndProperty;
	InitForm(windowTitleBase, t_id, editor);
}

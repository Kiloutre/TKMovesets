#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorMoveStartProperty.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorMoveStartProperty::EditorMoveStartProperty(std::string windowTitleBase, uint32_t t_id, Editor* editor)
{
	windowType = EditorWindowType_MoveBeginProperty;
	InitForm(windowTitleBase, t_id, editor);
}

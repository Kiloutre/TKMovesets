#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorStartOtherProp.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorStartOtherProp::EditorStartOtherProp(std::string windowTitleBase, uint32_t t_id, Editor* editor)
{
	windowType = EditorWindowType_StartOtherMoveProperty;
	InitForm(windowTitleBase, t_id, editor);
}

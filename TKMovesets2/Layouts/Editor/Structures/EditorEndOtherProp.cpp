#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorEndOtherProp.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorEndOtherProp::EditorEndOtherProp(std::string windowTitleBase, uint32_t t_id, Editor* editor)
{
	windowType = EditorWindowType_EndOtherMoveProperty;
	InitForm(windowTitleBase, t_id, editor);
}

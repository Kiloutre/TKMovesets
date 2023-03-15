#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorPushback.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorPushback::EditorPushback(std::string windowTitleBase, uint32_t t_id, Editor* editor)
{
	windowType = EditorWindowType_Pushback;
	InitForm(windowTitleBase, t_id, editor);
}

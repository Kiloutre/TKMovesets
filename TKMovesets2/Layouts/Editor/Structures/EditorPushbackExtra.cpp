#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorPushbackExtra.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorPushbackExtra::EditorPushbackExtra(std::string windowTitleBase, uint32_t t_id, Editor* editor)
{
	windowType = EditorWindowType_PushbackExtradata;
	InitForm(windowTitleBase, t_id, editor);
}

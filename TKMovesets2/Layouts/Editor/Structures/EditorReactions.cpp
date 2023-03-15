#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorReactions.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorReactions::EditorReactions(std::string windowTitleBase, uint32_t t_id, Editor* editor)
{
	windowType = EditorWindowType_Reactions;
	InitForm(windowTitleBase, t_id, editor);
}

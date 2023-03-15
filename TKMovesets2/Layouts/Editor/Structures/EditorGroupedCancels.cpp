#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorGroupedCancels.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorGroupedCancels::EditorGroupedCancels(std::string windowTitleBase, uint32_t t_id, Editor* editor)
{
	windowType = EditorWindowType_GroupedCancel;
	InitForm(windowTitleBase, t_id, editor);
}

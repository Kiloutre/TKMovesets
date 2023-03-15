#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorHitConditions.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorHitConditions::EditorHitConditions(std::string windowTitleBase, uint32_t t_id, Editor* editor)
{
	windowType = EditorWindowType_HitCondition;
	InitForm(windowTitleBase, t_id, editor);
}

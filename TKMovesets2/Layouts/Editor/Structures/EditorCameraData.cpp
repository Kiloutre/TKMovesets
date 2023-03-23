#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorCameraData.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorCameraData::EditorCameraData(std::string windowTitleBase, uint32_t t_id, Editor* editor)
{
	windowType = EditorWindowType_CameraData;
	InitForm(windowTitleBase, t_id, editor);
}

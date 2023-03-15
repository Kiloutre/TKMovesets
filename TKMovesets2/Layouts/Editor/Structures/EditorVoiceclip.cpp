#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorVoiceclip.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorVoiceclip::EditorVoiceclip(std::string windowTitleBase, uint32_t t_id, Editor* editor)
{
	windowType = EditorWindowType_Voiceclip;
	InitForm(windowTitleBase, t_id, editor);
}

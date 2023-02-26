#include <ImGui.h>

#include "Submenu_Extract.h"
#include "Localization.h"
#include "imgui_extras.h"

Submenu_Extract::Submenu_Extract()
{
	// Init
}
void Submenu_Extract::Render() 
{
	ImGuiExtra_RenderTextbox(_("extraction.explanation"));
	ImGui::SeparatorText(_("extraction.extracted_movesets"));
	ImGuiExtra_TextboxWarning(_("extraction.game_not_running"));
}

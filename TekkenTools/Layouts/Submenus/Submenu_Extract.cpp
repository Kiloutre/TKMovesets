#include "Submenu_Extract.h"
#include "imgui_extras.h"

Submenu_Extract* Submenu_Extract::s_instance = nullptr;

Submenu_Extract::Submenu_Extract()
{
	// Init
}
void Submenu_Extract::Render() 
{
	ImGuiExtra_RenderTextbox(_("extraction.explanation"));
	ImGui::SeparatorText(_("extraction.extracted_movesets"));
	ImGuiExtra_RenderTextbox(_("extraction.game_not_running"), TEXTBOX_BORDER_WARNING);
}

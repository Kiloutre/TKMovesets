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

	ImGuiExtra_RenderTextbox(_("extraction.explanation"), TEXTBOX_BORDER_WARNING);

	ImGui::Button(_("extraction.extract_1p"));
	ImGui::SameLine();
	ImGui::Button(_("extraction.extract_2p"));
	ImGui::SameLine();
	ImGui::Button(_("extraction.extract_both"));
}

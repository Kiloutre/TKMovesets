#include "Submenu_Extract.h"
#include "imgui_extras.h"

#include <stdio.h>

Submenu_Extract* Submenu_Extract::s_instance = nullptr;

Submenu_Extract::Submenu_Extract()
{
	// Init
}

void Submenu_Extract::Render() 
{
	ImGuiExtra_RenderTextbox("extraction.explanation");

	ImGui::Button("extraction.extract_1p");
	ImGui::SameLine();
	ImGui::Button("extraction.extract_2p");
	ImGui::SameLine();
	ImGui::Button("extraction.extract_both");
}

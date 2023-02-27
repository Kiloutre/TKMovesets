#include <ImGui.h>

#include "Submenu_Import.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"

void Submenu_Import::Render()
{

	ImGuiExtra::RenderTextbox(_("importation.explanation"));
	ImGui::SeparatorText(_("importation.select_moveset"));

	//ImGui::ListBox("##", &selectedMoveset, items, sizeof(items) / sizeof(*items));
}

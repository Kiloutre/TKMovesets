#include <ImGui.h>

#include "Submenu_Import.h"
#include "Localization.h"
#include "imgui_extras.h"

Submenu_Import::Submenu_Import()
{
	// Init
}

void Submenu_Import::Render()
{

	ImGuiExtra::RenderTextbox(_("importation.explanation"));
	ImGui::SeparatorText(_("importation.select_moveset"));

	//ImGui::ListBox("##", &selectedMoveset, items, sizeof(items) / sizeof(*items));
}

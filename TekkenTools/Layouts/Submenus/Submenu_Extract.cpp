#include <ImGui.h>

#include "Submenu_Extract.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"
#include "GameProcess.hpp"
#include "GameExtract.hpp"

Submenu_Extract::Submenu_Extract()
{
	// Init
	// Read from settings
	overwrite_same_filename = false;
}

const char* items[] = {
	"testtesttesttesttesttesttesttesttest",
	"mdr",
	"lol",
	"salut"
};

void Submenu_Extract::Render() 
{
	ImGuiExtra::RenderTextbox(_("extraction.explanation"));

	ImGui::SeparatorText(_("extraction.extracted_movesets"));

	GameProcess& p = GameProcess::getInstance();
	if (p.errcode != PROC_ATTACHED)
	{
		switch (p.errcode)
		{
		case PROC_NOT_ATTACHED:
		case PROC_EXITED:
			ImGuiExtra_TextboxWarning(_("process.game_not_attached"));
			break;
		case PROC_NOT_FOUND:
			ImGuiExtra_TextboxWarning(_("process.game_not_running"));
			break;
			ImGuiExtra_TextboxWarning(_("process.game_version_mismatch"));
			break;
		case PROC_VERSION_MISMATCH:
			ImGuiExtra_TextboxWarning(_("process.game_attach_err"));
			break;
		case PROC_ATTACHING:
			ImGuiExtra_TextboxTip(_("process.attaching"));
			break;
		}
	}

	ImGui::Checkbox(_("extraction.overwrite_duplicate"), &overwrite_same_filename);

	if (ImGui::BeginTable("nice", 4, ImGuiTableFlags_ContextMenuInBody | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg
									| ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable))
	{
		ImGui::TableSetupColumn("##", 0, 3.0f);
		ImGui::TableSetupColumn(_("moveset.origin"));
		ImGui::TableSetupColumn(_("moveset.target_character"));
		ImGui::TableSetupColumn(_("moveset.date"));
		ImGui::TableHeadersRow();

		for (int i = 0; i < sizeof(items) / sizeof(*items); ++i)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(items[i]);

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Tekken 7");

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Kazuya");

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("27/02/2023 16:22");
		}
		ImGui::EndTable();
	}


	if (ImGuiExtra::RenderButtonEnabled(_("extraction.extract_1p"), p.errcode == PROC_ATTACHED)) {

	}
	//GameExtract::getInstance()

	ImGui::SameLine();
	if (ImGuiExtra::RenderButtonEnabled(_("extraction.extract_2p"), p.errcode == PROC_ATTACHED)) {

	}

	ImGui::SameLine();
	if (ImGuiExtra::RenderButtonEnabled(_("extraction.extract_both"), p.errcode == PROC_ATTACHED)) {

	}
}

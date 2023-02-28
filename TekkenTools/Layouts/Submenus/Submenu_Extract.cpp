#include <ImGui.h>

#include "Submenu_Extract.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"
#include "GameProcess.hpp"
#include "GameExtract.hpp"

const char* items[] = {
	"testtesttesttesttesttesttesttesttest",
	"mdr",
	"lol",
	"salut"
};

struct gameProcessName
{
	const char* name;
	const char* processName;
};

const gameProcessName gameList[] = {
	{ "Tekken 7", "TekkenGame-Win64-Shipping.exe" },
	{ "Tekken 8", "Tekken8.exe" },
};

void Submenu_Extract::Render() 
{
	GameExtract& extractor = GameExtract::getInstance();
	ImGuiExtra::RenderTextbox(_("extraction.explanation"));

	//ImGui::BeginTable()
	{
		ImGui::SeparatorText(_("extraction.extraction"));
		size_t currentItem = extractor.currentGameId;
		if (ImGui::BeginCombo("##", gameList[currentItem].name))
		{
			for (size_t i = 0; i < sizeof(gameList) / sizeof(*gameList); ++i)
			{
				if (ImGui::Selectable(gameList[i].name, currentItem == i, 0, ImVec2(100, 0)))
				{
					extractor.SetTargetProcess(gameList[i].processName, i);
				}
			}
			ImGui::EndCombo();
		}
	}

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


	{
		// Todo: also take into account if an extraction is already ongoing
		bool busy = extractor.IsBusy();
		bool canExtract = p.errcode == PROC_ATTACHED && !busy;

		ImGui::Checkbox(_("extraction.overwrite_duplicate"), &m_overwrite_same_filename);
		ImGui::SameLine();
		if (ImGuiExtra::RenderButtonEnabled(_("extraction.extract_1p"), canExtract)) {
			extractor.ExtractP1();
		}

		ImGui::SameLine();
		if (ImGuiExtra::RenderButtonEnabled(_("extraction.extract_2p"), canExtract)) {
			extractor.ExtractP2();
		}

		ImGui::SameLine();
		if (ImGuiExtra::RenderButtonEnabled(_("extraction.extract_both"), canExtract)) {
			extractor.ExtractAll();
		}

		if (busy) {
			// Todo: identify what it is busy with
			ImGui::SameLine();
			ImGui::Text(_("extraction.extracting"), extractor.extractionProgress);
		}
	}

	//ImGui::EndTable()


	ImGui::SeparatorText(_("extraction.extracted_movesets"));
	if (ImGui::BeginTable("nice", 4, ImGuiTableFlags_ContextMenuInBody | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg
									| ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable))
	{
		ImGui::TableSetupColumn("##", 0, 3.0f);
		ImGui::TableSetupColumn(_("moveset.origin"));
		ImGui::TableSetupColumn(_("moveset.target_character"));
		ImGui::TableSetupColumn(_("moveset.date"));
		ImGui::TableHeadersRow();

		for (size_t i = 0; i < sizeof(items) / sizeof(*items); ++i)
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
}

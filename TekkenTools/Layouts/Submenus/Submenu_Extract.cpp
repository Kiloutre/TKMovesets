#include <format>
#include <ImGui.h>

#include "Submenu_Extract.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"
#include "GameProcess.hpp"
#include "GameExtract.hpp"

struct movesetInfo
{
	const char* filename;
	const char* name;
	const char* origin;
	const char* target_character;
	const char* date;
};

const movesetInfo movesetInfos[] = {
	{ "test.tkmvst", "Devilish Kazuya", "Tekken 7", "Kazuya", "15:05 28/02/2023" },
	{ "test.tkmvst", "EX Jin", "Tekken 7", "Jin", "15:05 28/02/2023" },
	{ "test.tkmvst", "Ascended Paul", "Tekken 8", "Paul", "15:05 28/02/2023" },
	{ "test.tkmvst", "Legacy T7 Kazuya", "Tekken 8", "Kazuya", "15:05 28/02/2023" },
};
const size_t movesetCount = sizeof(movesetInfos) / sizeof(*movesetInfos);

struct gameProcessName
{
	const char* name;
	const char* processName;
};

// Respect the ids of GameId enum here.
const gameProcessName gameList[] = {
	{ "Tekken 7", "TekkenGame-Win64-Shipping.exe" },
	{ "Tekken 8", "Tekken8.exe" },
};
const size_t gameListCount = sizeof(gameList) / sizeof(*gameList);

void Submenu_Extract::Render() 
{
	GameExtract& extractor = GameExtract::getInstance();
	ImGuiExtra::RenderTextbox(_("extraction.explanation"));

	//ImGui::BeginTable()
	{
		ImGui::SeparatorText(_("extraction.extraction"));
		size_t currentItem = extractor.currentGameId;
		ImGui::PushItemWidth(150.0f);
		if (ImGui::BeginCombo("##", currentItem == -1 ? _("extraction.select_game") : gameList[currentItem].name))
		{
			for (size_t i = 0; i < gameListCount; ++i)
			{
				if (ImGui::Selectable(gameList[i].name, currentItem == i, 0, ImVec2(100.0f, 0)))
				{
					extractor.SetTargetProcess(gameList[i].processName, i);
				}
			}
			ImGui::EndCombo();
		}
	}

	GameProcess* p = extractor.process;
	if (p->errcode != PROC_ATTACHED)
	{
		switch (p->errcode)
		{
		case PROC_NOT_ATTACHED:
		case PROC_EXITED:
		case PROC_ATTACHING:
			ImGuiExtra_TextboxWarning(_("process.game_not_attached"));
			break;
		case PROC_NOT_FOUND:
			ImGuiExtra_TextboxWarning(_("process.game_not_running"));
			break;
		case PROC_VERSION_MISMATCH:
			ImGuiExtra_TextboxWarning(_("process.game_version_mismatch"));
			break;
		case PROC_ATTACH_ERR:
			ImGuiExtra_TextboxWarning(_("process.game_attach_err"));
			break;
		}
	}


	{
		// Todo: also take into account if an extraction is already ongoing
		bool busy = extractor.IsBusy();
		bool canExtract = p->errcode == PROC_ATTACHED && !busy;

		ImGui::Checkbox(_("extraction.overwrite_duplicate"), &m_overwrite_same_filename);

		for (int playerId = 0; playerId < extractor.characterCount; ++playerId)
		{
			ImGui::SameLine();

			const char* characterName = extractor.GetCharacterName(playerId);
			std::string buttonText;

			const char playerIdTranslationId[3] = { '1' + (char)playerId , 'p', '\0' };
			if (characterName == nullptr) {
				buttonText = std::format("{} ({})", _("extraction.extract"), _(playerIdTranslationId));
			}
			else {
				buttonText = std::format("{} ({}, {})", _("extraction.extract"), _(playerIdTranslationId), characterName);
			}

			if (ImGuiExtra::RenderButtonEnabled(buttonText.c_str(), canExtract)) {
				extractor.QueueCharacterExtraction(playerId);
			}
		}

		ImGui::SameLine();
		if (ImGuiExtra::RenderButtonEnabled(_("extraction.extract_both"), canExtract)) {
			extractor.QueueCharacterExtraction(-1);
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

		for (size_t i = 0; i <  movesetCount; ++i)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(movesetInfos[i].name);

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(movesetInfos[i].origin);

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(movesetInfos[i].target_character);

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(movesetInfos[i].date);
		}
		ImGui::EndTable();
	}
}

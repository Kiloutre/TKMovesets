#include <format>
#include <ImGui.h>

#include "Submenu_Extract.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"
#include "GameProcess.hpp"
#include "GameExtract.hpp"
#include "Helpers.hpp"

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

void Submenu_Extract::Render(GameExtract* extractorHelper)
{
	ImGuiExtra::RenderTextbox(_("extraction.explanation"));

	//ImGui::BeginTable()
	{
		// Game list. Selecting a game will set the extraction thread to try to attach to it regularly
		ImGui::SeparatorText(_("extraction.extraction"));
		size_t currentItem = extractorHelper->currentGameId;
		ImGui::PushItemWidth(150.0f);
		if (ImGui::BeginCombo("##", currentItem == -1 ? _("extraction.select_game") : gameList[currentItem].name))
		{
			for (size_t i = 0; i < gameListCount; ++i)
			{
				if (ImGui::Selectable(gameList[i].name, currentItem == i, 0, ImVec2(100.0f, 0)))
				{
					extractorHelper->SetTargetProcess(gameList[i].processName, i);
				}
			}
			ImGui::EndCombo();
		}
	}

	// If we can't extract, display a warning detailling why
	GameProcess* p = extractorHelper->process;
	if (p->status != PROC_ATTACHED)
	{
		switch (p->status)
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
			ImGuiExtra_TextboxError(_("process.game_version_mismatch"));
			break;
		case PROC_ATTACH_ERR:
			ImGuiExtra_TextboxError(_("process.game_attach_err"));
			break;
		}
	}
	else if (!extractorHelper->CanExtract()) {
		ImGuiExtra_TextboxWarning(_("process.cant_extract"));
	}

	{
		bool busy = extractorHelper->IsBusy();
		bool canExtract = p->status == PROC_ATTACHED && !busy && extractorHelper->CanExtract();

		// Extraction settings
		ImGui::Checkbox(_("extraction.overwrite_duplicate"), &extractorHelper->overwriteSameFilename);
		ImGui::SameLine();
		ImGuiExtra::HelpMarker(_("extraction.overwrite_explanation"));

		// Extraction buttons, will be disabled if we can't extract
		for (int playerId = 0; playerId < extractorHelper->characterCount; ++playerId)
		{
			ImGui::SameLine();

			const char* characterName = extractorHelper->characterNames[playerId].c_str();
			std::string buttonText;

			const char playerIdTranslationId[3] = { '1' + (char)playerId , 'p', '\0' };
			if (characterName[0] == '\0') {
				buttonText = std::format("{} ({})", _("extraction.extract"), _(playerIdTranslationId));
			}
			else {
				buttonText = std::format("{} ({}, {})", _("extraction.extract"), _(playerIdTranslationId), characterName);
			}

			if (ImGuiExtra::RenderButtonEnabled(buttonText.c_str(), canExtract)) {
				extractorHelper->QueueCharacterExtraction(playerId);
			}
		}

		ImGui::SameLine();
		if (ImGuiExtra::RenderButtonEnabled(_("extraction.extract_both"), canExtract)) {
			extractorHelper->QueueCharacterExtraction(-1);
		}

		if (busy) {
			// Progress text. Extraction should generally be fast enough that this will be displayed briefly, but it's still nice to have
			// Todo: identify what it is busy with, calculate progress according to total
			ImGui::SameLine();
			ImGui::Text(_("extraction.progress"), extractorHelper->progress);
		}
	}

	//ImGui::EndTable()

	// List of extracted movesets
	ImGui::SeparatorText(_("extraction.extracted_movesets"));
	if (ImGui::BeginTable("nice", 6, ImGuiTableFlags_ContextMenuInBody | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg
									| ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable))
	{
		ImGui::TableSetupColumn("##", 0, 5.0f);
		ImGui::TableSetupColumn(_("moveset.origin"));
		ImGui::TableSetupColumn(_("moveset.target_character"));
		ImGui::TableSetupColumn(_("moveset.date"));
		ImGui::TableSetupColumn(_("moveset.size"));
		ImGui::TableSetupColumn("Delete", 0, 0.0f);
		ImGui::TableHeadersRow();

		// Yes, we don't use an iterator here because the vector might actually change size mid-iteration
		for (size_t i = 0; i < extractorHelper->storage->extractedMovesets.size(); ++i)
		{
			// moveset is guaranteed not to be freed until after this loop
			movesetInfo* moveset = extractorHelper->storage->extractedMovesets[i];

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(moveset->name.c_str());

			ImGui::TableNextColumn();
			if (moveset->origin == "INVALID") {
				// Badly formatted file. Display it, but mention it is invalid
				ImGui::TextUnformatted(_("moveset.invalid"));
			}
			else {
				ImGui::TextUnformatted(moveset->origin.c_str());

				ImGui::TableNextColumn();
				ImGui::TextUnformatted(moveset->target_character.c_str());

				ImGui::TableNextColumn();
				ImGui::TextUnformatted(moveset->date.c_str());

				ImGui::TableNextColumn();
				std::string sizeString = std::format("{:.2f} {}", moveset->size, _("moveset.size_mb"));
				ImGui::TextUnformatted(sizeString.c_str());

				ImGui::TableNextColumn();
				ImGui::PushID(moveset->filename.c_str());
				if (ImGui::Button("X"))
				{
					extractorHelper->storage->DeleteMoveset(moveset->filename.c_str());
				}
				ImGui::PopID();
			}
		}
		// Don't de-allocate moveset infos until we're done iterating on it
		extractorHelper->storage->CleanupUnusedMovesetInfos();

		ImGui::EndTable();
	}
}

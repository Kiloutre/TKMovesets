#include <format>
#include <ImGui.h>

#include "Submenu_Extract.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"
#include "Games.hpp"
#include "GameProcess.hpp"
#include "GameExtract.hpp"
#include "Helpers.hpp"

// -- Private methods -- //

Submenu_Extract::Submenu_Extract()
{
	m_motaExport[2] = true; // Hands
	m_motaExport[3] = true; // Hands
	m_motaExport[8] = true; // Camera
	m_motaExport[9] = true; // Camera
}

ExtractSettings Submenu_Extract::GetExtractionSettings()
{
	ExtractSettings settings = 0;

	if (m_overwriteSameFilename) {
		settings |= ExtractSettings_OVERWRITE_SAME_FILENAME;
	}

	for (uint8_t i = 0; i < 10; ++i) {
		if (m_motaExport[i]) {
			settings |= ((uint64_t)1 << i);
		}
	}

	return settings;
}

// -- Public methods -- //

void Submenu_Extract::Render(GameExtract& extractorHelper)
{
	ImGuiExtra::RenderTextbox(_("extraction.explanation"));

	//ImGui::BeginTable()
	// todo: show console or something? need status
	{
		ImGui::TextUnformatted(_("extraction.extract_from"));

		// Game list. Selecting a game will set the extraction thread to try to attach to it regularly
		uint8_t currentGameId = extractorHelper.currentGameId;
		ImGui::PushItemWidth(ImGui::CalcTextSize(_("select_game")).x * 1.5f);
		ImGui::PushID(&extractorHelper); // Have to push an ID here because extraction.select_game would cause a conflict
		uint8_t gameListCount = Games::GetGamesCount();
		if (ImGui::BeginCombo("##", currentGameId == -1 ? _("select_game") : Games::GetGameInfo(currentGameId)->name))
		{
			for (uint8_t i = 0; i < gameListCount; ++i)
			{
				GameInfo* game = Games::GetGameInfo(i);
				if (game->extractor != nullptr) {
					if (ImGui::Selectable(game->name, currentGameId == i, 0, ImVec2(100.0f, 0))) {
						extractorHelper.SetTargetProcess(game->processName, i);
					}
				}
			}
			ImGui::EndCombo();
		}
		ImGui::PopID();


		// Extraction settings
		ImGui::SameLine();
		ImGui::Checkbox(_("extraction.overwrite_duplicate"), &m_overwriteSameFilename);
		ImGui::SameLine();
		ImGuiExtra::HelpMarker(_("extraction.overwrite_explanation"));
	}

	if (ImGui::Button(_("extraction.settings"))) {
		ImGui::OpenPopup("ExtractionSettingsPopup");
	}

	if (ImGui::BeginPopupModal("ExtractionSettingsPopup"))
	{
		ImGui::TextUnformatted(_("extraction.mota_explanation"));

		for (uint8_t motaId = 0; motaId < 10; ++motaId) {
			char buf[8] = {"mota_00"};
			buf[5] += motaId / 10;
			buf[6] += motaId % 10;
			ImGui::Checkbox(_(buf), &m_motaExport[motaId]);

			if ((motaId & 1) == 0) {
				ImGui::SameLine();
			}

		}

		ImGui::NewLine();
		if (ImGui::Button(_("close"))) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	GameProcess* p = extractorHelper.process;

	{
		ImGui::SameLine();

		bool busy = extractorHelper.IsBusy();
		bool canExtract = p->IsAttached() && !busy && extractorHelper.CanStart();

		// Extraction buttons, will be disabled if we can't extract
		for (int playerId = 0; playerId < extractorHelper.GetCharacterCount(); ++playerId)
		{
			bool canExtractThisMoveset = canExtract;

			const char* characterName = extractorHelper.characterNames[playerId].c_str();
			std::string buttonText;

			const char playerIdTranslationId[3] = { '1' + (char)playerId , 'p', '\0' };
			if (characterName[0] == '\0') {
				buttonText = std::format("{} ({})", _("extraction.extract"), _(playerIdTranslationId));
			}
			else {
				buttonText = std::format("{} ({}, {})", _("extraction.extract"), _(playerIdTranslationId), characterName);
				if (strncmp(characterName, MOVESET_EXTRACTED_NAME_PREFIX, sizeof(MOVESET_EXTRACTED_NAME_PREFIX) - 1) == 0) {
					canExtractThisMoveset = false;
				}
			}

			if (ImGuiExtra::RenderButtonEnabled(buttonText.c_str(), canExtractThisMoveset)) {
				extractorHelper.QueueCharacterExtraction(playerId, GetExtractionSettings());
			}
			ImGui::SameLine();
		}

		if (ImGuiExtra::RenderButtonEnabled(_("extraction.extract_all"), canExtract)) {
			extractorHelper.QueueCharacterExtraction(-1, GetExtractionSettings());
		}

		if (extractorHelper.progress > 0) {
			// Progress text.
			ImGui::SameLine();
			if (extractorHelper.progress == 100) {
				ImGui::TextColored(ImVec4(0, 1.0f, 0, 1), _("extraction.progress_done"));
			}
			else {
				if (busy) {
					ImGui::Text(_("extraction.progress"), extractorHelper.progress);
				}
				else {
					ImGui::TextColored(ImVec4(1.0f, 0, 0, 1), _("extraction.progress_error"), extractorHelper.progress);
				}
			}
		}
	}


	// If we can't extract, display a warning detailling why
	switch (p->status)
	{
	case GameProcessErrcode_PROC_ATTACHED:
		if (!extractorHelper.CanStart()) {
			ImGuiExtra_TextboxWarning(_("extraction.cant_extract"));
		}
		break;
	case GameProcessErrcode_PROC_NOT_ATTACHED:
	case GameProcessErrcode_PROC_EXITED:
	case GameProcessErrcode_PROC_ATTACHING:
		ImGuiExtra_TextboxWarning(_("process.game_not_attached"));
		break;
	case GameProcessErrcode_PROC_NOT_FOUND:
		ImGuiExtra_TextboxWarning(_("process.game_not_running"));
		break;
	case GameProcessErrcode_PROC_VERSION_MISMATCH:
		ImGuiExtra_TextboxError(_("process.game_version_mismatch"));
		break;
	case GameProcessErrcode_PROC_ATTACH_ERR:
		ImGuiExtra_TextboxError(_("process.game_attach_err"));
		break;
	}

	//ImGui::EndTable()

	// List of extracted moveset
	ImGui::SeparatorText(_("extraction.extracted_movesets"));
	if (ImGui::BeginTable("##", 7, ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY
				| ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_NoHostExtendY, ImVec2(0, ImGui::GetContentRegionAvail().y)))
	{
		ImGui::TableSetupColumn("##", 0, 5.0f);
		ImGui::TableSetupColumn(_("moveset.origin"));
		ImGui::TableSetupColumn(_("moveset.target_character"));
		ImGui::TableSetupColumn(_("moveset.date"));
		ImGui::TableSetupColumn(_("moveset.size"));
		ImGui::TableSetupColumn("##");
		ImGui::TableSetupColumn(_("moveset.delete"), 0, 0.0f);
		ImGui::TableHeadersRow();

		// Yes, we don't use an iterator here because the vector might actually change size mid-iteration
		for (size_t i = 0; i < extractorHelper.storage->extractedMovesets.size(); ++i)
		{
			// moveset is guaranteed not to be freed until after this loop
			movesetInfo* moveset = extractorHelper.storage->extractedMovesets[i];

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(moveset->name.c_str());

			ImGui::TableNextColumn();
			if (moveset->origin == "INVALID") {
				// Badly formatted file. Display it, but mention it is invalid
				ImGui::TextUnformatted(_("moveset.invalid"));
				ImGui::TableNextColumn();
				ImGui::TableNextColumn();
				ImGui::TableNextColumn();
				ImGui::TableNextColumn();
				ImGui::TableNextColumn();
				ImGui::PushID(moveset->filename.c_str());
				if (ImGui::Button("X")) {
					extractorHelper.storage->DeleteMoveset(moveset->filename.c_str());
				}
				ImGui::PopID();
			}
			else {
				ImGui::TextUnformatted(moveset->origin.c_str());

				ImGui::TableNextColumn();
				ImGui::TextUnformatted(moveset->target_character.c_str());

				ImGui::TableNextColumn();
				ImGui::TextUnformatted(Helpers::currentDateTime(moveset->date).c_str());

				ImGui::TableNextColumn();
				std::string sizeString = std::format("{:.2f} {}", moveset->size, _("moveset.size_mb"));
				ImGui::TextUnformatted(sizeString.c_str());

				ImGui::TableNextColumn();
				ImGui::TextUnformatted(moveset->version_string.c_str());

				ImGui::TableNextColumn();
				ImGui::PushID(moveset->filename.c_str());
				if (ImGui::Button("X")) {
					extractorHelper.storage->DeleteMoveset(moveset->filename.c_str());
				}
				ImGui::PopID();
			}
		}

		ImGui::EndTable();
	}

	ExtractionErrcode_ err = extractorHelper.GetLastError();
	if (err != ExtractionErrcode_Successful) {
		ImGui::OpenPopup("ExtractionErrPopup");
		m_err = err;
	}

	// Show popup containing the error description
	if (ImGui::BeginPopupModal("ExtractionErrPopup"))
	{
		switch (m_err)
		{
		case ExtractionErrcode_AllocationErr:
			ImGui::Text(_("extraction.error_allocation"));
			break;
		case ExtractionErrcode_FileCreationErr:
			ImGui::Text(_("extraction.error_file_creation"));
			break;
		}

		if (ImGui::Button(_("close")))
		{
			// Reset the errcode
			m_err = ExtractionErrcode_Successful;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

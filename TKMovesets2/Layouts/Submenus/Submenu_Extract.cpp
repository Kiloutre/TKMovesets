#include <format>
#include <ImGui.h>

#include "Submenu_Extract.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"
#include "Games.hpp"
#include "GameProcess.hpp"
#include "GameExtract.hpp"
#include "Helpers.hpp"
#include "Compression.hpp"
#include "Settings.hpp"

// -- Private methods -- //

Submenu_Extract::Submenu_Extract()
{
	// Set all MOTA exportation to zero by default
	for (int i = 0; i < _countof(m_motaExport); ++i) {
		m_motaExport[i] = false;
	}

	m_motaExport[2] = true; // Hands
	m_motaExport[3] = true; // Hands
	m_motaExport[8] = true; // Camera
	m_motaExport[9] = true; // Camera

	m_overwriteSameFilename = Settings::Get(SETTING_EXPORT_OVERWRITE_KEY, SETTING_EXPORT_OVERWRITE);
	m_extractDisplayableMovelist = true;

	m_extractUnsupportedMotas = false;

	m_compressionIndex = CompressionUtils::GetDefaultCompressionSetting();
}

ExtractSettings Submenu_Extract::GetExtractionSettings()
{
	ExtractSettings settings = 0;

	settings |= CompressionUtils::GetCompressionSetting(m_compressionIndex).extractSetting;

	if (m_extractUnsupportedMotas) {
		settings |= ExtractSettings_UnknownMotas;
	}

	if (m_overwriteSameFilename) {
		settings |= ExtractSettings_OVERWRITE_SAME_FILENAME;
	}

	if (m_extractDisplayableMovelist) {
		settings |= ExtractSettings_DisplayableMovelist;
	}

	for (uint8_t i = 0; i < _countof(m_motaExport); ++i) {
		if (m_motaExport[i]) {
			settings |= ((uint64_t)1 << i);
		}
	}

	return settings;
}

// -- Public methods -- //

void Submenu_Extract::RenderSettingsPopup()
{
	ImGui::SeparatorText("MOTA");
	ImGui::TextUnformatted(_("extraction.mota_explanation"));

	char buf[8] = { "mota_00" };
	for (uint8_t motaId = 0; motaId < 12; ++motaId) {
		buf[5] = '0' + motaId / 10;
		buf[6] = '0' + motaId % 10;
		ImGui::Checkbox(_(buf), &m_motaExport[motaId]);

		if ((motaId & 1) == 0) {
			ImGui::SameLine();
		}

	}

	ImGui::Checkbox(_("extraction.settings.extract_unsupported_motas"), &m_extractUnsupportedMotas);

	ImGui::NewLine();
	ImGui::SeparatorText(_("extraction.settings.other"));
	ImGui::Checkbox(_("extraction.settings.displayable_movelist"), &m_extractDisplayableMovelist);

	ImGui::TextUnformatted(_("extraction.settings.compress_moveset"));
	if (ImGui::BeginCombo("##", m_compressionIndex == 0 ? _("extraction.settings.compression_type.none") : CompressionUtils::GetCompressionSetting(m_compressionIndex).name))
	{
		for (unsigned int i = 0; i < CompressionUtils::GetCompressionSettingCount(); ++i) {
			if (ImGui::Selectable(i == 0 ? _("extraction.settings.compression_type.none") : CompressionUtils::GetCompressionSetting(i).name, i == m_compressionIndex, 0, ImVec2(140.0f, 0))) {
				m_compressionIndex = i;
			}
		}
		ImGui::EndCombo();
	}

	ImGui::Separator();
	if (ImGui::Button(_("close")) || ImGui::IsKeyDown(ImGuiKey_Escape)) {
		ImGui::CloseCurrentPopup();
	}
}

void Submenu_Extract::Render(GameExtract& extractorHelper)
{
	bool busy = extractorHelper.IsBusy();

	ImGuiExtra::RenderTextbox(_("extraction.explanation"));
	ImGui::SeparatorText(_("extraction.extraction"));

	{
		ImGui::TextUnformatted(_("extraction.extract_from"));
		ImGui::SameLine();

		// Game list. Selecting a game will set the extraction thread to try to attach to it regularly
		auto currentGame = extractorHelper.currentGame;

        {
            ImGuiExtra::DisableBlockIf __(busy);

            ImGui::PushItemWidth(200);
            ImGui::PushID(&extractorHelper); // Have to push an ID here because extraction.select_game would cause a conflict
            uint8_t gameListCount = Games::GetGamesCount();
            if (ImGui::BeginCombo("##", currentGame == nullptr ? _("select_game") : currentGame->name))
            {
                for (uint8_t gameIdx = 0; gameIdx < gameListCount; ++gameIdx)
                {
                    auto gameInfo = Games::GetGameInfoFromIndex(gameIdx);
                    if (gameInfo->extractor != nullptr) {
                        if (ImGui::Selectable(gameInfo->name, currentGame == gameInfo, 0, ImVec2(200.0f, 0))) {
                            extractorHelper.SetTargetProcess(gameInfo);
                        }
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::PopID();
		}

		// Extraction settings
		ImGui::SameLine();
		if (ImGui::Checkbox(_("extraction.overwrite_duplicate"), &m_overwriteSameFilename)) {
			Settings::Set(SETTING_EXPORT_OVERWRITE_KEY, &m_overwriteSameFilename);
		}
		ImGui::SameLine();
		ImGuiExtra::HelpMarker(_("extraction.overwrite_explanation"));
	}

	if (ImGui::Button(_("extraction.settings"))) {
		ImGui::OpenPopup("ExtractionSettingsPopup");
	}

	// Extraction settings
	if (ImGui::BeginPopupModal("ExtractionSettingsPopup"))
	{
		RenderSettingsPopup();
		ImGui::EndPopup();
	}

	GameProcess* p = extractorHelper.process;

	{
		ImGui::SameLine();

		bool canExtract = p->IsAttached() && !busy && extractorHelper.CanStart();
		bool canExtractAll = canExtract;

		// Extraction buttons, will be disabled if we can't extract
		for (int playerId = 0; playerId < extractorHelper.GetCharacterCount(); ++playerId)
		{
			bool canExtractThisMoveset = canExtract;

			const char* characterName = extractorHelper.characterNames[playerId].c_str();
			std::string buttonText;

			const char playerIdTranslationId[3] = { '1' + (char)playerId , 'p', '\0' };
			if (!canExtract || characterName[0] == '\0') {
				buttonText = std::format("{} ({})", _("extraction.extract"), _(playerIdTranslationId));
			}
			else {
				buttonText = std::format("{} ({}, {})", _("extraction.extract"), _(playerIdTranslationId), characterName);
				if (strncmp(characterName, MOVESET_EXTRACTED_NAME_PREFIX, sizeof(MOVESET_EXTRACTED_NAME_PREFIX) - 1) == 0) {
					canExtractThisMoveset = false;
					canExtractAll = false;
				}
			}

			if (ImGuiExtra::RenderButtonEnabled(buttonText.c_str(), canExtractThisMoveset)) {
				extractorHelper.QueueCharacterExtraction(playerId, GetExtractionSettings());
			}
			ImGui::SameLine();
		}

		if (ImGuiExtra::RenderButtonEnabled(_("extraction.extract_all"), canExtractAll)) {
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

	auto availableSpace = ImGui::GetContentRegionAvail();
	ImVec2 tableSize = ImVec2(0, availableSpace.y - ImGui::GetFrameHeightWithSpacing());

	// List of extracted moveset
	ImGui::SeparatorText(_("extraction.extracted_movesets"));
	if (ImGui::BeginTable("MovesetExtractionList", 8, ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY
				| ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoHostExtendY, tableSize))
	{
		ImGui::TableSetupColumn("##", 0, 5.0f);
		ImGui::TableSetupColumn(_("moveset.origin"));
		ImGui::TableSetupColumn(_("moveset.original_character"));
		ImGui::TableSetupColumn(_("moveset.date"));
		ImGui::TableSetupColumn(_("moveset.size"));
		ImGui::TableSetupColumn(_("moveset.version"));
		ImGui::TableSetupColumn(_("persistent.hash"));
		ImGui::TableSetupColumn(_("moveset.delete"), 0, 0.0f);
		ImGui::TableHeadersRow();


		// drawList & windowPos are used to display a different row bg
		ImDrawList* drawlist = ImGui::GetWindowDrawList();
		auto windowPos = ImGui::GetWindowPos();

		// Yes, we don't use an iterator here because the vector might actually change size mid-iteration
		for (size_t i = 0; i < extractorHelper.storage->extractedMovesets.size(); ++i)
		{
			// moveset is guaranteed not to be freed until after this loop
			movesetInfo* moveset = extractorHelper.storage->extractedMovesets[i];

			ImGui::TableNextRow();
			ImGui::TableNextColumn();

			if (moveset->color != 0)
			{
				// Draw BG
				ImVec2 drawStart = windowPos + ImGui::GetCursorPos();
				drawStart.y -= ImGui::GetScrollY();
				ImVec2 drawArea = ImVec2(availableSpace.x, ImGui::GetFrameHeight());
				drawlist->AddRectFilled(drawStart, drawStart + drawArea, moveset->color);
			}

			ImGui::TextUnformatted(moveset->name.c_str());

			ImGui::TableNextColumn();

			if (moveset->origin == "INVALID") {
				// Badly formatted file. Display it, but mention it is invalid
				ImGui::TextUnformatted(_("moveset.invalid"));
				ImGui::TableSetColumnIndex(6);
				ImGui::PushID(moveset->filename.c_str());
				ImGui::PushStyleColor(ImGuiCol_Button, FORM_DELETE_BTN);
				if (ImGui::Button("X")) {
					extractorHelper.storage->DeleteMoveset(moveset->filename.c_str());
				}
				ImGui::PopStyleColor();
				ImGui::PopID();
			}
			else {
				ImGui::TextUnformatted(moveset->origin.c_str());

				ImGui::TableNextColumn();
				ImGui::TextUnformatted(moveset->target_character.c_str());

				ImGui::TableNextColumn();
				ImGui::TextUnformatted(moveset->date_str.c_str());

				ImGui::TableNextColumn();
				ImGui::TextUnformatted(moveset->sizeStr.c_str());

				ImGui::TableNextColumn();
				ImGui::TextUnformatted(moveset->version_string.c_str());

				ImGui::TableNextColumn();
				ImGui::TextUnformatted(moveset->hash.c_str());

				ImGui::TableNextColumn();
				ImGui::PushID(moveset->filename.c_str());
				ImGui::PushStyleColor(ImGuiCol_Button, FORM_DELETE_BTN);
				if (ImGui::Button("X")) {
					extractorHelper.storage->DeleteMoveset(moveset->filename.c_str());
				}
				ImGui::PopStyleColor();
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
		case ExtractionErrcode_CompressionFailure:
			ImGui::Text(_("extraction.error_compression"));
			break;
		}

		if (ImGui::Button(_("close")) || ImGui::IsKeyDown(ImGuiKey_Escape))
		{
			// Reset the errcode
			m_err = ExtractionErrcode_Successful;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

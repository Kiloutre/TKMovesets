#include <format>
#include <filesystem>
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
#include "MovesetFolders.hpp"
#include "MovesetListSort.hpp"

// -- Private methods -- //

namespace {
void ResetMotaSelection(bool (&selection)[12]) {
    constexpr auto defaults = ExtractSettings_MOTA_HANDS | ExtractSettings_MOTA_CAMERAS;
    for (unsigned slot = 0; slot < 12; ++slot) selection[slot] = (defaults & (1U << slot)) != 0;
}
}

Submenu_Extract::Submenu_Extract()
{
	ResetMotaSelection(m_motaExport);

	m_extractDisplayableMovelist = true;

	m_extractUnsupportedMotas = false;

	m_compressionIndex = CompressionUtils::GetDefaultCompressionSetting();
    m_retailCompressionIndex = CompressionUtils::GetCompressionSettingIndex(TKMovesetCompressionType_CompactZstd);
}

ExtractSettings Submenu_Extract::GetExtractionSettings(bool retailT8)
{
	ExtractSettings settings = 0;

	settings |= CompressionUtils::GetCompressionSetting(retailT8 ? m_retailCompressionIndex : m_compressionIndex).extractSetting;

	if (m_extractUnsupportedMotas) {
		settings |= ExtractSettings_UnknownMotas;
	}

	if (Settings::Get(SETTING_EXPORT_OVERWRITE_KEY, SETTING_EXPORT_OVERWRITE)) {
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

    if(retailT8) {
        settings|=ExtractSettings_T8_PANM_SELECTION;
        for(unsigned i=0;i<6;++i)if(m_panmExport[i])settings|=1ULL<<(18+i);
        if(!m_panmFromArchive)settings|=ExtractSettings_T8_NO_ARCHIVE;
    }
	return settings;
}

// -- Public methods -- //

void Submenu_Extract::RenderSettingsPopup(bool retailT8)
{
	if (!retailT8) {
	ImGui::SeparatorText("MOTA");
	if (ImGui::Button(_("extraction.settings.reset_motas"))) ResetMotaSelection(m_motaExport);
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
	} else {
        ImGui::SeparatorText(_("extraction.panm.title"));
        if(ImGui::Button(_("extraction.panm.reset"))) {
            for(unsigned i=0;i<6;++i)m_panmExport[i]=(ExtractSettings_T8_DEFAULT & (1ULL<<(18+i)))!=0;
        }
        ImGui::SameLine();if(ImGui::Button(_("extraction.panm.all")))std::fill_n(m_panmExport,6,true);
        ImGui::SameLine();if(ImGui::Button(_("extraction.panm.none")))std::fill_n(m_panmExport,6,false);
        ImGuiExtra::RenderTextbox(_("extraction.panm.explanation"));
        constexpr const char* labels[]={"extraction.panm.body","extraction.panm.hands","extraction.panm.face","extraction.panm.swing","extraction.panm.camera","extraction.panm.extra"};
        for(unsigned i=0;i<6;++i) {ImGui::Checkbox(_(labels[i]),&m_panmExport[i]);if(!(i&1))ImGui::SameLine();}
        ImGui::Checkbox(_("extraction.panm.from_archive"),&m_panmFromArchive);
        ImGuiExtra::HelpMarker(_("extraction.panm.from_archive_explanation"));
        ImGui::SeparatorText(_("extraction.settings.other"));
    }

	// Persisted: the choice is kept between sessions.
	bool overwriteSameFilename = Settings::Get(SETTING_EXPORT_OVERWRITE_KEY, SETTING_EXPORT_OVERWRITE);
	if (ImGui::Checkbox(_("settings.overwrite_duplicate"), &overwriteSameFilename)) {
		Settings::Set(SETTING_EXPORT_OVERWRITE_KEY, overwriteSameFilename);
	}
	ImGui::SameLine();
	ImGuiExtra::HelpMarker(_("settings.overwrite_explanation"));

	ImGui::TextUnformatted(_("extraction.settings.compress_moveset"));
    auto& compressionIndex = retailT8 ? m_retailCompressionIndex : m_compressionIndex;
	if (ImGui::BeginCombo("##", compressionIndex == 0 ? _("extraction.settings.compression_type.none") : CompressionUtils::GetCompressionSetting(compressionIndex).name))
	{
		for (unsigned int i = 0; i < CompressionUtils::GetCompressionSettingCount(); ++i) {
			if (ImGui::Selectable(i == 0 ? _("extraction.settings.compression_type.none") : CompressionUtils::GetCompressionSetting(i).name, i == compressionIndex, 0, ImVec2(180.0f, 0))) {
				compressionIndex = i;
			}
		}
		ImGui::EndCombo();
	}

	ImGui::Separator();
	if (ImGui::Button(_("edition.open_characters_directory"))) {
        const auto path = std::filesystem::absolute(MovesetFolders::ExtractionDirectory());
        std::filesystem::create_directories(path);
		ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
	}

	if (ImGui::Button(_("close")) || ImGui::IsKeyDown(ImGuiKey_Escape)) {
		ImGui::CloseCurrentPopup();
	}
}

void Submenu_Extract::Render(GameExtract& extractorHelper)
{
    const bool retailT8=extractorHelper.currentGame && extractorHelper.currentGame->gameId==GameId_T8 && extractorHelper.currentGame->minorVersion==GameVersions::T8::RETAIL;
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

					if (gameInfo->separatorText) {
						ImGui::SeparatorText(gameInfo->separatorText);
					}

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
	}

	ImGui::SameLine();
	if (ImGui::Button(_("extraction.settings"))) {
		ImGui::OpenPopup("###ExtractionSettingsPopup");
	}

	if (ImGuiExtra::BeginFittedModal("ExtractionSettingsPopup", _("extraction.settings"), nullptr, 36.0f))
	{
		RenderSettingsPopup(extractorHelper.currentGame && extractorHelper.currentGame->gameId == GameId_T8 &&
            extractorHelper.currentGame->minorVersion == GameVersions::T8::RETAIL);
		ImGui::EndPopup();
	}

	GameProcess& p = extractorHelper.process;

	{
		ImGui::Separator();

		bool canExtract = p.IsAttached() && !busy && extractorHelper.CanStart();
		bool canExtractAll = canExtract;

		// Extraction buttons, will be disabled if we can't extract
		for (int playerId = 0; playerId < extractorHelper.GetCharacterCount(); ++playerId)
		{
			bool canExtractThisMoveset = canExtract;

			const char* characterName = extractorHelper.characterNames[playerId].c_str();
			if (extractorHelper.currentGame && extractorHelper.currentGame->gameId == GameId_T8 &&
                extractorHelper.currentGame->minorVersion == GameVersions::T8::RETAIL && !characterName[0]) canExtractThisMoveset = false;
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
				extractorHelper.QueueCharacterExtraction(playerId, GetExtractionSettings(retailT8));
			}
			ImGui::SameLine();
		}

		if (ImGuiExtra::RenderButtonEnabled(_("extraction.extract_all"), canExtractAll)) {
			extractorHelper.QueueCharacterExtraction(-1, GetExtractionSettings(retailT8));
		}
		const bool t7 = extractorHelper.currentGame && extractorHelper.currentGame->gameId == GameId_T7;
		if (retailT8 || t7) {
			// Every moveset from the game's own files: the modal asks for the game's exe (filled in from the attached game), no loaded character.
			ImGui::SameLine();
			m_diskExtraction.Render(t7 ? T8DiskExtraction::Game::T7 : T8DiskExtraction::Game::T8Retail,
				p.IsAttached() ? std::filesystem::path(p.mainModule.path) : std::filesystem::path(), !busy, *extractorHelper.storage);
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
	switch (p.status)
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
				| ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoHostExtendY | ImGuiTableFlags_Sortable, tableSize))
	{
		ImGui::TableSetupColumn(_("moveset.name"), ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthStretch, 0.0f, MovesetListSort::Name);
		ImGui::TableSetupColumn(_("moveset.game"), 0, 0.0f, MovesetListSort::Game);
		ImGui::TableSetupColumn(_("moveset.original_character"), ImGuiTableColumnFlags_NoSort);
		ImGui::TableSetupColumn(_("moveset.date"), ImGuiTableColumnFlags_NoSort);
		ImGui::TableSetupColumn(_("moveset.size"), ImGuiTableColumnFlags_NoSort);
		ImGui::TableSetupColumn(_("moveset.version"), ImGuiTableColumnFlags_NoSort);
		ImGui::TableSetupColumn(_("persistent.hash"), ImGuiTableColumnFlags_NoSort);
		ImGui::TableSetupColumn(_("moveset.delete"), ImGuiTableColumnFlags_NoSort, 0.0f);
		ImGui::TableHeadersRow();



		// Movesets are guaranteed not to be freed until after this loop
		for (const auto& group : MovesetListSort::GroupedTableView(extractorHelper.storage->extractedMovesets))
		{
		if (!MovesetListSort::GroupHeader(group, group.game == "INVALID" ? _("moveset.invalid") : group.game.c_str())) continue;
		for (movesetInfo* moveset : group.movesets)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			MovesetListSort::RowHover hover;
			hover.Begin();

			if (moveset->color != 0)
			{
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,moveset->color);
			}

			ImGui::TextUnformatted(moveset->name.c_str());
			if (!moveset->notes.empty()) { ImGui::SameLine(); ImGuiExtra::HelpMarker(moveset->notes.c_str(), true); }

			ImGui::TableNextColumn();

			if (moveset->origin == "INVALID") {
				// Badly formatted file. Display it, but mention it is invalid
				ImGui::TextUnformatted(_("moveset.invalid"));
				ImGui::TableSetColumnIndex(7);
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
			hover.End();
		}
		}

		ImGui::EndTable();
	}

	std::string errorDetail;
	ExtractionErrcode_ err = extractorHelper.GetLastError(&errorDetail);
	if (err != ExtractionErrcode_Successful) {
		ImGui::OpenPopup("###ExtractionErrPopup");
		m_err = err;
		m_errorDetail = std::move(errorDetail);
	}

	// Show popup containing the error description
	if (ImGuiExtra::BeginFittedModal("ExtractionErrPopup", _("extraction.error_title")))
	{
		if (!m_errorDetail.empty()) ImGui::TextWrapped("%s", m_errorDetail.c_str());
		else switch (m_err)
		{
        case ExtractionErrcode_MovesetDataError:
            ImGui::TextUnformatted(_("extraction.error_moveset_data"));
            break;
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

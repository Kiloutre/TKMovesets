#include <ImGui.h>
#include <format>

#include "Submenu_Import.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"
#include "Helpers.hpp"
#include "Settings.hpp"
#include "MovesetListSort.hpp"


Submenu_Import::Submenu_Import()
{
	for (auto& selected : m_motaImport) selected = true;
};

void Submenu_Import::RenderMotaSettings()
{
    ImGui::SeparatorText("MOTA");
    ImGui::TextWrapped("%s", _("importation.mota_explanation"));
    char key[] = "mota_00";
    for (unsigned slot = 0; slot < 12; ++slot) {
        key[5] = '0' + slot / 10; key[6] = '0' + slot % 10;
        ImGui::Checkbox(_(key), &m_motaImport[slot]);
        if ((slot & 1) == 0) ImGui::SameLine();
    }
    if (ImGui::Button(_("close"))) ImGui::CloseCurrentPopup();
}

ImportSettings Submenu_Import::GetImportationSettings()
{
	ImportSettings settings = 0;

	if (Settings::Get(SETTING_IMPORT_INSTANT_KEY, SETTING_IMPORT_INSTANT)) {
		settings |= ImportSettings_ApplyInstantly;
	}

	if (Settings::Get(SETTING_FREE_UNUSED_KEY, SETTING_FREE_UNUSED)) {
		settings |= ImportSettings_FreeUnusedMovesets;
	}

    uint16_t motaMask = 0;
    for (unsigned slot = 0; slot < 12; ++slot) if (m_motaImport[slot]) motaMask |= 1U << slot;
	return ImportSettingsUtils::WithMotaMask(settings, motaMask);
}

void Submenu_Import::Render(GameImport& importerHelper)
{
	bool busy = importerHelper.IsBusy();

	ImGuiExtra::RenderTextbox(_("importation.explanation"));
	ImGui::SeparatorText(_("importation.importation"));

	{
		ImGui::TextUnformatted(_("importation.import_to"));
		ImGui::SameLine();

		// Game list
		auto currentGame = importerHelper.currentGame;

		ImGuiExtra::DisableBlockIf __(busy);

		ImGui::PushItemWidth(200);
		ImGui::PushID(&importerHelper); // Have to push an ID here because extraction.select_game would cause a conflict
		auto gameListCount = Games::GetGamesCount();
		if (ImGui::BeginCombo("##", currentGame == nullptr ? _("select_game") : currentGame->name))
		{
			for (uint8_t gameIdx = 0; gameIdx < gameListCount; ++gameIdx)
			{
				auto gameInfo = Games::GetGameInfoFromIndex(gameIdx);

				if (gameInfo->importer != nullptr) {
					if (ImGui::Selectable(gameInfo->name, currentGame == gameInfo, 0, ImVec2(200.0f, 0))) {
						importerHelper.SetTargetProcess(gameInfo);
					}
				}
			}
			ImGui::EndCombo();
		}
		ImGui::PopID();
	}


	{
		ImGui::SameLine();
		char buf[3] = { '1' + importerHelper.currentPlayerId, 'p', '\0' };
		ImGui::PushItemWidth(100.0f);
		if (ImGui::BeginCombo("##", _(buf)))
		{
			size_t currentPlayerId = importerHelper.currentPlayerId;
			for (int8_t i = 0; i < importerHelper.GetCharacterCount(); ++i)
			{
				buf[0] = '1' + i;
				if (ImGui::Selectable(_(buf), currentPlayerId == i, 0, ImVec2(100.0f, 0))) {
					importerHelper.currentPlayerId = i;
				}
			}
			ImGui::EndCombo();
		}
	}

    const bool retail = importerHelper.currentGame && importerHelper.currentGame->gameId == GameId_T8 &&
        importerHelper.currentGame->minorVersion == GameVersions::T8::RETAIL;
    ImGui::SameLine();
    if (!retail && ImGuiExtra::RenderButtonEnabled(_("extraction.settings"), !busy)) ImGui::OpenPopup("###ImportMotaSettings");
    if (ImGuiExtra::BeginFittedModal("ImportMotaSettings", _("extraction.settings"))) {
        RenderMotaSettings();
        ImGui::EndPopup();
    }
	// If we can't import, display a warning detailling why
	GameProcess& p = importerHelper.process;

	bool canImport = p.IsAttached() && !busy && importerHelper.CanStart();

	if (importerHelper.progress > 0) {
		// Progress text.
		ImGui::SameLine();
		if (importerHelper.progress == 100) {
			ImGui::TextColored(ImVec4(0, 1.0f, 0, 1), _("importation.progress_done"));
		}
		else {
			if (busy) {
				ImGui::Text(_("importation.progress"), importerHelper.progress);
			}
			else {
				ImGui::TextColored(ImVec4(1.0f, 0, 0, 1), _("importation.progress_error"), importerHelper.progress);
			}
		}
	}

	switch (p.status)
	{
	case GameProcessErrcode_PROC_ATTACHED:
		if (!importerHelper.CanStart()) {
			ImGuiExtra_TextboxWarning(_("importation.cant_import"));
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

	ImGui::SeparatorText(_("importation.select_moveset"));
	if (ImGui::BeginTable("MovesetImportationList", 5, ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY
		| ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoHostExtendY | ImGuiTableFlags_Sortable, tableSize))
	{
		ImGui::TableSetupColumn(_("moveset.name"), ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthStretch, 0.0f, MovesetListSort::Name);
		ImGui::TableSetupColumn(_("moveset.game"), 0, 0.0f, MovesetListSort::Game);
		ImGui::TableSetupColumn(_("moveset.target_character"), ImGuiTableColumnFlags_NoSort);
		ImGui::TableSetupColumn(_("moveset.date"), ImGuiTableColumnFlags_NoSort);
		ImGui::TableSetupColumn(_("moveset.import"), ImGuiTableColumnFlags_NoSort);
		ImGui::TableHeadersRow();

		ImGui::PushID(&importerHelper);

		// Movesets are guaranteed not to be freed until after this loop
		for (const auto& group : MovesetListSort::GroupedTableView(importerHelper.storage->extractedMovesets))
		{
		if (!MovesetListSort::GroupHeader(group, group.game == "INVALID" ? _("moveset.invalid") : group.game.c_str())) continue;
		for (movesetInfo* moveset : group.movesets)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			MovesetListSort::RowHover hover;
			hover.Begin();

			if (moveset->color != 0) {
				ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, moveset->color);
			}

			ImGui::TextUnformatted(moveset->name.c_str());
			if (!moveset->notes.empty()) { ImGui::SameLine(); ImGuiExtra::HelpMarker(moveset->notes.c_str(), true); }

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
				ImGui::TextUnformatted(moveset->date_str.c_str());

				ImGui::TableNextColumn();
				ImGui::PushID(moveset->filename.c_str());

				bool canImportMoveset = canImport && importerHelper.currentGame->SupportsGameImport(moveset->gameId);
				if (ImGuiExtra::RenderButtonEnabled(_("moveset.import"), canImportMoveset)) {
					importerHelper.QueueCharacterImportation(moveset->filename, GetImportationSettings());
				}
				ImGui::PopID();
			}
			hover.End();
		}
		}
		ImGui::PopID();

		ImGui::EndTable();
	}

    std::string errorDetail;
	ImportationErrcode_ err = importerHelper.GetLastError(&errorDetail);
	if (err != ImportationErrcode_Successful) {
		ImGui::OpenPopup("###ImportationErrPopup");
		m_err = err;
        m_errorDetail = std::move(errorDetail);
	}

	if (ImGuiExtra::BeginFittedModal("ImportationErrPopup", "Import failed"))
	{
        if (!m_errorDetail.empty()) ImGui::TextWrapped("%s",m_errorDetail.c_str());
        else {
		switch (m_err)
		{
        case ImportationErrcode_GameWriteError:
            ImGui::TextUnformatted(_("importation.error_game_write"));
            break;
        case ImportationErrcode_GameRollbackError:
            ImGui::TextUnformatted(_("importation.error_game_rollback"));
            break;
        case ImportationErrcode_UnsupportedGameVersion:
            ImGui::TextUnformatted(_("importation.error_unsupported_version"));
            break;
        case ImportationErrcode_MovesetDataError:
            ImGui::TextUnformatted(_("importation.error_moveset_data"));
            break;
		case ImportationErrcode_AllocationErr:
			ImGui::Text(_("importation.error_allocation"));
			break;
		case ImportationErrcode_GameAllocationErr:
			ImGui::Text(_("importation.error_game_allocation"));
			break;
		case ImportationErrcode_FileReadErr:
			ImGui::Text(_("importation.error_file_reading"));
			break;
		case ImportationErrcode_DecompressionError:
			ImGui::Text(_("importation.error_decompression"));
			break;
		}
        }

		if (ImGui::Button(_("close")) || ImGui::IsKeyDown(ImGuiKey_Escape))
		{
			m_err = ImportationErrcode_Successful;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	// The import went through, with something the user should know (e.g. moves falling back to the idle animation)
	ImportWarning warning;
	if (importerHelper.GetLastWarning(warning)) {
		ImGui::OpenPopup("###ImportationWarnPopup");
		const auto unavailable = warning.unavailableAnimations;
		m_warning = warning.targetFallback
			? std::vformat(_("importation.warning_target_fallback"), std::make_format_args(warning.fallbackMoves))
			: std::vformat(_("importation.warning_idle_fallback"), std::make_format_args(warning.fallbackMoves, unavailable));
		if (!warning.firstError.empty())
			m_warning += "\n" + std::vformat(_("importation.warning_first_error"), std::make_format_args(warning.firstError));
	}

	if (ImGuiExtra::BeginFittedModal("ImportationWarnPopup", _("importation.warning_title")))
	{
		ImGuiExtra_TextboxWarning(m_warning.c_str());

		if (ImGui::Button(_("close")) || ImGui::IsKeyDown(ImGuiKey_Escape))
		{
			m_warning.clear();
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

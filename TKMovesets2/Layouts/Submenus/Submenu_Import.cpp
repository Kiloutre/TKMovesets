#include <ImGui.h>
#include <format>

#include "Submenu_Import.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"
#include "Helpers.hpp"


Submenu_Import::Submenu_Import()
{
	m_applyInstantly = true;
	m_freeUnusedMoveset = true;
};

ImportSettings Submenu_Import::GetImportationSettings()
{
	ImportSettings retVal = 0;

	if (m_applyInstantly) {
		retVal |= ImportSettings_ApplyInstantly;
	}

	if (m_freeUnusedMoveset) {
		retVal |= ImportSettings_FreeUnusedMovesets;
	}

	return retVal;
}

void Submenu_Import::Render(GameImport& importerHelper)
{
	ImGuiExtra::RenderTextbox(_("importation.explanation"));
	ImGui::SeparatorText(_("importation.importation"));

	{
		ImGui::TextUnformatted(_("importation.import_to"));
		ImGui::SameLine();

		// Game list
		int8_t currentGameId = importerHelper.currentGameId;
		ImGui::PushItemWidth(160.);
		ImGui::PushID(&importerHelper); // Have to push an ID here because extraction.select_game would cause a conflict
		uint8_t gameListCount = Games::GetGamesCount();
		if (ImGui::BeginCombo("##", currentGameId == -1 ? _("select_game") : Games::GetGameInfo(currentGameId)->name))
		{
			for (uint8_t i = 0; i < gameListCount; ++i)
			{
				GameInfo* game = Games::GetGameInfo(i);
				if (game->importer != nullptr) {
					if (ImGui::Selectable(game->name, currentGameId == i, 0, ImVec2(100.0f, 0))) {
						importerHelper.SetTargetProcess(game->processName, i);
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

	// Basic importation settings
	ImGui::SameLine();
	ImGui::Checkbox(_("importation.apply_instantly"), &m_applyInstantly);
	ImGui::SameLine();
	ImGuiExtra::HelpMarker(_("importation.apply_instantly_explanation"));

	ImGui::SameLine();
	ImGui::Checkbox(_("importation.free_unused_movesets"), &m_freeUnusedMoveset);
	ImGui::SameLine();
	ImGuiExtra::HelpMarker(_("importation.free_unused_movesets_explanation"));

	// If we can't import, display a warning detailling why
	GameProcess* p = importerHelper.process;

	bool busy = importerHelper.IsBusy();
	bool canImport = p->IsAttached() && !busy && importerHelper.CanStart();

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

	switch (p->status)
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
	if (ImGui::BeginTable("MovesetImportationList", 6, ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY
		| ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoHostExtendY, tableSize))
	{
		ImGui::TableSetupColumn("##", 0, 5.0f);
		ImGui::TableSetupColumn(_("moveset.origin"));
		ImGui::TableSetupColumn(_("moveset.target_character"));
		ImGui::TableSetupColumn(_("moveset.date"));
		ImGui::TableSetupColumn(_("moveset.size"));
		ImGui::TableSetupColumn(_("moveset.import"));
		ImGui::TableHeadersRow();

		ImGui::PushID(&importerHelper);

		// drawList & windowPos are used to display a different row bg
		ImDrawList* drawlist = ImGui::GetWindowDrawList();
		auto windowPos = ImGui::GetWindowPos();

		// Yes, we don't use an iterator here because the vector might actually change size mid-iteration
		for (size_t i = 0; i < importerHelper.storage->extractedMovesets.size(); ++i)
		{
			// moveset is guaranteed not to be freed until after this loop
			movesetInfo* moveset = importerHelper.storage->extractedMovesets[i];

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
				ImGui::PushID(moveset->filename.c_str());

				if (ImGuiExtra::RenderButtonEnabled(_("moveset.import"), canImport)) {
					importerHelper.QueueCharacterImportation(moveset->filename, GetImportationSettings());
				}
				ImGui::PopID();
			}
		}
		ImGui::PopID();

		// Don't de-allocate moveset infos until we're done iterating on it
		importerHelper.storage->CleanupUnusedMovesetInfos();

		ImGui::EndTable();
	}

	ImportationErrcode_ err = importerHelper.GetLastError();
	if (err != ImportationErrcode_Successful) {
		ImGui::OpenPopup("ImportationErrPopup");
		m_err = err;
	}

	if (ImGui::BeginPopupModal("ImportationErrPopup"))
	{
		switch (m_err)
		{
		case ImportationErrcode_AllocationErr:
			ImGui::Text(_("importation.error_allocation"));
			break;
		case ImportationErrcode_GameAllocationErr:
			ImGui::Text(_("importation.error_game_allocation"));
			break;
		case ImportationErrcode_FileReadErr:
			ImGui::Text(_("importation.error_file_creation"));
			break;
		}

		if (ImGui::Button(_("close")))
		{
			m_err = ImportationErrcode_Successful;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

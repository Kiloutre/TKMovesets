#include <ImGui.h>

#include "Submenu_OnlinePlay.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"
#include "Games.hpp"

void Submenu_OnlinePlay::SelectMoveset(movesetInfo* moveset)
{
	unsigned int playerId = 0;
	gameHelper->QueueCharacterImportation(moveset, playerId, ImportSettings_BasicLoadOnly);
}

void Submenu_OnlinePlay::RenderMovesetList(bool canSelectMoveset)
{
	auto availableSpace = ImGui::GetContentRegionAvail();
	ImVec2& tableSize = availableSpace;

	ImGui::SeparatorText(_("importation.select_moveset"));
	if (ImGui::BeginTable("MovesetImportationList", 5, ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY
		| ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoHostExtendY, tableSize))
	{
		ImGui::TableSetupColumn("##", 0, 5.0f);
		ImGui::TableSetupColumn(_("moveset.origin"));
		ImGui::TableSetupColumn(_("moveset.target_character"));
		ImGui::TableSetupColumn(_("moveset.size"));
		ImGui::TableSetupColumn(_("moveset.import"));
		ImGui::TableHeadersRow();

		ImGui::PushID(&gameHelper);

		// drawList & windowPos are used to display a different row bg
		ImDrawList* drawlist = ImGui::GetWindowDrawList();
		auto windowPos = ImGui::GetWindowPos();

		// Yes, we don't use an iterator here because the vector might actually change size mid-iteration
		for (size_t i = 0; i < gameHelper->storage->extractedMovesets.size(); ++i)
		{
			// Moveset is guaranteed not to be freed until after this loop
			movesetInfo* moveset = gameHelper->storage->extractedMovesets[i];

			if (moveset->origin == "INVALID") {
				continue;
			}

			ImGui::TableNextRow();
			ImGui::TableNextColumn();

			bool isImportable = moveset->onlineImportable;
			if (!isImportable)
			{
				// Draw BG
				ImVec2 drawStart = windowPos + ImGui::GetCursorPos();
				drawStart.y -= ImGui::GetScrollY();
				ImVec2 drawArea = ImVec2(availableSpace.x, ImGui::GetFrameHeight());
				drawlist->AddRectFilled(drawStart, drawStart + drawArea, MOVESET_INVALID);
			}

			ImGui::TextUnformatted(moveset->name.c_str());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(moveset->origin.c_str());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(moveset->target_character.c_str());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(moveset->sizeStr.c_str());

			ImGui::TableNextColumn();
			ImGui::PushID(moveset->filename.c_str());

			bool canImportSpecificMoveset = canSelectMoveset && isImportable && gameHelper->currentGame->SupportsGameImport(moveset->gameId);
			if (ImGuiExtra::RenderButtonEnabled(_("online.select_moveset"), canImportSpecificMoveset)) {
				SelectMoveset(moveset);
			}
			ImGui::PopID();
		}
		ImGui::PopID();

		ImGui::EndTable();
	}
}

void Submenu_OnlinePlay::Render()
{
	ImGuiExtra::RenderTextbox(_("online.explanation"));
	ImGui::SeparatorText(_("select_game"));

	// Game list. Selecting a game will set the extraction thread to try to attach to it regularly
	auto currentGame = gameHelper->currentGame;

	ImGui::PushItemWidth(160);
	ImGui::PushID(&gameHelper); // Have to push an ID here because extraction.select_game would cause a conflict
	auto gameListCount = Games::GetGamesCount();
	if (ImGui::BeginCombo("##", currentGame == nullptr ? _("select_game") : currentGame->name))
	{
		for (uint8_t gameIdx = 0; gameIdx < gameListCount; ++gameIdx)
		{
			auto gameInfo = Games::GetGameInfoFromIndex(gameIdx);
			if (gameInfo->onlineHandler != nullptr) {
				if (ImGui::Selectable(gameInfo->name, currentGame == gameInfo, 0, ImVec2(140.0f, 0))) {
					gameHelper->SetTargetProcess(gameInfo);
				}
			}
		}
		ImGui::EndCombo();
	}
	ImGui::PopID();

	// Store whether or not the shared memory was loaded both in the remote and current process
	// If so, the link is established
	bool isAttached = gameHelper->IsAttached();
	bool sharedMemoryLoaded = gameHelper->isMemoryLoaded;
	bool isBusy = gameHelper->IsBusy();
	bool isInjecting = gameHelper->isInjecting;

	if (!sharedMemoryLoaded && isAttached) {
		ImGui::SameLine();
		if (isInjecting) {
			ImGuiExtra::RenderButtonEnabled(_("online.injecting_dll"), false);
		}
		else {
			if (ImGui::Button(_("online.inject_dll"))) {
				gameHelper->InjectDll();
			}
		}
		ImGuiExtra_TextboxWarning(_("online.dll_not_loaded"));
	}

	switch (gameHelper->process->status)
	{
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

	// Moveset list

	auto availableSpace = ImGui::GetContentRegionAvail();
	ImVec2 tableSize = ImVec2(availableSpace.x, availableSpace.y - ImGui::GetFrameHeightWithSpacing());

	if (ImGui::BeginTable("MovesetOnlineSelect", 2, ImGuiTableFlags_NoHostExtendY, tableSize))
	{
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		{

			bool canSelectMoveset = sharedMemoryLoaded && !isBusy;
			RenderMovesetList(canSelectMoveset);
		}

		ImGui::TableNextColumn();
		{
			// Progress text.
			if (0 < gameHelper->progress && gameHelper->progress < 100) {
				if (isBusy) {
					ImGui::Text(_("importation.progress"), gameHelper->progress);
				}
				else {
					ImGui::TextColored(ImVec4(1.0f, 0, 0, 1), _("importation.progress_error"), gameHelper->progress);
				}
			}

			// Show selected moveset info
			if (sharedMemoryLoaded)
			{
				// Purposefuly copy the whole list in case it gets cleared while we iterate on it
				std::vector<movesetInfo> selectedMovesetList_copy = gameHelper->displayedMovesets;
				for (auto& moveset : gameHelper->displayedMovesets) {
					ImGui::Text("%s %s", _("online.selected_moveset"), moveset.name.c_str());
				}
			}
		}

		ImGui::EndTable();
	}
}
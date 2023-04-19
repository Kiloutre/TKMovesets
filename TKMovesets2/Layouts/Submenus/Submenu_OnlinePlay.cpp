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

void Submenu_OnlinePlay::RenderMovelist(bool canSelectMoveset)
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

			bool isImportable = moveset->onlineImportable; // todo: think about how different games will be handled
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

			if (ImGuiExtra::RenderButtonEnabled(_("online.select_moveset"), canSelectMoveset && isImportable)) {
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
	int8_t currentGameId = gameHelper->currentGameId;
	ImGui::PushItemWidth(160);
	ImGui::PushID(&gameHelper); // Have to push an ID here because extraction.select_game would cause a conflict
	uint8_t gameListCount = Games::GetGamesCount();
	if (ImGui::BeginCombo("##", currentGameId == -1 ? _("select_game") : Games::GetGameInfoFromIndex(currentGameId)->name))
	{
		for (uint8_t i = 0; i < gameListCount; ++i)
		{
			auto game = Games::GetGameInfoFromIndex(i);
			if (game->onlineHandler != nullptr) {
				if (ImGui::Selectable(game->name, currentGameId == i, 0, ImVec2(100.0f, 0))) {
					gameHelper->SetTargetProcess(game->processName, i);
				}
			}
		}
		ImGui::EndCombo();
	}
	ImGui::PopID();

	// Store whether or not the shared memory was loaded both in the remote and current process
	// If so, the link is established
	bool sharedMemoryLoaded = false;
	bool isBusy = gameHelper->IsBusy();

	switch (gameHelper->process->status)
	{
	case GameProcessErrcode_PROC_ATTACHED:
		if (!gameHelper->sharedMemHandler->IsMemoryLoaded())
		{
			ImGuiExtra_TextboxWarning(_("online.dll_not_loaded"));
			if (ImGui::Button(_("online.inject_dll"))) {
				gameHelper->sharedMemHandler->InjectDll();
			}
		}
		else {
			sharedMemoryLoaded = true;
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

	// Moveset list

	auto availableSpace = ImGui::GetContentRegionAvail();
	ImVec2 tableSize = ImVec2(availableSpace.x, availableSpace.y - ImGui::GetFrameHeightWithSpacing());

	if (ImGui::BeginTable("MovesetOnlineSelect", 2, ImGuiTableFlags_NoHostExtendY, tableSize))
	{
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		{

			bool canSelectMoveset = sharedMemoryLoaded && !isBusy;
			RenderMovelist(canSelectMoveset);
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
				// Purposefuly copy the whole list in case it gets deallocated while we iterate on it
				auto selectedMovestList_copy = *gameHelper->sharedMemHandler->movesetInfos;
				for (auto& moveset : selectedMovestList_copy)
				{
					ImGui::Text("%s %s", _("online.selected_moveset"), moveset.name.c_str());
				}
			}
		}

		ImGui::EndTable();
	}
}
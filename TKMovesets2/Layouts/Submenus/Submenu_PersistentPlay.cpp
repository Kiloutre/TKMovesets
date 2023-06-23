#include <ImGui.h>

#include "Submenu_PersistentPlay.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"
#include "Games.hpp"

void Submenu_PersistentPlay::SelectMoveset(movesetInfo* moveset)
{
	gameHelper->QueueCharacterImportation(moveset, m_currentPlayerCursor, ImportSettings_BasicLoadOnly);
	m_currentPlayerCursor = -1;
}

void Submenu_PersistentPlay::ClearMoveset()
{
	// Pass a moveset of size 0 to indicate the GameSharedMem class that we want to clear and not import a moveset
	movesetInfo emptyMoveset{ .size = 0 };

	gameHelper->QueueCharacterImportation(&emptyMoveset, m_currentPlayerCursor, ImportSettings_BasicLoadOnly);
	m_currentPlayerCursor = -1;
}

void Submenu_PersistentPlay::RenderMovesetList(bool canSelectMoveset)
{
	auto availableSpace = ImGui::GetContentRegionAvail();
	availableSpace.y -= ImGui::GetFrameHeightWithSpacing();

	ImGui::SeparatorText(_("importation.select_moveset"));
	if (ImGui::BeginTable("MovesetImportationList", 6, ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY
		| ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoHostExtendY, availableSpace))
	{
		ImGui::TableSetupColumn("##", 0, 5.0f);
		ImGui::TableSetupColumn(_("moveset.origin"));
		ImGui::TableSetupColumn(_("moveset.target_character"));
		ImGui::TableSetupColumn(_("moveset.version"));
		ImGui::TableSetupColumn(_("persistent.hash"));
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

			bool isImportable = moveset->onlineImportable && gameHelper->currentGame->SupportsGameImport(moveset->gameId);
			int color = !isImportable ? MOVESET_INVALID : moveset->color;
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
			ImGui::TextUnformatted(moveset->origin.c_str());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(moveset->target_character.c_str());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(moveset->sizeStr.c_str());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(moveset->hash.c_str());

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

void Submenu_PersistentPlay::RenderGameControls()
{

	// Game list. Selecting a game will set the extraction thread to try to attach to it regularly
	auto currentGame = gameHelper->currentGame;
	bool canChangeGame = m_currentPlayerCursor == -1 && !gameHelper->lockedIn;


    {
        ImGuiExtra::DisableBlockIf __(!canChangeGame);

        ImGui::PushItemWidth(160);
        ImGui::PushID(&gameHelper); // Have to push an ID here because extraction.select_game would cause a conflict
        if (ImGui::BeginCombo("##", currentGame == nullptr ? _("select_game") : currentGame->name))
        {
            auto gameListCount = Games::GetGamesCount();


            for (uint8_t gameIdx = 0; gameIdx < gameListCount; ++gameIdx)
            {
                auto gameInfo = Games::GetGameInfoFromIndex(gameIdx);
                if (gameInfo->onlineHandler != nullptr) {
                    if (ImGui::Selectable(gameInfo->name, currentGame == gameInfo, 0, ImVec2(140.0f, 0)) && canChangeGame) {
                        gameHelper->SetTargetProcess(gameInfo);
                    }
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopID();
    }


	// Store whether or not the shared memory was loaded both in the remote and current process
	// If so, the link is established
	bool isAttached = gameHelper->IsAttached();
	bool isInjecting = gameHelper->isInjecting;
	bool sharedMemoryLoaded = gameHelper->isMemoryLoaded;
	bool isBusy = gameHelper->IsBusy();
	bool versionMatches = !gameHelper->versionMismatch;

	if (!sharedMemoryLoaded && isAttached) {
		ImGui::SameLine();
		if (isInjecting) {
			ImGuiExtra::RenderButtonEnabled(_("online.injecting_dll"), false);
		}
		else {
			if (ImGui::Button(_("online.inject_dll"))) {
				gameHelper->InjectDll();
				gameHelper->SetSharedMemDestroyBehaviour(true);
			}
		}
		ImGuiExtra_TextboxWarning(_("online.dll_not_loaded"));
	}
	else
	{
		// Import progress text.
		if (0 < gameHelper->progress && gameHelper->progress < 100) {
			ImGui::SameLine();
			if (isBusy) {
				ImGui::Text(_("importation.progress"), gameHelper->progress);
			}
			else {
				ImGui::TextColored(ImVec4(1.0f, 0, 0, 1), _("importation.progress_error"), gameHelper->progress);
			}
		}
		else if (isAttached && sharedMemoryLoaded && versionMatches) {
			if (gameHelper->lockedIn) {
				ImGui::PushStyleColor(ImGuiCol_Button, GREEN_BTN);
				if (ImGui::Button(_("persistent.unlock"), ImVec2(0, ImGui::GetFrameHeightWithSpacing()))) {
					gameHelper->lockedIn = false;
				}
				ImGui::PopStyleColor();
				
			}
			else {
				ImGui::PushStyleColor(ImGuiCol_Button, RED_BTN);
				if (ImGuiExtra::RenderButtonEnabled(_("persistent.lock"), m_currentPlayerCursor == -1, ImVec2(0, ImGui::GetFrameHeightWithSpacing()))) {
					gameHelper->lockedIn = true;
					gameHelper->movesetLoaderMode = MovesetLoaderMode_PersistentPlay;
				}
				ImGui::PopStyleColor();
				ImGui::TextUnformatted(_("online.lock_to_play"));
			}
		}
	}
}

void Submenu_PersistentPlay::Render()
{
	ImGuiExtra::RenderTextbox(_("persistent.explanation"));
	ImGui::SeparatorText(_("select_game"));

	bool versionMatches = !gameHelper->versionMismatch;
	bool sharedMemoryLoaded = gameHelper->isMemoryLoaded;
	bool isBusy = gameHelper->IsBusy();

	auto availableSpace = ImGui::GetContentRegionAvail();

	unsigned int playerCount = gameHelper->GetCharacterCount();
	if (ImGui::BeginTable("OnlinePlayCharSelect", 1 + playerCount, ImGuiTableFlags_NoHostExtendY, ImVec2(0, 75)))
	{
		ImGui::TableNextRow();
		ImGui::TableNextColumn();

		{
			RenderGameControls();

			switch (gameHelper->process.status)
			{
				case GameProcessErrcode_PROC_ATTACHED:
					if (!versionMatches) {
						ImGuiExtra_TextboxWarning(_("process.dll_version_mismatch"));
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
		}

		// List of players with their associated moveset
		std::vector<movesetInfo> selectedMovesetList_copy = gameHelper->displayedMovesets;
		auto rowWidth = availableSpace.x / 3;
		bool movesetListNotEmpty = gameHelper->storage->extractedMovesets.size();
		for (unsigned int playerid = 0; playerid < playerCount; ++playerid)
		{
			auto p = ImGui::GetCursorPos();
			ImGui::TableNextColumn();
			{
				std::string characterName = _("persistent.none");
				std::string crc32;

				if (selectedMovesetList_copy.size() > playerid && selectedMovesetList_copy[playerid].size != 0) {
					characterName = selectedMovesetList_copy[playerid].name;
					crc32 = selectedMovesetList_copy[playerid].hash;
				}

				char buf[] = "persistent.player_0";
				buf[18] = '1' + playerid;
				ImGui::TextUnformatted(_(buf));
				ImGui::SameLine();
				ImGui::TextUnformatted(characterName.c_str());

				ImGui::TextUnformatted(_("persistent.hash"));
				ImGui::SameLine();
				ImGui::TextUnformatted(crc32.c_str());

				// Select button becomes clear button if in moveset select mode (either you clear or you pick a moveset)
				bool canClick = sharedMemoryLoaded && movesetListNotEmpty && !gameHelper->lockedIn;

				// Since both players can have buttons of the same name shown at the same time, have to push an ID for the imgui stack to differentiate both
				ImGui::PushID(playerid);
				if (m_currentPlayerCursor == -1 || !sharedMemoryLoaded) {
					if (ImGuiExtra::RenderButtonEnabled(_("persistent.select_moveset"), versionMatches && canClick && m_currentPlayerCursor == -1)) {
						m_currentPlayerCursor = playerid;
						DEBUG_LOG("m_currentPlayerCursor = %d\n", playerid);
						// Moveset select mode
					}
				} else {
					if (ImGuiExtra::RenderButtonEnabled(_("persistent.clear_moveset"), versionMatches && canClick && m_currentPlayerCursor == playerid)) {
						ClearMoveset();
					}
				}
				ImGui::PopID();


			}
		}


		ImGui::EndTable();
	}

	// Moveset list
	bool canSelectMoveset = sharedMemoryLoaded && !isBusy && m_currentPlayerCursor != -1;
	RenderMovesetList(versionMatches && canSelectMoveset);
}
#include <ImGui.h>

#include "Submenu_OnlinePlay.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"
#include "Games.hpp"

void Submenu_OnlinePlay::Render()
{
	ImGuiExtra::RenderTextbox(_("online.explanation"));
	ImGui::SeparatorText(_("select_game"));

	// Game list. Selecting a game will set the extraction thread to try to attach to it regularly
	int8_t currentGameId = gameHelper.currentGameId;
	ImGui::PushItemWidth(160);
	ImGui::PushID(&gameHelper); // Have to push an ID here because extraction.select_game would cause a conflict
	uint8_t gameListCount = Games::GetGamesCount();
	if (ImGui::BeginCombo("##", currentGameId == -1 ? _("select_game") : Games::GetGameInfo(currentGameId)->name))
	{
		for (uint8_t i = 0; i < gameListCount; ++i)
		{
			GameInfo* game = Games::GetGameInfo(i);
			if (game->onlineHandler != nullptr) {
				if (ImGui::Selectable(game->name, currentGameId == i, 0, ImVec2(100.0f, 0))) {
					gameHelper.SetTargetProcess(game->processName, i);
				}
			}
		}
		ImGui::EndCombo();
	}
	ImGui::PopID();

	switch (gameHelper.process->status)
	{
	case GameProcessErrcode_PROC_ATTACHED:
		if (!gameHelper.sharedMemHandler->IsMemoryLoaded())
		{
			ImGuiExtra_TextboxWarning(_("online.dll_not_loaded"));
			if (ImGui::Button(_("online.inject_dll"))) {
				gameHelper.sharedMemHandler->InjectDll();
			}
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
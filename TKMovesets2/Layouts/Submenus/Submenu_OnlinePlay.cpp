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
	int8_t currentGameId = importerHelper.currentGameId;
	ImGui::PushItemWidth(120);
	ImGui::PushID(&importerHelper); // Have to push an ID here because extraction.select_game would cause a conflict
	uint8_t gameListCount = Games::GetGamesCount();
	if (ImGui::BeginCombo("##", currentGameId == -1 ? _("select_game") : Games::GetGameInfo(currentGameId)->name))
	{
		for (uint8_t i = 0; i < gameListCount; ++i)
		{
			GameInfo* game = Games::GetGameInfo(i);
			if (game->extractor != nullptr) {
				if (ImGui::Selectable(game->name, currentGameId == i, 0, ImVec2(100.0f, 0))) {
					importerHelper.SetTargetProcess(game->processName, i);
				}
			}
		}
		ImGui::EndCombo();
	}
	ImGui::PopID();

}


void Submenu_OnlinePlay::SetTargetProcess(const char* processName, uint8_t gameId)
{
	importerHelper.SetTargetProcess(processName, gameId);
}
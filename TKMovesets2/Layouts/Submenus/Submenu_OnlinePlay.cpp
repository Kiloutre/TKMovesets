#include <ImGui.h>

#include "Submenu_OnlinePlay.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"
#include "Games.hpp"

void Submenu_OnlinePlay::Render()
{
	// Detect change in attachment status
	{
		bool isAttached = gameHelper.IsAttached();
		if (isAttached != m_isAttached) {
			if (isAttached) {
				OnAttach();
			}
			else {
				OnDetach();
			}
			m_isAttached = isAttached;
		}
	}

	ImGuiExtra::RenderTextbox(_("online.explanation"));
	ImGui::SeparatorText(_("select_game"));

	// Game list. Selecting a game will set the extraction thread to try to attach to it regularly
	int8_t currentGameId = gameHelper.currentGameId;
	ImGui::PushItemWidth(120);
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
		if (!m_onlineHandler->IsMemoryLoaded()) {
			ImGuiExtra_TextboxWarning(_("online.dll_not_loaded"));
			if (ImGui::Button(_("online.inject_dll"))) {
				m_onlineHandler->InjectDll();
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

void Submenu_OnlinePlay::OnAttach()
{
	m_onlineHandler = Games::FactoryGetOnline(gameHelper.currentGameId, gameHelper.process, gameHelper.game);
}

void Submenu_OnlinePlay::OnDetach()
{
	if (m_onlineHandler != nullptr) {
		delete m_onlineHandler;
		m_onlineHandler = nullptr;
	}
}

void Submenu_OnlinePlay::SetTargetProcess(const char* processName, uint8_t gameId)
{
	// The importer (gameHelper) doesn't have this flag by default, so we have to add it here
	gameHelper.AddProcessFlags(PROCESS_CREATE_THREAD);
	gameHelper.SetTargetProcess(processName, gameId);
}

Submenu_OnlinePlay::~Submenu_OnlinePlay()
{
	delete m_onlineHandler;
}
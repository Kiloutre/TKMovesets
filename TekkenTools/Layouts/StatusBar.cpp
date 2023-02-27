#include <ImGui.h>
#include <stdio.h>

#include "StatusBar.hpp"
#include "GameProcess.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"

StatusBar::StatusBar()
{
	// Init
}
void StatusBar::Render()
{
	/*
	* Draw FPS
	{
		char buf[40];
		sprintf(buf, "%.1f FPS |", 1000.0f / ImGui::GetIO().Framerate);
		ImGui::TextUnformatted(buf);
		ImGui::SameLine();
	}
	*/

	// Todo: set layout mode as horizontal, look into menubars maybe

	GameProcess& p = GameProcess::getInstance();

	ImGui::TextUnformatted(_("process.status"));
	ImGui::SameLine();

	switch (p.errcode)
	{
		// Todo: apply color-coding
	default:
	case PROC_NOT_ATTACHED:
		ImGui::TextUnformatted(_("process.game_not_attached"));
		break;
	case PROC_NOT_FOUND:
		ImGui::TextUnformatted(_("process.game_not_running"));
		break;
	case PROC_ATTACHING:
		ImGui::TextUnformatted(_("process.attaching"));
		break;
	case PROC_VERSION_MISMATCH:
		ImGui::TextUnformatted(_("process.game_version_mismatch"));
		break;
	case PROC_ATTACH_ERR:
		ImGui::TextUnformatted(_("process.game_attach_err"));
		break;
	case PROC_ATTACHED:
		ImGui::TextUnformatted(_("process.game_attached"));
		break;
	case PROC_EXITED:
		ImGui::TextUnformatted(_("process.game_unexpected_exit"));
		break;
	}

	ImGui::SameLine();
}

#include <ImGui.h>

#include "StatusBar.h"
#include "GameProcess.h"
#include "Localization.h"
#include "imgui_extras.h"

StatusBar::StatusBar()
{
	// Init
}
void StatusBar::Render()
{
	// Todo: set layout mode as horizontal, look into menubars maybe

	GameProcess& p = GameProcess::getInstance();

	if (p.errcode != PROC_ATTACHED && p.errcode != PROC_ATTACHING) {
		if (ImGui::Button(_("process.attach"))) {
			p.Attach();
		}
		ImGui::SameLine();
	}

	ImGui::Text(_("process.status"));
	ImGui::SameLine();

	switch (p.errcode)
	{
		// Todo: apply color-coding
	case PROC_NOT_ATTACHED:
		ImGui::Text(_("process.game_not_attached"));
		break;
	case PROC_NOT_FOUND:
		ImGui::Text(_("process.game_not_running"));
		break;
	case PROC_ATTACHING:
		ImGui::Text(_("process.attaching"));
		break;
	case PROC_ATTACH_ERR:
		ImGui::Text(_("process.game_version_mismatch"));
		break;
	case PROC_VERSION_MISMATCH:
		ImGui::Text(_("process.game_attach_err"));
		break;
	case PROC_ATTACHED:
		ImGui::Text(_("process.game_attached"));
		break;
	case PROC_EXITED:
		ImGui::Text(_("process.game_unexpected_exit"));
		break;
	}
}

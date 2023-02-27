#include <ImGui.h>

#include "Submenu_Extract.h"
#include "Localization.h"
#include "imgui_extras.h"
#include "GameProcess.h"

Submenu_Extract::Submenu_Extract()
{
	// Init
}
void Submenu_Extract::Render() 
{
	ImGuiExtra::RenderTextbox(_("extraction.explanation"));
	ImGui::SeparatorText(_("extraction.extracted_movesets"));

	{
		GameProcess& p = GameProcess::getInstance();
		if (p.errcode != PROC_ATTACHED)
		{
			switch (p.errcode)
			{
			case PROC_NOT_ATTACHED:
			case PROC_EXITED:
				ImGuiExtra_TextboxWarning(_("process.game_not_loaded"));
				break;
			case PROC_NOT_FOUND:
				ImGuiExtra_TextboxWarning(_("process.game_not_running"));
				break;
			case PROC_ATTACH_ERR:
				ImGuiExtra_TextboxWarning(_("process.game_version_mismatch"));
				break;
			case PROC_VERSION_MISMATCH:
				ImGuiExtra_TextboxWarning(_("process.game_attach_err"));
				break;
			case PROC_ATTACHING:
				ImGuiExtra_TextboxTip(_("process.attaching"));
				break;
			}

			if (ImGui::Button(_("process.attach"))) p.Attach();
		}

	}

}

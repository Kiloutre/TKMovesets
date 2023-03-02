#include <ImGui.h>

#include "Submenu_Import.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"

struct gameProcessName
{
	const char* name;
	const char* processName;
};

// Respect the ids of GameId enum here.
const gameProcessName importGameList[] = {
	{ "Tekken 7", "TekkenGame-Win64-Shipping.exe" },
	{ "Tekken 8", "Tekken8.exe" },
};

const size_t gameListCount = sizeof(importGameList) / sizeof(*importGameList);

void Submenu_Import::Render(GameImport *importerHelper)
{
	ImGuiExtra::RenderTextbox(_("importation.explanation"));
	ImGui::SeparatorText(_("importation.importation"));

	{
		size_t currentGameId = importerHelper->currentGameId;
		ImGui::PushItemWidth(ImGui::CalcTextSize(_("extraction.select_game")).x * 1.5);
		ImGui::PushID(&importerHelper); // Have to push an ID here because extraction.select_game would cause a conflict
		if (ImGui::BeginCombo("##", currentGameId == -1 ? _("extraction.select_game") : importGameList[currentGameId].name))
		{
			for (size_t i = 0; i < gameListCount; ++i)
			{
				if (ImGui::Selectable(importGameList[i].name, currentGameId == i, 0, ImVec2(100.0f, 0)))
				{
					importerHelper->SetTargetProcess(importGameList[i].processName, i);
				}
			}
			ImGui::EndCombo();
		}
		ImGui::PopID();
		ImGui::SameLine();
	}

	{
		char buf[3] = { '1' + importerHelper->currentPlayerId, 'p', '\0' };
		ImGui::PushItemWidth(150.0f);
		if (ImGui::BeginCombo("##", _(buf)))
		{
			size_t currentPlayerId = importerHelper->currentPlayerId;
			for (size_t i = 0; i < importerHelper->characterCount; ++i)
			{
				buf[0] = '1' + i;
				if (ImGui::Selectable(_(buf), currentPlayerId == i, 0, ImVec2(100.0f, 0)))
				{
					importerHelper->currentPlayerId = i;
				}
			}
			ImGui::EndCombo();
		}
	}


	// If we can't extract, display a warning detailling why
	GameProcess* p = importerHelper->process;
	if (p->status != PROC_ATTACHED)
	{
		switch (p->status)
		{
		case PROC_NOT_ATTACHED:
		case PROC_EXITED:
		case PROC_ATTACHING:
			ImGuiExtra_TextboxWarning(_("process.game_not_attached"));
			break;
		case PROC_NOT_FOUND:
			ImGuiExtra_TextboxWarning(_("process.game_not_running"));
			break;
		case PROC_VERSION_MISMATCH:
			ImGuiExtra_TextboxError(_("process.game_version_mismatch"));
			break;
		case PROC_ATTACH_ERR:
			ImGuiExtra_TextboxError(_("process.game_attach_err"));
			break;
		}
	}
	else if (!importerHelper->CanStart()) {
		ImGuiExtra_TextboxWarning(_("importation.cant_import"));
	}

	ImGui::SeparatorText(_("importation.select_moveset"));
	if (ImGui::BeginTable("##", 5, ImGuiTableFlags_ContextMenuInBody | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg
		| ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
	{
		ImGui::TableSetupColumn("##", 0, 5.0f);
		ImGui::TableSetupColumn(_("moveset.origin"));
		ImGui::TableSetupColumn(_("moveset.target_character"));
		ImGui::TableSetupColumn(_("moveset.date"));
		ImGui::TableSetupColumn("##");
		ImGui::TableHeadersRow();

		// Yes, we don't use an iterator here because the vector might actually change size mid-iteration
		ImGui::PushID(&importerHelper);
		for (size_t i = 0; i < importerHelper->storage->extractedMovesets.size(); ++i)
		{
			// moveset is guaranteed not to be freed until after this loop
			movesetInfo* moveset = importerHelper->storage->extractedMovesets[i];

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
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
				ImGui::TextUnformatted(moveset->date.c_str());

				ImGui::TableNextColumn();
				ImGui::PushID(moveset->filename.c_str());
				if (ImGui::Button(_("moveset.import")))
				{
					importerHelper->QueueCharacterImportation(moveset->filename, i);
				}
				ImGui::PopID();
			}
		}
		ImGui::PopID();
		// Don't de-allocate moveset infos until we're done iterating on it
		importerHelper->storage->CleanupUnusedMovesetInfos();

		ImGui::EndTable();
	}
	//ImGui::ListBox("##", &selectedMoveset, items, sizeof(items) / sizeof(*items));
}

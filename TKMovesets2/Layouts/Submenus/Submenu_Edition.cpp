#include <ImGui.h>
#include <format>

#include "Submenu_Edition.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"
#include "helpers.hpp"

// -- Static helpers -- //


// -- Private methods -- //

void Submenu_Edition::Save()
{
	// todo
	m_savedLastChange = true;
}

void Submenu_Edition::RenderToolBar()
{
	const ImVec2& Size = ImGui::GetWindowSize();
	const ImVec2& Pos = ImGui::GetWindowPos();
	const float height = 60;
	const float width = Size.x;

	//ImGui::SetNextWindowPos(Pos);
	//ImGui::SetNextWindowSizeConstraints(ImVec2(0, height), ImVec2(width, height));
	//ImGui::Begin("Editor ToolBar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav);
	//ImGui::Begin("Editor ToolBar");
	ImGui::Text("TOOLS");
	//ImGui::End();
}

void Submenu_Edition::RenderStatusBar()
{
	const ImVec2& Size = ImGui::GetWindowSize();
	const ImVec2& Pos = ImGui::GetWindowPos();
	const float height = 60;
	const float width = Size.x;

	//ImGui::SetNextWindowPos(ImVec2(Pos.x, Pos.y + Size.y - height));
	//ImGui::SetNextWindowSizeConstraints(ImVec2(0, height), ImVec2(width, height));
	//ImGui::Begin("Editor StatusBar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav);


	if (ImGuiExtra::RenderButtonEnabled(_("edition.save"), !m_savedLastChange)) {
		Save();
	}
	ImGui::SameLine();
	
	ImGui::TextUnformatted(_("edition.last_saved"));
	ImGui::SameLine();
	ImGui::TextUnformatted(Helpers::currentDateTime(m_loadedCharacter.lastSavedDate).c_str());
	ImGui::SameLine();

	// Vertical separator
	ImGui::TextUnformatted("|");
	ImGui::SameLine();

	// Game list
	int8_t currentGameId = importer.currentGameId;
	ImGui::PushItemWidth(100.0f);
	ImGui::PushID(&importer); // Have to push an ID here because extraction.select_game would cause a conflict
	uint8_t gameListCount = Games::GetGamesCount();

	if (ImGui::BeginCombo("##", (currentGameId == -1) ? _("select_game") : Games::GetGameInfo(currentGameId)->name))
	{
		for (uint8_t i = 0; i < gameListCount; ++i)
		{
			GameInfo* game = Games::GetGameInfo(i);
			if (game->importer != nullptr) {
				if (ImGui::Selectable(game->name, currentGameId == i, 0, ImVec2(100.0f, 0))) {
					importer.SetTargetProcess(game->processName, i);
				}
			}
		}
		ImGui::EndCombo();
	}
	ImGui::PopID();
	ImGui::SameLine();


	// Process error
	bool isAttached = importer.process->IsAttached();
	if (currentGameId != -1 && !isAttached) {
		// Short process error message
		ImGuiExtra::RenderTextbox(_("edition.process_err"), TEXTBOX_BORDER_ERROR, TEXTBOX_BORDER_ERROR, 2.5f);
		ImGui::SameLine();
	}

	// Vertical separator
	ImGui::TextUnformatted("|");
	ImGui::SameLine();

	// Player list
	{
		ImGui::SameLine();
		char buf[3] = { '1' + importer.currentPlayerId, 'p', '\0' };
		ImGui::PushItemWidth(100.0f);

		uint8_t playerCount = min(2, importer.GetCharacterCount());
		if (ImGui::BeginCombo("##", _(buf)))
		{
			size_t currentPlayerId = importer.currentPlayerId;
			for (int8_t i = 0; i < playerCount; ++i)
			{
				buf[0] = '1' + i;
				if (ImGui::Selectable(_(buf), currentPlayerId == i, 0, ImVec2(100.0f, 0))) {
					importer.currentPlayerId = i;
				}
			}
			ImGui::EndCombo();
		}
	}
	
	// Import button
	ImGui::SameLine();
	bool canImport = isAttached && m_importNeeded && !importer.IsBusy();
	if (ImGuiExtra::RenderButtonEnabled(_("moveset.import"), canImport)) {
		importer.QueueCharacterImportation(m_loadedCharacter.filename);
	}

	// Live edition
	ImGui::SameLine();
	ImGui::Checkbox(_("edition.live_edition"), &m_liveEdition);

	//ImGui::End();
}

bool Submenu_Edition::LoadMoveset(movesetInfo* moveset)
{
	m_loadedCharacter.filename = moveset->filename;
	m_loadedCharacter.name = moveset->name;
	m_loadedCharacter.lastSavedDate = moveset->date;
	m_loadedCharacter.gameId = moveset->gameId;
	return true;
}

//

void Submenu_Edition::RenderMovesetSelector()
{
	ImGuiExtra::RenderTextbox(_("edition.explanation"));

	ImGui::SeparatorText(_("edition.select_moveset"));
	if (ImGui::BeginTable("##", 6, ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY
		| ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_NoHostExtendY | ImGuiTableFlags_Resizable, ImVec2(0, ImGui::GetContentRegionAvail().y)))
	{
		ImGui::TableSetupColumn("##", 0, 5.0f);
		ImGui::TableSetupColumn(_("moveset.origin"));
		ImGui::TableSetupColumn(_("moveset.target_character"));
		ImGui::TableSetupColumn(_("moveset.date"));
		ImGui::TableSetupColumn(_("moveset.size"));
		ImGui::TableSetupColumn(_("moveset.edit"));
		ImGui::TableHeadersRow();

		// Yes, we don't use an iterator here because the vector might actually change size mid-iteration
		ImGui::PushID(&importer.storage);
		for (size_t i = 0; i < importer.storage->extractedMovesets.size(); ++i)
		{
			// moveset is guaranteed not to be freed until after this loop
			movesetInfo* moveset = importer.storage->extractedMovesets[i];

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
				ImGui::TextUnformatted(Helpers::currentDateTime(moveset->date).c_str());

				ImGui::TableNextColumn();
				std::string sizeString = std::format("{:.2f} {}", moveset->size, _("moveset.size_mb"));
				ImGui::TextUnformatted(sizeString.c_str());

				ImGui::TableNextColumn();
				ImGui::PushID(moveset->filename.c_str());

				if (ImGuiExtra::RenderButtonEnabled(_("moveset.edit"), moveset->editable)) {
					if (LoadMoveset(moveset)) {
						popen = true;
						// todo: popup detecting moveset name change etc
					}
					else {
						// todo : Show error
					}
				}
				ImGui::PopID();
			}
		}
		ImGui::PopID();

		ImGui::EndTable();
	}

}

// -- Public methods -- //

void Submenu_Edition::Render()
{
	RenderMovesetSelector();

	ImGui::SetNextWindowSize(ImVec2(1280, 720), ImGuiCond_Once);
	if (m_popen)
	{
		// todo: title should be character name based
		if (ImGui::Begin("newin", &m_popen, ImGuiWindowFlags_NoDocking))
		{
			RenderToolBar();
			const ImVec2& Size = ImGui::GetContentRegionAvail();
			ImGuiID dockId = ImGui::DockSpace(2, ImVec2(Size.x, Size.y - 40));
			// Render windows, send dockId
			// RenderEditorWindows(dockId);
			RenderStatusBar();
		}
		ImGui::End();
	}
}

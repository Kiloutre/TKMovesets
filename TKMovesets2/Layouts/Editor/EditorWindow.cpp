#include <ImGui.h>

#include "EditorWindow.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"
#include "helpers.hpp"

// -- Private methods -- //

void EditorWindow::Save()
{
	// todo
	m_savedLastChange = true;
}

void EditorWindow::RenderToolBar()
{
	// todo: ImGuiWindowFlags_MenuBar ?
	ImGui::Text("TOOLS");
}

void EditorWindow::RenderStatusBar()
{
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
	if (currentGameId != -1 && !isAttached)
	{
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
	bool canImport = isAttached && m_importNeeded && !importer.IsBusy() && importer.CanStart();
	if (ImGuiExtra::RenderButtonEnabled(_("moveset.import"), canImport)) {
		importer.QueueCharacterImportation(m_moveset, m_movesetSize);
	}

	// Live edition
	ImGui::SameLine();
	ImGui::Checkbox(_("edition.live_edition"), &m_liveEdition);
	ImGui::SameLine();
	ImGuiExtra::HelpMarker(_("edition.live_edition_explanation"));
}

void EditorWindow::RenderMovesetData(ImGuiID dockId)
{

}

// -- Public methods -- //

EditorWindow::~EditorWindow()
{
	importer.StopThreadAndCleanup();
	delete importer.process;
	delete importer.game;
	free(m_moveset);
}

EditorWindow::EditorWindow(movesetInfo* movesetInfo)
{

	std::ifstream file(movesetInfo->filename.c_str(), std::ios::binary);

	if (file.fail()) {
		throw EditorWindow_MovesetLoadFail();
	}

	file.seekg(0, std::ios::end);
	m_movesetSize = file.tellg();

	m_moveset = (byte*)malloc(m_movesetSize);
	if (m_moveset == nullptr) {
		throw EditorWindow_MovesetLoadFail();
	}

	file.seekg(0, std::ios::beg);
	file.read((char*)m_moveset, m_movesetSize);

	file.close();

	m_loadedCharacter.filename = movesetInfo->filename;
	m_loadedCharacter.name = movesetInfo->name;
	m_loadedCharacter.lastSavedDate = movesetInfo->date;
	m_loadedCharacter.gameId = movesetInfo->gameId;
	filename = movesetInfo->filename;
}

void EditorWindow::Render(int dockid)
{
	ImGui::SetNextWindowSize(ImVec2(1280, 720), ImGuiCond_Once);
	ImGui::SetNextWindowSizeConstraints(ImVec2(200, 200), ImVec2(9999, 9999));

	if (setFocus) {
		ImGui::SetNextWindowFocus();
		setFocus = false;
	}

	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking;
	// ImGuiWindowFlags_MenuBar
	if (!m_savedLastChange) {
		windowFlags |= ImGuiWindowFlags_UnsavedDocument;
	}

	if (ImGui::Begin(m_loadedCharacter.name.c_str(), &popen, windowFlags))
	{
		RenderToolBar();

		const ImVec2& Size = ImGui::GetContentRegionAvail();
		ImGuiID dockId = ImGui::DockSpace(dockid + 2, ImVec2(Size.x, Size.y - 40));
		RenderMovesetData(dockId);

		RenderStatusBar();
	}
	ImGui::End();
}

#include <ImGui.h>

#include <algorithm>
#include <format>

#include "Localization.hpp"
#include "imgui_extras.hpp"
#include "helpers.hpp"
#include "EditorWindow.hpp"
// Structures
#include "EditorMove.hpp"
#include "EditorVoiceclip.hpp"
#include "EditorExtraproperties.hpp"
#include "EditorCancels.hpp"
#include "EditorCancelExtra.hpp"
#include "EditorRequirements.hpp"
#include "EditorHitConditions.hpp"

// -- Private methods -- //

EditorForm* EditorWindow::AllocateFormWindow(EditorWindowType_ windowType, uint16_t id)
{
	switch (windowType)
	{
	case EditorWindowType_Move:
		return new EditorMove(m_windowTitle, id, m_editor);;
		break;
	case EditorWindowType_Voiceclip:
		return new EditorVoiceclip(m_windowTitle, id, m_editor);;
		break;
	case EditorWindowType_Extraproperty:
		return new EditorExtraproperties(m_windowTitle, id, m_editor);;
		break;
	case EditorWindowType_Cancel:
		return new EditorCancels(m_windowTitle, id, m_editor);;
		break;
	case EditorWindowType_CancelExtradata:
		return new EditorCancelExtra(m_windowTitle, id, m_editor);;
		break;
	case EditorWindowType_Requirement:
		return new EditorRequirements(m_windowTitle, id, m_editor);;
		break;
	case EditorWindowType_HitCondition:
		return new EditorHitConditions(m_windowTitle, id, m_editor);;
		break;
	}

	return nullptr;
}

void EditorWindow::OnFormFieldClick(std::string fieldIdentifier, const char* buffer)
{
	int id = atoi(buffer);
	
	// Move
	if (fieldIdentifier == "edition.move_field.cancel_id") {
		// todo: also make it work for other cancel IDs
		if (id >= 0) {
			OpenFormWindow(EditorWindowType_Cancel, id);
		}
	}
	else if (fieldIdentifier == "edition.move_field.transition") {
		id = ValidateMoveId(buffer);
		if (id >= 0) {
			OpenFormWindow(EditorWindowType_Move, id);
		}
	} else if (fieldIdentifier == "edition.move_field.voiceclip_id") {
		if (id >= 0) {
			OpenFormWindow(EditorWindowType_Voiceclip, id);
		}
	}
	else if (fieldIdentifier == "edition.move_field.hit_condition_id") {
		if (id >= 0) {
			OpenFormWindow(EditorWindowType_HitCondition, id);
		}
	}
	else if (fieldIdentifier == "edition.move_field.extra_properties_id") {
		if (id >= 0) {
			OpenFormWindow(EditorWindowType_Extraproperty, id);
		}
	}
	// Cancels
	else if (fieldIdentifier == "edition.cancel_field.move_id") {
		// Validate move id will proceed to an alias conversion which i want here
		OpenFormWindow(EditorWindowType_Move, ValidateMoveId(buffer));
	}
	else if (fieldIdentifier == "edition.cancel_field.extradata_addr") {
		// Validate move id will proceed to an alias conversion which i want here
		OpenFormWindow(EditorWindowType_CancelExtradata, id);
	}
	else if (fieldIdentifier == "edition.cancel_field.requirement_addr") {
		// Validate move id will proceed to an alias conversion which i want here
		OpenFormWindow(EditorWindowType_Requirement, id);
	}
}

void EditorWindow::OpenFormWindow(EditorWindowType_ windowType, uint16_t moveId)
{
	// todo: template this function?
	int availableOverwriteIndex = -1;
	for (int i = 0; i < m_structWindows.size(); ++i) {
		EditorForm* structWin = m_structWindows[i];
		if (structWin->windowType != windowType) {
			continue;
		}

		if (structWin->id == moveId) {
			structWin->setFocus = true;
			// Prevent duplicate move window creation
			return;
		}
		if (structWin->unsavedChanges == false) {
			// Don't overwrite windows with unsaved changes
			availableOverwriteIndex = i;
		}
	}

	bool openNew = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
	EditorForm* newWin = AllocateFormWindow(windowType, moveId);
	if (openNew || availableOverwriteIndex == -1) {
		m_structWindows.push_back(newWin);
	}
	else {
		delete m_structWindows[availableOverwriteIndex];
		m_structWindows[availableOverwriteIndex] = newWin;
	}
}

void EditorWindow::FilterMovelist(EditorMovelistFilter_ filter)
{
	m_filteredMovelist.clear();

	m_movelistFilter = filter;
	if (filter == EditorMovelistFilter_All) {
		m_filteredMovelist = m_movelist;
		return;
	}

	if (filter == EditorMovelistFilter_PostIdle) {
		// Get idle move ID, only list moves beyond it
		size_t startingIndex = m_editorTable.aliases[1];
		for (; startingIndex < m_movelist.size(); ++startingIndex) {
			m_filteredMovelist.push_back(m_movelist[startingIndex]);
		}
		return;
	}

	EditorMoveFlags flags = 0;

	switch (filter)
	{
	case EditorMovelistFilter_Attacks:
		flags |= EditorMoveFlags_Attack | EditorMoveFlags_OtherAttack;
		break;
	case EditorMovelistFilter_Generic:
		flags |= EditorMoveFlags_Generic;
		break;
	case EditorMovelistFilter_Throws:
		flags |= EditorMoveFlags_Throw;
		break;
	case EditorMovelistFilter_Custom:
		flags |= EditorMoveFlags_Custom;
		break;
	}

	for (DisplayableMove* move : m_movelist)
	{
		if (move->flags & flags) {
			m_filteredMovelist.push_back(move);
		}
	}

	// If displaying generic moves, sort by generic id
	if (filter & EditorMovelistFilter_Generic) {
		struct sortByAlias {
			bool operator()(DisplayableMove const* a, DisplayableMove const* b) const {
				return a->aliasId < b->aliasId;
			}
		};
		std::sort(m_filteredMovelist.begin(), m_filteredMovelist.end(), sortByAlias());
	}
}

int32_t EditorWindow::ValidateMoveId(const char* buf)
{
	int32_t moveId = atoi(buf);

	const size_t movelistSize = m_movelist.size();
	if (moveId >= movelistSize)
	{
		const size_t aliasesCount = m_editorTable.aliases.size();
		if (moveId < 0x8000 || moveId >= (0x8000 + aliasesCount)) {
			return -1;
		}
		moveId = m_editorTable.aliases[moveId - (uint16_t)0x8000];
	}

	return moveId;
}

bool EditorWindow::MovesetStillLoaded()
{
	gameAddr movesetAddress = importerHelper.importer->GetMovesetAddress(importerHelper.currentPlayerId);
	return movesetAddress == m_loadedMoveset;
}

void EditorWindow::Save()
{
	printf("Save();\n");
	TKMovesetHeader* header = (TKMovesetHeader*)m_moveset;
	header->infos.date = duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

	std::ofstream file(m_loadedCharacter.filename, std::ios::binary);
	file.write((char*)m_moveset, m_movesetSize);
	file.close();
	m_savedLastChange = true;
	printf("m_savedLastChange = true\n");
}

void EditorWindow::RenderToolBar()
{
	// todo: ImGuiWindowFlags_MenuBar ?
	ImGui::Text("TOOLS: TODO");
	ImGui::SameLine();
	ImGuiExtra::HelpMarker(_("edition.window_controls_explanation"));
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
	int8_t currentGameId = importerHelper.currentGameId;
	ImGui::PushItemWidth(100.0f);
	ImGui::PushID(&importerHelper); // Have to push an ID here because extraction.select_game would cause a conflict
	uint8_t gameListCount = Games::GetGamesCount();

	if (ImGui::BeginCombo("##", (currentGameId == -1) ? _("select_game") : Games::GetGameInfo(currentGameId)->name))
	{
		for (uint8_t i = 0; i < gameListCount; ++i)
		{
			GameInfo* game = Games::GetGameInfo(i);
			if (game->importer != nullptr) {
				if (ImGui::Selectable(game->name, currentGameId == i, 0, ImVec2(100.0f, 0))) {
					importerHelper.SetTargetProcess(game->processName, i);
					m_loadedMoveset = 0;
					m_liveEdition = false;
					m_importNeeded = true;
				}
			}
		}
		ImGui::EndCombo();
	}
	ImGui::PopID();
	ImGui::SameLine();


	// Process error
	bool isAttached = importerHelper.process->IsAttached();
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
		char buf[3] = { '1' + importerHelper.currentPlayerId, 'p', '\0' };
		ImGui::PushItemWidth(100.0f);

		uint8_t playerCount = min(2, importerHelper.GetCharacterCount());
		if (ImGui::BeginCombo("##", _(buf)))
		{
			size_t currentPlayerId = importerHelper.currentPlayerId;
			for (int8_t i = 0; i < playerCount; ++i)
			{
				buf[0] = '1' + i;
				if (ImGui::Selectable(_(buf), currentPlayerId == i, 0, ImVec2(100.0f, 0))) {
					importerHelper.currentPlayerId = i;
					importerHelper.lastLoadedMoveset = 0;
				}
			}
			ImGui::EndCombo();
		}
	}
	
	// Import button
	ImGui::SameLine();
	bool canImport = isAttached && m_importNeeded && !importerHelper.IsBusy() && m_canInteractWithGame;
	if (ImGuiExtra::RenderButtonEnabled(_("moveset.import"), canImport)) {
		importerHelper.lastLoadedMoveset = 0;
		importerHelper.QueueCharacterImportation(m_moveset, m_movesetSize);
		m_loadedMoveset = 0; // We will get the loaded moveset later since the import is in another thread
		m_liveEdition = false;
		m_importNeeded = false;
	}

	// Live edition. Might not be implemented for every game.
	if (m_liveEditable)
	{
		if (m_loadedMoveset == 0) {
			ImGui::BeginDisabled();
		}
		ImGui::SameLine();
		ImGui::Checkbox(_("edition.live_edition"), &m_liveEdition);
		ImGui::SameLine();
		ImGuiExtra::HelpMarker(_("edition.live_edition_explanation"));
		if (m_loadedMoveset == 0) {
			ImGui::EndDisabled();
		}
	}

}

void EditorWindow::RenderMovesetData(ImGuiID dockId)
{
	for (size_t i = 0; i < m_structWindows.size();)
	{
		EditorForm* moveWin = m_structWindows[i];
		if (moveWin->popen)
		{
			ImGui::SetNextWindowDockID(dockId, ImGuiCond_Once);
			moveWin->Render();

			ClickableFieldEvent* ev = moveWin->GetFormClickEvent();
			if (ev != nullptr) {
				OnFormFieldClick(ev->eventName, ev->buffer);
			}

			if (moveWin->justAppliedChanges) {
				moveWin->justAppliedChanges = false;
				m_savedLastChange = false;
				m_importNeeded = true;
			}
			++i;
		}
		else {
			m_structWindows.erase(m_structWindows.begin() + i);
			delete moveWin;
		}
	}
}

void EditorWindow::RenderMovelist()
{
	// Filter / Sorting
	if (ImGui::BeginTabBar("MovelistTabs"))
	{
		// Todo: Do not do this using tab bars but buttons
		if (ImGui::BeginTabItem(_("edition.moves_all"))) {
			if (m_movelistFilter != EditorMovelistFilter_All) {
				FilterMovelist(EditorMovelistFilter_All);
			}
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem(_("edition.moves_attacks"))) {
			if (m_movelistFilter != EditorMovelistFilter_Attacks) {
				FilterMovelist(EditorMovelistFilter_Attacks);
			}
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem(_("edition.moves_generic"))) {
			if (m_movelistFilter != EditorMovelistFilter_Generic) {
				FilterMovelist(EditorMovelistFilter_Generic);
			}
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem(_("edition.moves_throws"))) {
			if (m_movelistFilter != EditorMovelistFilter_Throws) {
				FilterMovelist(EditorMovelistFilter_Throws);
			}
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem(_("edition.moves_custom"))) {
			if (m_movelistFilter != EditorMovelistFilter_Custom) {
				FilterMovelist(EditorMovelistFilter_Custom);
			}
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem(_("edition.moves_char_specific"))) {
			if (m_movelistFilter != EditorMovelistFilter_PostIdle) {
				FilterMovelist(EditorMovelistFilter_PostIdle);
			}
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	// Movelist. Leave some 50 units of space for move player
	{
		ImVec2 TableSize = ImGui::GetContentRegionAvail();
		TableSize.y -= 80;
		if (ImGui::BeginTable("MovelistTable", 3, ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY
			| ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable, TableSize))
		{
			ImGui::TableSetupColumn("ID");
			ImGui::TableSetupColumn(_("edition.move_name"));
			ImGui::TableSetupColumn(_("edition.move_generic_id"));
			ImGui::TableHeadersRow();

			for (DisplayableMove* move : m_filteredMovelist)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				if (move->flags != 0)
				{
					ImVec2 drawStart = ImGui::GetWindowPos() + ImGui::GetCursorPos();
					drawStart.y -= ImGui::GetScrollY();
					ImVec2 drawArea = ImVec2(TableSize.x, ImGui::GetTextLineHeight());
					ImDrawList* drawlist = ImGui::GetWindowDrawList();
					drawlist->AddRectFilled(drawStart, drawStart + drawArea, move->color);
					// Draw BG
				}

				ImGui::TextUnformatted(std::format("{}", move->moveId).c_str());

				ImGui::TableNextColumn();
				if (ImGui::Selectable(move->name.c_str(), move->moveId == m_highlightedMoveId)) {
					m_highlightedMoveId = move->moveId;
					m_moveToPlay = move->moveId;
					sprintf_s(m_moveToPlayBuf, sizeof(m_moveToPlayBuf), "%d", move->moveId);
					OpenFormWindow(EditorWindowType_Move, move->moveId);
				}

				if (move->aliasId != 0) {
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(std::format("{}", move->aliasId).c_str());
				}

				if (m_moveToScrollTo == move->moveId) {
					ImGui::SetScrollHereY();
					m_moveToScrollTo = -1;
				}
			}

			ImGui::EndTable();
		}
	}
	// Set player move
	ImVec2 buttonSize = ImVec2(ImGui::GetContentRegionAvail().x / 2 - 5, 0);
	ImGui::PushItemWidth(buttonSize.x);
	if (ImGui::InputTextWithHint("##", _("edition.move_id_hint"), m_moveToPlayBuf, sizeof(m_moveToPlayBuf) - 1), ImGuiInputTextFlags_CharsDecimal) {
		m_moveToPlay = ValidateMoveId(m_moveToPlayBuf);
	}
	ImGui::SameLine();
	if (ImGuiExtra::RenderButtonEnabled(_("edition.move_current"), m_loadedMoveset != 0, buttonSize)) {
		m_moveToScrollTo = (int16_t)m_editor->GetCurrentMoveID(importerHelper.currentPlayerId);
		sprintf_s(m_moveToPlayBuf, sizeof(m_moveToPlayBuf), "%d", m_moveToScrollTo);
		OpenFormWindow(EditorWindowType_Move, m_moveToScrollTo);
		m_highlightedMoveId = m_moveToScrollTo;
	}
	ImGui::PopItemWidth();

	if (ImGuiExtra::RenderButtonEnabled(_("edition.play_move_1p"), m_loadedMoveset != 0 && m_moveToPlay != -1, buttonSize)) {
		m_editor->SetCurrentMove(0, m_loadedMoveset, m_moveToPlay);
	}
	ImGui::SameLine();
	if (ImGuiExtra::RenderButtonEnabled(_("edition.play_move_2p"), m_loadedMoveset != 0 && m_moveToPlay != -1, buttonSize)) {
		m_editor->SetCurrentMove(1, m_loadedMoveset, m_moveToPlay);
	}
}

// -- Public methods -- //

EditorWindow::~EditorWindow()
{
	importerHelper.StopThreadAndCleanup();
	delete m_editor;
	free(m_moveset);
}

EditorWindow::EditorWindow(movesetInfo* movesetInfo, GameAddressesFile *addrFile, LocalStorage *storage)
{
	importerHelper.Init(addrFile, storage);
	importerHelper.StartThread();

	m_editor = Games::FactoryGetEditor(movesetInfo->gameId, importerHelper.process, importerHelper.game);

	std::ifstream file(movesetInfo->filename.c_str(), std::ios::binary);

	if (file.fail()) {
		throw EditorWindow_MovesetLoadFail();
	}

	file.seekg(0, std::ios::end);
	m_movesetSize = file.tellg();

	m_moveset = (Byte*)malloc(m_movesetSize);
	if (m_moveset == nullptr) {
		throw EditorWindow_MovesetLoadFail();
	}

	file.seekg(0, std::ios::beg);
	file.read((char*)m_moveset, m_movesetSize);

	file.close();

	m_editor->LoadMoveset(m_moveset, m_movesetSize);
	m_liveEditable = Games::IsGameLiveEditable(movesetInfo->gameId);

	m_loadedCharacter.filename = movesetInfo->filename;
	m_loadedCharacter.name = movesetInfo->name;
	m_loadedCharacter.lastSavedDate = movesetInfo->date;
	m_loadedCharacter.gameId = movesetInfo->gameId;
	filename = movesetInfo->filename;

	m_windowTitle = std::format("{}: {}", m_loadedCharacter.name.c_str(), _("edition.window_title"));

	// Read what needs to be read and potentially displayed right away
	m_movelist = m_editor->GetDisplayableMoveList();
	m_filteredMovelist = m_movelist;

	m_editorTable = m_editor->GetMovesetTable();
}

void EditorWindow::Render(int dockid)
{
	// Check for important changes here
	m_canInteractWithGame = importerHelper.CanStart();

	if (m_loadedMoveset != 0) {
		if (!m_canInteractWithGame || !MovesetStillLoaded())
		{
			m_liveEdition = false;
			m_importNeeded = true;
			m_loadedMoveset = 0;
		}
	}
	else {
		// If the moveset was successfully imported, this will be filled with a nonzero value
		m_loadedMoveset = importerHelper.lastLoadedMoveset;
	}

	// Layout start
	ImGui::SetNextWindowSize(ImVec2(1280, 720), ImGuiCond_Once);
	ImGui::SetNextWindowSizeConstraints(ImVec2(200, 400), ImVec2(9999, 9999));

	if (setFocus) {
		ImGui::SetNextWindowFocus();
		setFocus = false;
	}

	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking;
	// ImGuiWindowFlags_MenuBar
	if (!m_savedLastChange) {
		windowFlags |= ImGuiWindowFlags_UnsavedDocument;
	}

	if (ImGui::Begin(m_windowTitle.c_str(), &popen, windowFlags))
	{
		RenderToolBar();

		// We will render the statusbar afterward so we need to save FrameHeight of space
		ImVec2 Size = ImGui::GetContentRegionAvail();
		Size.y -= ImGui::GetFrameHeightWithSpacing();

		if (ImGui::BeginTable("MovesetMainTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_Borders
		| ImGuiTableFlags_NoHostExtendY, Size))
		{
			ImGui::TableSetupColumn("##", ImGuiTableColumnFlags_WidthFixed, 300.0f);
			ImGui::TableSetupColumn("##");
			ImGui::TableHeadersRow();

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			RenderMovelist();

			ImGui::TableNextColumn();
			// The dockspace loves to overflow past the table end for some reason so we have to re-substract FrameHeight.
			ImGuiID dockId = ImGui::DockSpace(dockid + 2, ImVec2(0, Size.y - ImGui::GetFrameHeightWithSpacing()));
			RenderMovesetData(dockId);

			ImGui::EndTable();
		}

		RenderStatusBar();
	}
	ImGui::End();
}

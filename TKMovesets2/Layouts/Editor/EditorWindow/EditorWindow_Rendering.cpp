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
#include "EditorGroupedCancels.hpp"
#include "EditorRequirements.hpp"
#include "EditorHitConditions.hpp"
#include "EditorReactions.hpp"
#include "EditorPushback.hpp"
#include "EditorPushbackExtra.hpp"
#include "EditorMoveStartProperty.hpp"
#include "EditorMoveEndProperty.hpp"

// -- Private methods -- //

void EditorWindow::RenderToolBar()
{
	// todo: ImGuiWindowFlags_MenuBar ?
	ImGui::BeginMenuBar();

	ImGuiExtra::HelpMarker(_("edition.window_controls_explanation"));

	ImGui::Separator();

	// Current move 1P
	if (ImGuiExtra::RenderButtonEnabled(_("edition.move_current_1p"), m_loadedMoveset != 0)) {
		m_moveToScrollTo = (int16_t)m_editor->GetCurrentMoveID(0);
		sprintf_s(m_moveToPlayBuf, sizeof(m_moveToPlayBuf), "%d", m_moveToScrollTo);
		OpenFormWindow(EditorWindowType_Move, m_moveToScrollTo);
		m_highlightedMoveId = m_moveToScrollTo;
	}
	// Current move 2P
	if (ImGuiExtra::RenderButtonEnabled(_("edition.move_current_2p"), m_loadedMoveset != 0)) {
		m_moveToScrollTo = (int16_t)m_editor->GetCurrentMoveID(1);
		sprintf_s(m_moveToPlayBuf, sizeof(m_moveToPlayBuf), "%d", m_moveToScrollTo);
		OpenFormWindow(EditorWindowType_Move, m_moveToScrollTo);
		m_highlightedMoveId = m_moveToScrollTo;
	}

	ImGui::Separator();
	
	if (ImGui::BeginMenu(_("edition.create_new")))
	{
		EditorWindowType_ structType = EditorWindowType_Invalid;

		if (ImGui::MenuItem(_("edition.requirement"))) {
			structType = EditorWindowType_Requirement;
		}

		ImGui::Separator();
		if (ImGui::MenuItem(_("edition.move"))) {
			structType = EditorWindowType_Move;
		}
		if (ImGui::MenuItem(_("edition.voiceclip"))) {
			structType = EditorWindowType_Voiceclip;
		}
		if (ImGui::MenuItem(_("edition.cancel_list"))) {
			structType = EditorWindowType_Cancel;
		}
		if (ImGui::MenuItem(_("edition.grouped_cancel_list"))) {
			structType = EditorWindowType_GroupedCancel;
		}
		if (ImGui::MenuItem(_("edition.cancel_extradata"))) {
			structType = EditorWindowType_CancelExtradata;
		}

		ImGui::Separator();
		if (ImGui::MenuItem(_("edition.extra_properties"))) {
			structType = EditorWindowType_Extraproperty;
		}
		if (ImGui::MenuItem(_("edition.move_begin_properties"))) {
			structType = EditorWindowType_MoveBeginProperty;
		}
		if (ImGui::MenuItem(_("edition.move_end_properties"))) {
			structType = EditorWindowType_MoveEndProperty;
		}

		ImGui::Separator();
		if (ImGui::MenuItem(_("edition.hit_conditions"))) {
			structType = EditorWindowType_HitCondition;
		}
		if (ImGui::MenuItem(_("edition.reactions"))) {
			structType = EditorWindowType_Reactions;
		}
		if (ImGui::MenuItem(_("edition.pushback"))) {
			structType = EditorWindowType_Pushback;
		}
		if (ImGui::MenuItem(_("edition.pushback_extra"))) {
			structType = EditorWindowType_PushbackExtradata;
		}

		ImGui::Separator();
		if (ImGui::MenuItem(_("edition.input_sequence"))) {
			structType = EditorWindowType_InputSequence;
		}

		if (structType != EditorWindowType_Invalid) {
			int32_t structId = m_editor->CreateNew(structType);
			if (structId != -1) {
				m_savedLastChange = false;
				m_importNeeded = true;
				OpenFormWindow(structType, structId);

				// Custom pre-creation behaviour implementations
				switch (structType)
				{
				case EditorWindowType_Move:
					m_editor->ReloadDisplayableMoveList();
					FilterMovelist(EditorMovelistFilter_All);
					m_moveToScrollTo = structId;
					m_highlightedMoveId = structId;
					break;
				}
			}
		}

		ImGui::EndMenu();
	}

	ImGui::EndMenuBar();
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

		uint64_t movesetSize;
		const Byte* moveset = m_editor->GetMoveset(movesetSize);

		importerHelper.QueueCharacterImportation(moveset, movesetSize);
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

void EditorWindow::RenderMovesetData()
{
	for (size_t i = 0; i < m_structWindows.size();)
	{
		EditorForm* moveWin = m_structWindows[i];

		if (moveWin->justAppliedChanges)
		{
			moveWin->justAppliedChanges = false;
			m_savedLastChange = false;
			m_importNeeded = true;
		}

		if (moveWin->popen)
		{
			moveWin->Render();
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

	// Movelist. Leave some 80 units of space for buttons & inputs under it
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

				if (move->color != 0)
				{
					ImVec2 drawStart = ImGui::GetWindowPos() + ImGui::GetCursorPos();
					drawStart.y -= ImGui::GetScrollY();
					ImVec2 drawArea = ImVec2(TableSize.x, ImGui::GetTextLineHeight());
					ImDrawList* drawlist = ImGui::GetWindowDrawList();
					drawlist->AddRectFilled(drawStart, drawStart + drawArea, move->color);
					// Draw BG
				}

				ImGui::TextUnformatted(move->moveId_str.c_str());

				ImGui::TableNextColumn();
				ImGui::PushID(move->moveId);
				if (ImGui::Selectable(move->name.c_str(), move->moveId == m_highlightedMoveId)) {
					// If clicked on the move
					m_highlightedMoveId = move->moveId;
					m_moveToPlay = move->moveId;
					sprintf_s(m_moveToPlayBuf, sizeof(m_moveToPlayBuf), "%d", move->moveId);
					OpenFormWindow(EditorWindowType_Move, move->moveId);
				}
				ImGui::PopID();

				if (move->aliasId != 0) {
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(move->alias_str.c_str());
				}

				if (m_moveToScrollTo == move->moveId) {
					ImGui::SetScrollHereY();
					m_moveToScrollTo = -1;
				}
			}

			ImGui::EndTable();
		}
	}

	// Move search / play
	ImVec2 buttonSize = ImVec2(ImGui::GetContentRegionAvail().x / 2 - 5, 0);
	ImGui::PushItemWidth(buttonSize.x);
	if (ImGui::InputTextWithHint("##", _("edition.move_id_hint"), m_moveToPlayBuf, sizeof(m_moveToPlayBuf) - 1, ImGuiInputTextFlags_CharsDecimal)) {
		m_moveToPlay = ValidateMoveId(m_moveToPlayBuf);
		if (m_moveToPlay != -1) {
			m_moveToScrollTo = m_moveToPlay;
			m_highlightedMoveId = m_moveToPlay;
		}
	}

	ImGui::SameLine();
	if (ImGuiExtra::RenderButtonEnabled(_("edition.move_current"), m_loadedMoveset != 0, buttonSize)) {
		m_moveToScrollTo = (int16_t)m_editor->GetCurrentMoveID(importerHelper.currentPlayerId);
		sprintf_s(m_moveToPlayBuf, sizeof(m_moveToPlayBuf), "%d", m_moveToScrollTo);
		OpenFormWindow(EditorWindowType_Move, m_moveToScrollTo);
		m_highlightedMoveId = m_moveToScrollTo;
	}
	ImGui::PopItemWidth();

	// Set player move
	if (ImGuiExtra::RenderButtonEnabled(_("edition.play_move_1p"), m_loadedMoveset != 0 && m_moveToPlay != -1, buttonSize)) {
		m_editor->SetCurrentMove(0, m_loadedMoveset, m_moveToPlay);
	}
	ImGui::SameLine();
	if (ImGuiExtra::RenderButtonEnabled(_("edition.play_move_2p"), m_loadedMoveset != 0 && m_moveToPlay != -1, buttonSize)) {
		m_editor->SetCurrentMove(1, m_loadedMoveset, m_moveToPlay);
	}
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

	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_MenuBar;
	// ImGuiWindowFlags_MenuBar
	if (!m_savedLastChange) {
		windowFlags |= ImGuiWindowFlags_UnsavedDocument;
	}

	if (ImGui::Begin(m_windowTitle.c_str(), &popen, windowFlags))
	{
		m_viewport = ImGui::GetWindowViewport();
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
			m_dockId = ImGui::DockSpace(dockid + 2, ImVec2(0, Size.y - ImGui::GetFrameHeightWithSpacing()));
			RenderMovesetData();

			ImGui::EndTable();
		}

		RenderStatusBar();
	}
	ImGui::End();


	// todo: if unsaved changes, prevent closure, popup to force saving
	if (!popen && !m_savedLastChange)
	{
		//popen = true;
	}
}
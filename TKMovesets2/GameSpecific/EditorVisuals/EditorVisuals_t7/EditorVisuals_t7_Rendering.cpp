#include <ImGui.h>
#include <algorithm>

#include "Localization.hpp"
#include "imgui_extras.hpp"
#include "helpers.hpp"
#include "EditorVisuals_t7.hpp"
#include "Compression.hpp"
#include "GameSharedMem.hpp"

using namespace EditorVisualsT7;

// -- Private methods -- //

void EditorVisuals_T7::FilterMovelistIfMoveNotFound(uint16_t moveToSet)
{
	bool found_in_filter = std::find_if(m_filteredMovelist.begin(), m_filteredMovelist.end(),
		[&moveToSet](const DisplayableMove* m) {
		return m->moveId == moveToSet;
		}
	) != m_filteredMovelist.end();
	if (!found_in_filter) {
		FilterMovelist(EditorMovelistFilter_All);
	};
}

void EditorVisuals_T7::RenderToolBar()
{
	// todo: ImGuiWindowFlags_MenuBar ?
	ImGui::BeginMenuBar();

	ImGuiExtra::HelpMarker(_("edition.window_controls_explanation"));

	ImGui::Separator();

	// Current move 1P

	{
		int32_t moveToSet = -1;
		if (ImGuiExtra::RenderButtonEnabled(_("edition.move_current_1p"), m_loadedMoveset != 0)) {
			moveToSet = (int16_t)m_editor->GetCurrentMoveID(0);
		}
		// Current move 2P
		if (ImGuiExtra::RenderButtonEnabled(_("edition.move_current_2p"), m_loadedMoveset != 0)) {
			moveToSet = (int16_t)m_editor->GetCurrentMoveID(1);
		}

		if (moveToSet != -1)
		{
			m_moveToPlay = moveToSet;
			sprintf_s(m_moveToPlayBuf, sizeof(m_moveToPlayBuf), "%d", moveToSet);
			OpenFormWindow(TEditorWindowType_Move, moveToSet);
			m_highlightedMoveId = moveToSet;
			FilterMovelistIfMoveNotFound((uint16_t)moveToSet);
		}
	}


	ImGui::Separator();

	if (ImGui::BeginMenu(_("edition.create_new")))
	{
		EditorWindowType structType = EditorWindowType_INVALID;
		int listSize = 0;

		if (ImGui::MenuItem(_("edition.requirement"))) {
			structType = TEditorWindowType_Requirement;
		}

		ImGui::Separator();
		if (ImGui::MenuItem(_("edition.move"))) {
			structType = TEditorWindowType_Move;
		}
		if (ImGui::MenuItem(_("edition.voiceclips"))) {
			structType = TEditorWindowType_Voiceclip;
		}
		if (ImGui::MenuItem(_("edition.cancel_list"))) {
			structType = TEditorWindowType_Cancel;
		}
		if (ImGui::MenuItem(_("edition.grouped_cancel_list"))) {
			structType = TEditorWindowType_GroupedCancel;
		}
		if (ImGui::MenuItem(_("edition.cancel_extradata"))) {
			structType = TEditorWindowType_CancelExtradata;
		}

		ImGui::Separator();
		if (ImGui::MenuItem(_("edition.extra_properties"))) {
			structType = TEditorWindowType_Extraproperty;
		}
		if (ImGui::MenuItem(_("edition.move_begin_properties"))) {
			structType = TEditorWindowType_MoveBeginProperty;
		}
		if (ImGui::MenuItem(_("edition.move_end_properties"))) {
			structType = TEditorWindowType_MoveEndProperty;
		}

		ImGui::Separator();
		if (ImGui::MenuItem(_("edition.hit_conditions"))) {
			structType = TEditorWindowType_HitCondition;
		}
		if (ImGui::MenuItem(_("edition.reactions"))) {
			structType = TEditorWindowType_Reactions;
		}
		if (ImGui::MenuItem(_("edition.pushback"))) {
			structType = TEditorWindowType_Pushback;
		}
		if (ImGui::MenuItem(_("edition.pushback_extra"))) {
			// Pushback extra list sizes must be known ahead of time when opening a window
			listSize = 1;
			structType = TEditorWindowType_PushbackExtradata;
		}
		ImGui::Separator();
		if (ImGui::MenuItem(_("edition.projectile"))) {
			structType = TEditorWindowType_Projectile;
		}

		ImGui::Separator();
		if (ImGui::MenuItem(_("edition.input_sequence"))) {
			structType = TEditorWindowType_InputSequence;
		}
		if (ImGui::MenuItem(_("edition.inputs"))) {
			// Input list sizes must be known ahead of time when opening a window
			listSize = 2;
			structType = TEditorWindowType_Input;
		}

		ImGui::Separator();
		if (ImGui::MenuItem(_("edition.throw_camera"))) {
			structType = TEditorWindowType_ThrowCamera;
		}
		if (ImGui::MenuItem(_("edition.camera_data"))) {
			structType = TEditorWindowType_CameraData;
		}

		ImGui::Separator();
		if (ImGui::MenuItem(_("edition.mvl_playable"))) {
			structType = TEditorWindowType_MovelistPlayable;
		}
		if (ImGui::MenuItem(_("edition.mvl_inputs"))) {
			// MVL Input list sizes must be known ahead of time when opening a window
			listSize = 2;
			structType = TEditorWindowType_MovelistInput;
		}


		if (structType != EditorWindowType_INVALID)
		{
			uint32_t structId = m_editor->CreateNew(structType);

			m_lastChangeDate = Helpers::getCurrentTimestamp();
			m_savedLastChange = false;
			m_importNeeded = true;
			OpenFormWindow(structType, structId, listSize);

			// Custom pre-creation behaviour implementations
			switch (structType)
			{
			case TEditorWindowType_Move:
				m_editor->ReloadDisplayableMoveList();
				ReloadMovelistFilter();
				m_moveToScrollTo = structId;
				m_highlightedMoveId = structId;
				FilterMovelistIfMoveNotFound((uint16_t)structId);
				break;
			}
		}

		ImGui::EndMenu();
	}

	ImGui::Separator();
	if (ImGui::MenuItem(_("edition.open_movelist"), nullptr, false, m_editor->hasDisplayableMovelist)) {
		OpenFormWindow(TEditorWindowType_MovelistDisplayable, 0);
	}

	ImGui::Separator();


	{
		const char* updateLabelText;
		if (labels->errored) {
			updateLabelText = "edition.updating_labels_err";
		}
		else if (labels->ongoingQuery) {
			updateLabelText = "edition.updating_labels";
		}
		else if (labels->updated) {
			updateLabelText = "edition.updated_labels";
		}
		else if (labels->startedThread) {
			updateLabelText = "edition.up_to_date_labels";
		}
		else {
			updateLabelText = "edition.update_labels";
		}

		if (ImGui::MenuItem(_(updateLabelText), nullptr, false, !labels->ongoingQuery)) {
			labels->UpdateFromWebsite();
		}
	}

	ImGui::EndMenuBar();
}

void EditorVisuals_T7::RenderStatusBar()
{
	if (ImGuiExtra::RenderButtonEnabled(_(m_savingError ? "edition.saving_error" : "edition.save"), !m_savedLastChange)) {
		Save();
	}

	ImGui::SameLine();

	ImGui::TextUnformatted(_("edition.last_saved"));
	ImGui::SameLine();
	ImGui::TextUnformatted(m_loadedCharacter.lastSavedDate.c_str());
	ImGui::SameLine();

	// Vertical separator
	ImGui::TextUnformatted("|");
	ImGui::SameLine();

	// Game list
	bool busy = m_importerHelper->IsBusy();
	auto currentGame = m_importerHelper->currentGame;

	ImGui::PushItemWidth(100.0f);
	ImGui::PushID(&m_importerHelper); // Have to push an ID here because extraction.select_game would cause a conflict
	uint8_t gameListCount = Games::GetGamesCount();

	{
		ImGuiExtra::DisableBlockIf __(busy);

		if (ImGui::BeginCombo("##", currentGame == nullptr ? _("select_game") : currentGame->name))
		{
			for (uint8_t gameIdx = 0; gameIdx < gameListCount; ++gameIdx)
			{
				auto gameInfo = Games::GetGameInfoFromIndex(gameIdx);

				if (gameInfo->importer != nullptr && gameInfo->SupportsGameImport(m_loadedCharacter.gameId)) {
					if (ImGui::Selectable(gameInfo->name, currentGame == gameInfo, 0, ImVec2(100.0f, 0))) {
						m_importerHelper->SetTargetProcess(gameInfo);
						if (gameInfo->onlineHandler != nullptr) {
							m_sharedMemHelper->SetTargetProcess(gameInfo);
						}
						else {
							m_sharedMemHelper->ResetTargetProcess();
						}
						m_loadedMoveset = 0;
						m_importNeeded = true;
						m_editor->live_loadedMoveset = 0;
					}
				}
			}
			ImGui::EndCombo();
		}
		ImGui::PopID();
	}

	ImGui::SameLine();
	// Process error
	bool isAttached = m_importerHelper->process.IsAttached();
	if (currentGame != nullptr && !isAttached)
	{
		// Short process error message
		ImGuiExtra::RenderTextbox(_("edition.process_err"), TEXTBOX_BORDER_ERROR, TEXTBOX_BORDER_ERROR, 2.0f);
		ImGui::SameLine();
	}

	// Vertical separator
	ImGui::TextUnformatted("|");
	ImGui::SameLine();

	// Player list
	{
		ImGui::SameLine();
		char buf[3] = { '1' + m_importerHelper->currentPlayerId, 'p', '\0' };
		ImGui::PushItemWidth(100.0f);

		uint8_t playerCount = min(2, m_importerHelper->GetCharacterCount());
		if (ImGui::BeginCombo("##", _(buf)))
		{
			size_t currentPlayerId = m_importerHelper->currentPlayerId;
			for (int8_t i = 0; i < playerCount; ++i)
			{
				buf[0] = '1' + i;
				if (ImGui::Selectable(_(buf), currentPlayerId == i, 0, ImVec2(100.0f, 0))) {
					m_editor->currentPlayerId = i;
					m_importerHelper->currentPlayerId = i;
					m_sharedMemHelper->currentPlayerId = i;
				}
			}
			ImGui::EndCombo();
		}
	}

	// Import button
	ImGui::SameLine();
	bool canImport = isAttached && !m_importerHelper->IsBusy() && m_canInteractWithGame && m_importNeeded;
	if (ImGuiExtra::RenderButtonEnabled(_("moveset.import"), canImport)) {
		ImportToPlayer(-2);
	}

	// Live edition. Might not be implemented for every game.
	if (m_liveEditable)
	{
		bool disabled = (m_loadedMoveset == 0);
		{
			ImGuiExtra::DisableBlockIf __(disabled);

			ImGui::SameLine();
			if (ImGui::Checkbox(_("edition.live_edition"), &m_liveEdition) && !disabled) {
				m_editor->live_loadedMoveset = m_liveEdition ? m_loadedMoveset : 0;
			}
			ImGui::SameLine();
		}
		// Highlight only if live edition is enabled & live_moveset is zero
		ImGuiExtra::HelpMarker(_("edition.live_edition_explanation"), m_liveEdition == false || m_editor->live_loadedMoveset != 0);
	}

	ImGui::SameLine();

	ImGui::PushID(&CompressionUtils::GetCompressionSetting(0));
	if (ImGui::BeginCombo("##", m_compressionIndex == 0 ? _("extraction.settings.compression_type.none") : CompressionUtils::GetCompressionSetting(m_compressionIndex).name))
	{
		for (unsigned int i = 0; i < CompressionUtils::GetCompressionSettingCount(); ++i) {
			if (ImGui::Selectable(i == 0 ? _("extraction.settings.compression_type.none") : CompressionUtils::GetCompressionSetting(i).name, i == m_compressionIndex, 0, ImVec2(140.0f, 0))) {
				m_compressionIndex = i;
			}
		}
		ImGui::EndCombo();
	}
	ImGui::PopID();
}

void EditorVisuals_T7::RenderMovelist()
{
	// Filter / Sorting
	{
		ImVec2 buttonSize = { ImGui::GetContentRegionAvail().x / 7, 0 };

		for (unsigned int i = 0; i < m_movelistTabs.size(); ++i)
		{
			if (i != 0) {
				ImGui::SameLine();
			}

			const auto& [label, filter] = m_movelistTabs[i];

			ImGuiExtra::PushStyleColorBlockIf __(m_movelistFilter != filter, ImGuiCol_Button, MOVELIST_TAB_INACTIVE);
			ImGuiExtra::PushStyleColorBlockIf ___(m_movelistFilter == filter, ImGuiCol_Button, MOVELIST_TAB_ACTIVE);
			if (ImGui::Button(label.c_str(), buttonSize)) {
				if (m_movelistFilter != filter) {
					FilterMovelist(filter);
				}
			}
			ImGuiExtra::OnHoverTooltip(label.c_str());
		}
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

			// drawList & windowPos are used to display a different row bg
			ImDrawList* drawlist = ImGui::GetWindowDrawList();
			auto windowPos = ImGui::GetWindowPos();

			for (const auto& move : m_filteredMovelist)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				if (move->color != 0)
				{
					// Draw BG
					ImVec2 drawStart = windowPos + ImGui::GetCursorPos();
					drawStart.y -= ImGui::GetScrollY();
					ImVec2 drawArea = ImVec2(TableSize.x, ImGui::GetTextLineHeight());
					drawlist->AddRectFilled(drawStart, drawStart + drawArea, move->color);
				}

				ImGui::TextUnformatted(move->moveId_str.c_str());

				ImGui::TableNextColumn();
				ImGui::PushID(move->moveId);
				if (ImGui::Selectable(move->name.c_str(), move->moveId == m_highlightedMoveId)) {
					// If clicked on the move
					m_highlightedMoveId = move->moveId;
					m_moveToPlay = move->moveId;
					sprintf_s(m_moveToPlayBuf, sizeof(m_moveToPlayBuf), "%d", move->moveId);
					OpenFormWindow(TEditorWindowType_Move, move->moveId);
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
	if (ImGui::InputTextWithHint("##", _("edition.move_id_hint"), m_moveToPlayBuf, sizeof(m_moveToPlayBuf) - 1)) {
		if (Helpers::is_string_digits(m_moveToPlayBuf)) {
			m_moveToPlay = ValidateMoveId(m_moveToPlayBuf);
			if (m_moveToPlay != -1) {
				FilterMovelistIfMoveNotFound((uint16_t)m_moveToPlay);
			}
			DEBUG_LOG("Search: '%s' is digits only, result = %d.\n", m_moveToPlayBuf, m_moveToPlay);
		}
		else if (m_moveToPlayBuf[0] != '\0') {
			std::string buf = m_moveToPlayBuf;
			std::transform(buf.begin(), buf.end(), buf.begin(), tolower);

			auto iter = std::find_if(m_filteredMovelist.begin(), m_filteredMovelist.end(),
				[&buf](const DisplayableMove* m) {
					std::string moveName = m->name;
					std::transform(moveName.begin(), moveName.end(), moveName.begin(), tolower);
					return moveName.find(buf) != std::string::npos;
				}
			);

			if (iter == m_filteredMovelist.end()) {
				m_moveToPlay = -1;
			}
			else {
				m_moveToPlay = (*iter)->moveId;
			}

			DEBUG_LOG("Search: '%s' is text, result = %d.\n", buf.c_str(), m_moveToPlay);
		}
		else {

			DEBUG_LOG("Search: Empty.\n");
			m_moveToPlay = -1;
		}

		if (m_moveToPlay != -1) {
			m_moveToScrollTo = m_moveToPlay;
			m_highlightedMoveId = m_moveToPlay;
		}
	}

	ImGui::SameLine();
	if (ImGuiExtra::RenderButtonEnabled(_("edition.move_current"), m_loadedMoveset != 0, buttonSize)) {
		m_moveToScrollTo = (int16_t)m_editor->GetCurrentMoveID(m_importerHelper->currentPlayerId);
		m_moveToPlay = m_moveToScrollTo;
		sprintf_s(m_moveToPlayBuf, sizeof(m_moveToPlayBuf), "%d", m_moveToScrollTo);
		OpenFormWindow(TEditorWindowType_Move, m_moveToScrollTo);
		m_highlightedMoveId = m_moveToScrollTo;
		FilterMovelistIfMoveNotFound((uint16_t)m_moveToPlay);
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

void EditorVisuals_T7::RenderGameSpecific(int dockid)
{

	if (m_lastAutoSave < m_lastChangeDate) {
		// Auto-save: A change was made since the last auto save
		if ((Helpers::getCurrentTimestamp() - m_lastAutoSave) >= (60 * EDITOR_BACKUP_FREQUENCY_MINUTES)) {
			// If the change was made 30 minutes ago or more
			SaveBackup();
		}
	}


	// Many windows rely on movelist data : need to update it early on
	if (m_editor->mustReloadMovelist) {
		m_editor->mustReloadMovelist = false;
		ReloadMovelistFilter();
	}

	// Check for important changes here
	m_canInteractWithGame = m_importerHelper->CanStart();

	if (m_loadedMoveset != m_prevLoadedMoveset) {
		// Only set live_loadedMoveset once per import: That way, setting it to zero anywhere will invalidate the live edition and force the player to re-import
		m_editor->live_loadedMoveset = m_loadedMoveset;
		m_prevLoadedMoveset = m_loadedMoveset;
	}

	if (m_loadedMoveset != 0) {
		// If the moveset was successfully imported, this will be filled with a nonzero value
		if (!m_canInteractWithGame || !MovesetStillLoaded())
		{
			// Previously loaded moveset is now unreachable
			m_importNeeded = true;
			m_loadedMoveset = 0;
			m_editor->live_loadedMoveset = 0;
		}
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
		if (m_windowFocused) {
			DetectKeyboardShortcuts();
		}

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
			RenderSubwindows();

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

	// -- Now that rendering is done, we can clean up what needs to be cleaned up -- //
	m_importerHelper->FreeExpiredFactoryClasses();
	m_sharedMemHelper->FreeExpiredFactoryClasses();
}
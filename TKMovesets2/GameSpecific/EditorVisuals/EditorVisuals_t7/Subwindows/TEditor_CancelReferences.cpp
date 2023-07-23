#include <format>
#include <Imgui.h>

#include "EditorVisuals.hpp"
#include "Localization.hpp"
#include "TEditor_CancelReferences.hpp"
#include "imgui_extras.hpp"

// -- Private methods -- //

void TEditor_CancelReferences::LoadReferenceList(const TEditor* editor, unsigned int cancelid)
{
	m_references.moves = editor->ListCancelsMoveReferences(cancelid);
	m_references.projectiles = editor->ListCancelsProjectilesReferences(cancelid);

	for (auto& projectile_ref : m_references.projectiles) {
		projectile_ref.references_count_str = std::format("{} {}", projectile_ref.moves_references.size(), _("references.moves"));
	}

	m_tabs.moves = std::format("{} {}", m_references.moves.size(), _("references.moves"));
	m_tabs.projectiles = std::format("{} {}", m_references.projectiles.size(), _("references.projectiles"));
}


void TEditor_CancelReferences::RenderMovesReferences()
{
	const float details_ratio = 0.65f;

	ImVec2 availableSize = ImGui::GetContentRegionAvail();
	ImVec2 tableSize = availableSize;

	if (m_references.current_move != nullptr) {
		tableSize.y *= (1 - details_ratio);
	}

	if (ImGui::BeginTable("##", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_NoHostExtendY | ImGuiTableFlags_ScrollY, tableSize))
	{
		ImGui::PushID(this);
		ImGui::TableSetupColumn(" ", ImGuiTableColumnFlags_WidthFixed, 20.0f);
		ImGui::TableSetupColumn(_("references.moves"));
		ImGui::TableHeadersRow();


		for (unsigned int i = 0; i < m_references.moves.size(); ++i)
		{
			auto& m = m_references.moves[i];
			bool details_opened = &m == m_references.current_move;

			ImGui::PushID(&m);

			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			/*
			if (!details_opened && ImGui::Button("V")) {
				m_references.current_move = &m;
				m_references.details_title.move = m.name;
			}
			*/

			ImGui::TableNextColumn();
			if (details_opened) {
				ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, MOVELIST_TAB_ACTIVE);
			}
			if (ImGui::Button(m.name.c_str())) {
				m_baseWindow->OpenFormWindow(TEditorWindowType_Move, m.move_id);
			}

			ImGui::PopID();
		}
		ImGui::PopID();
		ImGui::EndTable();
	}

	/*
	if (m_references.current_move != nullptr)
	{
		ImVec2 cursor = ImGui::GetCursorPos();

		ImVec2 detailsSize = { availableSize.x - 20, (availableSize.y * details_ratio) - 10.0f };
		if (ImGui::BeginTable("##", 1, ImGuiTableFlags_NoHostExtendY | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg, detailsSize))
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();

			ImGui::TextUnformatted(m_references.details_title.move.c_str());
			ImGui::SameLine();
			ImGui::TextUnformatted(_("references.used_by"));

			ImGui::PopID();

			ImGui::EndTable();
		}

		// Render close button
		{
			ImVec2 new_cursor = { cursor.x + availableSize.x - 15, cursor.y };
			ImGui::SetCursorPos(new_cursor);
			if (ImGui::Button("X")) {
				m_references.current_move = nullptr;
			}
		}
	}
	*/
}

void TEditor_CancelReferences::RenderProjectilesReferences()
{
	const float details_ratio = 0.65f;

	ImVec2 availableSize = ImGui::GetContentRegionAvail();
	ImVec2 tableSize = availableSize;

	if (m_references.current_projectile != nullptr) {
		tableSize.y *= (1 - details_ratio);
	}

	if (ImGui::BeginTable("##", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_NoHostExtendY | ImGuiTableFlags_ScrollY, tableSize))
	{
		ImGui::PushID(this);
		ImGui::TableSetupColumn(" ", ImGuiTableColumnFlags_WidthFixed, 20.0f);
		ImGui::TableSetupColumn(_("references.used_by"), 0, 100.0f);
		ImGui::TableSetupColumn(_("references.projectiles"));
		ImGui::TableHeadersRow();


		for (unsigned int i = 0; i < m_references.projectiles.size(); ++i)
		{
			auto& m = m_references.projectiles[i];
			bool details_opened = &m == m_references.current_projectile;

			ImGui::PushID(&m);

			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			if (!details_opened && ImGui::Button("V")) {
				m_references.current_projectile = &m;
				m_references.details_title.projectile = std::to_string(m.id);
			}

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(m.references_count_str.c_str());

			ImGui::TableNextColumn();
			if (details_opened) {
				ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, MOVELIST_TAB_ACTIVE);
			}
			if (ImGui::Button(m.id_str.c_str())) {
				m_baseWindow->OpenFormWindow(TEditorWindowType_Projectile, m.id);
			}

			ImGui::PopID();
		}
		ImGui::PopID();
		ImGui::EndTable();
	}

	if (m_references.current_projectile != nullptr)
	{
		ImVec2 cursor = ImGui::GetCursorPos();

		ImVec2 detailsSize = { availableSize.x - 20, (availableSize.y * details_ratio) - 10.0f };
		if (ImGui::BeginTable("##", 1, ImGuiTableFlags_NoHostExtendY | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg, detailsSize))
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();

			ImGui::TextUnformatted(m_references.details_title.projectile.c_str());
			ImGui::SameLine();
			ImGui::TextUnformatted(_("references.used_by"));

			if (m_references.current_projectile->moves_references.size() == 0) {
				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				ImGui::TextUnformatted(_("references.no_reference"));
			}

			ImGui::PushID(m_references.current_projectile);
			for (const auto& move_ref : m_references.current_projectile->moves_references)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				ImGui::PushID(move_ref.move_id);
				if (ImGui::Button(_("references.move.view_move"))) {
					m_baseWindow->OpenFormWindow(TEditorWindowType_Move, move_ref.move_id);
				}
				ImGui::PopID();
				ImGui::SameLine();
				ImGui::TextUnformatted(move_ref.name.c_str());
			}
			ImGui::PopID();

			ImGui::EndTable();
		}

		// Render close button
		{
			ImVec2 new_cursor = { cursor.x + availableSize.x - 15, cursor.y };
			ImGui::SetCursorPos(new_cursor);
			if (ImGui::Button("X")) {
				m_references.current_projectile = nullptr;
			}
		}
	}
}

// -- Public methods -- //

TEditor_CancelReferences::TEditor_CancelReferences(const TEditor* editor, EditorVisuals* baseWindow, unsigned int cancelid) : m_editor(editor), m_baseWindow(baseWindow), m_cancelid(cancelid)
{
	m_identifier = std::format("{} {}###{}", _("references.cancel.title"), cancelid, (uint64_t)this);
	identifier = TEditor_CancelReferences::identifier_prefix + std::to_string(cancelid);
	LoadReferenceList(m_editor, m_cancelid);
}

bool TEditor_CancelReferences::Render()
{
	if (nextDockId != -1) {
		ImGui::SetNextWindowDockID(nextDockId);
		nextDockId = -1;
	}

	if (m_firstRender) {
		ImGui::SetNextWindowFocus();
	}

	bool popen = true;
	ImGui::SetNextWindowSizeConstraints(ImVec2(720, 640), ImVec2(9999, 9999));
	if (ImGui::Begin(m_identifier.c_str(), &popen))
	{
		if (ImGui::BeginTabBar("CancelReferenceTabs"))
		{
			bool focused_tab = false;

			{
				bool is_empty = (m_references.moves.size() == 0);
				ImGuiExtra::DisableBlockIf __(is_empty);
				ImGuiTabItemFlags flags;

				if (m_firstRender && !is_empty) {
					flags = ImGuiTabItemFlags_SetSelected;
					focused_tab = true;
				}
				else {
					flags = ImGuiTabItemFlags_None;
				}

				if (ImGui::BeginTabItem(m_tabs.moves.c_str(), nullptr, flags)) {
					RenderMovesReferences();
					ImGui::EndTabItem();
				}
			}

			{
				bool is_empty = (m_references.projectiles.size() == 0);
				ImGuiExtra::DisableBlockIf __(is_empty);
				ImGuiTabItemFlags flags;

				if (m_firstRender && !focused_tab && !is_empty) {
					flags = ImGuiTabItemFlags_SetSelected;
					focused_tab = true;
				}
				else {
					flags = ImGuiTabItemFlags_None;
				}

				if (ImGui::BeginTabItem(m_tabs.projectiles.c_str(), nullptr, flags)) {
					RenderProjectilesReferences();
					ImGui::EndTabItem();
				}
			}

			ImGui::EndTabBar();
		}
	}
	ImGui::End();

	if (m_firstRender) {
		m_firstRender = false;
	}

	return popen;
}

void TEditor_CancelReferences::Refresh(ImGuiID t_nextDockId)
{
	nextDockId = (int)t_nextDockId;
	LoadReferenceList(m_editor, m_cancelid);
	m_firstRender = true;
}
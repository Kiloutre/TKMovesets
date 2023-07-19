#include <format>
#include <Imgui.h>

#include "EditorVisuals.hpp"
#include "Localization.hpp"
#include "TEditor_MoveReferences.hpp"
#include "imgui_extras.hpp"

// -- Private methods -- //

void TEditor_MoveReferences::LoadReferenceList(const TEditor* editor, unsigned int moveid)
{
	m_references.cancels = editor->ListMoveCancelsReferences(moveid);
	for (auto& cancel : m_references.cancels)
	{
		auto& conditions_str = cancel.conditions_str;
		for (auto& req : cancel.conditions)
		{
			if (conditions_str.size() > 0) {
				conditions_str += ", ";
			}
			const char* label = m_baseWindow->labels->GetText(req.id);
			conditions_str += label ? label : std::to_string(req.id);
			conditions_str += ": " + std::to_string((signed)req.value);
		}
	}

	m_references.grouped_cancels = editor->ListMoveGroupedCancelsReferences(moveid);
	for (auto& cancel : m_references.grouped_cancels)
	{
		auto& conditions_str = cancel.conditions_str;
		for (auto& req : cancel.conditions)
		{
			if (conditions_str.size() > 0) {
				conditions_str += ", ";
			}

			const char* label = m_baseWindow->labels->GetText(req.id);
			conditions_str = label ? label : std::to_string(req.id);
			conditions_str += ": " + std::to_string((signed)req.value);
		}
	}

	m_references.reactions = editor->ListMoveReactionsReferences(moveid);
	for (auto& reactions : m_references.reactions)
	{
		auto& situations_str = reactions.situations_str;
		for (auto& situation_idx : reactions.situation_ids)
		{
			if (situations_str.size() > 0) {
				situations_str += ", ";
			}

			situations_str += _(m_reactionLabels[situation_idx].c_str());
		}
		reactions.references_count_str += std::format(" {}", _("references.moves"));
	}

	m_tabs.cancels = std::format("{} {}", m_references.cancels.size(), _("references.cancels"));
	m_tabs.grouped_cancels = std::format("{} {}", m_references.grouped_cancels.size(), _("references.grouped_cancels"));
	m_tabs.reactions = std::format("{} {}", m_references.reactions.size(), _("references.reactions"));
}


void TEditor_MoveReferences::RenderCancelsReferences()
{
	const float details_ratio = 0.65f;

	ImVec2 availableSize = ImGui::GetContentRegionAvail();
	ImVec2 tableSize = availableSize;

	if (m_references.current_cancel != nullptr) {
		tableSize.y *= (1 - details_ratio);
	}

	if (ImGui::BeginTable("##", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_NoHostExtendY | ImGuiTableFlags_ScrollY, tableSize))
	{
		ImGui::PushID(this);
		ImGui::TableSetupColumn(" ", ImGuiTableColumnFlags_WidthFixed, 20.0f);
		ImGui::TableSetupColumn(_("references.list_start"), ImGuiTableColumnFlags_WidthFixed, 130.0f);
		ImGui::TableSetupColumn(_("edition.cancel.detection_start"), ImGuiTableColumnFlags_WidthFixed, 50.0f);
		ImGui::TableSetupColumn(_("edition.cancel.detection_end"), ImGuiTableColumnFlags_WidthFixed, 50.0f);
		ImGui::TableSetupColumn(_("edition.cancel.starting_frame"), ImGuiTableColumnFlags_WidthFixed, 50.0f);
		ImGui::TableSetupColumn(_("references.cancels.requirements"));
		ImGui::TableHeadersRow();

		
		for (unsigned int i = 0; i < m_references.cancels.size(); ++i)
		{
			auto& c = m_references.cancels[i];
			bool details_opened = &c == m_references.current_cancel;

			ImGui::PushID(&c);

			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			if (!details_opened && ImGui::Button("V")) {
				m_references.current_cancel = &c;
				m_references.details_title.cancel = std::format("{}:", c.list_start_id_str);
			}

			ImGui::TableNextColumn();
			if (details_opened) {
				ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, MOVELIST_TAB_ACTIVE);
			}
			if (ImGui::Button(c.list_start_id_str.c_str())) {
				m_baseWindow->OpenFormWindow(TEditorWindowType_Cancel, c.list_start_id);
			}
			ImGui::SameLine();
			ImGui::TextUnformatted(c.command_str.c_str());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(c.detection_start.c_str());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(c.detection_end.c_str());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(c.starting_frame.c_str());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(c.conditions_str.c_str());
			ImGuiExtra::OnHoverTooltip(c.conditions_str.c_str());

			ImGui::PopID();
		}
		ImGui::PopID();
		ImGui::EndTable();
	}

	if (m_references.current_cancel != nullptr)
	{
		ImVec2 cursor = ImGui::GetCursorPos();

		ImVec2 detailsSize = { availableSize.x - 20, (availableSize.y * details_ratio) - 10.0f };
		if (ImGui::BeginTable("##", 1, ImGuiTableFlags_NoHostExtendY | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg, detailsSize))
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();

			ImGui::TextUnformatted(m_references.details_title.cancel.c_str());
			ImGui::SameLine();
			ImGui::TextUnformatted(_("references.used_by"));

			if (m_references.current_cancel->move_references.size() == 0) {
				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				ImGui::TextUnformatted(_("references.no_reference"));
			}

			ImGui::PushID(m_references.current_cancel);
			for (const auto& move_ref : m_references.current_cancel->move_references)
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
				m_references.current_cancel = nullptr;
			}
		}
	}
}


void TEditor_MoveReferences::RenderGroupedCancelsReferences()
{
	const float details_ratio = 0.65f;

	ImVec2 availableSize = ImGui::GetContentRegionAvail();
	ImVec2 tableSize = availableSize;

	if (m_references.current_grouped_cancel != nullptr) {
		tableSize.y *= (1 - details_ratio);
	}

	if (ImGui::BeginTable("##", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_NoHostExtendY | ImGuiTableFlags_ScrollY, tableSize))
	{
		ImGui::PushID(this);
		ImGui::TableSetupColumn(" ", ImGuiTableColumnFlags_WidthFixed, 20.0f);
		ImGui::TableSetupColumn(_("references.list_start"), ImGuiTableColumnFlags_WidthFixed, 130.0f);
		ImGui::TableSetupColumn(_("edition.cancel.detection_start"), ImGuiTableColumnFlags_WidthFixed, 50.0f);
		ImGui::TableSetupColumn(_("edition.cancel.detection_end"), ImGuiTableColumnFlags_WidthFixed, 50.0f);
		ImGui::TableSetupColumn(_("edition.cancel.starting_frame"), ImGuiTableColumnFlags_WidthFixed, 50.0f);
		ImGui::TableSetupColumn(_("references.cancels.requirements"));
		ImGui::TableHeadersRow();

		for (unsigned int i = 0; i < m_references.grouped_cancels.size(); ++i)
		{
			auto& c = m_references.grouped_cancels[i];
			bool details_opened = &c == m_references.current_grouped_cancel;

			ImGui::PushID(&c);

			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			if (!details_opened && ImGui::Button("V")) {
				m_references.current_grouped_cancel = &c;
				m_references.details_title.grouped_cancel = std::format(" {}:", c.list_start_id_str);
			}

			ImGui::TableNextColumn();
			if (details_opened) {
				ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, MOVELIST_TAB_ACTIVE);
			}
			if (ImGui::Button(c.list_start_id_str.c_str())) {
				m_baseWindow->OpenFormWindow(TEditorWindowType_GroupedCancel, c.list_start_id);
			}
			ImGui::SameLine();
			ImGui::TextUnformatted(c.command_str.c_str());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(c.detection_start.c_str());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(c.detection_end.c_str());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(c.starting_frame.c_str());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(c.conditions_str.c_str());

			ImGui::PopID();
		}
		ImGui::PopID();
		ImGui::EndTable();
	}

	if (m_references.current_grouped_cancel != nullptr)
	{
		ImVec2 cursor = ImGui::GetCursorPos();

		ImVec2 detailsSize = { availableSize.x - 20, (availableSize.y * details_ratio) - 10.0f };
		if (ImGui::BeginTable("##", 1, ImGuiTableFlags_NoHostExtendY | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg, detailsSize))
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();

			ImGui::TextUnformatted(m_references.details_title.grouped_cancel.c_str());
			ImGui::SameLine();
			ImGui::TextUnformatted(_("references.used_by"));

			if (m_references.current_grouped_cancel->move_references.size() == 0 && m_references.current_grouped_cancel->cancel_references.size() == 0) {
				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				ImGui::TextUnformatted(_("references.no_reference"));
			}

			ImGui::PushID(m_references.current_cancel);

			for (const auto& move_ref : m_references.current_grouped_cancel->move_references)
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

			for (const auto& cancel_ref : m_references.current_grouped_cancel->cancel_references)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				ImGui::PushID(cancel_ref.list_start_id);
				if (ImGui::Button(_("references.cancel.view_cancel"))) {
					m_baseWindow->OpenFormWindow(TEditorWindowType_Cancel, cancel_ref.list_start_id);
				}

				ImGui::PopID();
				ImGui::SameLine();
				ImGui::TextUnformatted(cancel_ref.list_start_id_str.c_str());
			}


			ImGui::PopID();
			ImGui::EndTable();
		}

		// Render close button
		{
			ImVec2 new_cursor = { cursor.x + availableSize.x - 15, cursor.y };
			ImGui::SetCursorPos(new_cursor);
			if (ImGui::Button("X")) {
				m_references.current_grouped_cancel = nullptr;
			}
		}
	}
}


void TEditor_MoveReferences::RenderReactionsReferences()
{
	const float details_ratio = 0.65f;

	ImVec2 availableSize = ImGui::GetContentRegionAvail();
	ImVec2 tableSize = availableSize;

	if (m_references.current_reactions != nullptr) {
		tableSize.y *= (1 - details_ratio);
	}

	if (ImGui::BeginTable("##", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_NoHostExtendY | ImGuiTableFlags_ScrollY, tableSize))
	{
		ImGui::PushID(this);
		ImGui::TableSetupColumn(" ", ImGuiTableColumnFlags_WidthFixed, 20.0f);
		ImGui::TableSetupColumn(_("references.reactions"), ImGuiTableColumnFlags_WidthFixed, 100.0f);
		ImGui::TableSetupColumn(_("references.used_by"), ImGuiTableColumnFlags_WidthFixed, 100.0f);
		ImGui::TableSetupColumn(_("references.reactions.situations"));
		ImGui::TableHeadersRow();

		for (unsigned int i = 0; i < m_references.reactions.size(); ++i)
		{
			auto& r = m_references.reactions[i];
			bool details_opened = m_references.current_reactions == &r;

			ImGui::PushID(&r);

			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			if (!details_opened && ImGui::Button("V")) {
				m_references.current_reactions = &r;
				m_references.details_title.reactions = std::format(" {} {}:", _("references.reactions"), r.id_str);
			}

			ImGui::TableNextColumn();
			if (ImGui::Button(r.id_str.c_str())) {
				m_baseWindow->OpenFormWindow(TEditorWindowType_Reactions, r.id);
			}
			if (details_opened) {
				ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, MOVELIST_TAB_ACTIVE);
			}

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(r.references_count_str.c_str());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(r.situations_str.c_str());
			ImGuiExtra::OnHoverTooltip(r.situations_str.c_str());

			ImGui::PopID();
		}
		ImGui::PopID();
		ImGui::EndTable();
	}

	if (m_references.current_reactions != nullptr)
	{
		ImVec2 cursor = ImGui::GetCursorPos();

		ImVec2 detailsSize = { availableSize.x - 20, (availableSize.y * details_ratio) - 10.0f };
		if (ImGui::BeginTable("##", 1, ImGuiTableFlags_NoHostExtendY | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg, detailsSize))
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();

			ImGui::TextUnformatted(m_references.details_title.reactions.c_str());
			ImGui::SameLine();
			ImGui::TextUnformatted(_("references.used_by"));

			if (m_references.current_reactions->references.size() == 0) {
				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				ImGui::TextUnformatted(_("references.no_reference"));
			}

			ImGui::PushID(m_references.current_reactions);
			for (const auto& move_ref : m_references.current_reactions->references)
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
				m_references.current_reactions = nullptr;
			}
		}
	}
}

// -- Public methods -- //

TEditor_MoveReferences::TEditor_MoveReferences(const TEditor* editor, EditorVisuals* baseWindow, unsigned int moveid) : m_editor(editor), m_baseWindow(baseWindow), m_moveid(moveid)
{
	m_identifier = std::format("{} {} - {}###{}", _("references.move.title"), moveid, editor->GetMoveName(moveid), (uint64_t)this);
	identifier = TEditor_MoveReferences::identifier_prefix + std::to_string(moveid);
	LoadReferenceList(m_editor, m_moveid);
}

bool TEditor_MoveReferences::Render()
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
		if (ImGui::BeginTabBar("MovelistTabs"))
		{
			bool focused_tab = false;

			{
				bool is_empty = (m_references.cancels.size() == 0);
				ImGuiExtra::DisableBlockIf __(is_empty);
				ImGuiTabItemFlags flags;

				if (m_firstRender && !is_empty) {
					flags = ImGuiTabItemFlags_SetSelected;
					focused_tab = true;
				}
				else {
					flags = ImGuiTabItemFlags_None;
				}

				if (ImGui::BeginTabItem(m_tabs.cancels.c_str(), nullptr, flags)) {
					RenderCancelsReferences();
					ImGui::EndTabItem();
				}
			}

			{
				bool is_empty = (m_references.grouped_cancels.size() == 0);
				ImGuiExtra::DisableBlockIf __(is_empty);
				ImGuiTabItemFlags flags;

				if (m_firstRender && !focused_tab && !is_empty) {
					flags = ImGuiTabItemFlags_SetSelected;
					focused_tab = true;
				}
				else {
					flags = ImGuiTabItemFlags_None;
				}

				if (ImGui::BeginTabItem(m_tabs.grouped_cancels.c_str(), nullptr, flags)) {
					RenderGroupedCancelsReferences();
					ImGui::EndTabItem();
				}
			}

			{
				bool is_empty = (m_references.reactions.size() == 0);
				ImGuiExtra::DisableBlockIf __(is_empty);
				ImGuiTabItemFlags flags;

				if (m_firstRender && !focused_tab && !is_empty) {
					flags = ImGuiTabItemFlags_SetSelected;
					focused_tab = true;
				}
				else {
					flags = ImGuiTabItemFlags_None;
				}

				if (ImGui::BeginTabItem(m_tabs.reactions.c_str(), nullptr, flags)) {
					RenderReactionsReferences();
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

void TEditor_MoveReferences::Refresh(ImGuiID t_nextDockId)
{
	nextDockId = (int)t_nextDockId;
	LoadReferenceList(m_editor, m_moveid);
	m_firstRender = true;
}
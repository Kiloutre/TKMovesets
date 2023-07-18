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

	m_tabs.cancels = std::format("{} {}", m_references.cancels.size(), _("move_references.cancels"));
	m_tabs.grouped_cancels = std::format("{} {}", m_references.grouped_cancels.size(), _("move_references.grouped_cancels"));
	m_tabs.reactions = std::format("{} {}", m_references.reactions.size(), _("move_references.reactions"));
}


void TEditor_MoveReferences::RenderCancelsReferences()
{
	const char* cancel_id = _("moveset.size_kb");
	const char* animation_list__import = _("edition.animation_list.import");

	if (ImGui::BeginTable("##", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoHostExtendY, ImGui::GetContentRegionAvail()))
	{
		ImGui::TableSetupColumn(_("move_references.cancel_list_start"), ImGuiTableColumnFlags_WidthFixed, 130.0f);
		ImGui::TableSetupColumn(_("edition.cancel.detection_start"), ImGuiTableColumnFlags_WidthFixed, 50.0f);
		ImGui::TableSetupColumn(_("edition.cancel.detection_end"), ImGuiTableColumnFlags_WidthFixed, 50.0f);
		ImGui::TableSetupColumn(_("edition.cancel.starting_frame"), ImGuiTableColumnFlags_WidthFixed, 50.0f);
		ImGui::TableSetupColumn(_("move_references.cancel_conditions"));
		ImGui::TableHeadersRow();

		for (const auto& c : m_references.cancels)
		{
			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(c.list_start_id_str.c_str());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(c.detection_start.c_str());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(c.detection_end.c_str());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(c.starting_frame.c_str());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(c.conditions_str.c_str());
			ImGuiExtra::OnHoverTooltip(c.conditions_str.c_str());
		}
		ImGui::EndTable();
	}
}


void TEditor_MoveReferences::RenderGroupedCancelsReferences()
{
	const char* cancel_id = _("moveset.size_kb");
	const char* animation_list__import = _("edition.animation_list.import");

	if (ImGui::BeginTable("##", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoHostExtendY, ImGui::GetContentRegionAvail()))
	{
		ImGui::TableSetupColumn(_("move_references.cancel_list_start"), ImGuiTableColumnFlags_WidthFixed, 130.0f);
		ImGui::TableSetupColumn(_("edition.cancel.detection_start"), ImGuiTableColumnFlags_WidthFixed, 50.0f);
		ImGui::TableSetupColumn(_("edition.cancel.detection_end"), ImGuiTableColumnFlags_WidthFixed, 50.0f);
		ImGui::TableSetupColumn(_("edition.cancel.starting_frame"), ImGuiTableColumnFlags_WidthFixed, 50.0f);
		ImGui::TableSetupColumn(_("move_references.cancel_conditions"));
		ImGui::TableHeadersRow();

		for (const auto& c : m_references.grouped_cancels)
		{
			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(c.list_start_id_str.c_str());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(c.detection_start.c_str());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(c.detection_end.c_str());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(c.starting_frame.c_str());

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(c.conditions_str.c_str());
		}
		ImGui::EndTable();
	}
}


void TEditor_MoveReferences::RenderReactionsReferences()
{

}

// -- Public methods -- //

TEditor_MoveReferences::TEditor_MoveReferences(const TEditor* editor, EditorVisuals* baseWindow, unsigned int moveid) : m_editor(editor), m_baseWindow(baseWindow), m_moveid(moveid)
{
	m_identifier = std::format("{} {}###{}", _("move_references.title"), moveid, (uint64_t)this);
	identifier = "move_reference_" + std::to_string(moveid);
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
					DEBUG_LOG("Set selected cancels\n");
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
					DEBUG_LOG("Set selected grouped\n");
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
					DEBUG_LOG("Set selected reactions\n");
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
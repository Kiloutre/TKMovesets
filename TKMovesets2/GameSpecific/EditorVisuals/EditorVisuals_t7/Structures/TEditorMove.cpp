#include <format>

#include "TEditorStructures.hpp"
#include "Localization.hpp"
#include "constants.h"
#include "EditorVisuals_t7.hpp"
#include "imgui_extras.hpp"
#include "EditorVisuals_t7/Subwindows/TEditor_MoveReferences.hpp"

void TEditorMove::OnInitEnd()
{
	auto baseWindow = BaseWindow<EditorVisuals_T7>();

	// Build initial transition label
	{
		auto& field = m_fieldIdentifierMap["transition"];
		int moveId = baseWindow->ValidateMoveId(field->buffer);
		if (moveId != -1)
		{
			const char* moveName = baseWindow->movelist->at(moveId)->name.c_str();
			EditorFormUtils::SetFieldDisplayText(field, std::format("{} : {}", _(field->fullName.c_str()), moveName));
		}
	}

	// Only allow deletion of custom moves to avoid people breaking movesets by accident
	// Deletion of moves is hardly an important thing anyway
	if (strncmp(m_fieldIdentifierMap["move_name"]->buffer, "c_", 2) != 0) {
		m_isDeletable = false;
	}
}

void TEditorMove::RenderInternal()
{
	auto editor = Editor<EditorT7>();
	if (m_cancelGraphOpen) {
		m_ignoreRightClick = true;
		const uint16_t root = static_cast<uint16_t>(structureId);
		auto* baseWindow = m_baseWindow;
		const CancelGraphView::Callbacks callbacks{
			.build = [editor, root](int depth, bool incoming, bool hideCommon) { return editor->BuildCancelGraph(root, depth, incoming, hideCommon); },
			.animation = [editor](const CancelGraph::Node& node) {
				AnimationPreview::Input input;
				const uint16_t id = static_cast<uint16_t>(node.moveId);
				input.token = editor->MoveAnimationToken(id);
				input.load = [editor, id] { return editor->MoveAnimation(id); };
				return input;
			},
			.open = [baseWindow](uint32_t moveId) { baseWindow->OpenFormWindow(TEditorWindowType_Move, static_cast<uint16_t>(moveId)); },
		};
		m_cancelGraphOpen = m_cancelGraph.Render(callbacks, m_baseWindow);
		return;
	}
	// Values typed in the form show at once (before Apply); unparsable fields count as 0.
	auto number = [&](const char* name) -> int {
		auto found = m_fieldIdentifierMap.find(name);
		if (found == m_fieldIdentifierMap.end() || found->second->errored) return 0;
		return static_cast<int>(strtoll(found->second->buffer, nullptr, 0));
	};
	AnimationPreview::Input input;
	const uint16_t moveId = static_cast<uint16_t>(structureId);
	input.token = editor->MoveAnimationToken(moveId);
	input.load = [editor, moveId] { return editor->MoveAnimation(moveId); };
	const int first = number("first_active_frame"), last = number("last_active_frame");
	if (first > 0 && last >= first) input.active.emplace_back(first, last);
	input.moveFrames = number("anim_len");
	input.displacements = editor->MoveDisplacements(moveId);
	// Opponent mannequin: the reactions of the move's likely hit, and any move's animation (idle, reactions, pose).
	input.opponent.reactions = editor->MoveHitReactions(moveId, false);
	input.opponent.airReactions = editor->MoveHitReactions(moveId, true);
	input.opponent.move = [editor](int id) -> AnimationPreview::Input::Animation {
		const int32_t resolved = id < 0 ? -1 : editor->ResolveMoveId(static_cast<uint32_t>(id));
		if (resolved < 0) return {};
		const uint16_t move = static_cast<uint16_t>(resolved);
		return {editor->MoveAnimationToken(move), [editor, move] { return editor->MoveAnimation(move); }};
	};
	// T7 hitbox_location: two hit lines, low and high 16 bits, each limb A (byte 0) and limb B (byte 1), live on the
	// move's active frames.
	if (first > 0 && last >= first) {
		const uint32_t location = static_cast<uint32_t>(strtoull(m_fieldIdentifierMap["hitbox_location"]->buffer, nullptr, 16));
		for (uint32_t line : {location & 0xFFFF, location >> 16})
			if (line) input.hitboxes.push_back({first, last, line, 0, 0});
	}
	// Wide form with the preview shown: the fields on the left half and the preview on the right half, filling the
	// height (the fields scroll on their own). Otherwise the preview is stacked above the fields. It is shown and
	// hidden with the icon next to the Animation button (RenderLabel).
	if (m_preview.IsOpen() && AnimationPreview::SideBySide()) {
		const float height = (std::max)(1.0f, ImGui::GetContentRegionAvail().y - 30.0f);
		const float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2;
		RenderFieldsInChild("##moveFields", ImVec2(half, height));
		if (m_preview.IsOpen()) {
			ImGui::SameLine();
			ImGui::BeginChild("##movePreview", ImVec2(0, height), 0, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
			m_preview.Body(input, ImGui::GetContentRegionAvail().y);
			ImGui::EndChild();
		}
	}
	else {
		m_preview.Render(input);
		EditorForm::RenderInternal();
	}
	m_ignoreRightClick = m_preview.IsOpen() && m_preview.ViewportHovered();
}

void TEditorMove::RenderLabel(int listIdx, EditorInput* field)
{
	if (field->name == "cancel_addr") {
		// The Cancel button, with the cancel graph's icon at its right
		const float icon = ImGui::GetFrameHeight(), spacing = ImGui::GetStyle().ItemSpacing.x / 2;
		const char* label = _(field->displayName.c_str());
		ImGui::PushStyleColor(ImGuiCol_Header, FORM_LABEL_CLICKABLE);
		if (ImGui::Selectable(label, true, 0, ImVec2((std::max)(1.0f, ImGui::GetContentRegionAvail().x - icon - spacing), 0)))
			OnFieldLabelClick(listIdx, field);
		ImGui::PopStyleColor();
		ImGui::SameLine(0, spacing);
		if (CancelGraphView::IconButton("##cancelGraph")) {
			m_cancelGraph.Invalidate();
			m_cancelGraphOpen = true;
		}
		return;
	}
	if (!(field->name == "anim_name")) { EditorForm::RenderLabel(listIdx, field); return; }
	// The Animation button, with the preview's show / hide icon at its right.
	const float icon = AnimationPreview::ToggleButtonWidth(), spacing = ImGui::GetStyle().ItemSpacing.x / 2;
	const char* label = _(field->displayName.c_str());
	ImGui::PushStyleColor(ImGuiCol_Header, FORM_LABEL_CLICKABLE);
	if (ImGui::Selectable(label, true, 0, ImVec2((std::max)(1.0f, ImGui::GetContentRegionAvail().x - icon - spacing), 0)))
		OnFieldLabelClick(listIdx, field);
	ImGui::PopStyleColor();
	ImGui::SameLine(0, spacing);
	m_preview.ToggleButton();
}

void TEditorMove::PrepareDuplication(unsigned int moveId, unsigned int listSize)
{
	auto baseWindow = BaseWindow<EditorVisuals_T7>();
	baseWindow->OnMoveCreate(moveId);
}

void TEditorMove::ApplyWindowName()
{
	std::string windowName = _(std::format("edition.{}.window_name", m_windowTypeName).c_str());
	const char* moveName = m_fieldIdentifierMap["move_name"]->buffer;
	m_windowTitle = std::format("{} {} {} - {}###{}", windowName, structureId, moveName, m_windowTitleBase.c_str(), (uint64_t)this);
}

void TEditorMove::OnFieldLabelClick(int listIdx, EditorInput* field)
{
	auto baseWindow = BaseWindow<EditorVisuals_T7>();

	int id = atoi(field->buffer);
	auto& name = field->name;

	if (name.startsWith("cancel_addr")) {
		if (id >= 0) {
			m_baseWindow->OpenFormWindow(TEditorWindowType_Cancel, id);
		}
	}
	else if (name == "transition") {
		// Validation is only needed here for alias conversion
		baseWindow->OpenFormWindow(TEditorWindowType_Move, baseWindow->ValidateMoveId(field->buffer));
	}
	else if (name == "voiceclip_addr") {
		if (id >= 0) {
			m_baseWindow->OpenFormWindow(TEditorWindowType_Voiceclip, id);
		}
	}
	else if (name == "hit_condition_addr") {
		if (id >= 0) {
			m_baseWindow->OpenFormWindow(TEditorWindowType_HitCondition, id);
		}
	}
	else if (name == "extra_properties_addr") {
		if (id >= 0) {
			m_baseWindow->OpenFormWindow(TEditorWindowType_Extraproperty, id);
		}
	}
	else if (name == "move_start_extraprop_addr") {
		if (id >= 0) {
			m_baseWindow->OpenFormWindow(TEditorWindowType_MoveBeginProperty, id);
		}
	}
	else if (name == "move_end_extraprop_addr") {
		if (id >= 0) {
			m_baseWindow->OpenFormWindow(TEditorWindowType_MoveEndProperty, id);
		}
	}
	else if (name == "anim_name")
	{
		OpenAnimationList();
	}
}

void TEditorMove::OnApply()
{
	auto editor = Editor<EditorT7>();

	editor->RecomputeDisplayableMoveFlags(structureId);
	m_cancelGraph.Invalidate();
	// Mutation commit requests the next-frame filter refresh; do not publish working-row pointers here.
	if (m_renamed) {
		ApplyWindowName();
		m_renamed = false;
	}
}

void TEditorMove::RequestFieldUpdate(EditorWindowType winType, int valueChange, int listStart, int listEnd)
{
	m_cancelGraph.Invalidate();
	if ((winType & (TEditorWindowType_HitCondition | TEditorWindowType_Extraproperty | TEditorWindowType_Cancel
		| TEditorWindowType_MoveBeginProperty | TEditorWindowType_MoveEndProperty | TEditorWindowType_Voiceclip)) == 0) {
		return;
	}
	std::string fieldName;

	switch (winType)
	{
	case TEditorWindowType_HitCondition:
		fieldName = "hit_condition_addr";
		break;
	case TEditorWindowType_Extraproperty:
		fieldName = "extra_properties_addr";
		break;
	case TEditorWindowType_Cancel:
		fieldName = "cancel_addr";
		break;
	case TEditorWindowType_MoveBeginProperty:
		fieldName = "move_start_extraprop_addr";
		break;
	case TEditorWindowType_MoveEndProperty:
		fieldName = "move_end_extraprop_addr";
		break;
	case TEditorWindowType_Voiceclip:
		fieldName = "voiceclip_addr";
		break;
	default:
		return;
		break;
	}

	if (!m_fieldIdentifierMap[fieldName]->errored) {
		int currentId = atoi(m_fieldIdentifierMap[fieldName]->buffer);
		DEBUG_LOG("EditorMove::RequestFieldUpdate %u 22 - curr val is %d\n", winType, currentId);
		DEBUG_LOG("%d %d %d %d\n", currentId, valueChange, listStart, listEnd);
		if (MUST_SHIFT_ID(currentId, valueChange, listStart, listEnd)) {
			// Same shifting logic as in ListCreations
            auto& field = m_fieldIdentifierMap[fieldName];
			sprintf_s(field->buffer, field->bufsize, "%d", currentId + valueChange);
			DEBUG_LOG("Move: Shifting [%s] by %d\n", fieldName.c_str(), valueChange);
		}
	}
}

void TEditorMove::OnUpdate(int listIdx, EditorInput* field)
{
	auto baseWindow = BaseWindow<EditorVisuals_T7>();

	if (field->name == "transition")
	{
		int moveId = baseWindow->ValidateMoveId(field->buffer);
		const char* moveName = baseWindow->movelist->at(moveId)->name.c_str();
		EditorFormUtils::SetFieldDisplayText(field, std::format("{} : {}", _(field->fullName.c_str()), moveName));
	}
	else if (field->name == "move_name") {
		m_renamed = true;
	}

	m_editor->Live_OnFieldEdit(windowType, structureId + listIdx, field);
}

void TEditorMove::OpenAnimationList()
{
	m_animationListOpen = true;
	m_animationList = new TEditorMove_Animations;
}

void TEditorMove::PostRender()
{
	ImGui::PushID(structureId);

	if (m_animationListOpen) {
		ImGui::OpenPopup("AnimListPopup");

		bool animationListOpen = false;

		if (m_animationList->Render())
		{
			const wchar_t* animPath = m_animationList->animationToImport;
			if (animPath != nullptr) {
				// User clicked on "Import" button on an anim
				std::string animName = m_editor->ImportAnimation(animPath, structureId);

				// InmportAnimation() might return an empty name on error
				if (!animName.empty()) {
					// Update our own field
					auto& animNameField = m_fieldIdentifierMap["anim_name"];
					sprintf_s(animNameField->buffer, animNameField->bufsize, "%s", animName.c_str());
					animNameField->errored = false;
					// Mark changes as applied to tell the editor window to highlight the 'Save' button.
					// But don't update m_unsavedChanges because these changes do not require the use of the Apply() button
					justAppliedChanges = true;
				}
			}
			else {
				animationListOpen = true;
			}
		}
		
		if (!animationListOpen)
		{
			// Close the animation list now that it is not needed
			m_animationListOpen = false;
			delete m_animationList;
			m_animationList = nullptr;
		}
	}

	ImGui::PopID();
}

void TEditorMove::RenderExtraContextMenuItems()
{
	ImVec2 selectableSize(0, ImGui::GetFrameHeightWithSpacing());

	if (ImGui::Selectable(_("edition.form_popup.what_references_this_move"), false, 0, selectableSize))
	{
		std::string identifier = TEditor_MoveReferences::identifier_prefix + std::to_string(structureId);
		if (!m_baseWindow->RefreshSubwindowIfExists(identifier)) {
			m_baseWindow->AddSubwindow(new TEditor_MoveReferences((TEditor*)m_editor, m_baseWindow, structureId));
		}
	}
}
struct TEditorMove::RenameState final : EditorForm::TransactionState {
    std::unique_ptr<TransactionState> base;
    TEditorMove& form;
    bool renamed, committed = false;
    RenameState(TEditorMove& target, size_t& budget)
        : base(target.EditorForm::BeginTransaction(budget)), form(target), renamed(target.m_renamed) {}
    ~RenameState() override { if (!committed) form.m_renamed = renamed; }
    void Commit() noexcept override { base->Commit(); committed = true; }
};
std::unique_ptr<EditorForm::TransactionState> TEditorMove::BeginTransaction(size_t& budget) {
    return std::make_unique<RenameState>(*this, budget);
}

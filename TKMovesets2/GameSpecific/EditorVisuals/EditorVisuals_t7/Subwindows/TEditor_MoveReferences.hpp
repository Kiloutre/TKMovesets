#pragma once

#include "TEditor.hpp"
#include "EditorSubwindow.hpp"

class EditorVisuals;

// Calls with Render() method, displaying a method that lists references to a specific move
class TEditor_MoveReferences : public EditorSubwindow
{
private:
	EditorVisuals* m_baseWindow;

	// Reaction labels
	const std::vector<std::string> m_reactionLabels = {
		"edition.reactions.vertical_pushback",
		"edition.reactions.standing_moveId",
		"edition.reactions.default_moveid",
		"edition.reactions.crouch_moveid",
		"edition.reactions.counterhit_moveid",
		"edition.reactions.crouch_counterhit_moveid",
		"edition.reactions.left_side_moveid",
		"edition.reactions.crouch_left_side_moveid",
		"edition.reactions.right_side_moveid",
		"edition.reactions.crouch_right_side_moveid",
		"edition.reactions.backturned_moveid",
		"edition.reactions.crouch_backturned_moveid",
		"edition.reactions.block_moveid",
		"edition.reactions.crouch_block_moveid",
		"edition.reactions.wallslump_moveid",
		"edition.reactions.downed_moveid"
	};

	// Moveid to search
	unsigned int m_moveid;
	// Editor reference
	const TEditor* m_editor;

	// References lists
	struct {
		std::vector<TEditor::MoveCancelReference> cancels;
		std::vector<TEditor::MoveCancelReference> grouped_cancels;
		std::vector<TEditor::MoveReactionsReference> reactions;
	} m_references;

	struct {
		std::string cancels;
		std::string grouped_cancels;
		std::string reactions;
	} m_tabs;
	// Unique identifier of the popup
	std::string m_identifier;
	// Used to set focus to the window when it is first rendered
	bool m_firstRender = true;

	// Load the list of references in our list
	void LoadReferenceList(const TEditor* editor, unsigned int moveid);

	// Render references
	void RenderCancelsReferences();
	void RenderGroupedCancelsReferences();
	void RenderReactionsReferences();
public:
	TEditor_MoveReferences(const TEditor* editor, EditorVisuals* baseWindow, unsigned int moveid);
	static constexpr const char* identifier_prefix = "move_reference_";

	// Refresh the subwindow
	void Refresh(ImGuiID nextDockID) override;
	// Render method, returns false if the window is closed (class must be freed by parent window)
	bool Render() override;
};
#pragma once

#include "TEditor.hpp"
#include "EditorSubwindow.hpp"

class EditorVisuals;

// Calls with Render() method, displaying a method that lists references to a specific move
class TEditor_CancelReferences : public EditorSubwindow
{
private:
	EditorVisuals* m_baseWindow;

	// Cancel ID to search (will be treated as the start of the list)
	unsigned int m_cancelid;
	// Editor reference
	const TEditor* m_editor;

	// References lists
	struct {
		std::vector<TEditor::CancelMoveReference> moves;
		std::vector<TEditor::CancelProjectileReference> projectiles;

		TEditor::CancelMoveReference* current_move = nullptr;
		TEditor::CancelProjectileReference* current_projectile = nullptr;

		struct {
			std::string move;
			std::string projectile;
		} details_title;
	} m_references;

	struct {
		std::string moves;
		std::string projectiles;
	} m_tabs;
	// Unique identifier of the popup
	std::string m_identifier;
	// Used to set focus to the window when it is first rendered
	bool m_firstRender = true;

	// Load the list of references in our list
	void LoadReferenceList(const TEditor* editor, unsigned int moveid);

	// Render references
	void RenderProjectilesReferences();
	void RenderMovesReferences();
public:
	TEditor_CancelReferences(const TEditor* editor, EditorVisuals* baseWindow, unsigned int moveid);
	static constexpr const char* identifier_prefix = "cancel_reference_";

	// Refresh the subwindow
	void Refresh(ImGuiID nextDockID) override;
	// Render method, returns false if the window is closed (class must be freed by parent window)
	bool Render() override;
};
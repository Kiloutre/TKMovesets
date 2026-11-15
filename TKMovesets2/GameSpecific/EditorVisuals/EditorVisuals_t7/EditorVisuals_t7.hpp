#pragma once

#include "EditorVisuals.hpp"
#include "Editors/Editor_t7/Editor_t7.hpp"

namespace EditorVisualsT7
{
	enum EditorMovelistFilter_
	{
		EditorMovelistFilter_All,
		EditorMovelistFilter_Attacks,
		EditorMovelistFilter_Generic,
		EditorMovelistFilter_ThrowCameras,
		EditorMovelistFilter_Custom,
		EditorMovelistFilter_PostIdle,
	};
};

class EditorVisuals_T7 : public EditorVisuals
{
	struct s_movelistTabs {
		std::string label;
		EditorVisualsT7::EditorMovelistFilter_ filter;
	};

private:
    struct ViewState;
    std::unique_ptr<EditorForm::TransactionState> BeginWindowTransaction() override;
	// Buffer containing the move to play text
	char m_moveToPlayBuf[32]{ 0 };
	// Store the move to play as a int32_t, or -1 if the move id is invalid (checks are made against the moveset data)
	int32_t m_moveToPlay = -1;
	// Contains the movelist displayed at all times, may get sorted and/or filtered
	std::vector<DisplayableMove*> m_filteredMovelist;
	// Moves of m_filteredMovelist whose name matches the text typed in the move search box (Helpers::name_matches_search),
	// rebuilt when the text or m_filteredMovelist changes. A move id (digits) or an empty box does not filter.
	std::vector<DisplayableMove*> m_searchedMovelist, m_searchSource;
	std::string m_searchQuery;
	const std::vector<DisplayableMove*>& VisibleMovelist();
	// Contains the current display filter of the displayed movelist
	EditorVisualsT7::EditorMovelistFilter_ m_movelistFilter = EditorVisualsT7::EditorMovelistFilter_All;
	// The move to scroll to in the next frame
	int32_t m_moveToScrollTo = -1;
	// id of the highlithed move in the movelist
	int16_t m_highlightedMoveId = -1;
	// Frames left during which the move list takes the focus back (after it opened a move, which focuses its form).
	int m_movelistKeepFocus = 0;
	// Index typed in the toolbar's Go to menu
	int m_gotoId = 0;
	// Access moveset data through this variable. Uses polymorphism.
	TEditor* m_editor = nullptr;
	// Ask about unsaved edits on the next frame (closing the window with unsaved or unapplied edits)
	bool m_closePrompt = false;
	// List of tabs to display in the move list
	std::vector<s_movelistTabs> m_movelistTabs{
		{"edition.moves_all", EditorVisualsT7::EditorMovelistFilter_All},
		{"edition.moves_attacks", EditorVisualsT7::EditorMovelistFilter_Attacks},
		{"edition.moves_generic", EditorVisualsT7::EditorMovelistFilter_Generic},
		{"edition.moves_throws", EditorVisualsT7::EditorMovelistFilter_ThrowCameras},
		{"edition.moves_char_specific", EditorVisualsT7::EditorMovelistFilter_PostIdle},
		{"edition.moves_custom", EditorVisualsT7::EditorMovelistFilter_Custom},
	};

	// Render the top toolbar containing useful moveset editing tools
	void RenderToolBar();
	// Render the bottom statusbar that shows the save button and more
	void RenderStatusBar();
	// Render the list of moves
	void RenderMovelist();

	// Populate .m_windowCreatorMap so that windows may get created by type. Can have different contents depending on what the gmae allows.
	void PopulateWindowCreatorMap() override;

	// Called when a specific shortcut is detected
	void OnKeyboardShortcut(const std::string& identifier) override;

	// Imports a moveset to a specific player. -1 for both players, -2 for the currently selected player
	void ImportToPlayer(int playerid);

	// If wanting to scorll to a specific move, this function will reset the filter to ALL if needed, and do nothing if not
	void FilterMovelistIfMoveNotFound(uint16_t moveToSet);
	// Filters and sort the movelist according to the given argument
	void FilterMovelist(EditorVisualsT7::EditorMovelistFilter_ filter);
	// Returns true if our allocated moveset is still loaded on our character, in-game
	bool MovesetStillLoaded();
public:
	// Highlights a move in the move list and scrolls the list to it (captures).
	void ShowMoveInList(uint16_t moveId) { m_highlightedMoveId = static_cast<int16_t>(moveId); m_moveToScrollTo = moveId; }
	// Contains the movelist displayed at all times
	std::vector<DisplayableMove*>* movelist = nullptr;

	// Constructor that loads the moveset
	EditorVisuals_T7(const movesetInfo* movesetInfo, GameAddressesFile* addrFile, LocalStorage* storage);
	// Render the window
	void RenderGameSpecific(int dockid) override;

	// Called when a move is created, is used in order to refresh the movelist and scroll to the move inside of it
	void OnMoveCreate(unsigned int moveId);
	// Validates the move ID against the movelist size and alias list
	int32_t ValidateMoveId(const char* buf);
	// Reloads the movelist filter, used mostly when a move is renamed
	void ReloadMovelistFilter();
};
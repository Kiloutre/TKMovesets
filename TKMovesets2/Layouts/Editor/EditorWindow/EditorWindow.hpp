#pragma once

#include "GameImport.hpp"
#include "LocalStorage.hpp"
#include "Editor.hpp"
#include "EditorForm.hpp"
#include "EditorWindowBase.hpp"

#include "GameAddresses.h"

enum EditorMovelistFilter_
{
	EditorMovelistFilter_All,
	EditorMovelistFilter_Attacks,
	EditorMovelistFilter_Generic,
	EditorMovelistFilter_Throws,
	EditorMovelistFilter_Custom,
	EditorMovelistFilter_PostIdle,
};

struct EditorMovelist_Move
{
	std::string name;
	uint16_t aliasId;
	uint16_t flags;
};

struct EditorInfos
{
	std::string filename;
	std::string name;
	uint64_t lastSavedDate;
	int32_t gameId;
};

class EditorWindow_MovesetLoadFail : public std::exception
{
public:
	const char* what() {
		return "Error while loading moveset";
	}
};

class EditorWindow : public EditorWindowBase {
private:
	// True if the game and characters are loaded and we can start interacting with it
	bool m_canInteractWithGame = true;
	// Contains basic informations about the currently loaded character
	EditorInfos m_loadedCharacter = { "", "", 0, -1 };
	// True if we need to enable the save button
	bool m_savedLastChange = true;
	// True if moveset changes are to be applied to the game instantly
	bool m_liveEdition = false;
	// Stores whether or not importation is required (if live edition is on, importation is not always needed)
	bool m_importNeeded = true;
	// If the moveset can be live edited or not
	bool m_liveEditable = true;
	// Stores the address, in-game, of the moveset we loaded. 0 if none loaded.
	gameAddr m_loadedMoveset = 0;
	// Buffer containing the move to play text
	char m_moveToPlayBuf[7]{ 0 };
	// Store the move to play as a int32_t, or -1 if the move id is invalid (checks are made against the moveset data)
	int32_t m_moveToPlay = -1;
	// Access moveset data through this variable. Uses polymorphism.
	Editor* m_editor = nullptr;
	// Contains the movelist displayed at all times, may get sorted and/or filtered
	std::vector<DisplayableMove*> m_filteredMovelist;
	// Contains the current display filter of the displayed movelist
	EditorMovelistFilter_ m_movelistFilter = EditorMovelistFilter_All;
	// The move to scroll to in the next frame
	int32_t m_moveToScrollTo = -1;
	// Contains the window title, used to prefix any window created by this editor
	std::string m_windowTitle;
	// id of the highlithed move in the movelist
	int16_t m_highlightedMoveId = -1;
	// Store the list of move windows to render
	std::vector<EditorForm*> m_structWindows;
	// The dock id that new windows might dock on
	ImGuiID m_dockId;
	// The viewport of the editor window. Used to know which subwindow is docked or not.
	ImGuiViewport* m_viewport = nullptr;


	// Render the top toolbar containing useful moveset editing tools
	void RenderToolBar();
	// Render the moveset data in the main dockspace of the window
	void RenderMovesetData();
	// Render the bottom statusbar that shows the save button and more
	void RenderStatusBar();
	// Render the list of moves
	void RenderMovelist();

	// Factory instantiating the right class for the right identifier
	EditorForm* AllocateFormWindow(EditorWindowType_ windowType, uint16_t id);

	// Filters and sort the movelist according to the given argument
	void FilterMovelist(EditorMovelistFilter_ filter);
	// Returns true if our allocated moveset is still loaded on our character
	bool MovesetStillLoaded();
	// Save the loaded moveset to a file
	void Save();
public:
	// Determines if the main window is open or not. If not, this tells the MainWindow parent class to free the ressources we have allocated
	bool popen = true;
	// Store our own copy of the importer to not interfere with the other one. Not important but less prone to problems, really.
	GameImport importerHelper;
	// Stores the filename
	std::string filename;
	// True is we need to call SetNextWindowFocus() before rendering our own
	bool setFocus = false;

	// Constructor that loads the moveset and starts its own importer
	EditorWindow(movesetInfo* movesetInfo, GameAddressesFile* addrFile, LocalStorage* storage);
	// Destructor that deallocates the resources we allocated
	~EditorWindow();
	// Render the window
	void Render(int dockid);

	// Create a new window containing data about the given move. Can be called by subwidnows.
	void OpenFormWindow(EditorWindowType_ windowType, uint16_t moveId) override;
	// Validates the move ID against the movelist size and alias list
	int32_t ValidateMoveId(const char* buf) override;
	// Reloads the movelist filter, used mostly when a move is renamed
	void ReloadMovelistFilter() override;
};
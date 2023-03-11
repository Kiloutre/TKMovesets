#pragma once

#include "GameImport.hpp"
#include "LocalStorage.hpp"
#include "Editor.hpp"

#include "GameAddresses.h"

enum EditorMovelistFilter_
{
	EditorMovelistFilter_All = 0,
	EditorMovelistFilter_Attacks = 1,
	EditorMovelistFilter_Generic = 2,
	EditorMovelistFilter_Throws = 3,
	EditorMovelistFilter_Custom = 4,
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
	uint32_t gameId;
};

struct MovesetInfos
{
	std::vector<uint16_t> aliases;
	std::vector<void*> moves;
};

class EditorWindow_MovesetLoadFail : public std::exception
{
public:
	const char* what() {
		return "Error while loading moveset";
	}
};

class EditorWindow {
private:
	// Contains basic informations about the currently loaded character
	EditorInfos m_loadedCharacter;
	// True if we need to enable the save button
	bool m_savedLastChange = true;
	// True if moveset changes are to be applied to the game instantly
	bool m_liveEdition = false;
	// Stores whether or not importation is required (if live edition is on, importation is not always needed)
	bool m_importNeeded = true;
	// Current moveset, will be written to memory on next import
	Byte* m_moveset = nullptr;
	// Size of the moveset currently loaded
	uint64_t m_movesetSize = 0;
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
	// Contains the backup of movelist displayed at all times
	std::vector<DisplayableMove*> m_movelist;
	// Contains the movelist displayed at all times, may get sorted and/or filtered
	std::vector<DisplayableMove*> m_filteredMovelist;
	// Contains the current display filter of the displayed movelist
	EditorMovelistFilter_ m_movelistFilter = EditorMovelistFilter_All;


	// Render the top toolbar containing useful moveset editing tools
	void RenderToolBar();
	// Render the moveset data in the main dockspace of the window
	void RenderMovesetData(ImGuiID dockId);
	// Render the bottom statusbar that shows the save button and more
	void RenderStatusBar();
	// Render the list of moves
	void RenderMovelist();

	void FilterMovelist(EditorMovelistFilter_ filter);
	// Vlidates the move ID against the movelist and alias list
	int16_t ValidateMoveId(const char* buf);
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

	// Constructor that loads the moveset
	EditorWindow(movesetInfo* moveset);
	// Destructor that deallocates the resources we allocated
	~EditorWindow();
	// Render the window
	void Render(int dockid);
};
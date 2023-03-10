#pragma once

#include "GameImport.hpp"
#include "LocalStorage.hpp"

struct EditorInfos
{
	std::string filename;
	std::string name;
	uint64_t lastSavedDate;
	uint32_t gameId;
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
	byte* m_moveset = nullptr;
	// Size of the moveset currently loaded
	uint64_t m_movesetSize = 0;

	// Render the top toolbar containing useful moveset editing tools
	void RenderToolBar();
	// Render the moveset data in the main dockspace of the window
	void RenderMovesetData(ImGuiID dockId);
	// Render the bottom statusbar that shows the save button and more
	void RenderStatusBar();
	// Save the loaded moveset to a file
	void Save();
public:
	// Determines if the main window is open or not. If not, this tells the MainWindow parent class to free the ressources we have allocated
	bool popen = true;
	// Store our own copy of the importer to not interfere with the other one. Not important but less prone to problems, really.
	GameImport importer;
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
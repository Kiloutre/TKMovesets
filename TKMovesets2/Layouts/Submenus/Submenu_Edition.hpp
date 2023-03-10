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

class Submenu_Edition {
private:
	// Contains basic informations about the currently loaded character
	EditorInfos m_loadedCharacter;
	// True if we need to enable the save button
	bool m_savedLastChange = true;
	// Contains the moveset data, allocated within our memory
	void* m_moveset = nullptr;
	// True if moveset changes are to be applied to the game instantly
	bool m_liveEdition = false;
	// Stores whether or not importation is required (if live edition is on, importation is not always needed)
	bool m_importNeeded = true;
	// Determines if the main window is open or not
	bool m_popen = false;

	// Load the moveset basic infos in the editor
	bool LoadMoveset(movesetInfo* moveset);
	// Render the top toolbar containing useful moveset editing tools
	void RenderToolBar();
	// Render the bottom statusbar that shows the save button and more
	void RenderStatusBar();

	// Save the loaded moveset to a file
	void Save();
public:
	// Contains true if a character is currently loaded in the editor
	bool isEditing = false;
	// Store our own copy of the importer to not interfere with the other one. Not important but less prone to problems, really.
	GameImport importer;

	void Render();
	// Render the selector (only does so when editing is not going on)
	void RenderMovesetSelector();
};
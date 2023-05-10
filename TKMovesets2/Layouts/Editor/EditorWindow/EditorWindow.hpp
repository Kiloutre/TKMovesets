#pragma once

#include "GameSharedMem.hpp"
#include "LocalStorage.hpp"
#include "Editor.hpp"
#include "EditorForm.hpp"
#include "EditorWindowBase.hpp"

#include "GameTypes.h"

enum EditorMovelistFilter_
{
	EditorMovelistFilter_All,
	EditorMovelistFilter_Attacks,
	EditorMovelistFilter_Generic,
	EditorMovelistFilter_ThrowCameras,
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
	std::wstring filename;
	std::string name;
	std::string lastSavedDate;
	int32_t gameId;
};

class EditorWindow_MovesetLoadFail : public std::exception
{
public:
	const char* what() {
		return "Error while loading moveset";
	}
};
class EditorWindowFactory_Base
{
public:
	virtual ~EditorWindowFactory_Base() {}
	virtual void* allocate(const std::string& a, EditorWindowType_ b, uint16_t c, Editor* d, EditorWindowBase* e, int f) const = 0;
	virtual void* cast(void* obj) const = 0;
};

template<typename T> class EditorWindowFactory : public EditorWindowFactory_Base
{
public:
	virtual void* allocate(const std::string& a, EditorWindowType_ b, uint16_t c, Editor* d, EditorWindowBase* e, int f) const { return new T(a, b, c, d, e, f); }
	virtual void* cast(void* obj) const { return static_cast<T*>(obj); }
};

class EditorWindow : public EditorWindowBase {
private:
	// True if the game and characters are loaded and we can start interacting with it
	bool m_canInteractWithGame = true;
	// Contains basic informations about the currently loaded character
	EditorInfos m_loadedCharacter = { .filename = L"", .name = "", .lastSavedDate = "", .gameId = -1};
	// True if we need to enable the save button
	bool m_savedLastChange = true;
	// If the moveset can be live edited or not (need to actually code it)
	bool m_liveEditable = true;
	// Stores the address, in-game, of the moveset we loaded. 0 if none loaded.
	gameAddr m_loadedMoveset = 0;
	// Buffer containing the move to play text
	char m_moveToPlayBuf[7]{ 0 };
	// Store the move to play as a int32_t, or -1 if the move id is invalid (checks are made against the moveset data)
	int32_t m_moveToPlay = -1;
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
	// True if moveset changes are to be applied to the game instantly
	bool m_liveEdition = false;
	// Stores whether or not importation is required (if live edition is on, importation is not always needed)
	bool m_importNeeded = true;
	// Access moveset data through this variable. Uses polymorphism.
	Editor* m_editor = nullptr;
	// Store our own copy of the importer to not interfere with the other one. Not important but less prone to problems, really.
	GameImport m_importerHelper;
	// Copy of the shared mem helper, used to play extra propreties
	GameSharedMem m_sharedMemHelper;
	// If true, will compress the moveset file when saving
	bool m_compressOnSave = true;
	// Map that will determine which window type to allocate depending on the given type
	std::map<EditorWindowType_, EditorWindowFactory_Base*> m_windowCreatorMap;


	// Render the top toolbar containing useful moveset editing tools
	void RenderToolBar();
	// Render the moveset data in the main dockspace of the window
	void RenderMovesetData();
	// Render the bottom statusbar that shows the save button and more
	void RenderStatusBar();
	// Render the list of moves
	void RenderMovelist();

	// Populate .m_windowCreatorMap so that windows may get created by type. Can have different contents depending on what the gmae allows.
	void PopulateWindowCreatorMap();
	// Factory instantiating the right class for the right identifier.
	EditorForm* AllocateFormWindow(EditorWindowType_ windowType, uint16_t id, int listSize = 0);

	// Filters and sort the movelist according to the given argument
	void FilterMovelist(EditorMovelistFilter_ filter);
	// Returns true if our allocated moveset is still loaded on our character, in-game
	bool MovesetStillLoaded();
	// Save the loaded moveset to a file
	void Save();
public:
	// Determines if main window is open or not. If not, this tells the MainWindow parent class to destroy this very class
	bool popen = true;
	// True is we need to call SetNextWindowFocus() before rendering our own window
	bool setFocus = false;
	// Expose filename specifically and not the full editorInfos. Used to avoid editing the same moveset twice.
	const wchar_t* filename;

	// Constructor that loads the moveset and starts its own importer
	EditorWindow(movesetInfo* movesetInfo, GameAddressesFile* addrFile, LocalStorage* storage);
	// Destructor that deallocates the resources we allocated
	~EditorWindow();
	// Render the window
	void Render(int dockid);

	// Makes the save button available
	void SetChangesUnsaved() override;
	// Called when a move is created, is used in order to refresh the movelist and scroll to the move inside of it
	void OnMoveCreate(unsigned int moveId) override;
	// Create a new window containing data about the given move. Can be called by subwidnows.
	void OpenFormWindow(EditorWindowType_ windowType, uint16_t structId, int listSize = 0) override;
	// Validates the move ID against the movelist size and alias list
	int32_t ValidateMoveId(const char* buf) override;
	// Reloads the movelist filter, used mostly when a move is renamed
	void ReloadMovelistFilter() override;
	// Issue an integer field update by adding 'valueChange' to the existing field's value (if not errored).
	void IssueFieldUpdate(EditorWindowType_ winType, int valueChange, int listStart=-1, int listEnd = -1) override;
};
#pragma once

#include "LocalStorage.hpp"
#include "EditorLogic.hpp"
#include "EditorForm.hpp"
#include "EditorLabel.hpp"
#include "EditorSubwindow.hpp"

#include "GameTypes.h"

class GameImport;
class GameSharedMem;

struct EditorInfos
{
	std::wstring filename;
	std::wstring wname;
	std::string name;
	std::string lastSavedDate;
	int32_t gameId = -1;
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
	virtual void* allocate(const std::string& a, EditorWindowType b, uint16_t c, EditorLogic* d, EditorVisuals* e, int f, const char* g) const = 0;
	virtual void* cast(void* obj) const = 0;
};

template<typename T> class EditorWindowFactory : public EditorWindowFactory_Base
{
public:
	virtual void* allocate(const std::string& a, EditorWindowType b, uint16_t c, EditorLogic* d, EditorVisuals* e, int f, const char* g) const { return new T(a, b, c, d, e, f, g); }
	virtual void* cast(void* obj) const { return static_cast<T*>(obj); }
};

struct WindowCreator
{
	EditorWindowFactory_Base* allocator;
	const char* typeName;
};

class EditorVisuals
{
protected:
	// True if the game and characters are loaded and we can start interacting with it
	bool m_canInteractWithGame = true;
	// Contains basic informations about the currently loaded character
	EditorInfos m_loadedCharacter;
	// True if we need to enable the save button
	bool m_savedLastChange = true;
	// If the moveset can be live edited or not (need to actually code it)
	bool m_liveEditable = true;
	// Stores the value of m_loadedMoveset on the last render
	gameAddr m_prevLoadedMoveset = 0;
	// Stores the address, in-game, of the moveset we loaded. 0 if none loaded.
	gameAddr m_loadedMoveset = 0;
	// Contains the window title, contains the build version of the program and more info
	std::string m_windowTitle;
	// Contains the window title send to build the subwindows title on
	std::string m_subwindowsTitle;
	// Store the list of structure windows to render
	std::vector<EditorForm*> m_structWindows;
	// List of subwindows to render
	std::vector<EditorSubwindow*> m_subwindows;
	// The dock id that new windows might dock on
	ImGuiID m_dockId;
	// The viewport of the editor window. Used to know which subwindow is docked or not.
	ImGuiViewport* m_viewport = nullptr;
	// True if moveset changes are to be applied to the game instantly
	bool m_liveEdition = false;
	// Stores whether or not importation is required (if live edition is on, importation is not always needed)
	bool m_importNeeded = true;
	// Store our own copy of the importer to not interfere with the other one. Not important but less prone to problems, really.
	GameImport* m_importerHelper;
	// Copy of the shared mem helper, used to play extra propreties
	GameSharedMem* m_sharedMemHelper;
	// Map that will determine which window type to allocate depending on the given type
	std::map<EditorWindowType, WindowCreator> m_windowCreatorMap;
	// Index of the compression setting to use when saving
	unsigned int m_compressionIndex = 0;
	// Contains a ptr to the editor logic class
	EditorLogic* m_abstractEditor = nullptr;
	// Whether to show "Saving error" or not
	bool m_savingError = false;
	// Auto backups
	struct {
		// Timestamp of the last automatic save
		uint64_t m_lastAutoSave;
		// Timestamp o the last change
		uint64_t m_lastChangeDate;
		// Maximum amount of auto-saves
		const unsigned int m_maxAutoSaves = 5;
	};
	// True if the current editor window or its children have focused. Used for proper handling of right click behaviour.
	bool m_windowFocused = false;

	// Render each created subwindows
	void RenderSubwindows();
	// Populate .m_windowCreatorMap so that windows may get created by type. Can have different contents depending on what the gmae allows.
	virtual void PopulateWindowCreatorMap() = 0;
	// Factory instantiating the right class for the right identifier.
	EditorForm* AllocateFormWindow(EditorWindowType windowType, uint16_t id, int listSize = 0);

	// Render the actual content of the window which is game-specific
	virtual void RenderGameSpecific(int dockid) = 0;
	// Detect keyboard shortcuts
	void DetectKeyboardShortcuts();
	virtual void OnKeyboardShortcut(const std::string& identifier) = 0;

	// Save the loaded moveset to a file
	void Save();
	// Save the loaded moveset to a file in the backup folders
	void SaveBackup(bool saveOnlyIfNoneExist=false);
public:
	// Determines if main window is open or not. If not, this tells the MainWindow parent class to destroy this very class
	bool popen = true;
	// True is we need to call SetNextWindowFocus() before rendering our own window
	bool setFocus = false;
	// Expose filename specifically and not the full editorInfos. Used to avoid editing the same moveset twice.
	const wchar_t* filename;
	// Contains the labels to display in the editor
	EditorLabel* labels = nullptr;

	// Constructor that loads the moveset and starts its own importer
	EditorVisuals(const movesetInfo* movesetInfo, GameAddressesFile* addrFile, LocalStorage* storage);
	// Destructor that deallocates the resources we allocated
	virtual ~EditorVisuals();
	// Render the window
	void Render(int dockid);
	
	// Create a new window containing data about the given move. Can be called by subwidnows.
	void OpenFormWindow(EditorWindowType windowType, uint16_t structId, int listSize = 0);
	// Issue an integer field update by adding 'valueChange' to the existing field's value (if not errored).
	void IssueFieldUpdate(EditorWindowType winType, int valueChange, int listStart=-1, int listEnd = -1);
	// Appends a new subwindow to the list of subwindow list to render
	void AddSubwindow(EditorSubwindow* new_win);
	// Reloads a subwindow and returns true if the given identifier exists, otherwise returns false
	bool RefreshSubwindowIfExists(const std::string& identifier);
};
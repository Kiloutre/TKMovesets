#pragma once

#include "GameSharedMem.hpp"
#include "LocalStorage.hpp"
#include "Editor.hpp"
#include "EditorForm.hpp"
#include "EditorLabel.hpp"
#include "EditorVisualsBase.hpp"

#include "GameTypes.h"

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
	virtual void* allocate(const std::string& a, EditorWindowType_ b, uint16_t c, Editor* d, EditorVisualsBase* e, int f) const = 0;
	virtual void* cast(void* obj) const = 0;
};

template<typename T> class EditorWindowFactory : public EditorWindowFactory_Base
{
public:
	virtual void* allocate(const std::string& a, EditorWindowType_ b, uint16_t c, Editor* d, EditorVisualsBase* e, int f) const { return new T(a, b, c, d, e, f); }
	virtual void* cast(void* obj) const { return static_cast<T*>(obj); }
};

class EditorVisuals : public EditorVisualsBase
{
protected:
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
	// Contains the window title, contains the build version of the program and more info
	std::string m_windowTitle;
	// Contains the window title send to build the subwindows title on
	std::string m_subwindowsTitle;
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
	// Map that will determine which window type to allocate depending on the given type
	std::map<EditorWindowType_, EditorWindowFactory_Base*> m_windowCreatorMap;
	// Index of the compression setting to use when saving
	unsigned int m_compressionIndex = 0;
	// Contains a ptr to the editor logic class
	Editor* m_abstractEditor = nullptr;

	// Populate .m_windowCreatorMap so that windows may get created by type. Can have different contents depending on what the gmae allows.
	virtual void PopulateWindowCreatorMap() = 0;
	// Factory instantiating the right class for the right identifier.
	EditorForm* AllocateFormWindow(EditorWindowType_ windowType, uint16_t id, int listSize = 0);

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
	EditorVisuals(const movesetInfo* movesetInfo, GameAddressesFile* addrFile, LocalStorage* storage);
	// Destructor that deallocates the resources we allocated
	virtual ~EditorVisuals();
	// Render the window
	virtual void Render(int dockid) = 0;

	// Create a new window containing data about the given move. Can be called by subwidnows.
	void OpenFormWindow(EditorWindowType_ windowType, uint16_t structId, int listSize = 0) override;
	// Issue an integer field update by adding 'valueChange' to the existing field's value (if not errored).
	void IssueFieldUpdate(EditorWindowType_ winType, int valueChange, int listStart=-1, int listEnd = -1) override;
};
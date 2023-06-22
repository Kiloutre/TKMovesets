#include <ImGui.h>

#include <format>
#include <filesystem>

#include "Compression.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"
#include "helpers.hpp"
#include "EditorVisuals.hpp"
#include "GameSharedMem.hpp"
// Structures

// -- Private methods -- //
EditorForm* EditorVisuals::AllocateFormWindow(EditorWindowType windowType, uint16_t id, int listSize)
{
	auto windowCreator = m_windowCreatorMap.find(windowType);
	if (windowCreator != m_windowCreatorMap.end()) {
		auto typeName = windowCreator->second.typeName;
		auto newWin = (EditorForm*)windowCreator->second.allocator->allocate(m_subwindowsTitle, windowType, id, m_abstractEditor, this, listSize, typeName);
		newWin->OnInitEnd();
		return newWin;
	}

	DEBUG_LOG("ERROR: NO WINDOW FOUND FOR WINDOW TYPE %u\n", windowType);

	return nullptr;
}

void EditorVisuals::OpenFormWindow(EditorWindowType windowType, uint16_t structId, int listSize)
{
	int availableOverwriteIndex = -1;
	for (int i = 0; i < m_structWindows.size(); ++i) {
		EditorForm* structWin = m_structWindows[i];
		if (structWin->windowType != windowType) {
			continue;
		}

		if (structWin->structureId == structId) {
			structWin->setFocus = true;
			// Prevent duplicate move window creation
			return;
		}

		if ((structWin->unsavedChanges == false && structWin->currentViewport == m_viewport)
			|| structWin->uniqueType) {
			// Don't overwrite windows with unsaved changes OR that have been detached
			availableOverwriteIndex = i;
		}
	}

	bool openNew = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
	EditorForm* newWin = AllocateFormWindow(windowType, structId, listSize);
	if (newWin != nullptr)
	{
		newWin->nextDockId = m_dockId;

		if (!openNew && availableOverwriteIndex != -1) {
			// Can't delete here because OpenFormWindow can be called by EditorForm classes
			// Close the window and let deletion occur during next rendering
			m_structWindows[availableOverwriteIndex]->popen = false;
		}

		m_structWindows.push_back(newWin);
	}
}


void EditorVisuals::Save()
{
	uint64_t movesetSize;
	const Byte* moveset = m_abstractEditor->GetMoveset(movesetSize);

	TKMovesetHeader* header = (TKMovesetHeader*)moveset;
	header->crc32 = m_abstractEditor->CalculateCRC32();
	header->date = Helpers::getCurrentTimestamp();

	std::wstring dst_filename = m_loadedCharacter.filename;
	std::wstring tmp_filename = dst_filename;
	tmp_filename.erase(tmp_filename.find_last_of(L"."));
	tmp_filename += L"" MOVESET_TMPFILENAME_EXTENSION;

	CreateDirectoryW(L"" MOVESET_DIRECTORY, nullptr);
	std::ofstream file(tmp_filename, std::ios::binary);
	if (!file.fail()) {
		file.write((char*)moveset, movesetSize);
		file.close();

		bool success = true;

		if (m_compressionIndex != 0) {
			success = CompressionUtils::FILE::Moveset::Compress(dst_filename, tmp_filename, CompressionUtils::GetCompressionSetting(m_compressionIndex).compressionSetting);
		}
		else {
			std::filesystem::rename(tmp_filename, dst_filename);
		}

		if (success) {
			m_savedLastChange = true;
			m_loadedCharacter.lastSavedDate = Helpers::formatDateTime(header->date);
		}
		else {
			DEBUG_LOG("Err: failed to compress file\n");
		}
		m_savingError = success;
	}
	else {
		DEBUG_ERR("Editor: failed to save.");
		m_savingError = true;
	}
}


// -- Public methods -- //

EditorVisuals::~EditorVisuals()
{
	m_importerHelper->StopThreadAndCleanup();
	m_sharedMemHelper->StopThreadAndCleanup();

	delete m_importerHelper;
	delete m_sharedMemHelper;

	for (auto& win : m_structWindows) {
		delete win;
	}

	uint64_t movesetSize;
	Byte* moveset = (Byte*)m_abstractEditor->GetMoveset(movesetSize);
	free(moveset);

	delete m_abstractEditor;
	delete labels;
}

EditorVisuals::EditorVisuals(const movesetInfo* movesetInfo, GameAddressesFile* addrFile, LocalStorage* storage)
{
	{
		// Init classes that interact with game process
		m_importerHelper = new GameImport;
		m_sharedMemHelper = new GameSharedMem;

		m_importerHelper->Init(addrFile, storage);
		m_sharedMemHelper->Init(addrFile, storage);

		m_importerHelper->StartThread();
		m_sharedMemHelper->synchronizeLockin = false;
		m_sharedMemHelper->StartThread();
	}

	{
		// Init editor logic class
		auto gameInfo = Games::GetGameInfoFromIdentifier(movesetInfo->gameId, movesetInfo->minorVersion);

		m_abstractEditor = Games::FactoryGetEditorLogic(gameInfo, m_importerHelper->process, m_importerHelper->game);
		labels = new EditorLabel(gameInfo, addrFile);
	}

	m_liveEditable = Games::IsGameLiveEditable(movesetInfo->gameId, movesetInfo->minorVersion);

	m_loadedCharacter.filename = movesetInfo->filename;
	m_loadedCharacter.wname = movesetInfo->wname;
	m_loadedCharacter.name = movesetInfo->name;
	m_loadedCharacter.lastSavedDate = Helpers::formatDateTime(movesetInfo->date);
	filename = m_loadedCharacter.filename.c_str();
	m_loadedCharacter.gameId = movesetInfo->gameId;

	m_subwindowsTitle = std::format("{}: {}", m_loadedCharacter.name.c_str(), _("edition.window_title"));
	m_windowTitle = std::format("{} {} - Moveset v{}", m_subwindowsTitle, PROGRAM_VERSION, movesetInfo->version_string);

	// Set the shared mem handler
	m_abstractEditor->SetSharedMemHandler(m_sharedMemHelper->GetSharedMemHandler());

	// Attempt to attach to game associated to moveset
	auto processList = GameProcessUtils::GetRunningProcessList();
	for (unsigned int gameIdx = 0; gameIdx < Games::GetGamesCount(); ++gameIdx)
	{
		auto gameInfo = Games::GetGameInfoFromIndex(gameIdx);
		const char* processName = gameInfo->processName;

		// Detect if the game is running
		{
			bool isRunning = false;
			for (auto& process : processList)
			{
				if (process.name == processName)
				{
					isRunning = true;
					break;
				}
			}
			if (!isRunning) {
				continue;
			}
		}

		if (gameInfo->SupportsGameImport(movesetInfo->gameId))
		{
			m_importerHelper->SetTargetProcess(gameInfo);
			if (gameInfo->onlineHandler != nullptr) {
				m_sharedMemHelper->SetTargetProcess(gameInfo);
			}
			else {
				m_sharedMemHelper->ResetTargetProcess();
			}
			DEBUG_LOG("Editor-compatible game '%s' already running: attaching.\n", processName);
			break;
		}
	}
}

void EditorVisuals::IssueFieldUpdate(EditorWindowType winType, int valueChange, int listStart, int listEnd)
{
	DEBUG_LOG("EditorVisuals::IssueFieldUpdate(type: %u, change: %d, list start: %d, list end: %d)\n", winType, valueChange, listStart, listEnd);
	for (auto& window : m_structWindows)
	{
		if (window->popen) {
			window->RequestFieldUpdate(winType, valueChange, listStart, listEnd);
		}
	}
}

void EditorVisuals::RenderSubwindows()
{
	for (size_t i = 0; i < m_structWindows.size();)
	{
		EditorForm* moveWin = m_structWindows[i];

		if (moveWin->justAppliedChanges)
		{
			moveWin->justAppliedChanges = false;
			m_savedLastChange = false;
			m_importNeeded = true;
		}

		if (moveWin->popen)
		{
			moveWin->Render();

			if (moveWin->moveFocusAway) {
				moveWin->moveFocusAway = false;
				ImGui::SetKeyboardFocusHere();
			}

			++i;
		}
		else {
			m_structWindows.erase(m_structWindows.begin() + i);
			delete moveWin;
		}
	}
}
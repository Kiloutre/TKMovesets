#include <ImGui.h>

#include <format>
#include <filesystem>

#include "Compression.hpp"
#include "Localization.hpp"
#include "imgui_extras.hpp"
#include "helpers.hpp"
#include "EditorWindow.hpp"
// Structures
#include "EditorMove.hpp"
#include "EditorVoiceclip.hpp"
#include "EditorExtraproperties.hpp"
#include "EditorCancels.hpp"
#include "EditorCancelExtra.hpp"
#include "EditorGroupedCancels.hpp"
#include "EditorRequirements.hpp"
#include "EditorHitConditions.hpp"
#include "EditorReactions.hpp"
#include "EditorPushback.hpp"
#include "EditorPushbackExtra.hpp"
#include "EditorMoveStartProperty.hpp"
#include "EditorMoveEndProperty.hpp"
#include "EditorInputSequence.hpp"
#include "EditorInputStruct.hpp"
#include "EditorProjectile.hpp"
#include "EditorThrowCamera.hpp"
#include "EditorCameraData.hpp"
#include "EditorMovelistDisplayable.hpp"
#include "EditorMovelistPlayable.hpp"
#include "EditorMovelistInput.hpp"

// -- Private methods -- //

void EditorWindow::PopulateWindowCreatorMap()
{
	m_windowCreatorMap = {
		{EditorWindowType_Move, new EditorWindowFactory<EditorMove> },
		{EditorWindowType_Voiceclip, new EditorWindowFactory<EditorVoiceclip> },

		{EditorWindowType_Extraproperty, new EditorWindowFactory<EditorExtraproperties> },
		{EditorWindowType_MoveBeginProperty, new EditorWindowFactory<EditorMoveStartProperty> },
		{EditorWindowType_MoveEndProperty, new EditorWindowFactory<EditorMoveEndProperty> },

		{EditorWindowType_Requirement, new EditorWindowFactory<EditorRequirements> },

		{EditorWindowType_HitCondition, new EditorWindowFactory<EditorHitConditions> },
		{EditorWindowType_Reactions, new EditorWindowFactory<EditorReactions> },
		{EditorWindowType_Pushback, new EditorWindowFactory<EditorPushback> },
		{EditorWindowType_PushbackExtradata, new EditorWindowFactory<EditorPushbackExtra> },

		{EditorWindowType_Cancel, new EditorWindowFactory<EditorCancels> },
		{EditorWindowType_CancelExtradata, new EditorWindowFactory<EditorCancelExtra> },
		{EditorWindowType_GroupedCancel, new EditorWindowFactory<EditorGroupedCancels> },

		{EditorWindowType_InputSequence, new EditorWindowFactory<EditorInputSequence> },
		{EditorWindowType_Input, new EditorWindowFactory<EditorInputStruct> },

		{EditorWindowType_Projectile, new EditorWindowFactory<EditorProjectile> },

		{EditorWindowType_CameraData, new EditorWindowFactory<EditorCameraData> },
		{EditorWindowType_ThrowCamera, new EditorWindowFactory<EditorThrowCamera> },

		{EditorWindowType_MovelistDisplayable, new EditorWindowFactory<EditorMovelistDisplayable> },
		{EditorWindowType_MovelistPlayable, new EditorWindowFactory<EditorMovelistPlayable> },
		{EditorWindowType_MovelistInput, new EditorWindowFactory<EditorMovelistInput> }
	};
}

EditorForm* EditorWindow::AllocateFormWindow(EditorWindowType_ windowType, uint16_t id, int listSize)
{
	auto windowCreator = m_windowCreatorMap.find(windowType);
	if (windowCreator != m_windowCreatorMap.end()) {
		auto newWin = (EditorForm*)windowCreator->second->allocate(m_subwindowsTitle, windowType, id, m_editor, this, listSize);
		newWin->OnInitEnd();
		return newWin;
	}

	DEBUG_LOG("ERROR: NO WINDOW FOUND FOR WINDOW TYPE %u\n", windowType);

	return nullptr;
}

void EditorWindow::OpenFormWindow(EditorWindowType_ windowType, uint16_t structId, int listSize)
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

void EditorWindow::FilterMovelist(EditorMovelistFilter_ filter)
{
	m_filteredMovelist.clear();

	m_movelistFilter = filter;
	if (filter == EditorMovelistFilter_All) {
		m_filteredMovelist = *movelist;
		return;
	}

	if (filter == EditorMovelistFilter_PostIdle) {
		// Get idle move ID, only list moves beyond it
		size_t startingIndex = editorTable->aliases[1];
		for (; startingIndex < movelist->size(); ++startingIndex) {
			m_filteredMovelist.push_back(movelist->at(startingIndex));
		}
		return;
	}

	EditorMoveFlags flags = 0;

	switch (filter)
	{
	case EditorMovelistFilter_Attacks:
		flags |= EditorMoveFlags_Attack | EditorMoveFlags_OtherAttack;
		break;
	case EditorMovelistFilter_Generic:
		flags |= EditorMoveFlags_Generic;
		break;
	case EditorMovelistFilter_ThrowCameras:
		flags |= EditorMoveFlags_Throw | EditorMoveFlags_ThrowReaction;
		break;
	case EditorMovelistFilter_Custom:
		flags |= EditorMoveFlags_Custom;
		break;
	}

	for (DisplayableMove* move : *movelist)
	{
		if (move->flags & flags) {
			m_filteredMovelist.push_back(move);
		}
	}

	// If displaying generic moves, sort by generic id
	if (filter & EditorMovelistFilter_Generic) {
		struct sortByAlias {
			bool operator()(DisplayableMove const* a, DisplayableMove const* b) const {
				return a->aliasId < b->aliasId;
			}
		};
		std::sort(m_filteredMovelist.begin(), m_filteredMovelist.end(), sortByAlias());
	}
}

int32_t EditorWindow::ValidateMoveId(const char* buf)
{
	int32_t moveId = atoi(buf);

	const size_t movelistSize = movelist->size();
	if (moveId >= movelistSize)
	{
		const size_t aliasesCount = editorTable->aliases.size();
		if (moveId < 0x8000 || moveId >= (0x8000 + aliasesCount)) {
			return -1;
		}
		moveId = editorTable->aliases[(uint16_t)moveId - (uint16_t)0x8000];
	}

	return moveId;
}

void EditorWindow::OnMoveCreate(unsigned int moveId)
{
	m_editor->ReloadDisplayableMoveList();
	ReloadMovelistFilter();
	m_moveToScrollTo = moveId;
	m_highlightedMoveId = moveId;
}

bool EditorWindow::MovesetStillLoaded()
{
	gameAddr movesetAddress = m_importerHelper.GetCurrentPlayerMovesetAddr();
#ifdef BUILD_TYPE_DEBUG
	if (movesetAddress != m_loadedMoveset) {
		DEBUG_LOG("MovesetStillLoaded = false, current is %llx, old was %llx\n", movesetAddress, m_loadedMoveset);
	}
#endif
	return movesetAddress == m_loadedMoveset;
}

void EditorWindow::Save()
{
	uint64_t movesetSize;
	const Byte* moveset = m_editor->GetMoveset(movesetSize);

	TKMovesetHeader* header = (TKMovesetHeader*)moveset;
	header->crc32 = m_editor->CalculateCRC32();
	header->date = Helpers::getCurrentTimestamp();

	std::wstring dst_filename = m_loadedCharacter.filename;
	std::wstring tmp_filename = dst_filename;
	tmp_filename.erase(tmp_filename.find_last_of(L"."));
	tmp_filename += L"" MOVESET_TMPFILENAME_EXTENSION;

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
	}
	else {
		DEBUG_LOG("!! Editor: failed to save. !!\n");
		// Showing failure would be nice. Even simply changing the Save button text would suffice.
		//todo
	}
}


// -- Public methods -- //

EditorWindow::~EditorWindow()
{
	m_importerHelper.StopThreadAndCleanup();
	m_sharedMemHelper.StopThreadAndCleanup();

	for (auto& win : m_structWindows) {
		delete win;
	}

	uint64_t movesetSize;
	Byte* moveset = (Byte*)m_editor->GetMoveset(movesetSize);
	free(moveset);

	delete m_editor;
	delete labels;
}

EditorWindow::EditorWindow(const movesetInfo* movesetInfo, GameAddressesFile* addrFile, LocalStorage* storage)
{
	{
		// Init classes that interact with game process
		m_importerHelper.Init(addrFile, storage);
		m_sharedMemHelper.Init(addrFile, storage);

		m_importerHelper.StartThread();
		m_sharedMemHelper.synchronizeLockin = false;
		m_sharedMemHelper.StartThread();
	}

	{
		// Init editor logic class
		auto gameInfo = Games::GetGameInfoFromIdentifier(movesetInfo->gameId, movesetInfo->minorVersion);

		m_editor = Games::FactoryGetEditor(gameInfo, m_importerHelper.process, m_importerHelper.game);
		labels = new EditorLabel(gameInfo, addrFile);
	}

	Byte* moveset;
	uint64_t movesetSize;
	{
		// Read moveset
		std::ifstream file(movesetInfo->filename.c_str(), std::ios::binary);

		if (file.fail()) {
			throw EditorWindow_MovesetLoadFail();
		}

		file.seekg(0, std::ios::end);
		movesetSize = file.tellg();

		moveset = (Byte*)malloc(movesetSize);
		if (moveset == nullptr) {
			throw EditorWindow_MovesetLoadFail();
		}

		file.seekg(0, std::ios::beg);
		file.read((char*)moveset, movesetSize);

		file.close();
	}

	TKMovesetCompressionType_ compressionType = TKMovesetCompressionType_None;
	
	if (movesetSize >= sizeof(TKMovesetHeader)) {
		compressionType = (TKMovesetCompressionType_)((TKMovesetHeader*)moveset)->compressionType;
	}

	if (!m_editor->LoadMoveset(moveset, movesetSize)) {
		this->popen = false;
	}

	m_liveEditable = Games::IsGameLiveEditable(movesetInfo->gameId, movesetInfo->minorVersion);

	m_loadedCharacter.filename = movesetInfo->filename;
	m_loadedCharacter.name = movesetInfo->name;
	m_loadedCharacter.lastSavedDate = Helpers::formatDateTime(movesetInfo->date);
	filename = m_loadedCharacter.filename.c_str();
	m_loadedCharacter.gameId = movesetInfo->gameId;

	PopulateWindowCreatorMap();

	m_subwindowsTitle = std::format("{}: {}", m_loadedCharacter.name.c_str(), _("edition.window_title"));
	m_windowTitle = std::format("{} {} - Moveset v{}", m_subwindowsTitle, PROGRAM_VERSION, movesetInfo->version_string);

	// Read what needs to be read and potentially displayed right away
	m_editor->ReloadDisplayableMoveList();
	movelist = m_editor->displayableMovelist;
	ReloadMovelistFilter();

	editorTable = &m_editor->movesetTable;

	// Set the shared mem handler
	m_editor->SetSharedMemHandler(m_sharedMemHelper.GetSharedMemHandler());
	
	// Set compression setting as the pre-configured default one
	m_compressionIndex = CompressionUtils::GetCompressionSettingIndex(compressionType);

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
			m_importerHelper.SetTargetProcess(gameInfo);
			if (gameInfo->onlineHandler != nullptr) {
				m_sharedMemHelper.SetTargetProcess(gameInfo);
			}
			else {
				m_sharedMemHelper.ResetTargetProcess();
			}
			DEBUG_LOG("Editor-compatible game '%s' already running: attaching.\n", processName);
			break;
		}
	}
}

void EditorWindow::ReloadMovelistFilter()
{
	FilterMovelist(m_movelistFilter);
}

void EditorWindow::IssueFieldUpdate(EditorWindowType_ winType, int valueChange, int listStart, int listEnd)
{
	DEBUG_LOG("EditorWindow::IssueFieldUpdate(type: %u, change: %d, list start: %d, list end: %d)\n", winType, valueChange, listStart, listEnd);
	for (auto& window : m_structWindows)
	{
		if (window->popen) {
			window->RequestFieldUpdate(winType, valueChange, listStart, listEnd);
		}
	}
}

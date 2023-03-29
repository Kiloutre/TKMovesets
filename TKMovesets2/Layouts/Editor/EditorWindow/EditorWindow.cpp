#include <ImGui.h>

#include <algorithm>
#include <format>

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

// -- Private methods -- //

EditorForm* EditorWindow::AllocateFormWindow(EditorWindowType_ windowType, uint16_t id, int listSize)
{
	switch (windowType)
	{
	case EditorWindowType_Move:
		return new EditorMove(m_windowTitle, id, m_editor, this);
		break;
	case EditorWindowType_Voiceclip:
		return new EditorVoiceclip(m_windowTitle, id, m_editor, this);
		break;
	case EditorWindowType_Extraproperty:
		return new EditorExtraproperties(m_windowTitle, id, m_editor, this);
		break;
	case EditorWindowType_Cancel:
		return new EditorCancels(m_windowTitle, id, m_editor, this);
		break;
	case EditorWindowType_GroupedCancel:
		return new EditorGroupedCancels(m_windowTitle, id, m_editor, this);
		break;
	case EditorWindowType_CancelExtradata:
		return new EditorCancelExtra(m_windowTitle, id, m_editor);
		break;
	case EditorWindowType_Requirement:
		return new EditorRequirements(m_windowTitle, id, m_editor, this);
		break;
	case EditorWindowType_HitCondition:
		return new EditorHitConditions(m_windowTitle, id, m_editor, this);
		break;
	case EditorWindowType_Reactions:
		return new EditorReactions(m_windowTitle, id, m_editor, this);
		break;
	case EditorWindowType_Pushback:
		return new EditorPushback(m_windowTitle, id, m_editor, this);
		break;
	case EditorWindowType_PushbackExtradata:
		return new EditorPushbackExtra(m_windowTitle, id, m_editor, this, listSize);
		break;
	case EditorWindowType_MoveBeginProperty:
		return new EditorMoveStartProperty(m_windowTitle, id, m_editor, this);
		break;
	case EditorWindowType_MoveEndProperty:
		return new EditorMoveEndProperty(m_windowTitle, id, m_editor, this);
		break;
	case EditorWindowType_InputSequence:
		return new EditorInputSequence(m_windowTitle, id, m_editor, this);
		break;
	case EditorWindowType_Input:
		return new EditorInputStruct(m_windowTitle, id, m_editor, this, listSize);
		break;
	case EditorWindowType_Projectile:
		return new EditorProjectile(m_windowTitle, id, m_editor, this);
		break;
	case EditorWindowType_ThrowCamera:
		return new EditorThrowCamera(m_windowTitle, id, m_editor, this);
		break;
	case EditorWindowType_CameraData:
		return new EditorCameraData(m_windowTitle, id, m_editor);
		break;
	}

	return nullptr;
}

void EditorWindow::OpenFormWindow(EditorWindowType_ windowType, uint16_t structId, int listSize)
{
	// todo: template this function?
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

		if (structWin->unsavedChanges == false && structWin->currentViewport == m_viewport) {
			// Don't overwrite windows with unsaved changes OR that have been detached
			availableOverwriteIndex = i;
		}
	}

	bool openNew = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
	EditorForm* newWin = AllocateFormWindow(windowType, structId, listSize);
	newWin->nextDockId = m_dockId;
	if (openNew || availableOverwriteIndex == -1) {
		m_structWindows.push_back(newWin);
	}
	else {
		// Can't delete here because OpenFormWindow can be called by EditorForm classes
		// Close the window and let deletion occur during next rendering
		m_structWindows[availableOverwriteIndex]->popen = false;
		m_structWindows[availableOverwriteIndex] = newWin;
	}
}

void EditorWindow::FilterMovelist(EditorMovelistFilter_ filter)
{
	m_filteredMovelist.clear();

	m_movelistFilter = filter;
	if (filter == EditorMovelistFilter_All) {
		m_filteredMovelist = movelist;
		return;
	}

	if (filter == EditorMovelistFilter_PostIdle) {
		// Get idle move ID, only list moves beyond it
		size_t startingIndex = editorTable->aliases[1];
		for (; startingIndex < movelist.size(); ++startingIndex) {
			m_filteredMovelist.push_back(movelist[startingIndex]);
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

	for (DisplayableMove* move : movelist)
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

	const size_t movelistSize = movelist.size();
	if (moveId >= movelistSize)
	{
		const size_t aliasesCount = editorTable->aliases.size();
		if (moveId < 0x8000 || moveId >= (0x8000 + aliasesCount)) {
			return -1;
		}
		moveId = editorTable->aliases[moveId - (uint16_t)0x8000];
	}

	return moveId;
}

bool EditorWindow::MovesetStillLoaded()
{
	gameAddr movesetAddress = importerHelper.importer->GetMovesetAddress(importerHelper.currentPlayerId);
	return movesetAddress == m_loadedMoveset;
}

void EditorWindow::Save()
{
	uint64_t movesetSize;
	const Byte* moveset = m_editor->GetMoveset(movesetSize);

	TKMovesetHeader* header = (TKMovesetHeader*)moveset;

	Byte* movesetData = (Byte*)moveset + header->infos.header_size;
	auto& offsets = header->offsets;

	std::vector<std::pair<Byte*, uint64_t>> fileBlocks{
		// First block is always ignored in our CRC32 calculation because it is supposed to be our own header
		{nullptr, 0},

		{movesetData + offsets.movesetInfoBlock, offsets.tableBlock - offsets.movesetInfoBlock },
		{movesetData + offsets.tableBlock, offsets.motalistsBlock - offsets.tableBlock },
		{movesetData + offsets.motalistsBlock, offsets.nameBlock - offsets.motalistsBlock },
		{movesetData + offsets.nameBlock, offsets.movesetBlock - offsets.nameBlock },
		{movesetData + offsets.movesetBlock, offsets.animationBlock - offsets.movesetBlock },
		{movesetData + offsets.animationBlock, offsets.motaBlock - offsets.animationBlock },
		{movesetData + offsets.motaBlock, movesetSize - header->infos.header_size - offsets.motaBlock },
	};

	// Because the editor might make corrections to the moveset right as it loads it,
	// ... it is likely the CRC32 will be different from extraction (especially for the name block)

	header->infos.crc32 = Helpers::CalculateCrc32(fileBlocks);
	header->infos.date = duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

	std::ofstream file(m_loadedCharacter.filename, std::ios::binary);
	if (!file.fail()) {
		file.write((char*)moveset, movesetSize);
		m_savedLastChange = true;
		m_loadedCharacter.lastSavedDate = header->infos.date;
	}
	else {
		// Showing failure would be nice. Even simply changing the Save button text would suffice.
	}
}


// -- Public methods -- //

EditorWindow::~EditorWindow()
{
	if (m_editor->animationExtractionStatus != AnimExtractionStatus_NotStarted) {
		// Join the thread if it was ever created. Could be ongoing or finished, we still need to join it.
		m_editor->animExtractionThread->join();
	}

	importerHelper.StopThreadAndCleanup();

	uint64_t movesetSize;
	Byte* moveset = (Byte*)m_editor->GetMoveset(movesetSize);
	free(moveset);

	delete m_editor;
	delete labels;
}

EditorWindow::EditorWindow(movesetInfo* movesetInfo, GameAddressesFile *addrFile, LocalStorage *storage)
{
	importerHelper.Init(addrFile, storage);
	importerHelper.StartThread();

	m_editor = Games::FactoryGetEditor(movesetInfo->gameId, importerHelper.process, importerHelper.game);
	labels = new EditorLabel(movesetInfo->gameId);

	std::ifstream file(movesetInfo->filename.c_str(), std::ios::binary);

	if (file.fail()) {
		throw EditorWindow_MovesetLoadFail();
	}

	file.seekg(0, std::ios::end);
	uint64_t movesetSize = file.tellg();

	Byte* moveset = (Byte*)malloc(movesetSize);
	if (moveset == nullptr) {
		throw EditorWindow_MovesetLoadFail();
	}

	file.seekg(0, std::ios::beg);
	file.read((char*)moveset, movesetSize);

	file.close();

	m_editor->LoadMoveset(moveset, movesetSize);
	m_liveEditable = Games::IsGameLiveEditable(movesetInfo->gameId);

	m_loadedCharacter.filename = movesetInfo->filename;
	m_loadedCharacter.name = movesetInfo->name;
	m_loadedCharacter.lastSavedDate = movesetInfo->date;
	m_loadedCharacter.gameId = movesetInfo->gameId;
	filename = movesetInfo->filename;

	m_windowTitle = std::format("{}: {}", m_loadedCharacter.name.c_str(), _("edition.window_title"));

	// Read what needs to be read and potentially displayed right away
	m_editor->ReloadDisplayableMoveList(&movelist);
	m_filteredMovelist = movelist;

	editorTable = &m_editor->movesetTable;
}

void EditorWindow::ReloadMovelistFilter()
{
	FilterMovelist(m_movelistFilter);
}

void EditorWindow::IssueFieldUpdate(EditorWindowType_ winType, int valueChange, int listStart, int listEnd)
{
	for (auto& window : m_structWindows)
	{
		if (window->popen) {
			window->RequestFieldUpdate(winType, valueChange, listStart, listEnd);
		}
	}
}
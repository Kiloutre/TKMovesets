#include "EditorVisuals_t7.hpp"
#include "GameSharedMem.hpp"
#include "Compression.hpp"
// Structures
#include "Structures/TEditorStructures.hpp"
#include "Structures/DisplayableMovelist/TEditorMovelist.hpp"

using namespace EditorVisualsT7;

// -- Private methods -- //

void EditorVisuals_T7::PopulateWindowCreatorMap()
{
	m_windowCreatorMap = {
		{TEditorWindowType_Move, { new EditorWindowFactory<TEditorMove>, "move" }},

		{TEditorWindowType_Voiceclip, { new EditorWindowFactory<TEditorVoiceclip>, "voiceclip" }},

		{TEditorWindowType_Extraproperty, { new EditorWindowFactory<TEditorExtraproperties>, "extraproperty" }},
		{TEditorWindowType_MoveBeginProperty, { new EditorWindowFactory<TEditorMoveStartProperty>, "move_start_extraprop" }},
		{TEditorWindowType_MoveEndProperty, { new EditorWindowFactory<TEditorMoveEndProperty>, "move_end_extraprop" }},

		{TEditorWindowType_Requirement, { new EditorWindowFactory<TEditorRequirements>, "requirement" }},

		{TEditorWindowType_HitCondition, { new EditorWindowFactory<TEditorHitConditions>, "hit_condition" }},
		{TEditorWindowType_Reactions, { new EditorWindowFactory<TEditorReactions>, "reactions" }},
		{TEditorWindowType_Pushback, { new EditorWindowFactory<TEditorPushback>, "pushback" }},
		{TEditorWindowType_PushbackExtradata, { new EditorWindowFactory<TEditorPushbackExtra>, "pushback_extradata" }},

		{TEditorWindowType_Cancel, { new EditorWindowFactory<TEditorCancels>, "cancel" }},
		{TEditorWindowType_CancelExtradata, { new EditorWindowFactory<EditorForm>, "cancel_extra" }},
		{TEditorWindowType_GroupedCancel, { new EditorWindowFactory<TEditorGroupedCancels>, "grouped_cancel" }},

		{TEditorWindowType_InputSequence, { new EditorWindowFactory<TEditorInputSequence>, "input_sequence" }},
		{TEditorWindowType_Input, { new EditorWindowFactory<TEditorInputStruct>, "input" }},

		{TEditorWindowType_Projectile, { new EditorWindowFactory<TEditorProjectile>, "projectile" }},

		{TEditorWindowType_CameraData, { new EditorWindowFactory<EditorForm>, "camera_data" }},
		{TEditorWindowType_ThrowCamera, { new EditorWindowFactory<TEditorThrowCamera>, "throw_camera" }},

		{TEditorWindowType_MovelistDisplayable, { new EditorWindowFactory<TEditorMovelistDisplayable>, "mvl_displayable" }},
		{TEditorWindowType_MovelistPlayable, { new EditorWindowFactory<TEditorMovelistPlayable>, "mvl_playable" }},
		{TEditorWindowType_MovelistInput, { new EditorWindowFactory<TEditorMovelistInput>, "mvl_input"}}
	};
}

void EditorVisuals_T7::FilterMovelist(EditorMovelistFilter_ filter)
{
	m_filteredMovelist.clear();

	m_movelistFilter = filter;
	if (filter == EditorMovelistFilter_All) {
		m_filteredMovelist = *movelist;
		return;
	}

	if (filter == EditorMovelistFilter_PostIdle) {
		// Get idle move ID, only list moves beyond it
		size_t startingIndex = m_editor->aliases[1];
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

int32_t EditorVisuals_T7::ValidateMoveId(const char* buf)
{
	int32_t moveId = atoi(buf);

	const size_t movelistSize = movelist->size();
	if (moveId >= movelistSize)
	{
		const size_t aliasesCount = m_editor->aliases.size();
		if (moveId < 0x8000 || moveId >= (0x8000 + aliasesCount)) {
			return -1;
		}
		moveId = m_editor->aliases[(uint16_t)moveId - (uint16_t)0x8000];
	}

	return moveId;
}

void EditorVisuals_T7::OnMoveCreate(unsigned int moveId)
{
	m_editor->ReloadDisplayableMoveList();
	ReloadMovelistFilter();
	m_moveToScrollTo = moveId;
	m_highlightedMoveId = moveId;
}

bool EditorVisuals_T7::MovesetStillLoaded()
{
	gameAddr movesetAddress = m_importerHelper->GetCurrentPlayerMovesetAddr();
#ifdef BUILD_TYPE_DEBUG
	if (movesetAddress != m_loadedMoveset) {
		DEBUG_LOG("MovesetStillLoaded = false, current is %llx, old was %llx\n", movesetAddress, m_loadedMoveset);
	}
#endif
	return movesetAddress == m_loadedMoveset;
}

void EditorVisuals_T7::OnKeyboardShortcut()
{
	//todo
}

// -- Public methods -- //

EditorVisuals_T7::EditorVisuals_T7(const movesetInfo* movesetInfo, GameAddressesFile* addrFile, LocalStorage* storage)
	: EditorVisuals(movesetInfo, addrFile, storage)
{
	m_editor = (EditorT7*)m_abstractEditor;

	Byte* moveset;
	uint64_t movesetSize;
	{
		// Read moveset
		std::ifstream file(movesetInfo->filename.c_str(), std::ios::binary);

		if (file.fail()) {
			delete m_importerHelper;
			delete m_sharedMemHelper;
			throw EditorWindow_MovesetLoadFail();
		}

		file.seekg(0, std::ios::end);
		movesetSize = file.tellg();

		moveset = (Byte*)malloc(movesetSize);
		if (moveset == nullptr) {
			delete m_importerHelper;
			delete m_sharedMemHelper;
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

	PopulateWindowCreatorMap();
	// Read what needs to be read and potentially displayed right away
	m_editor->ReloadDisplayableMoveList();
	movelist = &m_editor->displayableMovelist;
	ReloadMovelistFilter();

	m_compressionIndex = CompressionUtils::GetDefaultCompressionSetting();
}

void EditorVisuals_T7::ReloadMovelistFilter()
{
	FilterMovelist(m_movelistFilter);
}

#include <ImGui.h>

#include <format>
#include <filesystem>

#include "EditorVisuals_t7.hpp"
// Structures
#include "Structures/EditorMove.hpp"
#include "Structures/EditorVoiceclip.hpp"
#include "Structures/EditorExtraproperties.hpp"
#include "Structures/EditorCancels.hpp"
#include "Structures/EditorCancelExtra.hpp"
#include "Structures/EditorGroupedCancels.hpp"
#include "Structures/EditorRequirements.hpp"
#include "Structures/EditorHitConditions.hpp"
#include "Structures/EditorReactions.hpp"
#include "Structures/EditorPushback.hpp"
#include "Structures/EditorPushbackExtra.hpp"
#include "Structures/EditorMoveStartProperty.hpp"
#include "Structures/EditorMoveEndProperty.hpp"
#include "Structures/EditorInputSequence.hpp"
#include "Structures/EditorInputStruct.hpp"
#include "Structures/EditorProjectile.hpp"
#include "Structures/EditorThrowCamera.hpp"
#include "Structures/EditorCameraData.hpp"
#include "Structures/EditorMovelistDisplayable.hpp"
#include "Structures/EditorMovelistPlayable.hpp"
#include "Structures/EditorMovelistInput.hpp"

using namespace EditorVisualsT7;

// -- Private methods -- //

void EditorVisualsT7::PopulateWindowCreatorMap()
{
	m_windowCreatorMap = {
		{EditorWindowType_Move, new EditorWindowFactory<EditorMove> },
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

void EditorVisualsT7::FilterMovelist(EditorMovelistFilter_ filter)
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

int32_t EditorVisualsT7::ValidateMoveId(const char* buf)
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

void EditorVisualsT7::OnMoveCreate(unsigned int moveId)
{
	m_editor->ReloadDisplayableMoveList();
	ReloadMovelistFilter();
	m_moveToScrollTo = moveId;
	m_highlightedMoveId = moveId;
}

bool EditorVisualsT7::MovesetStillLoaded()
{
	gameAddr movesetAddress = m_importerHelper.GetCurrentPlayerMovesetAddr();
#ifdef BUILD_TYPE_DEBUG
	if (movesetAddress != m_loadedMoveset) {
		DEBUG_LOG("MovesetStillLoaded = false, current is %llx, old was %llx\n", movesetAddress, m_loadedMoveset);
	}
#endif
	return movesetAddress == m_loadedMoveset;
}

// -- Public methods -- //

EditorVisualsT7::EditorVisualsT7(const movesetInfo* movesetInfo, GameAddressesFile* addrFile, LocalStorage* storage)
{
	m_editor = (EditorT7*)m_abstractEditor;

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

	PopulateWindowCreatorMap();
	// Read what needs to be read and potentially displayed right away
	m_editor->ReloadDisplayableMoveList();
	movelist = m_editor->displayableMovelist;
	ReloadMovelistFilter();

	editorTable = &m_editor->movesetTable;

	// Set compression setting as the pre-configured default one
	m_compressionIndex = CompressionUtils::GetCompressionSettingIndex(compressionType);
}

void EditorVisualsT7::ReloadMovelistFilter()
{
	FilterMovelist(m_movelistFilter);
}

#include "TEditor.hpp"

namespace TEditorUtils
{
	unsigned int GetMoveColorFromFlag(EditorMoveFlags flags)
	{
		if (flags & EditorMoveFlags_Generic) {
			return MOVE_GENERIC;
		}

		if (flags & EditorMoveFlags_CurrentGeneric) {
			return MOVE_CURRENT_GENERIC;
		}

		if (flags & EditorMoveFlags_Custom) {
			return MOVE_CUSTOM;
		}

		if (flags & EditorMoveFlags_Throw) {
			return MOVE_THROW;
		}
		else if (flags & EditorMoveFlags_ThrowReaction) {
			return MOVE_THROWREACTION;
		}

		if (flags & EditorMoveFlags_Attack) {
			return MOVE_ATK;
		}
		else if (flags & EditorMoveFlags_OtherAttack) {
			return MOVE_OTHERATK;
		}

		return 0;
	}
}

TEditor::TEditor(GameProcess* process, GameData* game, const GameInfo* gameInfo) : EditorLogic(process, game, gameInfo)
{
	displayableMovelist = new std::vector<DisplayableMove*>;

	m_animNameToOffsetMap = new std::map<std::string, gameAddr>;
	m_animOffsetToNameOffset = new std::map<gameAddr, gameAddr>;
}

TEditor::~TEditor()
{
	delete m_animNameToOffsetMap;
	delete m_animOffsetToNameOffset;

	for (auto& move : *displayableMovelist) {
		delete move;
	}
	delete displayableMovelist;
}
#include "Editor.hpp"

#include <format>

typedef unsigned int ImU32;

// Yes, i ripped this straight from ImGui, probably not a good idea but will do for now
// I didn't want to have to include the imgui lib in the dll
#ifndef IM_COL32_R_SHIFT
#ifdef IMGUI_USE_BGRA_PACKED_COLOR
#define IM_COL32_R_SHIFT    16
#define IM_COL32_G_SHIFT    8
#define IM_COL32_B_SHIFT    0
#define IM_COL32_A_SHIFT    24
#define IM_COL32_A_MASK     0xFF000000
#else
#define IM_COL32_R_SHIFT    0
#define IM_COL32_G_SHIFT    8
#define IM_COL32_B_SHIFT    16
#define IM_COL32_A_SHIFT    24
#define IM_COL32_A_MASK     0xFF000000
#endif
#endif
#define IM_COL32(R,G,B,A)    (((ImU32)(A)<<IM_COL32_A_SHIFT) | ((ImU32)(B)<<IM_COL32_B_SHIFT) | ((ImU32)(G)<<IM_COL32_G_SHIFT) | ((ImU32)(R)<<IM_COL32_R_SHIFT))
#define IM_COL32_WHITE       IM_COL32(255,255,255,255)  // Opaque white = 0xFFFFFFFF
#define IM_COL32_BLACK       IM_COL32(0,0,0,255)        // Opaque black
#define IM_COL32_BLACK_TRANS IM_COL32(0,0,0,0)          // Transparent black = 0x00000000

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

Editor::Editor(GameProcess* process, GameData* game, const GameInfo* gameInfo) : EditorLogic(process, game, gameInfo)
{
	m_aliases = new std::vector<uint16_t>;
	displayableMovelist = new std::vector<DisplayableMove*>;

	m_animNameToOffsetMap = new std::map<std::string, gameAddr>;
	m_animOffsetToNameOffset = new std::map<gameAddr, gameAddr>;
}

Editor::~Editor()
{
	delete m_aliases;

	delete m_animNameToOffsetMap;
	delete m_animOffsetToNameOffset;

	for (auto& move : *displayableMovelist) {
		delete move;
	}
	delete displayableMovelist;
}
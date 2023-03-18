#include "Editor.hpp"

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

namespace EditorUtils
{
	unsigned int GetMoveColorFromFlag(EditorMoveFlags flags)
	{
		if (flags & EditorMoveFlags_Generic) {
			return IM_COL32(77, 131, 219, 50);
		}

		if (flags & EditorMoveFlags_Custom) {
			return IM_COL32(119, 175, 58, 50);
		}

		if (flags & EditorMoveFlags_Throw) {
			return IM_COL32(119, 58, 199, 50);
		}
		else if (flags & EditorMoveFlags_ThrowReaction) {
			return IM_COL32(210, 100, 222, 50);
		}

		if (flags & EditorMoveFlags_Attack) {
			return IM_COL32(186, 54, 54, 50);
		}
		else if (flags & EditorMoveFlags_OtherAttack) {
			return IM_COL32(140, 0, 0, 50);
		}

		return 0;
	}

	ImGuiInputTextFlags GetFieldCharset(EditorInputFlag flags)
	{
		if (flags & (EditorInput_H64 | EditorInput_H32 | EditorInput_H16)) {
			return ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase;
		}
		return ImGuiInputTextFlags_CharsDecimal;
	}

	const char* GetFieldFormat(EditorInputFlag flags)
	{
		if (flags & (EditorInput_H64)) {
			return "%llX";
		}
		if (flags & (EditorInput_H32 | EditorInput_H16)) {
			return "%X";
		}
		if (flags & EditorInput_S64) {
			return "%lld";
		}
		if (flags & EditorInput_U64) {
			return "%llu";
		}
		if (flags & (EditorInput_U32 | EditorInput_U16)) {
			return "%u";
		}
		return "%d";
	}
}

const Byte* Editor::GetMoveset(uint64_t& movesetSize_out)
{
	movesetSize_out = m_movesetSize;
	return m_moveset;
}
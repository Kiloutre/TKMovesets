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
	void SetInputfieldColor(EditorInput* field)
	{
		if (field->flags & EditorInput_Hex) {
			field->color = FORM_INPUT_HEX;
		}

		else if (field->flags & EditorInput_Float) {
			field->color = FORM_INPUT_FLOAT;
		}

		else if (field->flags & EditorInput_PTR) {
			field->color = FORM_INPUT_REF;
		}

		else if (field->flags & EditorInput_Signed) {
			field->color = FORM_INPUT_SIGNED;	
		}
	}

	unsigned int GetMoveColorFromFlag(EditorMoveFlags flags)
	{
		if (flags & EditorMoveFlags_Generic) {
			return MOVE_GENERIC;
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

	ImGuiInputTextFlags GetFieldCharset(EditorInputFlag flags)
	{
		if (flags & EditorInput_Hex) {
			//return ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase;
			return ImGuiInputTextFlags_CharsNoBlank;
		}
		if (flags & EditorInput_Float) {
			return ImGuiInputTextFlags_CharsDecimal;
		}
		return ImGuiInputTextFlags_CharsDecimal;
	}

	const char* GetFieldFormat(EditorInputFlag flags)
	{
		if (flags & (EditorInput_H64)) {
			return "0x%llX";
		}
		if (flags & (EditorInput_H32 | EditorInput_H16)) {
			return "0x%X";
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
		if (flags & (EditorInput_Float)) {
			return "%f";
		}
		return "%d";
	}

	uint64_t GetFieldValue(EditorInput* field)
	{
		const char* buffer = field->buffer;
		auto& flags = field->flags;

		if (flags & (EditorInput_Hex)) {
			return (uint64_t)strtoll(buffer, nullptr, 16);
		}
		if (flags & EditorInput_Float) {
			union fieldValue {
				uint64_t uint64 = 0;
				float floatingPoint;
			} value;
			value.floatingPoint = std::atof(buffer);
			return value.uint64;
		}
		return (uint64_t)strtoll(buffer, nullptr, 10);
	}

	void SetMemberValue(void* memberPtr, EditorInput* field)
	{
		const char* buffer = field->buffer;
		auto& flags = field->flags;
		uint64_t value = 0;

		if (flags & EditorInput_Hex) {
			value = (uint64_t)strtoll(buffer, nullptr, 16);
		}
		else if (flags & EditorInput_Float) {
			union fieldValue {
				uint32_t uint32 = 0;
				float floatingPoint;
			} bufferValue;
			bufferValue.floatingPoint = (float)std::atof(buffer);
			value = bufferValue.uint32;
		}
		else {
			value = (uint64_t)strtoll(buffer, nullptr, 10);
		}

		if (flags & EditorInput_64b) {
			*(uint64_t*)memberPtr = value;
		}
		else if (flags & EditorInput_32b) {
			*(uint32_t*)memberPtr = value;
		}
		else if (flags & EditorInput_16b) {
			*(uint16_t*)memberPtr = value;
		}
		else {
			throw;
		}
	}
}

const Byte* Editor::GetMoveset(uint64_t& movesetSize_out)
{
	movesetSize_out = m_movesetSize;
	return m_moveset;
}

bool Editor::ValidateFieldType(EditorInput* field)
{
	if (field->buffer[0] == '\0') {
		// There are no fields that should be allowed to be empty
		return false;
	}

	auto flags = field->flags;

	if (flags & EditorInput_H64) {
		if (strlen(field->buffer) > 16 + 2) {
			return false;
		}
	}
	else if (flags & EditorInput_H32) {
		if (strlen(field->buffer) > 8 + 2) {
			return false;
		}
	}
	else if (flags & EditorInput_H16) {
		if (strlen(field->buffer) > 4 + 2) {
			return false;
		}
	}
	else if (flags & (EditorInput_U64 | EditorInput_S64)) {
		if (strlen(field->buffer) > 19) {
			return false;
		}
		if ((flags & EditorInput_U64) && atoll(field->buffer) < 0) {
			return false;
		}
	}
	else if (flags & EditorInput_U32) {
		long long num = atoll(field->buffer);
		if (num > UINT_MAX || num < 0) {
			return false;
		}
	}
	else if (flags & EditorInput_S32) {
		long long num = atoll(field->buffer);
		if (num > INT_MAX || num < INT_MIN) {
			return false;
		}
	}
	else if (flags & EditorInput_U16) {
		int num = atoi(field->buffer);
		if (num > USHRT_MAX || num < 0) {
			return false;
		}
	}
	else if (flags & EditorInput_S16) {
		int num = atoi(field->buffer);
		if (num > SHRT_MAX || num < SHRT_MIN) {
			return false;
		}
	}
	
	return true;
}
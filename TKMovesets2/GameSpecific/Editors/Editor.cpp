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

namespace EditorUtils
{
	bool MustShiftId(int structureId, int valueChange, int listStart, int old_listEnd)
	{
		// If ID comes before list then there is no shifting to be done
		if (structureId < listStart) return false;
		// If ID comes after current list end then shifting is guaranteed to be needed
		if (structureId >= old_listEnd) return true;

		// From this point on, structureId is guaranteed to be listStart <= structureId < old_listEnd

		int new_listEnd = old_listEnd + valueChange;

		// An item was deleted from the list
		if (valueChange < 0)
		{
			// Item is situated between the new list end and the old one, we want to keep it in the current list, shift ID down
			if (valueChange >= new_listEnd) return true;
		}
		// An item was added to the list
		else if (valueChange > 0)
		{
			// At the current time, appending to a list does not shift any of the list's IDs up
			return false;
		}

		return false;
	}

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

		else {
			field->color = 0;
		}
	}

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

	ImGuiInputTextFlags GetFieldCharset(EditorInputFlag flags)
	{
		if (flags & EditorInput_String) {
			return 0;
		}
		if (flags & EditorInput_Hex) {
			// Imgui's charset doesn't allow for the 'x' in '0x'...
			//return ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase;
			return ImGuiInputTextFlags_CharsNoBlank;
		}
		if (flags & EditorInput_Float) {
			return ImGuiInputTextFlags_CharsDecimal;
		}
		return ImGuiInputTextFlags_CharsDecimal;
	}

	constexpr const char* GetFieldFormat(EditorInputFlag flags)
	{
		if (flags & (EditorInput_H64)) {
			return "0x%llX";
		}
		if (flags & (EditorInput_H32 | EditorInput_H16 | EditorInput_H8)) {
			return "0x%X";
		}
		if (flags & EditorInput_S64) {
			return "%lld";
		}
		if (flags & EditorInput_U64) {
			return "%llu";
		}
		if (flags & (EditorInput_U32 | EditorInput_U16 | EditorInput_U8)) {
			return "%u";
		}
		if (flags & (EditorInput_Float)) {
			return "%.7g";
		}
		if (flags & (EditorInput_String)) {
			return "%s";
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
			value.floatingPoint = (float)std::atof(buffer);
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
			*(uint32_t*)memberPtr = *(uint32_t*)&value;
		}
		else if (flags & EditorInput_16b) {
			*(uint16_t*)memberPtr = *(uint16_t*)&value;
		}
		else if (flags & EditorInput_8b) {
			*(uint8_t*)memberPtr = *(uint8_t*)&value;
		}
	}

	void ChangeFieldDataType(EditorInput* field)
	{
		auto& flags = field->flags;
		uint64_t value = GetFieldValue(field);

		if (flags & EditorInput_64b)
		{
			if (flags & EditorInput_U64) {
				field->flags = (flags ^ EditorInput_U64) | EditorInput_S64;
				DEBUG_LOG("Setting field as S64\n");
			}
			else if (flags & EditorInput_S64) {
				field->flags = (flags ^ EditorInput_S64) | EditorInput_H64;
				DEBUG_LOG("Setting field as H64\n");
			}
			else {
				field->flags = (flags ^ EditorInput_H64) | EditorInput_U64;
				DEBUG_LOG("Setting field as U64\n");
			}
		}
		else if (flags & EditorInput_32b)
		{
			if (flags & EditorInput_U32) {
				field->flags = (flags ^ EditorInput_U32) | EditorInput_S32;
				DEBUG_LOG("Setting field as S32\n");
			}
			else if (flags & EditorInput_S32) {
				field->flags = (flags ^ EditorInput_S32) | EditorInput_H32;
				DEBUG_LOG("Setting field as H32\n");
			}
			else {
				field->flags = (flags ^ EditorInput_H32) | EditorInput_U32;
				DEBUG_LOG("Setting field as U32\n");
			}
		}
		else if (flags & EditorInput_16b)
		{
			if (flags & EditorInput_U16) {
				field->flags = (flags ^ EditorInput_U16) | EditorInput_S16;
				DEBUG_LOG("Setting field as S16\n");
			}
			else if (flags & EditorInput_S16) {
				field->flags = (flags ^ EditorInput_S16) | EditorInput_H16;
				DEBUG_LOG("Setting field as H16\n");
			}
			else {
				field->flags = (flags ^ EditorInput_H16) | EditorInput_U16;
				DEBUG_LOG("Setting field as U16\n");
			}
		}
		else if (flags & EditorInput_8b)
		{
			if (flags & EditorInput_U8) {
				field->flags = (flags ^ EditorInput_U8) | EditorInput_S8;
				DEBUG_LOG("Setting field as S8\n");
			}
			else if (flags & EditorInput_S8) {
				field->flags = (flags ^ EditorInput_S8) | EditorInput_H8;
				DEBUG_LOG("Setting field as H8\n");
			}
			else {
				field->flags = (flags ^ EditorInput_H8) | EditorInput_U8;
				DEBUG_LOG("Setting field as U8\n");
			}
		}
		else {
			return;
		}

		const unsigned int bufSize = 32;
		char buf[bufSize];

		sprintf_s(buf, bufSize, GetFieldFormat(field->flags), value);

		DEBUG_LOG("Format is '%s', value is %llu, result is '%s'\n", GetFieldFormat(field->flags), value, buf);

		field->nextValue = std::string(buf);

		SetInputfieldColor(field);
	}

	void WriteFieldFullname(InputMap& inputMap, const std::string& baseIdentifier)
	{
		// Finishing touch
		for (auto& [name, input] : inputMap) {
			// Duplicate the name inside the structure, this is more convenient for me in some places, helps writing a lot shorter code
			input->fullName = std::format("edition.{}.{}", baseIdentifier.c_str(), name.c_str());
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
	auto& buffer = field->buffer;
	if (buffer[0] == '\0') {
		// There are no fields that should be allowed to be empty
		return false;
	}


	auto flags = field->flags;

	if (flags & EditorInput_H64) {
		int len = (int)strlen(buffer);
		if (len > 16 && (len > (16 + 2) || strncmp(buffer, "0x", 2) != 0)) {
			return false;
		}
	}
	else if (flags & EditorInput_H32) {
		int len = (int)strlen(buffer);
		if (len > 8 && (len > (8 + 2) || strncmp(buffer, "0x", 2) != 0)) {
			return false;
		}
	}
	else if (flags & EditorInput_H16) {
		int len = (int)strlen(buffer);
		if (len > 4 && (len > (4 + 2) || strncmp(buffer, "0x", 2) != 0)) {
			return false;
		}
	}
	else if (flags & (EditorInput_U64 | EditorInput_S64)) {
		if (strlen(buffer) > 19) {
			return false;
		}
		if ((flags & EditorInput_U64) && buffer[0] == '-') {
			return false;
		}
	}
	else if (flags & EditorInput_U32) {
		long long num = atoll(buffer);
		if (num > UINT_MAX || num < 0) {
			return false;
		}
	}
	else if (flags & EditorInput_S32) {
		long long num = atoll(buffer);
		if (num > INT_MAX || num < INT_MIN) {
			return false;
		}
	}
	else if (flags & EditorInput_U16) {
		int num = atoi(buffer);
		if (num > USHRT_MAX || num < 0) {
			return false;
		}
	}
	else if (flags & EditorInput_S16) {
		int num = atoi(buffer);
		if (num > SHRT_MAX || num < SHRT_MIN) {
			return false;
		}
	}
	else if (flags & EditorInput_U8) {
		int num = atoi(buffer);
		if (num > UCHAR_MAX || num < 0) {
			return false;
		}
	}
	else if (flags & EditorInput_S8) {
		int num = atoi(buffer);
		if (num > CHAR_MAX || num < CHAR_MIN) {
			return false;
		}
	}
	
	return true;
}

Editor::Editor(GameProcess* process, GameData* game, const GameInfo* gameInfo) : BaseGameSpecificClass(process, game, gameInfo)
{
	m_aliases = new std::vector<uint16_t>;
	displayableMovelist = new std::vector<DisplayableMove*>;

	m_animNameToOffsetMap = new std::map<std::string, gameAddr>;
	m_animOffsetToNameOffset = new std::map<gameAddr, gameAddr>;

	constants = new std::map<EditorConstants_, unsigned int >();
	animExtractionThread = new std::thread();
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

	delete constants;
	delete animExtractionThread; // todo: join this thread here
}
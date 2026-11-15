#include <stdexcept>
#include "EditorLogic.hpp"

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
	bool MustShiftId(gameAddr32 structureId, int valueChange, int listStart, int old_listEnd)
	{
		if (structureId == (gameAddr32)-1) return false;
		return MustShiftId((int)structureId, valueChange, listStart, old_listEnd);
	}

	bool MustShiftId(gameAddr structureId, int valueChange, int listStart, int old_listEnd)
	{
		if (structureId == (gameAddr)-1) return false;
		return MustShiftId((int)structureId, valueChange, listStart, old_listEnd);
	}

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
			if (structureId >= new_listEnd) return true;
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
		// Floats are not cycled: their bits shown as an integer would be read back as a float value
		// (EditorInput_32b includes EditorInput_Float, so the 32-bit cycle below would also mix the flags).
		if (flags & EditorInput_Float) return;

		// Raw bits of the current text within the field's width, so cycling the display never changes the stored
		// value: no saturation of 64-bit values >= 2^63, no sign extension of negative 8/16/32-bit values.
		const unsigned int width = (flags & EditorInput_64b) ? 64 : (flags & EditorInput_32b) ? 32 : (flags & EditorInput_16b) ? 16 : 8;
		uint64_t value;
		if (flags & EditorInput_Hex) value = strtoull(field->buffer, nullptr, 16);
		else if (flags & EditorInput_Signed) value = (uint64_t)strtoll(field->buffer, nullptr, 10);
		else value = strtoull(field->buffer, nullptr, 10);
		if (width < 64) value &= (uint64_t(1) << width) - 1;

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

		if (field->flags & EditorInput_Hex) sprintf_s(buf, bufSize, "0x%llX", (unsigned long long)value);
		else if (field->flags & EditorInput_Signed) {
			// Sign-extend from the field width
			const long long signedValue = width < 64 ? (long long)(value << (64 - width)) >> (64 - width) : (long long)value;
			sprintf_s(buf, bufSize, "%lld", signedValue);
		}
		else sprintf_s(buf, bufSize, "%llu", (unsigned long long)value);

		DEBUG_LOG("Value is %llu, result is '%s'\n", value, buf);

		field->QueueValue(std::string(buf));

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

const Byte* EditorLogic::GetMoveset(uint64_t& movesetSize_out)
{
	movesetSize_out = m_movesetSize;
	return m_moveset;
}

bool EditorLogic::ValidateFieldType(EditorInput* field)
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

EditorLogic::EditorLogic(GameProcess& process, GameData& game, const GameInfo* gameInfo) : BaseGameSpecificClass(process, game, gameInfo)
{
}

EditorLogic::~EditorLogic()
{
	free(m_moveset);
}
EditorLogic::Mutation::Mutation(EditorLogic& editor, bool batch)
    : m_editor(editor), m_original(editor.m_moveset), m_size(editor.m_movesetSize),
      m_liveAddress(editor.live_loadedMoveset), m_parentBatch(editor.m_activeBatch)
{
    if (!m_original || !m_size) throw std::invalid_argument("No moveset is loaded");
    std::unique_ptr<Byte, decltype(&free)> copy(editor.AllocateMutationBuffer(m_size), &free);
    if (!copy) throw std::bad_alloc();
    memcpy(copy.get(), m_original, m_size);
    m_state = editor.CaptureMutationState();
    try {
        editor.LoadMovesetPtr(copy.get(), m_size);
    } catch (...) {
        if (m_state) m_state->Restore();
        editor.RestoreMovesetPtr(m_original, m_size);
        editor.live_loadedMoveset = m_liveAddress;
        throw;
    }
    copy.release();
    if (batch || m_parentBatch) {
        editor.m_activeBatch = this;
        m_registered = true;
    }
}

EditorLogic::Mutation::~Mutation()
{
    if (m_registered) m_editor.m_activeBatch = m_parentBatch;
    if (m_original) {
        if (m_state) m_state->Restore();
        free(m_editor.m_moveset);
        m_editor.RestoreMovesetPtr(m_original, m_size);
        m_editor.live_loadedMoveset = m_liveAddress;
    }
}

void EditorLogic::Mutation::Commit()
{
    if (m_registered && m_editor.m_activeBatch != this) throw std::logic_error("Edit transactions must commit in nesting order");
    if (m_failed) throw std::runtime_error("Cannot commit a failed edit batch");
    if (m_registered) {
        m_editor.m_activeBatch = m_parentBatch;
        m_registered = false;
    }
    if (m_editor.m_movesetSize == m_size && memcmp(m_editor.m_moveset, m_original, m_size) == 0) return;
    if (m_state) m_state->Commit();
    free(m_original);
    m_original = nullptr;
}

std::string_view EditorLogic::InputString(const EditorInput* input)
{
    if (!input || !input->buffer || !input->bufsize) throw std::invalid_argument("Missing input field");
    auto end = static_cast<const char*>(memchr(input->buffer, 0, input->bufsize));
    if (!end) throw std::invalid_argument("Unterminated input field");
    return {input->buffer, static_cast<size_t>(end - input->buffer)};
}

std::string_view EditorLogic::BlockString(size_t block, uint64_t offset) const
{
    if (!m_header || m_header->moveset_data_start > m_movesetSize ||
        m_header->block_list > m_header->moveset_data_start ||
        m_header->block_list_size > (m_header->moveset_data_start - m_header->block_list) / sizeof(uint64_t) ||
        block >= m_header->block_list_size) throw std::out_of_range("Invalid string section");
    auto blocks = reinterpret_cast<const uint64_t*>(m_moveset + m_header->block_list);
    auto start = blocks[block];
    auto end = block + 1 < m_header->block_list_size ? blocks[block + 1] : m_movesetDataSize;
    if (start > end || end > m_movesetDataSize || offset >= end - start)
        throw std::out_of_range("String outside its section");
    auto text = reinterpret_cast<const char*>(m_movesetData + start + offset);
    auto terminator = static_cast<const char*>(memchr(text, 0, end - start - offset));
    if (!terminator) throw std::invalid_argument("Unterminated section string");
    return {text, static_cast<size_t>(terminator - text)};
}

void EditorLogic::ReplaceBlockRange(size_t block, size_t offset, size_t removed, std::span<const Byte> inserted)
{
    constexpr size_t budget = 256ULL << 20;
    if (!m_header || m_movesetSize > budget || m_header->moveset_data_start > m_movesetSize ||
        m_header->block_list > m_header->moveset_data_start ||
        m_header->block_list_size > (m_header->moveset_data_start - m_header->block_list) / sizeof(uint64_t) ||
        block >= m_header->block_list_size)
        throw std::invalid_argument("Invalid section table");
    auto blocks = reinterpret_cast<const uint64_t*>(m_moveset + m_header->block_list);
    const size_t start = blocks[block];
    const size_t end = block + 1 < m_header->block_list_size ? blocks[block + 1] : m_movesetDataSize;
    if (start > end || end > m_movesetDataSize || offset > end - start || removed > end - start - offset ||
        inserted.size() > budget) throw std::out_of_range("Section edit outside its bounds");
    size_t newEnd = end - removed + inserted.size();
    if (block + 1 < m_header->block_list_size) newEnd = (newEnd + 7) & ~size_t(7);
    const int64_t delta = static_cast<int64_t>(newEnd) - static_cast<int64_t>(end);
    const int64_t newSize = static_cast<int64_t>(m_movesetSize) + delta;
    if (newSize < 0 || newSize > budget) throw std::length_error("Edited moveset exceeds the 256 MiB budget");
    std::unique_ptr<Byte, decltype(&free)> replacement(AllocateMutationBuffer(static_cast<size_t>(newSize)), &free);
    if (!replacement) throw std::bad_alloc();
    memset(replacement.get(), 0, static_cast<size_t>(newSize));
    const size_t absolute = m_header->moveset_data_start + start + offset;
    const size_t oldEnd = m_header->moveset_data_start + end;
    memcpy(replacement.get(), m_moveset, absolute);
    if (!inserted.empty()) memcpy(replacement.get() + absolute, inserted.data(), inserted.size());
    memcpy(replacement.get() + absolute + inserted.size(), m_moveset + absolute + removed,
        oldEnd - absolute - removed);
    memcpy(replacement.get() + m_header->moveset_data_start + newEnd, m_moveset + oldEnd, m_movesetSize - oldEnd);
    auto nextBlocks = reinterpret_cast<uint64_t*>(replacement.get() + m_header->block_list);
    for (size_t i = block + 1; i < m_header->block_list_size; ++i) {
        if (blocks[i] < end || blocks[i] > m_movesetDataSize) throw std::invalid_argument("Unordered section table");
        nextBlocks[i] = static_cast<uint64_t>(static_cast<int64_t>(blocks[i]) + delta);
    }
    auto previous = m_moveset;
    const auto previousSize = m_movesetSize;
    const auto previousLive = live_loadedMoveset;
    try {
        LoadMovesetPtr(replacement.get(), static_cast<uint64_t>(newSize));
    } catch (...) {
        LoadMovesetPtr(previous, previousSize);
        live_loadedMoveset = previousLive;
        throw;
    }
    replacement.release();
    free(previous);
}

std::vector<Byte> EditorLogic::BuildSaveSnapshot()
{
    if (!m_moveset || m_movesetSize < sizeof(TKMovesetHeader)) throw std::runtime_error("No moveset to save");
    std::vector<Byte> snapshot(m_moveset, m_moveset + m_movesetSize);
    auto header = reinterpret_cast<TKMovesetHeader*>(snapshot.data());
    header->crc32 = CalculateCRC32();
    header->date = Helpers::getCurrentTimestamp();
    return snapshot;
}

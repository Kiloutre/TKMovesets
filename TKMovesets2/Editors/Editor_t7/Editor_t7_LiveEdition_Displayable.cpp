#include "Editor_t7.hpp"

#include "helpers.hpp"

// Live edition callbacks that will only be called if live edition is enabled, the moveset is loaded in memory AND if the modified field is valid

void EditorT7::Live_OnMovelistDisplayableEdit(int id, EditorInput* field)
{
	std::string& name = field->name;
	auto& buffer = field->buffer;

	uint64_t blockStart = live_loadedMoveset + m_header->offsets.movelistBlock;
	gameAddr structAddr = blockStart + (uint64_t)m_mvlHead->displayables_offset + id * sizeof(MvlDisplayable);
	union {
		uint64_t value_u64;
		int64_t value_s64;
		uint32_t value_u32;
		int32_t value_s32;
		uint32_t value_u16;
		int32_t value_s16;
		float value_float;
	};
	value_u64 = EditorUtils::GetFieldValue(field);

	// todo
	/*
	if (name == "_0x0_int") {
		m_process->writeInt32(structAddr + offsetof(MvlDisplayable, _0x0_int), value_s32);
	}
	else if (name == "_0x4_short") {
		m_process->writeInt16(structAddr + offsetof(MvlDisplayable, _0x4_short), value_s16);
	}
	else if (name == "left_side_camera_data") {
		m_process->writeInt16(structAddr + offsetof(MvlDisplayable, left_side_camera_data), value_s16);
	}
	else if (name == "right_side_camera_data") {
		m_process->writeInt16(structAddr + offsetof(MvlDisplayable, right_side_camera_data), value_s16);
	}
	else if (name == "_0xA_short") {
		m_process->writeInt16(structAddr + offsetof(MvlDisplayable, _0xA_short), value_s16);
	}
	*/
}

void EditorT7::Live_OnMovelistPlayableEdit(int id, EditorInput* field)
{
	std::string& name = field->name;
	auto& buffer = field->buffer;

	uint64_t blockStart = live_loadedMoveset + m_header->offsets.movelistBlock;
	gameAddr structAddr = blockStart + (uint64_t)m_mvlHead->playables_offset + id * sizeof(MvlPlayable);
	union {
		uint64_t value_u64;
		int64_t value_s64;
		uint32_t value_u32;
		int32_t value_s32;
		uint32_t value_u16;
		int32_t value_s16;
		float value_float;
	};
	value_u64 = EditorUtils::GetFieldValue(field);

	if (name == "_unk0x0") {
		m_process->writeInt32(structAddr + offsetof(MvlPlayable, _unk0x0), value_s16);
	}
	else if (name == "distance") {
		m_process->writeInt16(structAddr + offsetof(MvlPlayable, distance), value_s16);
	}
	else if (name == "p2_rotation") {
		m_process->writeInt16(structAddr + offsetof(MvlPlayable, p2_rotation), value_s16);
	}
	else if (name == "_unk0x6") {
		m_process->writeInt16(structAddr + offsetof(MvlPlayable, _unk0x6), value_s16);
	}
	else if (name == "_unk0x8") {
		m_process->writeInt16(structAddr + offsetof(MvlPlayable, _unk0x8), value_s16);
	}
	else if (name == "p1_facing_related") {
		m_process->writeInt16(structAddr + offsetof(MvlPlayable, p1_facing_related), value_s16);
	}
	else if (name == "_unk0xc") {
		m_process->writeInt16(structAddr + offsetof(MvlPlayable, _unk0xc), value_s16);
	}
	else if (name == "input_count") {
		m_process->writeInt16(structAddr + offsetof(MvlPlayable, input_count), value_s16);
	}
	else if (name == "input_sequence_offset") {
		uint32_t playable_addr = m_mvlHead->playables_offset + (int32_t)(sizeof(MvlPlayable) * id);
		uint32_t input_sequence_id = value_s16;
		uint32_t input_sequence_addr = input_sequence_id * sizeof(MvlInput) + m_mvlHead->inputs_offset;

		m_process->writeInt16(structAddr + offsetof(MvlPlayable, input_sequence_offset), (int16_t)(input_sequence_addr - playable_addr));
	}
	else if (name == "has_rage") {
		m_process->writeInt16(structAddr + offsetof(MvlPlayable, has_rage), value_s16);
	}
	else if (name == "_unk0x16") {
		m_process->writeInt16(structAddr + offsetof(MvlPlayable, _unk0x16), value_s16);
	}
}

void EditorT7::Live_OnMovelistInputEdit(int id, EditorInput* field)
{
	std::string& name = field->name;
	auto& buffer = field->buffer;

	uint64_t blockStart = live_loadedMoveset + m_header->offsets.movelistBlock;
	gameAddr structAddr = blockStart + (uint64_t)m_mvlHead->inputs_offset + id * sizeof(MvlInput);
	union {
		uint64_t value_u64;
		int64_t value_s64;
		uint32_t value_u32;
		int32_t value_s32;
		uint32_t value_u16;
		int32_t value_s16;
		int8_t value_s8;
		float value_float;
	};
	value_u64 = EditorUtils::GetFieldValue(field);

	if (name == "directions") {
		m_process->writeInt8(structAddr + offsetof(MvlInput, directions), value_s8);
	}
	else if (name == "buttons") {
		m_process->writeInt8(structAddr + offsetof(MvlInput, buttons), value_s8);
	}
	else if (name == "frame_duration") {
		m_process->writeInt8(structAddr + offsetof(MvlInput, frame_duration), value_s8);
	}
	else if (name == "_unkn_0x2") {
		m_process->writeInt8(structAddr + offsetof(MvlInput, _unkn_0x2), value_s8);
	}
}

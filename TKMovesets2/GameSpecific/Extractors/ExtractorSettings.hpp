#pragma once

typedef uint64_t ExtractSettings; // 64 flags
enum ExtractSettings_
{
	// These 12 must occupy the first 12 bits
	ExtractSettings_MOTA_0_ANIM = (1 << 0),
	ExtractSettings_MOTA_1_ANIM = (1 << 1),
	ExtractSettings_MOTA_2_HAND = (1 << 2),
	ExtractSettings_MOTA_3_HAND = (1 << 3),
	ExtractSettings_MOTA_4_FACE = (1 << 4),
	ExtractSettings_MOTA_5_FACE = (1 << 5),
	ExtractSettings_MOTA_6_WINGS = (1 << 6),
	ExtractSettings_MOTA_7_WINGS = (1 << 7),
	ExtractSettings_MOTA_8_CAMERA = (1 << 8),
	ExtractSettings_MOTA_9_CAMERA = (1 << 9),
	ExtractSettings_MOTA_10_UNKNOWN = (1 << 10),
	ExtractSettings_MOTA_11_UNKNOWN = (1 << 11),

	// Shortcuts for matching pairs
	ExtractSettings_MOTA_ANIM = ExtractSettings_MOTA_0_ANIM | ExtractSettings_MOTA_1_ANIM,
	ExtractSettings_MOTA_HANDS = ExtractSettings_MOTA_2_HAND | ExtractSettings_MOTA_3_HAND,
	ExtractSettings_MOTA_FACES = ExtractSettings_MOTA_4_FACE | ExtractSettings_MOTA_5_FACE,
	ExtractSettings_MOTA_CAMERAS = ExtractSettings_MOTA_8_CAMERA | ExtractSettings_MOTA_9_CAMERA,
	ExtractSettings_MOTA_WINGS = ExtractSettings_MOTA_6_WINGS | ExtractSettings_MOTA_7_WINGS,
	ExtractSettings_MOTA_UNKNOWNS = ExtractSettings_MOTA_10_UNKNOWN | ExtractSettings_MOTA_11_UNKNOWN,


	ExtractSettings_OVERWRITE_SAME_FILENAME = (1 << 12),
	ExtractSettings_DisplayableMovelist = (1 << 13),

	ExtractSettings_CompressLZMA = (1 << 14),
	ExtractSettings_CompressLZ4 = (1 << 15),

	ExtractSettings_Compress = ExtractSettings_CompressLZMA | ExtractSettings_CompressLZ4,
};

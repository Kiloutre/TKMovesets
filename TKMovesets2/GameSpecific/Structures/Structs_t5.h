#pragma once

#include "GameTypes.h"

// Most of the names in this file indicating offsets (_0x00_int etc...) are wrong
// This is because i copy-pasted the structures from T7
// Fix it up anytime you want

namespace StructsT5
{
	enum TKMovesetHeaderBlocks_
	{
		TKMovesetHeaderBlocks_MovesetInfo = 0,
		TKMovesetHeaderBlocks_Name        = 1,
		TKMovesetHeaderBlocks_Moveset     = 2,
		TKMovesetHeaderBlocks_Animation   = 3,
		TKMovesetHeaderBlocks_Mota        = 4,
	};

	// Custom-made offset list containing the list of blocks that compose our extracted moveset
	// These are arbitrary blocks and do not match the game's moveset 1:1
	struct TKMovesetHeaderBlocks
	{
		union {
			struct {
				// Store moveid aliases and some more i don't understand
				uint64_t movesetInfoBlock;
				// Stores an offset to the block containing move names & anim names (they are the same)
				uint64_t nameBlock;
				// Stores an offset to the block containing stucture lists
				uint64_t movesetBlock;
				// Store an offset to the block containing every regular animation used in the moveset. (customly-built)
				uint64_t animationBlock;
				// Store an offset to the block containing the multiple mota files for those that are extracted . (customly-built)
				uint64_t motaBlock;
			};
			uint64_t blocks[5];
		};

		Byte* GetBlock(TKMovesetHeaderBlocks_ block, Byte* moveset) const
		{
			return moveset + blocks[(unsigned int)block];
		}

		uint64_t GetBlockSize(TKMovesetHeaderBlocks_ block, uint64_t s_moveset = 0) const
		{
			unsigned int b = (unsigned int)block;
			if ((b + 1) >= _countof(blocks)) {
				return s_moveset - blocks[block];
			}
			else {
				return blocks[block + 1] - blocks[block];
			}
		}
	};

	// -- Main data type -- //

	struct PushbackExtradata
	{
		uint16_t horizontal_offset;
	};

	struct Pushback
	{
		uint16_t duration;
		uint16_t displacement;
		uint32_t num_of_loops;
		gameAddr32 extradata_addr;
	};

	struct Requirement
	{
		uint16_t condition;
		union
		{
			uint16_t param_unsigned;
			int16_t param_signed;
		};
	};

	struct CancelExtradata
	{
		uint32_t value;
	};

	struct Cancel
	{
		union
		{
			uint32_t command;
			struct
			{
				uint16_t direction;
				uint16_t button;
			};
		};
		gameAddr32 requirements_addr;
		uint32_t move_id;
		gameAddr32 extradata_addr;
		uint16_t detection_start;
		uint16_t detection_end;
		uint16_t starting_frame;
		uint16_t cancel_option;
	};

	struct Reactions
	{
		gameAddr32 front_pushback;
		gameAddr32 backturned_pushback;
		gameAddr32 left_side_pushback;
		gameAddr32 right_side_pushback;
		gameAddr32 front_counterhit_pushback; // If you ever wondered why your CH launcher didn't launch after a sidestep, that's why
		gameAddr32 downed_pushback;
		gameAddr32 block_pushback;

		uint16_t front_direction;
		uint16_t back_direction;
		uint16_t left_side_direction;
		uint16_t right_side_direction;
		uint16_t front_counterhit_direction;
		uint16_t downed_direction;

		uint32_t _0x28_int;
		uint32_t _0x2C_int;

		uint16_t vertical_pushback;
		uint16_t standing_moveid;
		uint16_t default_moveid;
		uint16_t crouch_moveid;
		uint16_t counterhit_moveid;
		uint16_t crouch_counterhit_moveid;
		uint16_t left_side_moveid;
		uint16_t crouch_left_side_moveid;
		uint16_t right_side_moveid;
		uint16_t crouch_right_side_moveid;
		uint16_t backturned_moveid;
		uint16_t crouch_backturned_moveid;
		uint16_t block_moveid;
		uint16_t crouch_block_moveid;
		uint16_t wallslump_moveid;
		uint16_t downed_moveid;
	};

	struct HitCondition
	{
		gameAddr32 requirements_addr;
		uint32_t damage;
		gameAddr32 reactions_addr;
	};

	struct Voiceclip
	{
		uint16_t id;
	};

	struct ExtraMoveProperty
	{
		uint16_t starting_frame;
		uint16_t id;
		union
		{
			uint32_t value_unsigned;
			int32_t value_signed;
			float value_float;
		};
	};

	struct Input
	{
		union
		{
			uint32_t command;
			struct
			{
				uint16_t direction;
				uint16_t button;
			};
		};
	};

	struct InputSequence
	{
		uint8_t input_window_frames; // I assume the max amount of frames between the first &last input
		int8_t _0x0_int8; // Apparently unused
		uint16_t input_amount; // The amount of input to read from input_addr
		gameAddr32 input_addr;
	};

	struct Projectile
	{
		// As it seems, the projectile format is different between ttt2 & t7
		char gap[0x88];

		// 0x24: Maybe a moveid
		// 0x38: Probably voiceclip
		// 
		// 0x40: Probably voiceclip
		// 0x48: Probably voiceclip
	};

	struct CameraData
	{
		uint32_t _0x0_int;
		uint16_t _0x4_short;
		uint16_t left_side_camera_data;
		uint16_t right_side_camera_data;
		uint16_t _0xA_short;
	};

	struct ThrowCamera
	{
		uint32_t _0x0_uint;
		gameAddr32 cameradata_addr;
	};

	struct StructA1
	{
		uint16_t value;
	};

	struct StructA3
	{
		uint16_t _0x0; // Assumed
		uint16_t _0x2; // Assumed
		uint16_t _0x4; // Assumed
		uint16_t _0x6; // Assumed
	};

	// Struct for Extra Move Properties that play when a move starts or ends
	struct OtherMoveProperty
	{
		gameAddr32 requirements_addr; // Assumed
		uint16_t extraprop; // Assumed
		uint16_t value; // Assumed
	};

	struct Move
	{
		gameAddr32 name_addr;
		gameAddr32 anim_name_addr;
		gameAddr32 anim_addr;
		uint32_t vuln;
		uint32_t hitlevel;
		gameAddr32 cancel_addr;
		uint16_t transition;
		uint16_t moveId_val2; // Clearly related to current character ID
		uint16_t moveId_val1; // Clearly related to move ID
		int16_t _0x1e_short;
		gameAddr32 hit_condition_addr;
		int16_t anim_len;
		uint16_t airborne_start;
		uint16_t airborne_end;
		uint16_t ground_fall;
		gameAddr32 voicelip_addr; // Can be NULL
		gameAddr32 extra_move_property_addr; // Can be NULL
		gameAddr32 move_start_extraprop_addr; // Can be NULL
		gameAddr32 move_end_extraprop_addr; // Can be NULL
		int32_t _0x98_int; // facing/extras
		uint32_t hitbox_location;
		uint16_t first_active_frame;
		uint16_t last_active_frame;
		int16_t _0x6c_short;
		uint16_t distance;
	};


	struct MvlPlayable
	{
		uint16_t p2_action;
		uint16_t distance;
		uint16_t p2_rotation;
		uint16_t _unk0x6;
		uint16_t _unk0x8;
		uint16_t p1_facing_related;
		uint16_t _unk0xc;
		uint16_t input_count;
		gameAddr32 input_addr;
	};

	struct MvlInput
	{
		union {
			struct {
				Byte directions;
				Byte buttons;
			};
			uint16_t command;
		};
		uint8_t frame_duration;
		uint8_t trigger_highlight;
	};

	struct MvlDisplayable
	{
		gameAddr32 all_translation_offsets[7];
		uint32_t _0x1c;
		uint32_t _0x20;
		uint32_t _0x24;
	};

	// -- Other -- //

	struct MovesetTable
	{
		union {
			struct {
				gameAddr32 reactions;
				uint32_t reactionsCount;

				gameAddr32 requirement;
				uint32_t requirementCount;

				gameAddr32 hitCondition;
				uint32_t hitConditionCount;

				gameAddr32 pushback;
				uint32_t pushbackCount;

				gameAddr32 pushbackExtradata;
				uint32_t pushbackExtradataCount;

				gameAddr32 cancel;
				uint32_t cancelCount;

				gameAddr32 groupCancel;
				uint32_t groupCancelCount;

				gameAddr32 cancelExtradata;
				uint32_t cancelExtradataCount;

				gameAddr32 extraMoveProperty;
				uint32_t extraMovePropertyCount;

				gameAddr32 moveBeginningProp;
				uint32_t moveBeginningPropCount; // 0 on every playable character

				gameAddr32 moveEndingProp;
				uint32_t moveEndingPropCount; // 0 on every playable character

				gameAddr32 move;
				uint32_t moveCount;

				gameAddr32 voiceclip;
				uint32_t voiceclipCount;

				gameAddr32 inputSequence;
				uint32_t inputSequenceCount;

				gameAddr32 input;
				uint32_t inputCount;

				gameAddr32 StructA1; // Size ??
				uint32_t StructA1Count; // These are 0 for every moveset i tried in T6's training mode

				gameAddr32 mvlPlayable;
				uint32_t mvlPlayableCount;

				gameAddr32 mvlInput;
				uint32_t mvlInputCount;

				gameAddr32 mvlDisplayable;
				uint32_t mvlDisplayableCount;

				gameAddr32 StructA2; // Size ??
				uint32_t StructA2Count; // These are 0 for every moveset i tried in T6's training mode

				gameAddr32 StructA3; // Size ??
				uint32_t StructA3Count; // These are 0 for every moveset i tried in T6's training mode

				gameAddr32 StructA4; // Size ??
				uint32_t StructA4Count; // These are 0 for every moveset i tried in T6's training mode

				gameAddr32 StructA5; // Size ??
				uint32_t StructA5Count; // These are 0 for every moveset i tried in T6's training mode
			};
			struct {
				gameAddr32 listAddr;
				uint32_t listCount;
			} entries[23];
		};
	};

	struct MotaHeader
	{
		char mota_string[4];
		bool _is_little_endian; // The first 2 bytes after the string are always 0x0001 or 0x0100
		bool _is_big_endian; // So they can be used to determine endian but they shouldn't, they might be used for different purposes somewhere
		uint16_t unk0x6;
		uint32_t unk0x8;
		uint32_t anim_count;
		char unk0x10;
		bool is_little_endian; // When the game checks for Mota endian, it checks this byte specifically and not the one above
		char unk0x13;
		char unk0x14;
		unsigned int anim_offset_list[1];

		bool IsValid(uint32_t size) const
		{
			return size >= (sizeof(MotaHeader) - 4) && strncmp(mota_string, "MOTA", 4) == 0;
		}

		bool IsBigEndian() const
		{
			return !is_little_endian;
		}
	};

	// Some alternate MOTA format that is not compatible with T7
	struct AltMotaHeader
	{
		uint16_t _0x0; // Seemingly always 1
		uint16_t anim_count;
		uint16_t _0x4; // Has to be smaller than _0x6
		uint16_t _0x6;

		// -- Size depends on anim_count. -- //
		// List of offsets, points to anim_lengths[?] (first bit is discarded to do so)
		uint32_t offsets[1];
		// List of anim lengths
		uint16_t anim_lengths[1];
	};

	struct MotaList
	{
		union {
			struct {
				gameAddr32 mota_0; // Anims
				gameAddr32 mota_1; // Anims
				gameAddr32 hand_1; // Hand
				gameAddr32 hand_2; // Hand
				gameAddr32 face_1; // Face
				gameAddr32 face_2; // Face
				gameAddr32 wings_1; // Wings (probably more to it)
				gameAddr32 wings_2; // Wings (probably more to it)
				gameAddr32 camera_1; // Camera
				gameAddr32 camera_2; // Camera
				gameAddr32 mota_10; // Unknown
				gameAddr32 mota_11; // Unknown
				gameAddr32 _unknown_; // Unknown, but clearly a pointer too, sometimes point to stuff right before a MOTA
			};
			gameAddr32 motas[13];
		};
	};

	struct MovesetInfo
	{
		char _0x0[2];
		bool isInitialized;
		char _0x3[5];
		gameAddr32 character_name_addr;
		gameAddr32 character_creator_addr;
		gameAddr32 date_addr;
		gameAddr32 fulldate_addr;
		uint32_t orig_aliases[36];
		uint32_t current_aliases[36];
		uint16_t unknown_values[36];
		MovesetTable table;
		MotaList motas;
	};
}

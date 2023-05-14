#pragma once

#include <stdint.h>

namespace StructsTTT2
{
	// Custom-made offset list containing the list of blocks that compose our extracted moveset
	// These are arbitrary blocks and do not match the game's moveset 1:1
	struct TKMovesetHeaderBlocks
	{
		union {
			struct {
				// Store moveid aliases and some more i don't understand
				uint64_t movesetInfoBlock;
				// Store a list of offsets toward various structure lists paired with their size
				uint64_t tableBlock;
				// Store a list of offets pointing to mota files
				uint64_t motalistsBlock;
				// Stores an offset to the block containing move names & anim names (they are the same)
				uint64_t nameBlock;
				// Stores an offset to the block containing stucture lists
				uint64_t movesetBlock;
				// Store an offset to the block containing every regular animation used in the moveset. (customly-built)
				uint64_t animationBlock;
				// Store an offset to the block containing the multiple mota files for those that are extracted . (customly-built)
				uint64_t motaBlock;
				// Stores the .mvl file in order to show the custom movelist in training mode
				uint64_t movelistBlock;
			};
			uint64_t blocks[8];
		};
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
		uint32_t condition;
		union
		{
			uint32_t param_unsigned;
			int32_t param_signed;
			float param_float;
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
			uint64_t command;
			struct
			{
				uint32_t direction;
				uint32_t button;
			};
		};
		gameAddr32 requirements_addr;
		gameAddr32 extradata_addr;
		uint32_t detection_start;
		uint32_t detection_end;
		uint32_t starting_frame;
		uint16_t move_id;
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
		uint32_t id;
	};

	struct ExtraMoveProperty
	{
		uint32_t starting_frame;
		uint32_t id;
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
			uint64_t command;
			struct
			{
				uint32_t direction;
				uint32_t button;
			};
		};
	};

	struct InputSequence // todo
	{
		int8_t _0x4_int8; // Apparently unused
		uint8_t input_window_frames; // I assume the max amount of frames between the first &last input
		uint16_t input_amount; // The amount of input to read from input_addr
		gameAddr32 input_addr;
	};

	struct Projectile // todo
	{
		uint32_t vfx_id;
		int32_t _0x4_int;
		uint32_t vfx_variation_id;
		int32_t _0xC_int;
		int32_t _0x10_int;
		int32_t _0x14_int;
		uint32_t delay; // Frames before both velocity components can be applied
		uint32_t vertical_velocity;
		uint32_t horizontal_velocity;
		int32_t _0x24_int; // 0: disable projectile
		uint32_t duration;
		uint32_t no_collision; // default: 0. Non-zero: no collision
		uint32_t size; // 0: no collision
		int32_t _0x34_int;
		uint32_t hit_level;
		int32_t _0x3C_int[6];
		uint32_t voiceclip_on_hit;
		int32_t _0x58_int;
		int32_t _0x5C_int;
		gameAddr32 hit_condition_addr;
		gameAddr32 cancel_addr;
		int32_t _0x70_int;
		int32_t _0x74_int;
		uint32_t can_hitbox_connect;
		int32_t _0x7C_int;
		int32_t _0x80_int;
		uint32_t gravity;
		int32_t _0x88_int[8];
	};

	struct CameraData // todo
	{
		uint32_t _0x0_int;
		uint16_t _0x4_short;
		uint16_t left_side_camera_data;
		uint16_t right_side_camera_data;
		uint16_t _0xA_short;
	};

	struct ThrowCamera // todo
	{
		uint64_t _0x0_llong;
		gameAddr32 cameradata_addr;
	};

	struct UnknownParryRelated
	{
		uint32_t value;
	};

	// Struct for Extra Move Properties that play when a move starts or ends
	struct OtherMoveProperty // todo
	{
		gameAddr32 requirements_addr;
		uint32_t extraprop; // 881 list end value & extraprop values
		uint32_t value;
	};

	struct Move // todo
	{
		gameAddr32 name_addr;
		gameAddr32 anim_name_addr;
		gameAddr32 anim_addr;
		uint32_t vuln;
		uint32_t hitlevel;
		gameAddr32 cancel_addr;
		gameAddr32 _0x28_cancel_addr;
		int32_t _0x30_int__0x28_related;
		int32_t _0x34_int;
		gameAddr32 _0x38_cancel_addr;
		int32_t _0x40_int__0x38_related;
		int32_t _0x44_int;
		gameAddr32 _0x48_cancel_addr;
		uint32_t _0x50_int__0x48_related;
		uint16_t transition;
		int16_t _0x56_short;
		uint16_t moveId_val1; // Clearly related to move ID
		uint16_t moveId_val2; // Clearly related to current character ID
		int16_t _0x5C_short; // Might be the same member as 0x5e (int32)
		int16_t _0x5E_short;
		gameAddr32 hit_condition_addr;
		int32_t anim_len;
		uint32_t airborne_start;
		uint32_t airborne_end;
		uint32_t ground_fall;
		gameAddr32 voicelip_addr; // Can be NULL
		gameAddr32 extra_move_property_addr; // Can be NULL
		gameAddr32 move_start_extraprop_addr; // Can be NULL
		gameAddr32 move_end_extraprop_addr; // Can be NULL
		int32_t _0x98_int; // facing/extras
		uint32_t hitbox_location;
		uint32_t first_active_frame;
		uint32_t last_active_frame;
		int16_t _0xA8_short;
		uint16_t distance;
		int16_t _0xAC_short;
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

				gameAddr32 projectile;
				uint32_t projectileCount;

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

				gameAddr32 moveBeginningProp; // Extraprops that play when a move starts
				uint32_t moveBeginningPropCount;

				gameAddr32 moveEndingProp; // Extraprops that play when a move ends
				uint32_t moveEndingPropCount;

				gameAddr32 move;
				uint32_t moveCount;

				gameAddr32 voiceclip;
				uint32_t voiceclipCount;

				gameAddr32 inputSequence;
				uint32_t inputSequenceCount;

				gameAddr32 input;
				uint32_t inputCount;

				gameAddr32 unknownParryRelated;
				uint32_t unknownParryRelatedCount;

				gameAddr32 cameraData;
				uint32_t cameraDataCount;

				gameAddr32 throwCameras;
				uint32_t throwCamerasCount;
			};
			struct {
				gameAddr32 listAddr;
				uint32_t listCount;
			} entries[19];
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

		bool IsBigEndian()
		{
			return !is_little_endian;
		}
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
		uint16_t orig_aliases[56];
		uint16_t current_aliases[56];
		uint16_t unknown_aliases[36];
		MovesetTable table;
		MotaList motas;
	};
}

#pragma once

#include <stdint.h>

// Contains the structs used for Extraction/Importation
namespace StructsT7
{
	// -- Main data type -- //

	struct PushbackExtradata
	{
		uint16_t horizontal_offset;
	};

	struct Pushback
	{
		int16_t duration;
		int16_t displacement;
		int32_t num_of_loops;
		PushbackExtradata* extradata_addr;
	};

	struct Requirement
	{
		uint32_t req;
		uint32_t param;
	};

	struct CancelExtradata
	{
		uint32_t value;
	};

	struct Cancel
	{
		uint64_t command;
		Requirement* requirement_addr;
		CancelExtradata* extradata_addr;
		uint32_t detection_start;
		uint32_t detection_end;
		uint32_t starting_frame;
		uint16_t move_id;
		uint16_t cancel_option;
	};

	struct Reactions
	{
		// 0 = 0 when writing (might be a ptr)
		Pushback* front_pushback;
		Pushback* backturned_pushback;
		Pushback* left_side_pushback;
		Pushback* right_side_pushback;
		Pushback* front_counterhit_pushback; // If you ever wondered why your CH launcher didn't launch after a sidestep, that's why
		Pushback* downed_pushback;
		Pushback* block_pushback;

		uint16_t front_direction;
		uint16_t back_direction;
		uint16_t left_side_direction;
		uint16_t right_side_direction;
		uint16_t front_counterhit_direction;
		uint16_t downed_direction;

		uint64_t _0x44_long; // 0,  Not actually sure it is a long

		uint16_t vertical_pushback;
		uint16_t standing_moveId;
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
		Requirement* requirement_addr;
		uint64_t damage;
		Reactions* reactions_addr;
	};

	struct Voiceclip
	{
		uint32_t id;
	};

	struct ExtraMoveProperty
	{
		uint32_t starting_frame;
		uint32_t id;
		uint32_t param;
	};

	struct Input
	{
		// todo: name properly 
		int32_t _0x00_int;
		int32_t _0x04_int;
	};

	struct InputSequence
	{
		uint16_t input_window_frames; // I assume the max amount of frames between the first &last input
		uint16_t input_amount;
		int32_t _0x4_int; // Apparently unused
		Input* input_addr;
	};

	struct Projectile
	{
		uint32_t vfx_id;
		int32_t _0x4_int;
		uint32_t vfx_variation_id;
		int32_t _0xC_int;
		int32_t _0x10_int;
		int32_t _0x14_int;
		uint32_t delay; // Frames before both velocity components can be applied
		uint32_t vertical_velocity;
		uint32_t horizonal_velocity;
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
		HitCondition* hit_condition_addr;
		Cancel* cancel_addr;
		int32_t _0x70_int;
		int32_t _0x74_int;
		uint32_t can_hitbox_connect;
		int32_t _0x7C_int;
		int32_t _0x80_int;
		uint32_t gravity;
		int32_t _0x88_int[8];
	};

	struct CameraData
	{
		int32_t _0x0_int;
		int16_t _0x4_short;
		int16_t left_side_camera_data;
		int16_t right_side_camera_data;
		int16_t _0xA_short;
	};

	struct ThrowData
	{
		int32_t _0x0_int;
		CameraData* cameradata_addr;
	};

	struct UnknownParryRelated
	{
		uint32_t value;
	};

	struct Move
	{
		// 0 = 0 when writing (might be a ptr)
		char* name;
		char* anim_name;
		void* anim_addr;
		uint32_t vuln;
		uint32_t hitlevel;
		Cancel* cancel_addr;
		int64_t _0x28_llong; // 0
		int64_t _0x30_llong;
		int64_t _0x38_llong;
		int64_t _0x40_llong;
		int64_t _0x48_llong; // 0
		int32_t _0x50_int;
		uint16_t transition;
		int16_t _0x56_short;
		uint16_t moveId_val1; // Clearly related to move ID
		uint16_t moveId_val2; // Clearly related to current character ID
		int16_t _0x5C_short; // Might be the same member as 0x5e (int32)
		int16_t _0x5E_short; // 0
		HitCondition* hit_condition_addr;
		uint32_t anim_len;
		uint32_t airborne_start;
		uint32_t airborne_end;
		uint32_t ground_fall;
		Voiceclip* voicelip_addr;
		ExtraMoveProperty* extra_move_property_addr;
		int64_t _0x88_llong; // 0
		int64_t _0x90_llong; // 0
		int32_t _0x98_int;
		int32_t hitbox_location;
		uint32_t first_active_frame;
		uint32_t last_active_frame;
		int16_t _0xA8_short; // facing/extras
		uint16_t distance;
		int16_t _0xAC_short;
	};


	// -- Other -- //

	struct MotaList
	{
		void* mota_0; // Anims
		void* mota_1; // Anims
		void* mota_2; // Hand
		void* mota_3; // Hand
		void* mota_4; // Face
		void* mota_5; // Face
		void* mota_6; // Wings (probably more to it)
		void* mota_7; // Wings (probablty more to it)
		void* mota_8; // Camera
		void* mota_9; // Camera
		void* mota_10; // Unknown
		void* mota_11; // Unknown
		void* _unknown_; // Unknown, but clearly a pointer too, sometimes point to stuff right before a MOTA
	};

	struct MovesetLists
	{
		Reactions* reactions;
		uint64_t reactionsCount;

		Requirement* requirement;
		uint64_t requirementCount;

		HitCondition* hitCondition;
		uint64_t hitConditionCount;

		Projectile* projectile;
		uint64_t projectileCount;

		Pushback* pushback;
		uint64_t pushbackCount;

		PushbackExtradata* pushbackExtradata;
		uint64_t pushbackExtradataCount;

		Cancel* cancel;
		uint64_t cancelCount;

		Cancel* groupCancel;
		uint64_t groupCancelCount;

		CancelExtradata* cancelExtradata;
		uint64_t cancelExtradataCount;

		ExtraMoveProperty* extraMoveProperty;
		uint64_t extraMovePropertyCount;

		void* unknown_0x1f0;
		uint64_t unknown_0x1f8;

		void* unknown_0x200;
		uint64_t unknown_0x208;

		Move* move;
		uint64_t moveCount;

		Voiceclip* voiceclip;
		uint64_t voiceclipCount;

		InputSequence* inputSequence;
		uint64_t inputSequenceCount;

		Input* input;
		uint64_t inputCount;

		UnknownParryRelated* unknownParryRelated;
		uint64_t unknownParryRelatedCount;

		CameraData* cameraData;
		uint64_t cameraDataCount;

		ThrowData* throws;
		uint64_t throwsCount;
	};
}

// Contains most of the same structs as above, but with no ptr (gameAddr instead)
// Helps reduce the need of casting throughout the code and that is important for code clarity
namespace StructsT7Helpers
{
	struct Pushback
	{
		int16_t duration;
		int16_t displacement;
		int32_t num_of_loops;
		gameAddr extradata_addr;
	};

	struct Cancel
	{
		uint64_t command;
		gameAddr requirement_addr;
		gameAddr extradata_addr;
		uint32_t detection_start;
		uint32_t detection_end;
		uint32_t starting_frame;
		uint16_t move_id;
		uint16_t cancel_option;
	};

	struct Reactions
	{
		// 0 = 0 when writing (might be a ptr)
		gameAddr front_pushback;
		gameAddr backturned_pushback;
		gameAddr left_side_pushback;
		gameAddr right_side_pushback;
		gameAddr front_counterhit_pushback; // If you ever wondered why your CH launcher didn't launch after a sidestep, that's why
		gameAddr downed_pushback;
		gameAddr block_pushback;

		uint16_t front_direction;
		uint16_t back_direction;
		uint16_t left_side_direction;
		uint16_t right_side_direction;
		uint16_t front_counterhit_direction;
		uint16_t downed_direction;

		uint64_t _0x44_long; // 0,  Not actually sure it is a long

		uint16_t vertical_pushback;
		uint16_t standing_moveId;
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
		gameAddr requirement_addr;
		uint64_t damage;
		gameAddr reactions_addr;
	};

	struct InputSequence
	{
		uint16_t input_window_frames; // I assume the max amount of frames between the first &last input
		uint16_t input_amount;
		int32_t _0x4_int; // Apparently unused
		gameAddr input_addr;
	};

	struct Projectile
	{
		uint32_t vfx_id;
		int32_t _0x4_int;
		uint32_t vfx_variation_id;
		int32_t _0xC_int;
		int32_t _0x10_int;
		int32_t _0x14_int;
		uint32_t delay; // Frames before both velocity components can be applied
		uint32_t vertical_velocity;
		uint32_t horizonal_velocity;
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
		gameAddr hit_condition_addr;
		gameAddr cancel_addr;
		int32_t _0x70_int;
		int32_t _0x74_int;
		uint32_t can_hitbox_connect;
		int32_t _0x7C_int;
		int32_t _0x80_int;
		uint32_t gravity;
		int32_t _0x88_int[8];
	};


	struct ThrowData
	{
		int32_t _0x0_int;
		gameAddr cameradata_addr;
	};

	struct Move
	{
		// 0 = 0 when writing (might be a ptr)
		gameAddr name;
		gameAddr anim_name;
		gameAddr anim_addr;
		uint32_t vuln;
		uint32_t hitlevel;
		gameAddr cancel_addr;
		int64_t _0x28_llong; // 0
		int64_t _0x30_llong;
		int64_t _0x38_llong;
		int64_t _0x40_llong;
		int64_t _0x48_llong; // 0
		int32_t _0x50_int;
		uint16_t transition;
		int16_t _0x56_short;
		uint16_t moveId_val1; // Clearly related to move ID
		uint16_t moveId_val2; // Clearly related to current character ID
		int16_t _0x5C_short; // Might be the same member as 0x5e (int32)
		int16_t _0x5E_short; // 0
		gameAddr hit_condition_addr;
		uint32_t anim_len;
		uint32_t airborne_start;
		uint32_t airborne_end;
		uint32_t ground_fall;
		gameAddr voicelip_addr;
		gameAddr extra_move_property_addr;
		int64_t _0x88_llong; // 0
		int64_t _0x90_llong; // 0
		int32_t _0x98_int;
		int32_t hitbox_location;
		uint32_t first_active_frame;
		uint32_t last_active_frame;
		int16_t _0xA8_short; // facing/extras
		uint16_t distance;
		int16_t _0xAC_short;
	};

	// -- Other -- //

	struct MovesetLists
	{
		gameAddr reactions;
		uint64_t reactionsCount;

		gameAddr requirement;
		uint64_t requirementCount;

		gameAddr hitCondition;
		uint64_t hitConditionCount;

		gameAddr projectile;
		uint64_t projectileCount;

		gameAddr pushback;
		uint64_t pushbackCount;

		gameAddr pushbackExtradata;
		uint64_t pushbackExtradataCount;

		gameAddr cancel;
		uint64_t cancelCount;

		gameAddr groupCancel;
		uint64_t groupCancelCount;

		gameAddr cancelExtradata;
		uint64_t cancelExtradataCount;

		gameAddr extraMoveProperty;
		uint64_t extraMovePropertyCount;

		gameAddr unknown_0x1f0;
		uint64_t unknown_0x1f8;

		gameAddr unknown_0x200;
		uint64_t unknown_0x208;

		gameAddr move;
		uint64_t moveCount;

		gameAddr voiceclip;
		uint64_t voiceclipCount;

		gameAddr inputSequence;
		uint64_t inputSequenceCount;

		gameAddr input;
		uint64_t inputCount;

		gameAddr unknownParryRelated;
		uint64_t unknownParryRelatedCount;

		gameAddr cameraData;
		uint64_t cameraDataCount;

		gameAddr throws;
		uint64_t throwsCount;
	};
};

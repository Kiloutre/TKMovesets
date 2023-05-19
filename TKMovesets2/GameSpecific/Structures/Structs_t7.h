#pragma once

#include "GameTypes.h"

namespace StructsT7
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
		PushbackExtradata* extradata_addr;
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
		Requirement* requirements_addr;
		CancelExtradata* extradata_addr;
		uint32_t detection_start;
		uint32_t detection_end;
		uint32_t starting_frame;
		uint16_t move_id;
		uint16_t cancel_option;
	};

	struct Reactions
	{
		union {
			struct {
				Pushback* front_pushback;
				Pushback* backturned_pushback;
				Pushback* left_side_pushback;
				Pushback* right_side_pushback;
				Pushback* front_counterhit_pushback; // If you ever wondered why your CH launcher didn't launch after a sidestep, that's why
				Pushback* downed_pushback;
				Pushback* block_pushback;
			};
			Pushback* pushbacks[7];
		};

		uint16_t front_direction;
		uint16_t back_direction;
		uint16_t left_side_direction;
		uint16_t right_side_direction;
		uint16_t front_counterhit_direction;
		uint16_t downed_direction;

		uint32_t _0x44_int; 
		uint32_t _0x48_int; 

		union {
			struct {
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
			uint16_t moveids[16];
		};
	};

	struct HitCondition
	{
		Requirement* requirements_addr;
		uint32_t damage;
		uint32_t _0xC_int; // What the hell is this? since when has this been here?
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

	struct InputSequence
	{
		uint16_t input_window_frames; // I assume the max amount of frames between the first &last input
		uint16_t input_amount; // The amount of input to read from input_addr
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
		uint32_t _0x0_int;
		uint16_t _0x4_short;
		uint16_t left_side_camera_data;
		uint16_t right_side_camera_data;
		uint16_t _0xA_short;
	};

	struct ThrowCamera
	{
		uint64_t _0x0_llong;
		CameraData* cameradata_addr;
	};

	struct UnknownParryRelated
	{
		uint32_t value;
	};

	// Struct for Extra Move Properties that play when a move starts or ends
	struct OtherMoveProperty
	{
		Requirement* requirements_addr;
		uint32_t extraprop; // 881 list end value & extraprop values
		uint32_t value;
	};

	struct Move
	{
		char* name_addr;
		char* anim_name_addr;
		void* anim_addr;
		uint32_t vuln;
		uint32_t hitlevel;
		Cancel* cancel_addr;
		Cancel* _0x28_cancel_addr;
		int32_t _0x30_int__0x28_related;
		int32_t _0x34_int;
		Cancel* _0x38_cancel_addr;
		int32_t _0x40_int__0x38_related;
		int32_t _0x44_int;
		Cancel* _0x48_cancel_addr;
		uint32_t _0x50_int__0x48_related;
		uint16_t transition;
		int16_t _0x56_short;
		uint16_t moveId_val1; // Clearly related to move ID
		uint16_t moveId_val2; // Clearly related to current character ID
		int16_t _0x5C_short; // Might be the same member as 0x5e (int32)
		int16_t _0x5E_short;
		HitCondition* hit_condition_addr;
		int32_t anim_len;
		uint32_t airborne_start;
		uint32_t airborne_end;
		uint32_t ground_fall;
		Voiceclip* voicelip_addr; // Can be NULL
		ExtraMoveProperty* extra_move_property_addr; // Can be NULL
		OtherMoveProperty* move_start_extraprop_addr; // Can be NULL
		OtherMoveProperty* move_end_extraprop_addr; // Can be NULL
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

				OtherMoveProperty* moveBeginningProp; // Extraprops that play when a move starts
				uint64_t moveBeginningPropCount;

				OtherMoveProperty* moveEndingProp; // Extraprops that play when a move ends
				uint64_t moveEndingPropCount;

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

				ThrowCamera* throwCameras;
				uint64_t throwCamerasCount;
			};
			struct {
				void* listAddr;
				uint64_t listCount;
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
					MotaHeader* mota_0; // Anims
					MotaHeader* mota_1; // Anims
					MotaHeader* hand_1; // Hand
					MotaHeader* hand_2; // Hand
					MotaHeader* face_1; // Face
					MotaHeader* face_2; // Face
					MotaHeader* wings_1; // Wings (probably more to it)
					MotaHeader* wings_2; // Wings (probably more to it)
					MotaHeader* camera_1; // Camera
					MotaHeader* camera_2; // Camera
					MotaHeader* mota_10; // Unknown
					MotaHeader* mota_11; // Unknown
					MotaHeader* _unknown_; // Unknown, but clearly a pointer too, sometimes point to stuff right before a MOTA
			};
			MotaHeader* motas[13];
		};
	};

	struct MovesetInfo
	{
		char _0x0[2];
		bool isInitialized;
		char _0x3[5];
		char* character_name_addr;
		char* character_creator_addr;
		char* date_addr;
		char* fulldate_addr;
		uint16_t orig_aliases[56];
		uint16_t current_aliases[56];
		uint16_t unknown_aliases[36];
		MovesetTable table;
		MotaList motas;
	};

	// Displayable movelist structures

	// Inputs played by bot in movelist display
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
		uint8_t _unkn_0x2;
	};

	// .mvl file
	struct MvlHead
	{
		char mvlString[4];
		int32_t _unk0x4;
		int32_t displayables_count;
		int32_t playables_count;
		int32_t _unk0x10;
		uint32_t displayables_offset;
		uint32_t playables_offset;
		uint32_t inputs_offset;
		char translation_strings[1];
	};

	typedef uint32_t MvlDisplayableType;
	enum MvlDisplayableType_
	{
		MvlDisplayableType_Disabled       = 0x2,
		MvlDisplayableType_Category       = 0x8,
		MvlDisplayableType_Regular        = 0x10000,
		MvlDisplayableType_ComboListStart = 0xA,
		MvlDisplayableType_Unknown        = 0xD,
	};

	struct MvlDisplayable
	{
		union {
			// Normally, these are uints in files and negative ints in memory, both in our software and in the actual game
			// They are never really high though so we can just store them as ints all the times with no worries
			struct {
				int32_t title_translation_japanese;
				int32_t title_translation_english;
				int32_t title_translation_japanese_2;
				int32_t title_translation_other;
				int32_t translation_offsets_japanese;
				int32_t translation_offsets_english;
				int32_t translation_offsets_portuguese;
				int32_t translation_offsets_spanish;
				int32_t translation_offsets_french;
				int32_t translation_offsets_italian;
				int32_t translation_offsets_german;
				int32_t translation_offsets_mexican;
				int32_t translation_offsets_cyrillic;
				int32_t translation_offsets_arabic;
				int32_t translation_offsets_chinese;
				int32_t translation_offsets_korean;
			};
			struct {
				int32_t title_translation_offsets[4];
				int32_t translation_offsets[12];
			};
			int32_t all_translation_offsets[16];
		};
		uint32_t _unk0x40;
		int16_t playable_id;
		uint16_t _unk0x46;
		MvlDisplayableType type;
		uint32_t _unk0x4c;
		uint32_t _unk0x50;
		uint32_t _unk0x54;
		uint32_t _unk0x58;
		uint32_t _unk0x5c;
		uint32_t _unk0x60;
		uint32_t _unk0x64;
		uint32_t _unk0x68;
		uint32_t _unk0x6c;
		uint32_t _unk0x70;
		uint32_t _unk0x74;
		uint32_t _unk0x78;
		uint32_t _unk0x7c;
		uint32_t _unk0x80;
		uint32_t _unk0x84;
		uint32_t _unk0x88;
		uint32_t _unk0x8c;
		uint32_t _unk0x90;
		uint32_t _unk0x94;
		uint32_t _unk0x98;
		uint32_t _unk0x9c;
		uint32_t _unk0xa0;
		uint32_t _unk0xa4;
		uint32_t _unk0xa8;
		uint32_t _unk0xac;
		uint32_t _unk0xb0;
		uint32_t _unk0xb4;
		uint32_t _unk0xb8;
		uint32_t _unk0xbc;
		uint32_t _unk0xc0;
		uint32_t _unk0xc4;
		uint32_t _unk0xc8;
		uint32_t _unk0xcc;
		uint32_t _unk0xd0;
		uint32_t _unk0xd4;
		uint32_t _unk0xd8;
		uint32_t _unk0xdc;
		uint32_t _unk0xe0;
		uint32_t _unk0xe4;
		uint32_t _unk0xe8;
		uint32_t _unk0xec;
		uint32_t _unk0xf0;
		uint32_t _unk0xf4;
		uint32_t _unk0xf8;
		uint32_t _unk0xfc;
		uint32_t _unk0x100;
		uint32_t _unk0x104;
		uint32_t _unk0x108;
		uint32_t _unk0x10c;
		uint32_t _unk0x110;
		uint32_t _unk0x114;
		uint32_t _unk0x118;
		uint32_t _unk0x11c;
		uint32_t _unk0x120;
		uint32_t _unk0x124;
		uint32_t _unk0x128;
		uint32_t _unk0x12c;
		uint32_t _unk0x130;
		uint32_t _unk0x134;
		uint32_t _unk0x138;
		uint32_t _unk0x13c;
		uint32_t _unk0x140;
		uint32_t _unk0x144;
		uint32_t _unk0x148;
		uint32_t icons;
		uint8_t icons_2;
		uint8_t combo_damage;
		uint8_t combo_difficulty;
		uint8_t _unk0x153;
		uint32_t _unk0x154;
		uint32_t _unk0x158;
		uint32_t _unk0x15c;
		uint32_t _unk0x160;
		uint32_t _unk0x164;
		uint32_t _unk0x168;
		uint32_t _unk0x16c;
		uint32_t _unk0x170;
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
		uint32_t input_sequence_offset;
		uint16_t has_rage;
		uint16_t _unk0x16;
	};

	// Struct that manages the training mode movelist in memory
	struct MvlManager
	{
		uint32_t playingType;
		uint32_t _unk0x4;
		void* player;
		uint32_t displayableEntriesCount;
		uint32_t _unk0x14;
		void* mvlDisplayableBlock;
		MvlHead* mvlHead;
		uint32_t _unk0x28;
		uint32_t _unk0x2c;
		uint32_t selected_entry;
		uint32_t _unk0x34;
		uint32_t _unk0x38;
		uint32_t _unk0x3c;
		uint32_t _unk0x40;
		uint32_t _unk0x44;
		uint32_t _unk0x48;
		uint32_t _unk0x4c;
		uint32_t _unk0x50;
		uint32_t _unk0x54;
		uint32_t _unk0x58;
		uint32_t _unk0x5c;
		MvlInput* sequenceEnd;
		MvlInput* sequenceStart;
		int32_t frame;
		int32_t currentInputFrame;
		uint32_t _unk0x78;
		uint32_t _unk0x7c;
	};
}

// Contains most of the same structs as above, but with no ptr (gameAddr instead)
// Helps reduce the need of casting throughout the code and that is important for code clarity
namespace StructsT7_gameAddr
{
	struct Pushback
	{
		uint16_t duration;
		uint16_t displacement;
		uint32_t num_of_loops;
		gameAddr extradata_addr;
	};

	struct Cancel
	{
		union
		{
			int64_t command;
			struct
			{
				int32_t input;
				int32_t direction;
			};
		};
		gameAddr requirements_addr;
		gameAddr extradata_addr;
		uint32_t detection_start;
		uint32_t detection_end;
		uint32_t starting_frame;
		uint16_t move_id;
		uint16_t cancel_option;
	};

	struct Reactions
	{
		union {
			struct {
				gameAddr front_pushback;
				gameAddr backturned_pushback;
				gameAddr left_side_pushback;
				gameAddr right_side_pushback;
				gameAddr front_counterhit_pushback; // If you ever wondered why your CH launcher didn't launch after a sidestep, that's why
				gameAddr downed_pushback;
				gameAddr block_pushback;
			};
			gameAddr pushbacks[7];
		};

		uint16_t front_direction;
		uint16_t back_direction;
		uint16_t left_side_direction;
		uint16_t right_side_direction;
		uint16_t front_counterhit_direction;
		uint16_t downed_direction;

		uint32_t _0x44_int;
		uint32_t _0x48_int;

		union {
			struct {
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
			uint16_t moveids[16];
		};
	};

	struct HitCondition
	{
		gameAddr requirements_addr;
		uint32_t damage;
		uint32_t _0xC_int; // What the hell is this? since when has this been here?
		gameAddr reactions_addr;
	};

	struct InputSequence
	{
		uint16_t input_window_frames; // I assume the max amount of frames between the first &last input
		uint16_t input_amount; // The amount of inputs to read from inputAddr
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


	struct ThrowCamera
	{
		uint64_t _0x0_llong;
		gameAddr cameradata_addr;
	};

	struct OtherMoveProperty
	{
		gameAddr requirements_addr;
		uint32_t extraprop; // 881 list end value & extraprop values
		uint32_t value; // Often small values
	};

	struct Move
	{
		gameAddr name_addr;
		gameAddr anim_name_addr;
		gameAddr anim_addr;
		uint32_t vuln;
		uint32_t hitlevel;
		gameAddr cancel_addr;
		gameAddr _0x28_cancel_addr;
		int32_t _0x30_int__0x28_related; // i believe this is 2x4 bytes
		int32_t _0x34_int;
		gameAddr _0x38_cancel_addr;
		int32_t _0x40_int__0x38_related; // i believe this is 2x4 bytes
		uint32_t _0x44_int;
		gameAddr _0x48_cancel_addr;
		int32_t _0x50_int__0x48_related;
		uint16_t transition;
		int16_t _0x56_short;
		uint16_t moveId_val1; // Clearly related to move ID
		uint16_t moveId_val2; // Clearly related to current character ID
		int16_t _0x5C_short; // Might be the same member as 0x5e (int32)
		int16_t _0x5E_short;
		gameAddr hit_condition_addr;
		uint32_t anim_len;
		uint32_t airborne_start;
		uint32_t airborne_end;
		uint32_t ground_fall;
		gameAddr voicelip_addr; // Can be NULL
		gameAddr extra_move_property_addr; // Can be NULL
		gameAddr move_start_extraprop_addr; // Can be NULL
		gameAddr move_end_extraprop_addr; // Can be NULL
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

				gameAddr moveBeginningProp; // Extraprops that play when a move starts
				uint64_t moveBeginningPropCount;

				gameAddr moveEndingProp; // Extraprops that play when a move ends
				uint64_t moveEndingPropCount;

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

				gameAddr throwCameras;
				uint64_t throwCamerasCount;
			};
			struct {
				gameAddr listAddr;
				uint64_t listCount;
			} entries[19];
		};
	};

	struct MotaList
	{
		union {
			struct {
				gameAddr mota_0; // Anims
				gameAddr mota_1; // Anims
				gameAddr mota_2; // Hand
				gameAddr mota_3; // Hand
				gameAddr mota_4; // Face
				gameAddr mota_5; // Face
				gameAddr mota_6; // Wings (probably more to it)
				gameAddr mota_7; // Wings (probablty more to it)
				gameAddr mota_8; // Camera
				gameAddr mota_9; // Camera
				gameAddr mota_10; // Unknown
				gameAddr mota_11; // Unknown
				gameAddr _unknown_; // Unknown, but clearly a pointer too, sometimes point to stuff right before a MOTA
			};
			gameAddr motas[13];
		};
	};

	struct MovesetInfo
	{
		char _0x0[2];
		bool isInitialized;
		char _0x3[5];
		char* character_name_addr;
		char* character_creator_addr;
		char* date_addr;
		char* fulldate_addr;
		uint16_t orig_aliases[56];
		uint16_t current_aliases[56];
		uint16_t unknown_aliases[36];
		MovesetTable table;
		MotaList motas;
	};
};

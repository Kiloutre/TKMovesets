#pragma once

#include "GameAddresses.h"
// Contains the structs used for Extraction/Importation
//template<size_t S> class Sizer { }; Sizer<sizeof(t7structs::Move)> foo;

namespace t7structs
{
	// -- Main data type -- //

	struct Pushback
	{
		char content[0x10];
	};

	struct PushbackExtradata
	{
		int16_t value;
	};

	struct Requirement
	{
		int32_t req;
		int32_t param;
	};

	struct CancelExtradata
	{
		int32_t value;
	};

	struct Cancel
	{

		char content[0x28];
	};

	struct Reactions
	{
		char content[0x70];
	};

	struct HitCondition
	{
		char content[0x18];
	};

	struct ExtraMoveProperty
	{
		int32_t starting_frame;
		int32_t id;
		int32_t param;
	};

	struct Voiceclip
	{
		int32_t value;
	};

	struct InputExtradata
	{
		int32_t _u1_int;
		int32_t _u2_int;
	};

	struct InputSequence
	{
		char content[0x10];
	};

	struct Projectile
	{
		char content[0xA8];
	};

	struct ThrowExtra
	{
		char content[0xC];
	};

	struct Throw
	{
		char content[0x10];
	};

	struct UnknownParryRelated
	{
		int32_t value;
	};

	struct Move
	{
		// Todo: rename what needs to be renamed
		char* name;
		char* anim_name;
		void* anim_addr;
		uint32_t vuln;
		uint32_t hitlevel;
		t7structs::Cancel* cancel_addr;
		int64_t _u1_llong; // = 0 always when writin, is a ptr but no idea what it points to
		int64_t _u2_llong;
		int64_t _u3_llong;
		int64_t _u4_llong;
		int64_t _u5_llong; // = 0 always when writin, is a ptr but no idea what it points to
		int32_t _u6_int;
		uint16_t transition;
		int16_t _u7_short;
		int16_t moveId_val1; // currmoveId + 1 - your_character_id
		int16_t moveId_val2;
		int32_t _u10_int;
		t7structs::HitCondition* hit_condition_addr;
		uint32_t anim_len;
		int32_t _u11_int;
		int32_t _u12_int;
		int32_t _u13_int;
		t7structs::Voiceclip* voicelip_addr;
		t7structs::ExtraMoveProperty* extra_move_property_addr;
		int64_t _u14_llong; // = 0 always when writing, is a ptr but no idea what it points to
		int64_t _u15_llong; // = 0 always when writin, is a ptr but no idea what it points to
		int32_t hitbox_location;
		uint32_t first_active_frame;
		uint32_t last_active_frame;
		int16_t _u17_short;
		int16_t _u18_short;
		int16_t _u19_short;
	};


	// -- Other -- //

	struct motaList
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
		void* mota_11; // Unnown
	};

	struct movesetLists
	{
		t7structs::Reactions* reactions;
		uint64_t reactionsCount;

		t7structs::Requirement* requirement;
		uint64_t requirementCount;

		t7structs::HitCondition* hitCondition;
		uint64_t hitConditionCount;

		t7structs::Projectile* projectile;
		uint64_t projectileCount;

		t7structs::Pushback* pushback;
		uint64_t pushbackCount;

		t7structs::PushbackExtradata* pushbackExtradata;
		uint64_t pushbackExtradataCount;

		t7structs::Cancel* cancel;
		uint64_t cancelCount;

		t7structs::Cancel* groupCancel;
		uint64_t groupCancelCount;

		t7structs::CancelExtradata* cancelExtradata;
		uint64_t cancelExtradataCount;

		t7structs::ExtraMoveProperty* extraMoveProperty;
		uint64_t extraMovePropertyCount;

		void* unknown_0x1f0;
		uint64_t unknown_0x1f8;

		void* unknown_0x200;
		uint64_t unknown_0x208;

		t7structs::Move* move;
		uint64_t moveCount;

		t7structs::Voiceclip* voiceclip;
		uint64_t voiceclipCount;

		t7structs::InputSequence* inputSequence;
		uint64_t inputSequenceCount;

		t7structs::InputExtradata* inputExtradata;
		uint64_t inputExtradataCount;

		t7structs::UnknownParryRelated* unknownParryRelated;
		uint64_t unknownParryRelatedCount;

		t7structs::ThrowExtra* throwExtra;
		uint64_t throwExtraCount;

		t7structs::Throw* throws;
		uint64_t throwsCount;
	};
}

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
		char content[0x2];
	};

	struct Requirement
	{
		char content[0x8];
	};

	struct CancelExtradata
	{
		char content[0x4];
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
		char content[0xC];
	};

	struct Voiceclip
	{
		char content[0x4];
	};

	struct InputExtradata
	{
		char content[0x8];
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
		char content[0x4];
	};

	struct Move
	{
		// Todo: rename what needs to be renamed
		char* name;
		char* anim_name;
		void* anim_addr;
		int32_t vuln;
		int32_t hitlevel;
		t7structs::Cancel* cancel_addr;
		int64_t _u1_llong; // = 0 always when writin, is a ptr but no idea what it points to
		int64_t _u2_llong;
		int64_t _u3_llong;
		int64_t _u4_llong;
		int64_t _u5_llong; // = 0 always when writin, is a ptr but no idea what it points to
		int32_t _u6_int;
		int16_t transition;
		int16_t _u7_short;
		int16_t _u8_short;
		int16_t _u9_short;
		int32_t _u10_int;
		t7structs::HitCondition* hit_condition_addr;
		int32_t anim_max_len;
		int32_t _u11_int;
		int32_t _u12_int;
		int32_t _u13_int;
		t7structs::Voiceclip* voicelip_addr;
		t7structs::ExtraMoveProperty* extra_move_property_addr;
		int64_t _u14_llong; // = 0 always when writing, is a ptr but no idea what it points to
		int64_t _u15_llong; // = 0 always when writin, is a ptr but no idea what it points to
		int32_t hitbox_location;
		int32_t first_active_frame;
		int32_t last_active_frame;
		int16_t _u17_short;
		int16_t _u18_short;
		int16_t _u19_short;
	};


	// -- Other -- //

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

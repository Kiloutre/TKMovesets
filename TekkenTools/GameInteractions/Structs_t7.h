#pragma once

// Contains the structs used for Extraction/Importation

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

	struct Move
	{
		char content[0xB0];
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


	// -- Other -- //

	struct movesetLists
	{
		t7structs::Reactions* reactions;
		int64_t reactionsCount;

		t7structs::Requirement* requirement;
		int64_t requirementCount;

		t7structs::HitCondition* hitCondition;
		int64_t hitConditionCount;

		t7structs::Projectile* projectile;
		int64_t projectileCount;

		t7structs::Pushback* pushback;
		int64_t pushbackCount;

		t7structs::PushbackExtradata* pushbackExtradata;
		int64_t pushbackExtradataCount;

		t7structs::Cancel* cancel;
		int64_t cancelCount;

		t7structs::Cancel* groupCancel;
		int64_t groupCancelCount;

		t7structs::CancelExtradata* cancelExtradata;
		int64_t cancelExtradataCount;

		t7structs::ExtraMoveProperty* extraMoveProperty;
		int64_t extraMovePropertyCount;

		void* unknown_0x1f0;
		int64_t unknown_0x1f8;

		void* unknown_0x200;
		int64_t unknown_0x208;

		t7structs::Move* move;
		int64_t moveCount;

		t7structs::Voiceclip* Voiceclip;
		int64_t VoiceclipCount;

		t7structs::InputSequence* inputSequence;
		int64_t inputSequenceCount;

		t7structs::InputExtradata* inputExtradata;
		int64_t inputExtradataCount;

		t7structs::UnknownParryRelated* unknownParryRelated;
		int64_t unknownParryRelatedCount;

		t7structs::ThrowExtra* throwExtra;
		int64_t throwExtraCount;

		t7structs::Throw* throws;
		int64_t throwsCount;
	};
}
